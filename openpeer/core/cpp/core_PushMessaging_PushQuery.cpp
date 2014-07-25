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

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
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
      #pragma mark PushMessaging::PushQuery
      #pragma mark

      //-----------------------------------------------------------------------
      PushMessaging::PushQuery::PushQuery(
                                          IMessageQueuePtr queue,
                                          const SharedRecursiveLock &lock,
                                          IPushMessagingQueryDelegatePtr delegate,
                                          PushMessagePtr message
                                          ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDelegate(IPushMessagingQueryDelegateProxy::createWeak(UseStack::queueApplication(), delegate)),
        mMessage(message)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void PushMessaging::PushQuery::init()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("init called"))
      }

      //-----------------------------------------------------------------------
      PushMessaging::PushQuery::~PushQuery()
      {
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging::PushQuery => friend PushMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      PushMessaging::PushQueryPtr PushMessaging::PushQuery::create(
                                                                   IMessageQueuePtr queue,
                                                                   const SharedRecursiveLock &lock,
                                                                   IPushMessagingQueryDelegatePtr delegate,
                                                                   PushMessagePtr message
                                                                   )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)

        PushQueryPtr pThis(new PushQuery(UseStack::queueCore(), lock, delegate, message));
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PushMessaging::PushQuery::attachMailbox(
                                                   UseAccountPtr account,
                                                   IServicePushMailboxSessionPtr mailbox
                                                   )
      {
        ZS_LOG_DEBUG(log("attach mailbox called"))

        ZS_THROW_INVALID_ARGUMENT_IF(!account)
        ZS_THROW_INVALID_ARGUMENT_IF(!mailbox)

        AutoRecursiveLock lock(*this);

        mAccount = account;

        IServicePushMailboxSession::PushMessage message;

        PushMessaging::copy(*mMessage, message);

        mQuery = mailbox->sendMessage(mThisWeak.lock(), message, services::ISettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MAILBOX_FOLDER), true);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging::PushQuery => IPushMessagingQuery
      #pragma mark

      //-----------------------------------------------------------------------
      void PushMessaging::PushQuery::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        AutoRecursiveLock lock(*this);

        if (mQuery) {
          mQuery->cancel();
          mQuery.reset();
        }

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      bool PushMessaging::PushQuery::isUploaded() const
      {
        AutoRecursiveLock lock(*this);
        if (mReportedUploaded) return true;

        if (!mQuery) return false;

        get(mReportedUploaded) = mQuery->isUploaded();
        return mReportedUploaded;
      }

      //-----------------------------------------------------------------------
      PushMessaging::PushMessagePtr PushMessaging::PushQuery::getPushMessage()
      {
        AutoRecursiveLock lock(*this);

        if (!mMessage) {
          ZS_LOG_WARNING(Detail, log("no mesage found"))
          return PushMessagePtr();
        }

        PushMessagePtr result(new PushMessage);
        (*result) = (*mMessage);

        if (!mQuery) {
          ZS_LOG_WARNING(Detail, log("query is now gone"))
          return result;
        }

        IServicePushMailboxSession::PushMessagePtr realMessage = mQuery->getPushMessage();
        if (!realMessage) {
          ZS_LOG_WARNING(Detail, log("push mailbox message is not available"))
          return result;
        }

        PushMessaging::copy(mAccount, *realMessage, *mMessage);

        (*result) = (*mMessage);

        return result;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging::PushQuery => IServicePushMailboxSendQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushMessaging::PushQuery::onPushMailboxSendQueryMessageUploaded(IServicePushMailboxSendQueryPtr query)
      {
        ZS_LOG_DEBUG(log("notified message uploaded"))
        AutoRecursiveLock lock(*this);

        get(mReportedUploaded) = true;

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
          return;
        }

        try {
          mDelegate->onPushMessagingQueryUploaded(mThisWeak.lock());
        } catch(IPushMessagingQueryDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void PushMessaging::PushQuery::onPushMailboxSendQueryPushStatesChanged(IServicePushMailboxSendQueryPtr query)
      {
        ZS_LOG_DEBUG(log("notified send push state changed"))
        AutoRecursiveLock lock(*this);

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
          return;
        }

        try {
          mDelegate->onPushMessagingQueryPushStatesChanged(mThisWeak.lock());
        } catch(IPushMessagingQueryDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging::PushQuery => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PushMessaging::PushQuery::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::PushMessaging::PushQuery");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

  }
}
