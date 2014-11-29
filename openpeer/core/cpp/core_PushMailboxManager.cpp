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

#include <openpeer/core/internal/core_PushMailboxManager.h>
//#include <openpeer/core/internal/core_Stack.h>
//#include <openpeer/core/internal/core_Account.h>
//#include <openpeer/core/internal/core_Helper.h>
//
#include <openpeer/core/IAccount.h>
//#include <openpeer/core/IHelper.h>
//#include <openpeer/core/IPushMessaging.h>
//
#include <openpeer/stack/IBootstrappedNetwork.h>
//#include <openpeer/stack/IServicePushMailbox.h>
//#include <openpeer/stack/message/IMessageHelper.h>
//
#include <openpeer/services/IHelper.h>
//#include <openpeer/services/ISettings.h>
//
//#include <zsLib/Stringize.h>
#include <zsLib/XML.h>
//#include <zsLib/helpers.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
//
//    using stack::IServicePushMailboxSession;

    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession, IServicePushMailboxSession)

//      ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailbox, IServicePushMailbox)
//
//      ZS_DECLARE_TYPEDEF_PTR(IStackForInternal, UseStack)
//
//      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)
//
//      ZS_DECLARE_TYPEDEF_PTR(stack::message::IMessageHelper, UseMessageHelper)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPushMailboxManagerForPushMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      IServicePushMailboxSessionPtr IPushMailboxManagerForPushMessaging::create(
                                                                                IServicePushMailboxSessionDelegatePtr inDelegate,
                                                                                IServicePushMailboxSessionTransferDelegatePtr inTransferDelegate,
                                                                                IAccountPtr inAccount,
                                                                                IServicePushMailboxSessionSubscriptionPtr &outSubscription
                                                                                )
      {
        PushMailboxManagerPtr singleton = IPushMailboxManagerFactory::singleton().singletonManager();

        return singleton->create(inDelegate, inTransferDelegate, inAccount, outSubscription);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPushMailboxManagerForPushPresence
      #pragma mark

      //-----------------------------------------------------------------------
      IServicePushMailboxSessionPtr IPushMailboxManagerForPushPresence::create(
                                                                               IServicePushMailboxSessionDelegatePtr inDelegate,
                                                                               IServicePushMailboxSessionTransferDelegatePtr inTransferDelegate,
                                                                               IAccountPtr inAccount,
                                                                               IServicePushMailboxSessionSubscriptionPtr &outSubscription
                                                                               )
      {
        PushMailboxManagerPtr singleton = IPushMailboxManagerFactory::singleton().singletonManager();

        return singleton->create(inDelegate, inTransferDelegate, inAccount, outSubscription);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMailbox
      #pragma mark

      //-----------------------------------------------------------------------
      PushMailboxManager::PushMailboxManager() :
        SharedRecursiveLock(SharedRecursiveLock::create())
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void PushMailboxManager::init()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("init called"))
      }

      //-----------------------------------------------------------------------
      PushMailboxManager::~PushMailboxManager()
      {
        if (isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      PushMailboxManagerPtr PushMailboxManager::create()
      {
        PushMailboxManagerPtr pThis(new PushMailboxManager);
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      PushMailboxManagerPtr PushMailboxManager::convert(ForPushMessagingPtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(PushMailboxManager, object);
      }

      //-----------------------------------------------------------------------
      PushMailboxManagerPtr PushMailboxManager::convert(ForPushPresencePtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(PushMailboxManager, object);
      }

      //-----------------------------------------------------------------------
      PushMailboxManagerPtr PushMailboxManager::singleton()
      {
        static SingletonLazySharedPtr<PushMailboxManager> singleton(PushMailboxManager::create());
        return singleton.singleton();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMailboxManager => IPushMailboxManagerForPushMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      IServicePushMailboxSessionPtr PushMailboxManager::create(
                                                               IServicePushMailboxSessionDelegatePtr inDelegate,
                                                               IServicePushMailboxSessionTransferDelegatePtr inTransferDelegate,
                                                               IAccountPtr inAccount,
                                                               IServicePushMailboxSessionSubscriptionPtr &outSubscription
                                                               )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inTransferDelegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)

        AutoRecursiveLock lock(*this);

        UseAccountPtr expectingAccount(Account::convert(inAccount));

        for (MailboxList::iterator iter_doNotUse = mMailboxes.begin(); iter_doNotUse != mMailboxes.end(); ) {
          MailboxList::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          Mailbox &mailbox = (*current);

          UseAccountPtr account = mailbox.mAccount.lock();
          IServicePushMailboxSessionPtr pushMailbox = mailbox.mMailbox.lock();

          if ((!account) ||
              (!pushMailbox)) {
            ZS_LOG_WARNING(Detail, log("previous account or pushmail box service is gone") + ZS_PARAM("account", (bool)account) + ZS_PARAM("mailbox", (bool)pushMailbox))
            mMailboxes.erase(current);
            continue;
          }

          if (account->getID() != expectingAccount->getID()) {
            ZS_LOG_TRACE(log("account does not match") + ZS_PARAM("account", account->getID()) + ZS_PARAM("expecting", expectingAccount->getID()))
            continue;
          }

          ZS_LOG_DEBUG(log("found existing push mailbox to use") + IServicePushMailboxSession::toDebug(pushMailbox))

          if (inDelegate) {
            outSubscription = pushMailbox->subscribe(inDelegate);
          }

          // found existing push mailbox
          return pushMailbox;
        }

        // did not find a match

        IBootstrappedNetworkPtr network = expectingAccount->getLockboxBootstrapper();
        IServicePushMailboxPtr servicePushMailbox = IServicePushMailbox::createServicePushMailboxFrom(network);

        ZS_LOG_TRACE(log("creating new push mailbox for domain") + ZS_PARAM("account", expectingAccount->getID()) + ZS_PARAM("domain", network->getDomain()))

        Mailbox mailbox;
        mailbox.mAccount = expectingAccount;

        IServicePushMailboxSessionPtr pushMailbox = IServicePushMailboxSession::create(inDelegate, inTransferDelegate, servicePushMailbox, expectingAccount->getStackAccount(), expectingAccount->getNamespaceGrantSession(), expectingAccount->getLockboxSession());
        mailbox.mMailbox = pushMailbox;

        if (inDelegate) {
          outSubscription = pushMailbox->subscribe(IServicePushMailboxSessionDelegatePtr());
        }

        mMailboxes.push_back(mailbox);

        return pushMailbox;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMailboxManager => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PushMailboxManager::slog(const char *message)
      {
        ElementPtr objectEl = Element::create("core::PushMailboxManager");
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params PushMailboxManager::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::PushMailboxManager");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params PushMailboxManager::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr PushMailboxManager::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::PushMailboxManager");

        UseServicesHelper::debugAppend(resultEl, "id", mID);

        UseServicesHelper::debugAppend(resultEl, "mailboxes", mMailboxes.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PushMailboxManager::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        mMailboxes.clear();
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPushPresenceFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPushMailboxManagerFactory &IPushMailboxManagerFactory::singleton()
      {
        return PushMailboxManagerFactory::singleton();
      }

      //-----------------------------------------------------------------------
      PushMailboxManagerPtr IPushMailboxManagerFactory::singletonManager()
      {
        if (this) {}
        return PushMailboxManager::singleton();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }
  }
}
