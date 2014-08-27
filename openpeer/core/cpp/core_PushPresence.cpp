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

#include <openpeer/core/internal/core_PushPresence.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/core/IContact.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IServicePushMailbox.h>
#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/Stringize.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    using stack::IServicePushMailboxSession;

    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailbox, IServicePushMailbox)

      ZS_DECLARE_TYPEDEF_PTR(IStackForInternal, UseStack)

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(stack::message::IMessageHelper, UseMessageHelper)

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
      #pragma mark PushPresence
      #pragma mark

      //-----------------------------------------------------------------------
      PushPresence::PushPresence(
                                   IMessageQueuePtr queue,
                                   IPushPresenceDelegatePtr delegate,
                                   IPushPresenceDatabaseAbstractionDelegatePtr databaseDelegate,
                                   UseAccountPtr account
                                   ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDelegate(IPushPresenceDelegateProxy::createWeak(UseStack::queueApplication(), delegate)),
        mDatabase(databaseDelegate),
        mAccount(account),
        mCurrentState(PushPresenceState_Pending)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void PushPresence::init()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("init called"))

        mAccountSubscription = mAccount->subscribe(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      PushPresence::~PushPresence()
      {
        if (isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      PushPresencePtr PushPresence::convert(IPushPresencePtr messaging)
      {
        return ZS_DYNAMIC_PTR_CAST(PushPresence, messaging);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushPresence => IPushPresence
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PushPresence::toDebug(IPushPresencePtr messaging)
      {
        if (!messaging) return ElementPtr();
        return PushPresence::convert(messaging)->toDebug();
      }

      //-----------------------------------------------------------------------
      PushPresencePtr PushPresence::create(
                                             IPushPresenceDelegatePtr delegate,
                                             IPushPresenceDatabaseAbstractionDelegatePtr databaseDelegate,
                                             IAccountPtr inAccount
                                             )
      {
        PushPresencePtr pThis(new PushPresence(UseStack::queueCore(), delegate, databaseDelegate, Account::convert(inAccount)));
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IPushPresence::PushPresenceStates PushPresence::getState(
                                                                  WORD *outErrorCode,
                                                                  String *outErrorReason
                                                                  ) const
      {
        AutoRecursiveLock lock(*this);
        if (outErrorCode)
          *outErrorCode = mLastError;
        if (outErrorReason)
          *outErrorReason = mLastErrorReason;
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      void PushPresence::shutdown()
      {
        ZS_LOG_DEBUG(log("shutdown called"))
        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      IPushPresenceRegisterQueryPtr PushPresence::registerDevice(
                                                                   IPushPresenceRegisterQueryDelegatePtr inDelegate,
                                                                   const char *inDeviceToken,
                                                                   Time inExpires,
                                                                   const char *inMappedType,
                                                                   bool inUnreadBadge,
                                                                   const char *inSound,
                                                                   const char *inAction,
                                                                   const char *inLaunchImage,
                                                                   unsigned int inPriority,
                                                                   const ValueNameList &inValueNames
                                                                   )
      {
        ZS_LOG_DEBUG(log("register called") + ZS_PARAM("delegate", (bool)inDelegate) + ZS_PARAM("device token", inDeviceToken) + ZS_PARAM("expires", inExpires) + ZS_PARAM("mapped type", inMappedType) + ZS_PARAM("unread badge", inUnreadBadge) + ZS_PARAM("sound", inSound) + ZS_PARAM("action", inAction) + ZS_PARAM("launch image", inLaunchImage) + ZS_PARAM("priority", inPriority) + ZS_PARAM("value names", inValueNames.size()))

        RegisterQueryPtr query = RegisterQuery::create(getAssociatedMessageQueue(), *this, inDelegate, inDeviceToken, inExpires, inMappedType, inUnreadBadge, inSound, inAction, inLaunchImage, inPriority, inValueNames);
        if (mMailbox) query->attachMailbox(mMailbox);

        AutoRecursiveLock lock(*this);

        mPendingAttachmentRegisterQueries.push_back(query);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        
        return query;
      }

      //-----------------------------------------------------------------------
      void PushPresence::send(
                              const ContactList &toContactList,
                              const Status &status
                              )
      {
        ZS_LOG_DEBUG(log("send called") + ZS_PARAM("contacts", toContactList.size()))

        AutoRecursiveLock lock(*this);
      }
      
      //-----------------------------------------------------------------------
      void PushPresence::recheckNow()
      {
        ZS_LOG_DEBUG(log("recheck now called"))

        AutoRecursiveLock lock(*this);
        if (!mMailbox) {
          ZS_LOG_WARNING(Detail, log("mailbox not setup yet"))
          return;
        }
        mMailbox->recheckNow();
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushPresence => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushPresence::onWake()
      {
        ZS_LOG_TRACE(log("on wake"))
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushPresence => IAccountDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushPresence::onAccountStateChanged(
                                                IAccountPtr account,
                                                AccountStates state
                                                )
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("account state changed"))
        step();
      }

      //-----------------------------------------------------------------------
      void PushPresence::onAccountAssociatedIdentitiesChanged(IAccountPtr account)
      {
      }

      //-----------------------------------------------------------------------
      void PushPresence::onAccountPendingMessageForInnerBrowserWindowFrame(IAccountPtr account)
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushPresence => IServicePushPresenceSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushPresence::onServicePushMailboxSessionStateChanged(
                                                                  IServicePushMailboxSessionPtr session,
                                                                  SessionStates state
                                                                  )
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("push mailbox state changed"))
        step();
      }

      //-----------------------------------------------------------------------
      void PushPresence::onServicePushMailboxSessionFolderChanged(
                                                                   IServicePushMailboxSessionPtr session,
                                                                   const char *folder
                                                                   )
      {
        AutoRecursiveLock lock(*this);
        if (folder != services::ISettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MAILBOX_FOLDER)) {
          ZS_LOG_TRACE(log("not interested in this folder update") + ZS_PARAM("folder", folder))
          return;
        }

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
          return;
        }

//        try {
//          mDelegate->onPushPresenceNewMessages(mThisWeak.lock());
//        } catch(IPushPresenceDelegateProxy::Exceptions::DelegateGone &) {
//          ZS_LOG_WARNING(Detail, log("delegate gone"))
//        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushPresence => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PushPresence::slog(const char *message)
      {
        ElementPtr objectEl = Element::create("core::PushPresence");
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params PushPresence::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::PushPresence");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params PushPresence::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr PushPresence::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::PushPresence");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "graceful shutdown reference", (bool)mGracefulShutdownReference);

        UseServicesHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        UseServicesHelper::debugAppend(resultEl, "account", mAccount ? mAccount->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "account subscription", mAccountSubscription ? mAccountSubscription->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "previously ready", mPreviouslyReady);

        UseServicesHelper::debugAppend(resultEl, "current state", toString(mCurrentState));
        UseServicesHelper::debugAppend(resultEl, "last error", mLastError);
        UseServicesHelper::debugAppend(resultEl, "last error reason", mLastErrorReason);

        UseServicesHelper::debugAppend(resultEl, "pending attachment register queries", mPendingAttachmentRegisterQueries.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PushPresence::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        setCurrentState(PushPresenceState_ShuttingDown);

        if (mMailbox) {
          mMailbox->shutdown();
        }

        if (mGracefulShutdownReference) {
          if (IServicePushMailboxSession::SessionState_Shutdown != mMailbox->getState()) {
            ZS_LOG_TRACE(log("waiting for mailbox to shutdown"))
            return;
          }
        }

        setCurrentState(PushPresenceState_Shutdown);

        for (RegisterQueryList::iterator iter = mPendingAttachmentRegisterQueries.begin(); iter != mPendingAttachmentRegisterQueries.end(); ++iter)
        {
          ZS_LOG_DEBUG(log("cancelling register query"))
          RegisterQueryPtr query = (*iter);
          query->cancel();
        }
        mPendingAttachmentRegisterQueries.clear();

        mMailbox.reset();

        mDelegate.reset();

        mAccount.reset();
        if (mAccountSubscription) {
          mAccountSubscription->cancel();
          mAccountSubscription.reset();
        }
      }

      //-----------------------------------------------------------------------
      void PushPresence::setCurrentState(PushPresenceStates state)
      {
        if (state == mCurrentState) return;
        if (PushPresenceState_Shutdown == mCurrentState) return;

        ZS_LOG_DEBUG(log("state changed") + ZS_PARAM("old state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;

        PushPresencePtr pThis = mThisWeak.lock();
        if ((pThis) &&
            (mDelegate)) {
          try {
            mDelegate->onPushPresenceStateChanged(pThis, state);
          } catch(IPushPresenceDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      void PushPresence::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());

        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }
        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, debug("error already set thus ignoring new error") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        mLastError = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, log("error set") + ZS_PARAM("code", mLastError) + ZS_PARAM("reason", mLastErrorReason))
      }
      
      //-----------------------------------------------------------------------
      void PushPresence::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_TRACE(log("step going to cancel due to shutdown state"))
          cancel();
          return;
        }

        if (!stepAccount()) goto post_step;
        if (!stepMailbox()) goto post_step;
        if (!stepAttach()) goto post_step;

        setCurrentState(PushPresenceState_Ready);

      post_step:
        {
        }

      }

      //-----------------------------------------------------------------------
      bool PushPresence::stepAccount()
      {
        WORD errorCode = 0;
        String errorReason;
        switch (mAccount->getState(&errorCode, &errorReason)) {
          case IAccount::AccountState_Pending:
          case IAccount::AccountState_PendingPeerFilesGeneration:
          case IAccount::AccountState_WaitingForAssociationToIdentity:
          case IAccount::AccountState_WaitingForBrowserWindowToBeLoaded:
          case IAccount::AccountState_WaitingForBrowserWindowToBeMadeVisible:
          case IAccount::AccountState_WaitingForBrowserWindowToBeRedirected:
          case IAccount::AccountState_WaitingForBrowserWindowToClose:
          {
            if (!mPreviouslyReady) {
              ZS_LOG_TRACE(log("step account - waiting for account to be ready"))
              return false;
            }
            ZS_LOG_TRACE(log("step account - previously ready"))
            return true;
          }
          case IAccount::AccountState_Ready:
          {
            ZS_LOG_TRACE(log("step account - ready"))
            mPreviouslyReady = true;
            break;
          }
          case IAccount::AccountState_ShuttingDown:
          case IAccount::AccountState_Shutdown:
          {
            ZS_LOG_WARNING(Detail, log("account is shutdown"))
            setError(errorCode, errorReason);
            cancel();
            return false;
          }
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool PushPresence::stepMailbox()
      {
        if (mMailbox) {
          WORD errorCode = 0;
          String errorReason;
          switch (mMailbox->getState()) {
            case IServicePushMailboxSession::SessionState_Pending:
            case IServicePushMailboxSession::SessionState_Connecting:
            {
              ZS_LOG_TRACE(log("step mailbox - push mailbox is pending"))
            }
            case IServicePushMailboxSession::SessionState_Connected:
            case IServicePushMailboxSession::SessionState_GoingToSleep:
            case IServicePushMailboxSession::SessionState_Sleeping:
            {
              ZS_LOG_TRACE(log("step mailbox - push mailbox is ready"))
              break;
            }
            case IServicePushMailboxSession::SessionState_ShuttingDown:
            case IServicePushMailboxSession::SessionState_Shutdown:
            {
              ZS_LOG_WARNING(Detail, log("step mailbox - push mailbox is shutdown"))
              setError(errorCode, errorReason);
              cancel();
              return false;
            }
          }

          return true;
        }

        ZS_LOG_DEBUG(log("step mailbox - setting up mailbox"))

        IBootstrappedNetworkPtr network = mAccount->getLockboxBootstrapper();

        IServicePushMailboxPtr servicePushmailbox = IServicePushMailbox::createServicePushMailboxFrom(network);

        mMailbox = IServicePushMailboxSession::create(mThisWeak.lock(), mDatabase, UseStack::queueApplication(), servicePushmailbox, mAccount->getStackAccount(), mAccount->getNamespaceGrantSession(), mAccount->getLockboxSession());

        String monitorFolder = services::ISettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MAILBOX_FOLDER);

        mMailbox->monitorFolder(monitorFolder);

        if (mLastVersionDownloaded.hasData()) {
          ZS_LOG_DEBUG(log("checking if there are new downloads available at this time"))
          String latestAvailableVersion = mMailbox->getLatestDownloadVersionAvailableForFolder(monitorFolder);

          if (latestAvailableVersion != mLastVersionDownloaded) {
            if (mDelegate) {
//              try {
//                mDelegate->onPushPresenceNewMessages(mThisWeak.lock());
//              } catch(IPushPresenceDelegateProxy::Exceptions::DelegateGone &) {
//                ZS_LOG_WARNING(Detail, log("delegate gone"))
//              }
            }
          }

          mLastVersionDownloaded.clear();
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool PushPresence::stepAttach()
      {
        ZS_LOG_TRACE(log("step attach"))

        for (RegisterQueryList::iterator iter = mPendingAttachmentRegisterQueries.begin(); iter != mPendingAttachmentRegisterQueries.end(); ++iter)
        {
          ZS_LOG_DEBUG(log("attaching register query"))
          RegisterQueryPtr query = (*iter);
          query->attachMailbox(mMailbox);
        }
        mPendingAttachmentRegisterQueries.clear();

        return true;
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPushPresenceFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPushPresenceFactory &IPushPresenceFactory::singleton()
      {
        return PushPresenceFactory::singleton();
      }

      //-----------------------------------------------------------------------
      PushPresencePtr IPushPresenceFactory::create(
                                                   IPushPresenceDelegatePtr delegate,
                                                   IPushPresenceDatabaseAbstractionDelegatePtr databaseDelegate,
                                                   IAccountPtr account
                                                   )
      {
        if (this) {}
        return PushPresence::create(delegate, databaseDelegate, account);
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
    #pragma mark IPushPresence
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IPushPresence::toDebug(IPushPresencePtr identity)
    {
      return internal::PushPresence::toDebug(identity);
    }

    //-------------------------------------------------------------------------
    const char *IPushPresence::toString(PushPresenceStates state)
    {
      switch (state) {
        case PushPresenceState_Pending:       return "Pending";
        case PushPresenceState_Ready:         return "Ready";
        case PushPresenceState_ShuttingDown:  return "Shutting down";
        case PushPresenceState_Shutdown:      return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IPushPresencePtr IPushPresence::create(
                                             IPushPresenceDelegatePtr delegate,
                                             IPushPresenceDatabaseAbstractionDelegatePtr databaseDelegate,
                                             IAccountPtr account
                                             )
    {
      return internal::IPushPresenceFactory::singleton().create(delegate, databaseDelegate, account);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
