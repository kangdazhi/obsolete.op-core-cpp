/*

 Copyright (c) 2014, Hookflash Inc.
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

#include <openpeer/core/internal/core_Identity.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Stringize.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    using stack::IServiceIdentitySession;
    using stack::message::IdentityInfo;

    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(Identity::ForAccount, ForAccount)

      typedef IStackForInternal UseStack;

      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static IServiceIdentitySession::SessionStates toState(IIdentity::IdentityStates state)
      {
        switch (state)
        {
          case IIdentity::IdentityState_Pending:                                return IServiceIdentitySession::SessionState_Pending;
          case IIdentity::IdentityState_PendingAssociation:                     return IServiceIdentitySession::SessionState_WaitingForAssociationToLockbox;
          case IIdentity::IdentityState_WaitingAttachmentOfDelegate:            return IServiceIdentitySession::SessionState_WaitingAttachmentOfDelegate;
          case IIdentity::IdentityState_WaitingForBrowserWindowToBeLoaded:      return IServiceIdentitySession::SessionState_WaitingForBrowserWindowToBeLoaded;
          case IIdentity::IdentityState_WaitingForBrowserWindowToBeMadeVisible: return IServiceIdentitySession::SessionState_WaitingForBrowserWindowToBeMadeVisible;
          case IIdentity::IdentityState_WaitingForBrowserWindowToClose:         return IServiceIdentitySession::SessionState_WaitingForBrowserWindowToClose;
          case IIdentity::IdentityState_Ready:                                  return IServiceIdentitySession::SessionState_Ready;
          case IIdentity::IdentityState_Shutdown:                               return IServiceIdentitySession::SessionState_Shutdown;
        }

        return IServiceIdentitySession::SessionState_Pending;
      }

      //-----------------------------------------------------------------------
      static IIdentity::IdentityStates toState(IServiceIdentitySession::SessionStates state)
      {
        switch (state)
        {
          case IServiceIdentitySession::SessionState_Pending:                                 return IIdentity::IdentityState_Pending;
          case IServiceIdentitySession::SessionState_WaitingForAssociationToLockbox:          return IIdentity::IdentityState_PendingAssociation;
          case IServiceIdentitySession::SessionState_WaitingAttachmentOfDelegate:             return IIdentity::IdentityState_WaitingAttachmentOfDelegate;
          case IServiceIdentitySession::SessionState_WaitingForBrowserWindowToBeLoaded:       return IIdentity::IdentityState_WaitingForBrowserWindowToBeLoaded;
          case IServiceIdentitySession::SessionState_WaitingForBrowserWindowToBeMadeVisible:  return IIdentity::IdentityState_WaitingForBrowserWindowToBeMadeVisible;
          case IServiceIdentitySession::SessionState_WaitingForBrowserWindowToClose:          return IIdentity::IdentityState_WaitingForBrowserWindowToClose;
          case IServiceIdentitySession::SessionState_Ready:                                   return IIdentity::IdentityState_Ready;
          case IServiceIdentitySession::SessionState_Shutdown:                                return IIdentity::IdentityState_Shutdown;
        }

        return IIdentity::IdentityState_Pending;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IIdentityForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IIdentityForAccount::toDebug(ForAccountPtr identity)
      {
        return Identity::toDebug(Identity::convert(identity));
      }

      //-----------------------------------------------------------------------
      ForAccountPtr IIdentityForAccount::createFromExistingSession(IServiceIdentitySessionPtr session)
      {
        return IIdentityFactory::singleton().createFromExistingSession(session);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Identity
      #pragma mark

      //-----------------------------------------------------------------------
      Identity::Identity(IMessageQueuePtr queue) :
        MessageQueueAssociator(queue),
        mID(zsLib::createPUID())
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void Identity::init()
      {
        AutoRecursiveLock lock(mLock);
        ZS_LOG_DEBUG(log("init called") + ZS_PARAM("identity session id", mSession->getID()))
      }

      //-----------------------------------------------------------------------
      Identity::~Identity()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      IdentityPtr Identity::convert(IIdentityPtr contact)
      {
        return dynamic_pointer_cast<Identity>(contact);
      }

      //-----------------------------------------------------------------------
      IdentityPtr Identity::convert(ForAccountPtr contact)
      {
        return dynamic_pointer_cast<Identity>(contact);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Identity => IIdentity
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Identity::toDebug(IIdentityPtr identity)
      {
        if (!identity) return ElementPtr();
        return Identity::convert(identity)->toDebug();
      }

      //-----------------------------------------------------------------------
      IdentityPtr Identity::login(
                                  IAccountPtr inAccount,
                                  IIdentityDelegatePtr delegate,
                                  const char *identityProviderDomain,
                                  const char *identityURI_or_identityBaseURI,
                                  const char *outerFrameURLUponReload
                                  )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!outerFrameURLUponReload)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityURI_or_identityBaseURI)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityProviderDomain)

        UseAccountPtr account = Account::convert(inAccount);

        IdentityPtr pThis(new Identity(UseStack::queueCore()));
        pThis->mThisWeak = pThis;

        AutoRecursiveLock lock(pThis->mLock);

        pThis->mDelegate = IIdentityDelegateProxy::createWeak(UseStack::queueApplication(), delegate);

        String identity(identityURI_or_identityBaseURI);
        String domain(identityProviderDomain);

        ZS_THROW_INVALID_ARGUMENT_IF(!services::IHelper::isValidDomain(domain))

        IServiceIdentityPtr provider;
        ZS_LOG_DEBUG(pThis->log("preparing bootstrapped network domain") + ZS_PARAM("identity", identity) + ZS_PARAM("domain", domain))

        IBootstrappedNetworkPtr network = IBootstrappedNetwork::prepare(domain);
        if (!network) {
          ZS_LOG_ERROR(Detail, pThis->log("bootstrapper failed for domain specified") + ZS_PARAM("identity", identity) + ZS_PARAM("domain", domain))
          return IdentityPtr();
        }

        provider = IServiceIdentity::createServiceIdentityFrom(network);
        ZS_THROW_BAD_STATE_IF(!provider)

        IServiceNamespaceGrantSessionPtr grantSession = account->getNamespaceGrantSession();
        IServiceLockboxSessionPtr lockboxSession = account->getLockboxSession();

        pThis->mSession = IServiceIdentitySession::loginWithIdentity(pThis, provider, grantSession, lockboxSession, outerFrameURLUponReload, identityURI_or_identityBaseURI);
        if (!pThis->mSession) return IdentityPtr();
        pThis->init();
        account->associateIdentity(pThis);

        return pThis;
      }

      //-----------------------------------------------------------------------
      IdentityPtr Identity::loginWithIdentityPreauthorized(
                                                           IAccountPtr inAccount,
                                                           IIdentityDelegatePtr delegate,
                                                           const char *identityProviderDomain,
                                                           const char *identityURI,
                                                           const char *identityAccessToken,
                                                           const char *identityAccessSecret,
                                                           Time identityAccessSecretExpires
                                                           )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityURI)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityAccessToken)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityAccessSecret)

        UseAccountPtr account = Account::convert(inAccount);

        IdentityPtr pThis(new Identity(UseStack::queueCore()));
        pThis->mThisWeak = pThis;
        pThis->mDelegate = IIdentityDelegateProxy::createWeak(UseStack::queueApplication(), delegate);

        String identity(identityURI);

        String domain(identityProviderDomain);

        ZS_THROW_INVALID_ARGUMENT_IF(!services::IHelper::isValidDomain(domain))

        IServiceIdentityPtr provider;
        ZS_LOG_DEBUG(pThis->log("preparing bootstrapped network domain") + ZS_PARAM("identity", identity) + ZS_PARAM("domain", domain))

        IBootstrappedNetworkPtr network = IBootstrappedNetwork::prepare(domain);
        if (!network) {
          ZS_LOG_ERROR(Detail, pThis->log("bootstrapper failed for domain specified") + ZS_PARAM("identity", identity) + ZS_PARAM("domain", domain))
          return IdentityPtr();
        }

        provider = IServiceIdentity::createServiceIdentityFrom(network);
        ZS_THROW_BAD_STATE_IF(!provider)

        IServiceNamespaceGrantSessionPtr grantSession = account->getNamespaceGrantSession();
        IServiceLockboxSessionPtr lockboxSession = account->getLockboxSession();

        pThis->mSession = IServiceIdentitySession::loginWithIdentityPreauthorized(pThis, provider, grantSession, lockboxSession, identityURI, identityAccessToken, identityAccessSecret, identityAccessSecretExpires);
        if (!pThis->mSession) return IdentityPtr();
        pThis->init();
        account->associateIdentity(pThis);

        return pThis;
      }

      //-----------------------------------------------------------------------
      IIdentity::IdentityStates Identity::getState(
                                                   WORD *outLastErrorCode,
                                                   String *outLastErrorReason
                                                   ) const
      {
        return toState(mSession->getState(outLastErrorCode, outLastErrorReason));
      }

      //-----------------------------------------------------------------------
      bool Identity::isDelegateAttached() const
      {
        return mSession->isDelegateAttached();
      }

      //-----------------------------------------------------------------------
      void Identity::attachDelegate(
                                    IIdentityDelegatePtr delegate,
                                    const char *outerFrameURLUponReload
                                    )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!outerFrameURLUponReload)

        ZS_LOG_DEBUG(log("delegate attach called"))

        // scope: set delegate
        {
          AutoRecursiveLock lock(mLock);
          mDelegate = IIdentityDelegateProxy::createWeak(Stack::queueApplication(), delegate);
        }
        mSession->attachDelegate(mThisWeak.lock(), outerFrameURLUponReload);
      }

      //-----------------------------------------------------------------------
      void Identity::attachDelegateAndPreauthorizedLogin(
                                                         IIdentityDelegatePtr delegate,
                                                         const char *identityAccessToken,
                                                         const char *identityAccessSecret,
                                                         Time identityAccessSecretExpires
                                                         )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityAccessToken)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityAccessSecret)

        ZS_LOG_DEBUG(log("delegate attach and preauthorize login called"))

        // scope: set delegate
        {
          AutoRecursiveLock lock(mLock);
          mDelegate = IIdentityDelegateProxy::createWeak(UseStack::queueApplication(), delegate);
        }
        mSession->attachDelegateAndPreauthorizeLogin(mThisWeak.lock(), identityAccessToken, identityAccessSecret, identityAccessSecretExpires);
      }

      //-----------------------------------------------------------------------
      String Identity::getIdentityURI() const
      {
        return mSession->getIdentityURI();
      }

      //-----------------------------------------------------------------------
      String Identity::getIdentityProviderDomain() const
      {
        return mSession->getIdentityProviderDomain();
      }

      //-----------------------------------------------------------------------
      void Identity::getSelfIdentityContact(IdentityContact &outIdentityContact) const
      {
        IdentityInfo identityInfo;
        mSession->getIdentityInfo(identityInfo);

        Helper::convert(identityInfo, outIdentityContact);
      }

      //-----------------------------------------------------------------------
      String Identity::getInnerBrowserWindowFrameURL() const
      {
        return mSession->getInnerBrowserWindowFrameURL();
      }

      //-----------------------------------------------------------------------
      void Identity::notifyBrowserWindowVisible()
      {
        mSession->notifyBrowserWindowVisible();
      }

      //-----------------------------------------------------------------------
      void Identity::notifyBrowserWindowClosed()
      {
        mSession->notifyBrowserWindowClosed();
      }

      //-----------------------------------------------------------------------
      ElementPtr Identity::getNextMessageForInnerBrowerWindowFrame()
      {
        DocumentPtr doc = mSession->getNextMessageForInnerBrowerWindowFrame();
        if (!doc) return ElementPtr();
        ElementPtr root = doc->getFirstChildElement();
        root->orphan();
        return root;
      }

      //-----------------------------------------------------------------------
      void Identity::handleMessageFromInnerBrowserWindowFrame(ElementPtr message)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)
        DocumentPtr doc = Document::create();
        doc->adoptAsLastChild(message);
        mSession->handleMessageFromInnerBrowserWindowFrame(doc);
      }

      //-----------------------------------------------------------------------
      void Identity::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))
        if (!mSession) return;  // can happen if exception is thrown before pThis is fully constructed

        mSession->cancel();
      }

      //-----------------------------------------------------------------------
      void Identity::startRolodexDownload(const char *inLastDownloadedVersion)
      {
        ZS_LOG_DEBUG(log("start rolodex download called") + ZS_PARAM("identity session id", mSession->getID()) + ZS_PARAM("last downloaded", inLastDownloadedVersion))
        mSession->startRolodexDownload(inLastDownloadedVersion);
      }

      //-----------------------------------------------------------------------
      void Identity::refreshRolodexContacts()
      {
        ZS_LOG_DEBUG(log("refresh rolodex contacts called") + ZS_PARAM("identity session id", mSession->getID()))
        mSession->refreshRolodexContacts();
      }

      //-----------------------------------------------------------------------
      bool Identity::getDownloadedRolodexContacts(
                                                  bool &outFlushAllRolodexContacts,
                                                  String &outVersionDownloaded,
                                                  RolodexContactListPtr &outRolodexContacts
                                                  )
      {
        typedef stack::message::IdentityInfo StackIdentityInfo;
        typedef stack::message::IdentityInfoList StackIdentityInfoList;
        typedef stack::message::IdentityInfoListPtr StackIdentityInfoListPtr;

        ZS_LOG_DEBUG(log("download rolodex contacts") + ZS_PARAM("identity session id", mSession->getID()))

        StackIdentityInfoListPtr identities;
        bool result = mSession->getDownloadedRolodexContacts(
                                                             outFlushAllRolodexContacts,
                                                             outVersionDownloaded,
                                                             identities
                                                             );
        if (!result) {
          outRolodexContacts = RolodexContactListPtr();
          return result;
        }

        RolodexContactListPtr rolodexInfoList(new RolodexContactList);
        outRolodexContacts = rolodexInfoList;

        ZS_THROW_BAD_STATE_IF(!identities)

        for (StackIdentityInfoList::iterator iter = identities->begin(); iter != identities->end(); ++iter)
        {
          const StackIdentityInfo &identityInfo = (*iter);

          RolodexContact rolodexInfo;
          switch (identityInfo.mDisposition) {
            case StackIdentityInfo::Disposition_NA:     rolodexInfo.mDisposition = RolodexContact::Disposition_NA; break;
            case StackIdentityInfo::Disposition_Update: rolodexInfo.mDisposition = RolodexContact::Disposition_Update; break;
            case StackIdentityInfo::Disposition_Remove: rolodexInfo.mDisposition = RolodexContact::Disposition_Remove; break;
          }

          rolodexInfo.mIdentityURI = identityInfo.mURI;
          rolodexInfo.mIdentityProvider = identityInfo.mProvider;

          rolodexInfo.mName = identityInfo.mName;
          rolodexInfo.mProfileURL = identityInfo.mProfile;
          rolodexInfo.mVProfileURL = identityInfo.mVProfile;

          for (StackIdentityInfo::AvatarList::const_iterator avIter = identityInfo.mAvatars.begin();  avIter != identityInfo.mAvatars.end(); ++avIter)
          {
            const StackIdentityInfo::Avatar &resultAvatar = (*avIter);
            RolodexContact::Avatar avatar;

            avatar.mName = resultAvatar.mName;
            avatar.mURL = resultAvatar.mURL;
            avatar.mWidth = resultAvatar.mWidth;
            avatar.mHeight = resultAvatar.mHeight;
            rolodexInfo.mAvatars.push_back(avatar);
          }

          rolodexInfoList->push_back(rolodexInfo);
        }

        return result;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Identity => IIdentityForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      IdentityPtr Identity::createFromExistingSession(IServiceIdentitySessionPtr session)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!session)

        IdentityPtr pThis(new Identity(UseStack::queueCore()));
        pThis->mThisWeak = pThis;
        pThis->mSession = session;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IServiceIdentitySessionPtr Identity::getSession() const
      {
        return mSession;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Identity => IServiceIdentitySessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Identity::onServiceIdentitySessionStateChanged(
                                                          IServiceIdentitySessionPtr session,
                                                          SessionStates state
                                                          )
      {
        ZS_LOG_BASIC(log("state changed") + ZS_PARAM("identity session id", session->getID()) + ZS_PARAM("state", IServiceIdentitySession::toString(state)))

        ZS_THROW_BAD_STATE_IF(!mDelegate)
        try {
          mDelegate->onIdentityStateChanged(mThisWeak.lock(), toState(state));
        } catch (IIdentityDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void Identity::onServiceIdentitySessionPendingMessageForInnerBrowserWindowFrame(IServiceIdentitySessionPtr session)
      {
        ZS_LOG_DEBUG(log("pending inner browser window frame message") + ZS_PARAM("identity session id", session->getID()))

        ZS_THROW_BAD_STATE_IF(!mDelegate)
        try {
          mDelegate->onIdentityPendingMessageForInnerBrowserWindowFrame(mThisWeak.lock());
        } catch (IIdentityDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void Identity::onServiceIdentitySessionRolodexContactsDownloaded(IServiceIdentitySessionPtr session)
      {
        ZS_LOG_DEBUG(log("rolodex contacts downloaded") + ZS_PARAM("identity session id", session->getID()))

        ZS_THROW_BAD_STATE_IF(!mDelegate)
        try {
          mDelegate->onIdentityRolodexContactsDownloaded(mThisWeak.lock());
        } catch (IIdentityDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Identity => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Identity::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Identity");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr Identity::toDebug() const
      {
        return IServiceIdentitySession::toDebug(mSession);
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
    #pragma mark IIdentity
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IIdentity::toDebug(IIdentityPtr identity)
    {
      return internal::Identity::toDebug(identity);
    }

    //-------------------------------------------------------------------------
    const char *IIdentity::toString(IdentityStates state)
    {
      return IServiceIdentitySession::toString(internal::toState(state));
    }

    //-------------------------------------------------------------------------
    IIdentityPtr IIdentity::login(
                                  IAccountPtr account,
                                  IIdentityDelegatePtr delegate,
                                  const char *identityProviderDomain,
                                  const char *identityURI_or_identityBaseURI,
                                  const char *outerFrameURLUponReload
                                  )
    {
      return internal::IIdentityFactory::singleton().login(account, delegate, identityProviderDomain, identityURI_or_identityBaseURI, outerFrameURLUponReload);
    }

    //-------------------------------------------------------------------------
    IIdentityPtr IIdentity::loginWithIdentityPreauthorized(
                                                           IAccountPtr account,
                                                           IIdentityDelegatePtr delegate,
                                                           const char *identityProviderDomain,
                                                           const char *identityURI,
                                                           const char *identityAccessToken,
                                                           const char *identityAccessSecret,
                                                           Time identityAccessSecretExpires
                                                           )
    {
      return internal::IIdentityFactory::singleton().loginWithIdentityPreauthorized(account, delegate, identityProviderDomain, identityURI, identityAccessToken, identityAccessSecret, identityAccessSecretExpires);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
