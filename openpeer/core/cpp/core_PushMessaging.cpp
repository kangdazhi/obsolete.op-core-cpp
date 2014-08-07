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
      static void copy(const stack::IServicePushMailboxSession::PushInfoList &source, IPushMessaging::PushInfoList &dest)
      {
        typedef stack::IServicePushMailboxSession::PushInfo SourceType;
        typedef stack::IServicePushMailboxSession::PushInfoList SourceListType;
        typedef IPushMessaging::PushInfo DestType;
        typedef IPushMessaging::PushInfoList DestListType;

        for (SourceListType::const_iterator iter = source.begin(); iter != source.end(); ++iter)
        {
          const SourceType &sourceValue = (*iter);

          DestType destValue;

          destValue.mServiceType = sourceValue.mServiceType;
          destValue.mValues = sourceValue.mValues ? sourceValue.mValues->clone()->toElement() : ElementPtr();
          destValue.mCustom = sourceValue.mCustom ? sourceValue.mCustom->clone()->toElement() : ElementPtr();

          dest.push_back(destValue);
        }
      }

      //-----------------------------------------------------------------------
      static void copy(const IPushMessaging::PushInfoList &source, stack::IServicePushMailboxSession::PushInfoList &dest)
      {
        typedef IPushMessaging::PushInfo SourceType;
        typedef IPushMessaging::PushInfoList SourceListType;
        typedef stack::IServicePushMailboxSession::PushInfo DestType;
        typedef stack::IServicePushMailboxSession::PushInfoList DestListType;

        for (SourceListType::const_iterator iter = source.begin(); iter != source.end(); ++iter)
        {
          const SourceType &sourceValue = (*iter);

          DestType destValue;

          destValue.mServiceType = sourceValue.mServiceType;
          destValue.mValues = sourceValue.mValues ? sourceValue.mValues->clone()->toElement() : ElementPtr();
          destValue.mCustom = sourceValue.mCustom ? sourceValue.mCustom->clone()->toElement() : ElementPtr();

          dest.push_back(destValue);
        }
      }
      
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
                                                                   IPushMessagingRegisterQueryDelegatePtr inDelegate,
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
      IPushMessagingQueryPtr PushMessaging::push(
                                                 IPushMessagingQueryDelegatePtr delegate,
                                                 const ContactList &toContactList,
                                                 const PushMessage &message
                                                 )
      {
        ZS_LOG_DEBUG(log("push called") + ZS_PARAM("message", message.mFullMessage) + ZS_PARAM("raw data", (bool)message.mRawFullMessage))

        AutoRecursiveLock lock(*this);

        PushMessagePtr pushMessage(new PushMessage);
        (*pushMessage) = message;

        if (Time() == pushMessage->mSent) {
          pushMessage->mSent = zsLib::now();
        }
        if (Time() == pushMessage->mExpires) {
          pushMessage->mSent = zsLib::now() + Seconds(services::ISettings::getUInt(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_EXPIRES_IN_SECONDS));
        }

        if (mAccount) {
          pushMessage->mFrom = IContact::getForSelf(Account::convert(mAccount));
        }

        PushQueryPtr query = PushQuery::create(getAssociatedMessageQueue(), *this, delegate, pushMessage);
        if ((mAccount) &&
            (mMailbox))
          query->attachMailbox(mAccount, mMailbox);
        else
          mPendingAttachmentPushQueries.push_back(query);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return query;
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
      bool PushMessaging::getMessagesUpdates(
                                             const char *inLastVersionDownloaded,
                                             String &outUpdatedToVersion,
                                             PushMessageList &outNewMessages
                                             )
      {
        typedef IServicePushMailboxSession::PushMessageList MailboxPushMessageList;
        ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSession::PushMessage, MailboxPushMessage)

        String lastVersionDownloaded(inLastVersionDownloaded);

        AutoRecursiveLock lock(*this);
        if ((!mMailbox) ||
            (!mAccount)) {
          ZS_LOG_WARNING(Detail, log("cannot download messages at this time"))
          mLastVersionDownloaded = lastVersionDownloaded;
          outUpdatedToVersion = lastVersionDownloaded;
          return true;
        }

        IServicePushMailboxSession::PushMessageListPtr added;
        IServicePushMailboxSession::MessageIDListPtr removed;

        bool result = mMailbox->getFolderMessageUpdates(services::ISettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MAILBOX_FOLDER), lastVersionDownloaded, outUpdatedToVersion, added, removed);

        if (!result) {
          outUpdatedToVersion = String();
          return false;
        }

        String pushType = services::ISettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MESSAGE_TYPE);

        if (added) {
          for (MailboxPushMessageList::const_iterator iter = added->begin(); iter != added->end(); ++iter) {
            const MailboxPushMessagePtr &message = (*iter);

            if (!message) {
              ZS_LOG_WARNING(Detail, log("message was null"))
              continue;
            }

            if (message->mPushType != pushType) {
              ZS_LOG_WARNING(Trace, log("ignoring message that is not of proper push type") + ZS_PARAM("push type", message->mPushType) + ZS_PARAM("expecting", pushType))
              continue;
            }

            PushMessagePtr newMessage(new PushMessage);
            PushMessaging::copy(mAccount, *message, *newMessage);

            outNewMessages.push_back(newMessage);
          }
        }

        return true;
      }

      //-----------------------------------------------------------------------
      PushMessaging::NameValueMapPtr PushMessaging::getValues(const PushInfo &pushInfo)
      {
        NameValueMapPtr result(new NameValueMap);

        if (pushInfo.mValues) {
          ElementPtr valueEl = pushInfo.mValues->getFirstChildElement();
          while (valueEl) {
            String name = valueEl->getValue();
            String value = UseMessageHelper::getElementTextAndDecode(valueEl);

            if (name.hasData()) {
              (*result)[name] = value;
            }

            valueEl = valueEl->getNextSiblingElement();
          }
        }

        ZS_LOG_DEBUG(slog("get values") + ZS_PARAM("values", (bool)pushInfo.mValues) + ZS_PARAM("total found", result->size()))
        return result;
      }

      //-----------------------------------------------------------------------
      void PushMessaging::markPushMessageRead(const char *messageID)
      {
        ZS_LOG_DEBUG(log("mark read") + ZS_PARAM("message id", messageID))
        AutoRecursiveLock lock(*this);
        mMarkReadMessages.push_back(messageID);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void PushMessaging::deletePushMessage(const char *messageID)
      {
        ZS_LOG_DEBUG(log("delete message") + ZS_PARAM("message id", messageID))
        AutoRecursiveLock lock(*this);
        mDeleteMessages.push_back(messageID);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushMessaging::onWake()
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
        AutoRecursiveLock lock(*this);
        if (folder != services::ISettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MAILBOX_FOLDER)) {
          ZS_LOG_TRACE(log("not interested in this folder update") + ZS_PARAM("folder", folder))
          return;
        }

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
          return;
        }

        try {
          mDelegate->onPushMessagingNewMessages(mThisWeak.lock());
        } catch(IPushMessagingDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PushMessaging::slog(const char *message)
      {
        ElementPtr objectEl = Element::create("core::PushMessaging");
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params PushMessaging::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::PushMessaging");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
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

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "graceful shutdown reference", (bool)mGracefulShutdownReference);

        UseServicesHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        UseServicesHelper::debugAppend(resultEl, "account", mAccount ? mAccount->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "account subscription", mAccountSubscription ? mAccountSubscription->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "previously ready", mPreviouslyReady);

        UseServicesHelper::debugAppend(resultEl, "current state", toString(mCurrentState));
        UseServicesHelper::debugAppend(resultEl, "last error", mLastError);
        UseServicesHelper::debugAppend(resultEl, "last error reason", mLastErrorReason);

        UseServicesHelper::debugAppend(resultEl, "pending attachment push queries", mPendingAttachmentPushQueries.size());
        UseServicesHelper::debugAppend(resultEl, "pending attachment register queries", mPendingAttachmentRegisterQueries.size());

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

        mMarkReadMessages.clear();
        mDeleteMessages.clear();

        if (mGracefulShutdownReference) {
          if (IServicePushMailboxSession::SessionState_Shutdown != mMailbox->getState()) {
            ZS_LOG_TRACE(log("waiting for mailbox to shutdown"))
            return;
          }
        }

        setCurrentState(PushMessagingStates_Shutdown);

        for (RegisterQueryList::iterator iter = mPendingAttachmentRegisterQueries.begin(); iter != mPendingAttachmentRegisterQueries.end(); ++iter)
        {
          ZS_LOG_DEBUG(log("cancelling register query"))
          RegisterQueryPtr query = (*iter);
          query->cancel();
        }
        mPendingAttachmentRegisterQueries.clear();

        for (PushQueryList::iterator iter = mPendingAttachmentPushQueries.begin(); iter != mPendingAttachmentPushQueries.end(); ++iter)
        {
          ZS_LOG_DEBUG(log("canelling push query"))
          PushQueryPtr query = (*iter);
          query->cancel();
        }
        mPendingAttachmentPushQueries.clear();
        
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
        if (PushMessagingStates_Shutdown == mCurrentState) return;

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
        if (!stepAttach()) goto post_step;
        if (!stepMarkReadAndDelete()) goto post_step;

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

        mMailbox = IServicePushMailboxSession::create(mThisWeak.lock(), mDatabase, UseStack::queueApplication(), servicePushmailbox, mAccount->getStackAccount(), mAccount->getNamespaceGrantSession(), mAccount->getLockboxSession());

        String monitorFolder = services::ISettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MAILBOX_FOLDER);

        mMailbox->monitorFolder(monitorFolder);

        if (mLastVersionDownloaded.hasData()) {
          ZS_LOG_DEBUG(log("checking if there are new downloads available at this time"))
          String latestAvailableVersion = mMailbox->getLatestDownloadVersionAvailableForFolder(monitorFolder);

          if (latestAvailableVersion != mLastVersionDownloaded) {
            if (mDelegate) {
              try {
                mDelegate->onPushMessagingNewMessages(mThisWeak.lock());
              } catch(IPushMessagingDelegateProxy::Exceptions::DelegateGone &) {
                ZS_LOG_WARNING(Detail, log("delegate gone"))
              }
            }
          }

          mLastVersionDownloaded.clear();
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool PushMessaging::stepAttach()
      {
        ZS_LOG_TRACE(log("step attach"))

        for (RegisterQueryList::iterator iter = mPendingAttachmentRegisterQueries.begin(); iter != mPendingAttachmentRegisterQueries.end(); ++iter)
        {
          ZS_LOG_DEBUG(log("attaching register query"))
          RegisterQueryPtr query = (*iter);
          query->attachMailbox(mMailbox);
        }
        mPendingAttachmentRegisterQueries.clear();

        for (PushQueryList::iterator iter = mPendingAttachmentPushQueries.begin(); iter != mPendingAttachmentPushQueries.end(); ++iter)
        {
          ZS_LOG_DEBUG(log("attaching push query"))
          PushQueryPtr query = (*iter);
          query->attachMailbox(mAccount, mMailbox);
        }
        mPendingAttachmentPushQueries.clear();

        return true;
      }

      //-----------------------------------------------------------------------
      bool PushMessaging::stepMarkReadAndDelete()
      {
        ZS_LOG_TRACE(log("step mark read and delete"))

        for (MessageIDList::iterator iter = mMarkReadMessages.begin(); iter != mMarkReadMessages.end(); ++iter)
        {
          const MessageID &messageID = (*iter);
          ZS_LOG_DEBUG(log("step mark read and delete - mark read") + ZS_PARAM("message id", messageID))

          mMailbox->markPushMessageRead(messageID);
        }
        mMarkReadMessages.clear();

        for (MessageIDList::iterator iter = mDeleteMessages.begin(); iter != mDeleteMessages.end(); ++iter)
        {
          const MessageID &messageID = (*iter);
          ZS_LOG_DEBUG(log("step mark read and delete - delete") + ZS_PARAM("message id", messageID))

          mMailbox->deletePushMessage(messageID);
        }
        mDeleteMessages.clear();

        return true;
      }

      //-----------------------------------------------------------------------
      void PushMessaging::copy(
                               UseAccountPtr account,
                               const IServicePushMailboxSession::PushMessage &source,
                               PushMessage &dest
                               )
      {
        typedef IServicePushMailboxSession::PushStateDetailMap MailboxPushStateDetailMap;
        typedef IServicePushMailboxSession::PushStatePeerDetailList MailboxPushStatePeerDetailList;
        typedef IServicePushMailboxSession::PushStatePeerDetail MailboxPushStatePeerDetail;
        typedef IServicePushMailboxSession::PushStates MailboxPushStates;

        // update the local message
        dest.mMessageID = source.mMessageID;

        if (dest.mMimeType.isEmpty())
          dest.mMimeType = source.mMimeType;

        if (dest.mFullMessage.isEmpty()) {
          if (source.mFullMessage) {
            if (0 == strncmp(source.mMimeType, OPENPEER_CORE_PUSH_MESSAGING_MIMETYPE_FILTER_PREFIX, strlen(OPENPEER_CORE_PUSH_MESSAGING_MIMETYPE_FILTER_PREFIX))) {
              dest.mFullMessage = UseServicesHelper::convertToString(*source.mFullMessage);
            }
          }
        }

        if (!dest.mRawFullMessage) {
          if (source.mFullMessage) {
            dest.mRawFullMessage = UseServicesHelper::clone(source.mFullMessage);
          }
        }

        if (dest.mPushType.isEmpty()) {
          dest.mPushType = source.mPushType;
        }

        if (dest.mPushInfos.size() < 1) {
          internal::copy(source.mPushInfos, dest.mPushInfos);
        }

        if (Time() == dest.mSent) {
          dest.mSent = source.mSent;
        }

        if (Time() == dest.mExpires) {
          dest.mExpires = source.mExpires;
        }

        if (!dest.mFrom) {
          if (source.mFrom) {
            dest.mFrom = Contact::convert(UseContact::createFromPeer(Account::convert(account), source.mFrom));
          }
        }

        for (MailboxPushStateDetailMap::const_iterator iter = source.mPushStateDetails.begin(); iter != source.mPushStateDetails.end(); ++iter)
        {
          const MailboxPushStates &sourceState = (*iter).first;
          const MailboxPushStatePeerDetailList &sourceDetailList = (*iter).second;

          PushStates destState = IPushMessaging::toPushState(IServicePushMailboxSession::toString(sourceState));
          if (PushState_None == destState) {
            ZS_LOG_WARNING(Detail, slog("state did not translate") + ZS_PARAM("original state", IServicePushMailboxSession::toString(sourceState)))
            continue;
          }

          PushStateDetailMap::iterator found = dest.mPushStateDetails.find(destState);
          if (found == dest.mPushStateDetails.end()) {
            dest.mPushStateDetails[destState] = PushStateContactDetailList();
            found = dest.mPushStateDetails.find(destState);
          }

          PushStateContactDetailList &destDetailList = (*found).second;

          for (MailboxPushStatePeerDetailList::const_iterator peerIter = sourceDetailList.begin(); peerIter != sourceDetailList.end(); ++peerIter)
          {
            const MailboxPushStatePeerDetail &detail = (*peerIter);

            PushStateContactDetail destDetail;
            if (account) {
              destDetail.mRemotePeer = Contact::convert(UseContact::createFromPeerURI(Account::convert(account), detail.mURI));
            }

            if (!destDetail.mRemotePeer) {
              ZS_LOG_WARNING(Detail, slog("unable to create contact") + ZS_PARAM("uri", detail.mURI))
              continue;
            }
            destDetail.mErrorCode = detail.mErrorCode;
            destDetail.mErrorReason = detail.mErrorReason;

            destDetailList.push_back(destDetail);
          }
        }
      }
      
      //-----------------------------------------------------------------------
      void PushMessaging::copy(
                               const PushMessage &source,
                               IServicePushMailboxSession::PushMessage &dest
                               )
      {
        typedef IServicePushMailboxSession::PushStateDetailMap MailboxPushStateDetailMap;
        typedef IServicePushMailboxSession::PushStatePeerDetailList MailboxPushStatePeerDetailList;
        typedef IServicePushMailboxSession::PushStatePeerDetail MailboxPushStatePeerDetail;
        typedef IServicePushMailboxSession::PushStates MailboxPushStates;

        if (dest.mMessageID.isEmpty()) {
          dest.mMessageID = source.mMessageID;
        }

        if (dest.mMessageType.isEmpty()) {
          dest.mMessageType = services::ISettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MESSAGE_TYPE);
        }

        if (dest.mMimeType.isEmpty()) {
          dest.mMimeType = source.mMimeType;
        }

        if (!dest.mFullMessage) {
          if (source.mFullMessage.hasData()) {
            dest.mFullMessage = UseServicesHelper::convertToBuffer(source.mFullMessage);
          } else if (source.mRawFullMessage) {
            dest.mFullMessage = UseServicesHelper::clone(source.mRawFullMessage);
          }
        }

        if (dest.mPushType.isEmpty()) {
          dest.mPushType = source.mPushType;
        }

        if (dest.mPushInfos.size() < 1) {
          internal::copy(source.mPushInfos, dest.mPushInfos);
        }

        if (Time() == dest.mSent) {
          dest.mSent = source.mSent;
        }
        if (Time() == dest.mExpires) {
          dest.mExpires = source.mExpires;
        }

        if (!dest.mFrom) {
          if (source.mFrom) {
            dest.mFrom = UseContactPtr(Contact::convert(source.mFrom))->getPeer();
          }
        }

        for (PushStateDetailMap::const_iterator iter = source.mPushStateDetails.begin(); iter != source.mPushStateDetails.end(); ++iter)
        {
          const PushStates &sourceState = (*iter).first;
          const PushStateContactDetailList &sourceList = (*iter).second;

          MailboxPushStates destState = IServicePushMailboxSession::toPushState(IPushMessaging::toString(sourceState));
          if (IServicePushMailboxSession::PushState_None == destState) {
            ZS_LOG_WARNING(Detail, slog("push state failed to convert") + ZS_PARAM("state", IPushMessaging::toString(sourceState)))
            continue;
          }

          MailboxPushStateDetailMap::iterator found = dest.mPushStateDetails.find(destState);
          if (found == dest.mPushStateDetails.end()) {
            dest.mPushStateDetails[destState] = MailboxPushStatePeerDetailList();
            found = dest.mPushStateDetails.find(destState);
          }

          MailboxPushStatePeerDetailList &destList = (*found).second;

          for (PushStateContactDetailList::const_iterator contactIter = sourceList.begin(); contactIter != sourceList.end(); ++contactIter)
          {
            const PushStateContactDetail &sourceDetail = (*contactIter);
            if (!sourceDetail.mRemotePeer) {
              ZS_LOG_WARNING(Detail, slog("contact is empty in source list"))
              continue;
            }

            MailboxPushStatePeerDetail destDetail;

            destDetail.mURI = sourceDetail.mRemotePeer->getPeerURI();
            destDetail.mErrorCode = sourceDetail.mErrorCode;
            destDetail.mErrorReason = sourceDetail.mErrorReason;

            destList.push_back(destDetail);
          }
        }
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
    IPushMessaging::NameValueMapPtr IPushMessaging::getValues(const PushInfo &pushInfo)
    {
      return internal::PushMessaging::getValues(pushInfo);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
