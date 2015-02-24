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
#include <openpeer/core/IHelper.h>
#include <openpeer/core/IPushMessaging.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IServicePushMailbox.h>
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
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

    using stack::IServicePushMailboxSession;

    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailbox, IServicePushMailbox)

      ZS_DECLARE_TYPEDEF_PTR(IStackForInternal, UseStack)

      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)

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
                                 IPushPresenceTransferDelegatePtr transferDelegate,
                                 UseAccountPtr account
                                 ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDelegate(IPushPresenceDelegateProxy::createWeak(UseStack::queueApplication(), delegate)),
        mTransferDelegate(IPushPresenceTransferDelegateProxy::createWeak(UseStack::queueApplication(), transferDelegate)),
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

        mLastVersionDownloaded = UseSettings::getString(OPENPEER_CORE_SETTING_PUSH_PRESENCE_LAST_DOWNLOADED_MESSAGE);

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
                                           IPushPresenceTransferDelegatePtr transferDelegate,
                                           IAccountPtr inAccount
                                           )
      {
        PushPresencePtr pThis(new PushPresence(UseStack::queueCore(), delegate, transferDelegate, Account::convert(inAccount)));
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
                                                                 const RegisterDeviceInfo &deviceInfo
                                                                 )
      {
        ZS_LOG_DEBUG(log("register called") + ZS_PARAM("delegate", (bool)inDelegate) + deviceInfo.toDebug())

        RegisterQueryPtr query = RegisterQuery::create(getAssociatedMessageQueue(), *this, inDelegate, deviceInfo);
        if (mMailbox) query->attachMailbox(mMailbox);

        AutoRecursiveLock lock(*this);

        mPendingAttachmentRegisterQueries.push_back(query);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        
        return query;
      }

      //-----------------------------------------------------------------------
      void PushPresence::send(
                              const ContactList &toContactList,
                              const Status &inStatus
                              )
      {
        ZS_LOG_DEBUG(log("send called") + ZS_PARAM("contacts", toContactList.size()) + inStatus.toDebug())

        AutoRecursiveLock lock(*this);

        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_WARNING(Detail, log("cannot send presence while shutting down/shutdown") + inStatus.toDebug())
          return;
        }

        PeerOrIdentityListPtr to(new PeerOrIdentityList);

        for (ContactList::const_iterator iter = toContactList.begin(); iter != toContactList.end(); ++iter) {
          IContactPtr contact = (*iter);
          ZS_THROW_INVALID_ARGUMENT_IF(!contact)

          String uri = contact->getPeerURI();
          ZS_THROW_INVALID_ASSUMPTION_IF(! uri.hasData())

          to->push_back(uri);
        }

        if (to->size() < 1) {
          ZS_LOG_WARNING(Detail, log("not deliverying to any remote contacts"))
          return;
        }

        StatusPtr status(new Status);

        status->mStatusID = inStatus.mStatusID;
        status->mSent = inStatus.mSent;
        status->mExpires = inStatus.mExpires;
        status->mPresenceEl = inStatus.mPresenceEl ? inStatus.mPresenceEl->clone()->toElement() : ElementPtr();
        status->mFrom = inStatus.mFrom;

        if (Time() == status->mSent) {
          status->mSent = zsLib::now();
        }

        if (Time() == status->mExpires) {
          status->mExpires = zsLib::now() + Seconds(UseSettings::getUInt(OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_EXPIRES_IN_SECONDS));
        }

        if (mAccount) {
          status->mFrom = IContact::getForSelf(Account::convert(mAccount));
        }

        for (PushInfoList::const_iterator iter = status->mPushInfos.begin(); iter != status->mPushInfos.end(); ++iter) {
          const PushInfo &sourcePushInfo = (*iter);

          PushInfo pushInfo;

          pushInfo.mServiceType = sourcePushInfo.mServiceType;
          pushInfo.mValues = sourcePushInfo.mValues ? sourcePushInfo.mValues->clone()->toElement() : ElementPtr();
          pushInfo.mCustom = sourcePushInfo.mValues ? sourcePushInfo.mCustom->clone()->toElement() : ElementPtr();

          status->mPushInfos.push_back(pushInfo);
        }

        mPendingDelivery.push_back(ToAndStatusPair(to, status));

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
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
        if (folder != UseSettings::getString(OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MAILBOX_FOLDER)) {
          ZS_LOG_TRACE(log("not interested in this folder update") + ZS_PARAM("folder", folder))
          return;
        }

        ZS_LOG_DEBUG(log("push mailbox folder changed"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushPresence => IServicePushMailboxSessionTransferDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushPresence::onServicePushMailboxSessionTransferUploadFileDataToURL(
                                                                                IServicePushMailboxSessionPtr session,
                                                                                const char *postURL,
                                                                                const char *fileNameContainingData,
                                                                                ULONGEST totalFileSizeInBytes,
                                                                                ULONGEST remainingBytesToUpload,
                                                                                IServicePushMailboxSessionTransferNotifierPtr inNotifier
                                                                                )
      {
        ZS_LOG_DEBUG(log("upload file to url") + ZS_PARAM("session", session->getID()) + ZS_PARAM("url", postURL) + ZS_PARAM("filename", fileNameContainingData) + ZS_PARAM("total file size", totalFileSizeInBytes) + ZS_PARAM("remaining", remainingBytesToUpload))

        AutoRecursiveLock lock(*this);

        TransferNotifierPtr notifier = TransferNotifier::create(inNotifier);

        try {
          mTransferDelegate->onPushPresenceTransferUploadFileDataToURL(mThisWeak.lock(), postURL, fileNameContainingData, totalFileSizeInBytes, remainingBytesToUpload, notifier);
        } catch (IPushPresenceTransferDelegateProxy::Exceptions::DelegateGone &) {
        }
      }

      //-----------------------------------------------------------------------
      void PushPresence::onServicePushMailboxSessionTransferDownloadDataFromURL(
                                                                                IServicePushMailboxSessionPtr session,
                                                                                const char *getURL,
                                                                                const char *fileNameToAppendData,
                                                                                ULONGEST finalFileSizeInBytes,
                                                                                ULONGEST remainingBytesToBeDownloaded,
                                                                                IServicePushMailboxSessionTransferNotifierPtr inNotifier
                                                                                )
      {
        ZS_LOG_DEBUG(log("download file from url") + ZS_PARAM("session", session->getID()) + ZS_PARAM("url", getURL) + ZS_PARAM("filename", fileNameToAppendData) + ZS_PARAM("total file size", finalFileSizeInBytes) + ZS_PARAM("remaining", remainingBytesToBeDownloaded))

        AutoRecursiveLock lock(*this);

        TransferNotifierPtr notifier = TransferNotifier::create(inNotifier);

        try {
          mTransferDelegate->onPushPresenceTransferDownloadDataFromURL(mThisWeak.lock(), getURL, fileNameToAppendData, finalFileSizeInBytes, remainingBytesToBeDownloaded, notifier);
        } catch (IPushPresenceTransferDelegateProxy::Exceptions::DelegateGone &) {
        }
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
        UseServicesHelper::debugAppend(resultEl, "transfer delegate", (bool)mTransferDelegate);

        UseServicesHelper::debugAppend(resultEl, IServicePushMailboxSession::toDebug(mMailbox));
        UseServicesHelper::debugAppend(resultEl, "mailbox subscription", mMailboxSubscription ? mMailboxSubscription->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "account", mAccount ? mAccount->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "account subscription", mAccountSubscription ? mAccountSubscription->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "previously ready", mPreviouslyReady);

        UseServicesHelper::debugAppend(resultEl, "current state", toString(mCurrentState));
        UseServicesHelper::debugAppend(resultEl, "last error", mLastError);
        UseServicesHelper::debugAppend(resultEl, "last error reason", mLastErrorReason);

        UseServicesHelper::debugAppend(resultEl, "pending attachment register queries", mPendingAttachmentRegisterQueries.size());

        UseServicesHelper::debugAppend(resultEl, "last version downloaded", mLastVersionDownloaded);

        UseServicesHelper::debugAppend(resultEl, "pending delivery", mPendingDelivery.size());

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
        if (mMailboxSubscription) {
          mMailboxSubscription->cancel();
          mMailboxSubscription.reset();
        }

        mDelegate.reset();

        mAccount.reset();
        if (mAccountSubscription) {
          mAccountSubscription->cancel();
          mAccountSubscription.reset();
        }

        mPendingDelivery.clear();
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
        if (!stepGetMessages()) goto post_step;
        if (!stepDeliverMessages()) goto post_step;

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

        mMailbox = UsePushMailboxManager::create(mThisWeak.lock(), mThisWeak.lock(), Account::convert(mAccount), mMailboxSubscription);

        String monitorFolder = UseSettings::getString(OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MAILBOX_FOLDER);

        mMailbox->monitorFolder(monitorFolder);

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
      bool PushPresence::stepGetMessages()
      {
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession::PushMessage, PushMessage)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession::PushMessageList, PushMessageList)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession::MessageIDList, MessageIDList)

        ZS_LOG_TRACE(log("step get messages"))

        String monitorFolder = UseSettings::getString(OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MAILBOX_FOLDER);
        String latestAvailableVersion = mMailbox->getLatestDownloadVersionAvailableForFolder(monitorFolder);

        if (latestAvailableVersion != mLastVersionDownloaded) {

          PushMessageListPtr added;
          MessageIDListPtr removed;

          bool result = mMailbox->getFolderMessageUpdates(monitorFolder, mLastVersionDownloaded, mLastVersionDownloaded, added, removed);

          if (!result) {
            mLastVersionDownloaded = String();
            mMailbox->getFolderMessageUpdates(monitorFolder, mLastVersionDownloaded, mLastVersionDownloaded, added, removed);
          }

          if (removed) {
            // not needed
          }

          if (added) {
            for (PushMessageList::iterator iter = added->begin(); iter != added->end(); ++iter) {
              const PushMessagePtr &message = (*iter);

              if (OPENPEER_CORE_PUSH_PRESENCE_JSON_MIME_TYPE != message->mMimeType) {
                ZS_LOG_WARNING(Detail, log("presence mime type was not understood") + ZS_PARAM("mime type", message->mMimeType) + ZS_PARAM("expecting", OPENPEER_CORE_PUSH_PRESENCE_JSON_MIME_TYPE))
                continue;
              }

              if (!message->mFullMessage) {
                ZS_LOG_WARNING(Detail, log("will not download messages that are too big to fit into memory") + ZS_PARAM("filename", message->mFullMessageFileName))
                continue;
              }

              ElementPtr presenceEl = IHelper::createElement(UseServicesHelper::convertToString(*message->mFullMessage));

              StatusPtr status(new Status);
              status->mStatusID = message->mMessageID;

              status->mPresenceEl = presenceEl;

              status->mSent = message->mSent;
              status->mExpires = message->mExpires;

              if (message->mFrom) {
                status->mFrom = Contact::convert(UseContact::createFromPeer(Account::convert(mAccount), message->mFrom));
              }

              if (status->hasData()) {
                try {
                  ZS_LOG_DEBUG(log("notifying about new status") + status->toDebug())
                  mDelegate->onPushPresenceNewStatus(mThisWeak.lock(), status);
                } catch(IPushPresenceDelegateProxy::Exceptions::DelegateGone &) {
                  ZS_LOG_WARNING(Detail, log("delegate gone"))
                }
              }

            }
          }

          UseSettings::setString(OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MAILBOX_FOLDER, mLastVersionDownloaded);
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool PushPresence::stepDeliverMessages()
      {
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession::PushMessage, PushMessage)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession::PushInfo, MailboxPushInfo)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSendQueryDelegateProxy, IServicePushMailboxSendQueryDelegateProxy)

        ZS_LOG_TRACE(log("step deliver message"))
        if (mPendingDelivery.size() < 1) {
          ZS_LOG_TRACE(log("step deliver message - nothing to deliver"))
          return true;
        }

        for (StatusList::iterator iter = mPendingDelivery.begin(); iter != mPendingDelivery.end(); ++iter) {
          ToListPtr to = (*iter).first;
          StatusPtr status = (*iter).second;

          PushMessage message;

          message.mMessageID = status->mStatusID;
          message.mTo = to;
          message.mSent = status->mSent;
          message.mExpires = status->mExpires;
          message.mFullMessage = status->mPresenceEl ? UseServicesHelper::convertToBuffer(IHelper::convertToString(status->mPresenceEl)) : SecureByteBlockPtr();
          message.mFrom = status->mFrom ? UseContactPtr(Contact::convert(status->mFrom))->getPeer() : IPeerPtr();
          message.mMimeType = OPENPEER_CORE_PUSH_PRESENCE_JSON_MIME_TYPE;

          message.mPushType = UseSettings::getString(OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MESSAGE_TYPE);

          for (PushInfoList::iterator pushIter = status->mPushInfos.begin(); pushIter != status->mPushInfos.end(); ++pushIter) {
            const PushInfo &sourcePushInfo = (*pushIter);

            MailboxPushInfo pushInfo;
            pushInfo.mServiceType = sourcePushInfo.mServiceType;
            pushInfo.mValues = sourcePushInfo.mValues ? sourcePushInfo.mValues->clone()->toElement() : ElementPtr();
            pushInfo.mCustom = sourcePushInfo.mValues ? sourcePushInfo.mCustom->clone()->toElement() : ElementPtr();

            message.mPushInfos.push_back(pushInfo);
          }

          mMailbox->sendMessage(IServicePushMailboxSendQueryDelegateProxy::createNoop(getAssociatedMessageQueue()), message, UseSettings::getString(OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MAILBOX_FOLDER), false);
        }

        mPendingDelivery.clear();

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
                                                   IPushPresenceTransferDelegatePtr transferDelegate,
                                                   IAccountPtr account
                                                   )
      {
        if (this) {}
        return PushPresence::create(delegate, transferDelegate, account);
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
                                           IPushPresenceTransferDelegatePtr transferDelegate,
                                           IAccountPtr account
                                           )
    {
      return internal::IPushPresenceFactory::singleton().create(delegate, transferDelegate, account);
    }

    //-------------------------------------------------------------------------
    IPushPresence::NameValueMapPtr IPushPresence::getValues(const PushInfo &inPushInfo)
    {
      ZS_DECLARE_TYPEDEF_PTR(IPushMessaging::PushInfo, MessagingPushInfo)

      MessagingPushInfo pushInfo;
      pushInfo.mServiceType = inPushInfo.mServiceType;
      pushInfo.mValues = inPushInfo.mValues;
      pushInfo.mCustom = inPushInfo.mCustom;

      return IPushMessaging::getValues(pushInfo);
    }

    //-------------------------------------------------------------------------
    ElementPtr IPushPresence::createValues(const NameValueMap &values)
    {
      return IPushMessaging::createValues(values);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushPresence::Status
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IPushPresence::Status::createEmptyPresence()
    {
      return Element::create("presence");
    }

    //-------------------------------------------------------------------------
    bool IPushPresence::Status::hasData() const
    {
      return ((mStatusID.hasData()) ||

              ((bool)mPresenceEl) ||

              (Time() != mSent) ||
              (Time() != mExpires) ||

              ((bool)mFrom));
    }

    //-------------------------------------------------------------------------
    ElementPtr IPushPresence::Status::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::IPushPresence::Status");

      UseServicesHelper::debugAppend(resultEl, "id", mStatusID);

      UseServicesHelper::debugAppend(resultEl, "presence", (bool)mPresenceEl);

      UseServicesHelper::debugAppend(resultEl, "sent", mSent);
      UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

      UseServicesHelper::debugAppend(resultEl, IContact::toDebug(mFrom));

      return resultEl;
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushPresence::Status
    #pragma mark

    //-------------------------------------------------------------------------
    bool IPushPresence::RegisterDeviceInfo::hasData() const
    {
      return ((mDeviceToken.hasData()) ||
              (Time() != mExpires) ||
              (mMappedType.hasData()) ||
              (mUnreadBadge) ||
              (mSound.hasData()) ||
              (mAction.hasData()) ||
              (mLaunchImage.hasData()) ||
              (0 != mPriority) ||
              (mValueNames.size() > 0));
    }

    //-------------------------------------------------------------------------
    ElementPtr IPushPresence::RegisterDeviceInfo::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::IPushPresence::RegisterDeviceInfo");

      UseServicesHelper::debugAppend(resultEl, "device token", mDeviceToken);
      UseServicesHelper::debugAppend(resultEl, "expires", mExpires);
      UseServicesHelper::debugAppend(resultEl, "mapped type", mMappedType);
      UseServicesHelper::debugAppend(resultEl, "unread badge", mUnreadBadge);
      UseServicesHelper::debugAppend(resultEl, "sound", mSound);
      UseServicesHelper::debugAppend(resultEl, "action", mAction);
      UseServicesHelper::debugAppend(resultEl, "launch image", mLaunchImage);
      UseServicesHelper::debugAppend(resultEl, "priority", mPriority);
      UseServicesHelper::debugAppend(resultEl, "value names", mValueNames.size());

      return resultEl;
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}