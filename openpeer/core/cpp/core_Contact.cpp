/*

 Copyright (c) 2013, SMB Phone Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies,
 either expressed or implied, of the FreeBSD Project.

 */

#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/IHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Stringize.h>
#include <zsLib/helpers.h>
#include <zsLib/XML.h>


namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      typedef IContactForAccount::ForAccountPtr ForAccountPtr;

      ZS_DECLARE_TYPEDEF_PTR(IContactForConversationThread::ForConversationThread, ForConversationThread)
      ZS_DECLARE_TYPEDEF_PTR(IContactForCall::ForCall, ForCall)

      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "core::Contact");
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IContactForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IContactForAccount::toDebug(ForAccountPtr contact)
      {
        return Contact::toDebug(Contact::convert(contact));
      }

      //-----------------------------------------------------------------------
      ForAccountPtr IContactForAccount::createFromPeer(
                                                       AccountPtr account,
                                                       IPeerPtr peer
                                                       )
      {
        return IContactFactory::singleton().createFromPeer(account, peer);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IContactForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IContactForConversationThread::toDebug(ForConversationThreadPtr contact)
      {
        return Contact::toDebug(Contact::convert(contact));
      }

      //-----------------------------------------------------------------------
      ForConversationThreadPtr IContactForConversationThread::createFromPeerFilePublic(
                                                                                       AccountPtr account,
                                                                                       IPeerFilePublicPtr peerFilePublic
                                                                                       )
      {
        return IContactFactory::singleton().createFromPeerFilePublic(account, peerFilePublic);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IContactForCall
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IContactForCall::toDebug(ForCallPtr contact)
      {
        return Contact::toDebug(Contact::convert(contact));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Contact
      #pragma mark

      //-----------------------------------------------------------------------
      Contact::Contact()
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      void Contact::init()
      {
        ZS_LOG_DEBUG(debug("init"))
      }

      //-----------------------------------------------------------------------
      Contact::~Contact()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      ContactPtr Contact::convert(IContactPtr contact)
      {
        return dynamic_pointer_cast<Contact>(contact);
      }

      //-----------------------------------------------------------------------
      ContactPtr Contact::convert(ForAccountPtr contact)
      {
        return dynamic_pointer_cast<Contact>(contact);
      }

      //-----------------------------------------------------------------------
      ContactPtr Contact::convert(ForConversationThreadPtr contact)
      {
        return dynamic_pointer_cast<Contact>(contact);
      }

      //-----------------------------------------------------------------------
      ContactPtr Contact::convert(ForCallPtr contact)
      {
        return dynamic_pointer_cast<Contact>(contact);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Contact => IContact
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Contact::toDebug(IContactPtr contact)
      {
        if (!contact) return ElementPtr();
        return Contact::convert(contact)->toDebug();
      }

      //-----------------------------------------------------------------------
      ContactPtr Contact::createFromPeerFilePublic(
                                                   AccountPtr inAccount,
                                                   IPeerFilePublicPtr peerFilePublic
                                                   )
      {
        UseAccountPtr account(inAccount);

        ZS_THROW_INVALID_ARGUMENT_IF(!peerFilePublic)
        stack::IAccountPtr stackAcount = account->getStackAccount();

        if (!stackAcount) {
          ZS_LOG_ERROR(Detail, slog("stack account is not ready"))
          return ContactPtr();
        }

        IPeerPtr peer = IPeer::create(stackAcount, peerFilePublic);
        if (!peer) {
          ZS_LOG_ERROR(Detail, slog("failed to create peer object"))
          return ContactPtr();
        }

        AutoRecursiveLock lock(*inAccount);

        String peerURI = peer->getPeerURI();
        ContactPtr existingPeer = account->findContact(peerURI);
        if (existingPeer) {
          return existingPeer;
        }

        ContactPtr pThis(new Contact);
        pThis->mThisWeak = pThis;
        pThis->mAccount = account;
        pThis->mPeer = peer;
        pThis->init();
        account->notifyAboutContact(pThis);
        return pThis;
      }
      
      //-----------------------------------------------------------------------
      ContactPtr Contact::getForSelf(IAccountPtr inAccount)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)

        UseAccountPtr account = Account::convert(inAccount);
        return account->getSelfContact();
      }

      //-----------------------------------------------------------------------
      bool Contact::isSelf() const
      {
        ContactPtr pThis = mThisWeak.lock();
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_ERROR(Detail, log("account object is gone"))
          return false;
        }
        return (account->getSelfContact() == pThis);
      }

      //-----------------------------------------------------------------------
      String Contact::getPeerURI() const
      {
        return mPeer->getPeerURI();
      }

      //-----------------------------------------------------------------------
      IPeerFilePublicPtr Contact::getPeerFilePublic() const
      {
        return mPeer->getPeerFilePublic();
      }

      //-----------------------------------------------------------------------
      IAccountPtr Contact::getAssociatedAccount() const
      {
        return Account::convert(mAccount.lock());
      }

      //-----------------------------------------------------------------------
      void Contact::hintAboutLocation(const char *contactsLocationID)
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_ERROR(Detail, log("account object is gone"))
          return;
        }
        account->hintAboutContactLocation(mThisWeak.lock(), contactsLocationID);
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Contact => IContactForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ContactPtr Contact::createFromPeer(
                                         AccountPtr inAccount,
                                         IPeerPtr peer
                                         )
      {
        UseAccountPtr account(inAccount);
        stack::IAccountPtr stackAcount = account->getStackAccount();

        if (!stackAcount) {
          ZS_LOG_ERROR(Detail, slog("stack account is not ready"))
          return ContactPtr();
        }

        AutoRecursiveLock lock(*inAccount);

        ContactPtr existingPeer = account->findContact(peer->getPeerURI());
        if (existingPeer) {
          return existingPeer;
        }

        ContactPtr pThis(new Contact);
        pThis->mThisWeak = pThis;
        pThis->mAccount = account;
        pThis->mPeer = peer;
        pThis->init();
        account->notifyAboutContact(pThis);
        return pThis;
      }

      //-----------------------------------------------------------------------
      IPeerPtr Contact::getPeer() const
      {
        return mPeer;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Contact => IContactForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Contact => IContactForCall
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Contact => IContactForIdentityLookup
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Contact => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Contact::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Contact");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params Contact::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr Contact::toDebug() const
      {
        ElementPtr resultEl = Element::create("core::Contact");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, IPeer::toDebug(mPeer));
        IHelper::debugAppend(resultEl, "is self", isSelf());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IContact
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IContact::toDebug(IContactPtr contact)
    {
      return internal::Contact::toDebug(contact);
    }

    //-------------------------------------------------------------------------
    IContactPtr IContact::createFromPeerFilePublic(
                                                   IAccountPtr account,
                                                   IPeerFilePublicPtr peerFilePublic
                                                   )
    {
      return internal::IContactFactory::singleton().createFromPeerFilePublic(internal::Account::convert(account), peerFilePublic);
    }

    //-------------------------------------------------------------------------
    IContactPtr IContact::getForSelf(IAccountPtr account)
    {
      return internal::IContactFactory::singleton().getForSelf(account);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
