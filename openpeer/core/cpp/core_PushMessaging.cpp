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

#include <openpeer/core/internal/core_PushMessaging.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IServicePushMailbox.h>
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
    using stack::IServicePushMailboxSession;

    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailbox, IServicePushMailbox)

      ZS_DECLARE_TYPEDEF_PTR(IStackForInternal, UseStack)

      using services::IHelper;

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
      #pragma mark PushMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      PushMessaging::PushMessaging(
                                   IMessageQueuePtr queue,
                                   IPushMessagingDelegatePtr delegate,
                                   IPushMessagingDatabaseAbstractionDelegatePtr databaseDelegate,
                                   UseAccountPtr account
                                   ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDelegate(IPushMessagingDelegateProxy::createWeak(UseStack::queueApplication(), delegate)),
        mDatabase(databaseDelegate),
        mAccount(account),
        mCurrentState(PushMessagingStates_Pending)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void PushMessaging::init()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("init called"))

        mAccountSubscription = mAccount->subscribe(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      PushMessaging::~PushMessaging()
      {
        if (isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      PushMessagingPtr PushMessaging::convert(IPushMessagingPtr messaging)
      {
        return dynamic_pointer_cast<PushMessaging>(messaging);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging => IPushMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PushMessaging::toDebug(IPushMessagingPtr messaging)
      {
        if (!messaging) return ElementPtr();
        return PushMessaging::convert(messaging)->toDebug();
      }

      //-----------------------------------------------------------------------
      PushMessagingPtr PushMessaging::create(
                                             IPushMessagingDelegatePtr delegate,
                                             IPushMessagingDatabaseAbstractionDelegatePtr databaseDelegate,
                                             IAccountPtr inAccount
                                             )
      {
        PushMessagingPtr pThis(new PushMessaging(UseStack::queueCore(), delegate, databaseDelegate, Account::convert(inAccount)));
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      PUID PushMessaging::getID() const
      {
        return mID;
      }

      //-----------------------------------------------------------------------
      IPushMessaging::PushMessagingStates PushMessaging::getState(
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
      void PushMessaging::shutdown()
      {
        ZS_LOG_DEBUG(log("shutdown called"))
        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      IPushMessagingRegisterQueryPtr PushMessaging::registerDevice(
                                                                   const char *deviceToken,
                                                                   Time expires,
                                                                   const char *mappedType,
                                                                   bool unreadBadge,
                                                                   const char *sound,
                                                                   const char *action,
                                                                   const char *launchImage,
                                                                   unsigned int priority
                                                                   )
      {
        return IPushMessagingRegisterQueryPtr();
      }

      //-----------------------------------------------------------------------
      IPushMessagingQueryPtr PushMessaging::push(
                                                 IPushMessagingQueryDelegatePtr delegate,
                                                 const ContactList &toContactList,
                                                 const PushMessage &message
                                                 )
      {
        return IPushMessagingQueryPtr();
      }

      //-----------------------------------------------------------------------
      void PushMessaging::recheckNow()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("recheck now called"))
        if (!mMailbox) {
          ZS_LOG_WARNING(Detail, log("mailbox not setup yet"))
          return;
        }
        mMailbox->recheckNow();
      }

      //-----------------------------------------------------------------------
      void PushMessaging::markPushMessageRead(const char *messageID)
      {
      }

      //-----------------------------------------------------------------------
      void PushMessaging::deletePushMessage(const char *messageID)
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging => IAccountDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushMessaging::onAccountStateChanged(
                                                IAccountPtr account,
                                                AccountStates state
                                                )
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("account state changed"))
        step();
      }

      //-----------------------------------------------------------------------
      void PushMessaging::onAccountAssociatedIdentitiesChanged(IAccountPtr account)
      {
      }

      //-----------------------------------------------------------------------
      void PushMessaging::onAccountPendingMessageForInnerBrowserWindowFrame(IAccountPtr account)
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging => IServicePushMessagingSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushMessaging::onServicePushMailboxSessionStateChanged(
                                                                  IServicePushMailboxSessionPtr session,
                                                                  SessionStates state
                                                                  )
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("push mailbox state changed"))
        step();
      }

      //-----------------------------------------------------------------------
      void PushMessaging::onServicePushMailboxSessionFolderChanged(
                                                                   IServicePushMailboxSessionPtr session,
                                                                   const char *folder
                                                                   )
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PushMessaging::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::PushMessaging");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params PushMessaging::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr PushMessaging::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::PushMessaging");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "graceful shutdown reference", (bool)mGracefulShutdownReference);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        IHelper::debugAppend(resultEl, "account", mAccount ? mAccount->getID() : 0);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PushMessaging::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        setCurrentState(PushMessagingStates_ShuttingDown);

        if (mMailbox) {
          mMailbox->shutdown();
        }

        if (mGracefulShutdownReference) {
          if (IServicePushMailboxSession::SessionState_Shutdown != mMailbox->getState()) {
            ZS_LOG_TRACE(log("waiting for mailbox to shutdown"))
            return;
          }
        }

        setCurrentState(PushMessagingStates_Shutdown);

        mMailbox.reset();

        mDelegate.reset();

        mAccount.reset();
        if (mAccountSubscription) {
          mAccountSubscription->cancel();
          mAccountSubscription.reset();
        }
      }

      //-----------------------------------------------------------------------
      void PushMessaging::setCurrentState(PushMessagingStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_DEBUG(log("state changed") + ZS_PARAM("old state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;

        PushMessagingPtr pThis = mThisWeak.lock();
        if ((pThis) &&
            (mDelegate)) {
          try {
            mDelegate->onPushMessagingStateChanged(pThis, state);
          } catch(IPushMessagingDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      void PushMessaging::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());

        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }
        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, debug("error already set thus ignoring new error") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        get(mLastError) = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, log("error set") + ZS_PARAM("code", mLastError) + ZS_PARAM("reason", mLastErrorReason))
      }
      
      //-----------------------------------------------------------------------
      void PushMessaging::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_TRACE(log("step going to cancel due to shutdown state"))
          cancel();
          return;
        }

        if (!stepAccount()) goto post_step;
        if (!stepMailbox()) goto post_step;

        setCurrentState(PushMessagingStates_Ready);

      post_step:
        {
        }

      }

      //-----------------------------------------------------------------------
      bool PushMessaging::stepAccount()
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
            get(mPreviouslyReady) = true;
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
      bool PushMessaging::stepMailbox()
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

        mMailbox = IServicePushMailboxSession::create(mThisWeak.lock(), mDatabase, servicePushmailbox, mAccount->getStackAccount(), mAccount->getNamespaceGrantSession(), mAccount->getLockboxSession());

        return true;
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
    #pragma mark IPushMessaging
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IPushMessaging::toDebug(IPushMessagingPtr identity)
    {
      return internal::PushMessaging::toDebug(identity);
    }

    //-------------------------------------------------------------------------
    const char *IPushMessaging::toString(PushMessagingStates state)
    {
      switch (state) {
        case PushMessagingStates_Pending:       return "Pending";
        case PushMessagingStates_Ready:         return "Ready";
        case PushMessagingStates_ShuttingDown:  return "Shutting down";
        case PushMessagingStates_Shutdown:      return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    const char *IPushMessaging::toString(PushStates state)
    {
      switch (state) {
        case PushState_None:      return "none";

        case PushState_Read:      return "read";
        case PushState_Answered:  return "answered";
        case PushState_Flagged:   return "flagged";
        case PushState_Deleted:   return "deleted";
        case PushState_Draft:     return "draft";
        case PushState_Recent:    return "recent";
        case PushState_Delivered: return "delivered";
        case PushState_Sent:      return "sent";
        case PushState_Pushed:    return "pushed";
        case PushState_Error:     return "error";
      }

      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IPushMessaging::PushStates IPushMessaging::toPushState(const char *state)
    {
      static PushStates states[] =
      {
        PushState_Read,
        PushState_Answered,
        PushState_Flagged,
        PushState_Deleted,
        PushState_Draft,
        PushState_Recent,
        PushState_Delivered,
        PushState_Sent,
        PushState_Pushed,
        PushState_Error,

        PushState_None
      };

      if (NULL == state) return PushState_None;

      for (int index = 0; states[index] != PushState_None; ++index) {
        if (0 == strcmp(toString(states[index]), state)) return states[index];
      }
      return PushState_None;
    }

    //-------------------------------------------------------------------------
    IPushMessagingPtr IPushMessaging::create(
                                             IPushMessagingDelegatePtr delegate,
                                             IPushMessagingDatabaseAbstractionDelegatePtr databaseDelegate,
                                             IAccountPtr account
                                             )
    {
      return internal::IPushMessagingFactory::singleton().create(delegate, databaseDelegate, account);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
