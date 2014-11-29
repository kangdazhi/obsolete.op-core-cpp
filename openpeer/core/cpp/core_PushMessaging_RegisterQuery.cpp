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

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)

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
      #pragma mark PushMessaging::RegisterQuery
      #pragma mark

      //-----------------------------------------------------------------------
      PushMessaging::RegisterQuery::RegisterQuery(
                                                  IMessageQueuePtr queue,
                                                  const SharedRecursiveLock &lock,
                                                  IPushMessagingRegisterQueryDelegatePtr delegate,
                                                  const RegisterDeviceInfo &deviceInfo
                                                  ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDelegate(IPushMessagingRegisterQueryDelegateProxy::createWeak(UseStack::queueApplication(), delegate)),
        mDeviceInfo(deviceInfo)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void PushMessaging::RegisterQuery::init()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("init called"))
      }

      //-----------------------------------------------------------------------
      PushMessaging::RegisterQuery::~RegisterQuery()
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
      #pragma mark PushMessaging::RegisterQuery => friend PushMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      PushMessaging::RegisterQueryPtr PushMessaging::RegisterQuery::create(
                                                                           IMessageQueuePtr queue,
                                                                           const SharedRecursiveLock &lock,
                                                                           IPushMessagingRegisterQueryDelegatePtr delegate,
                                                                           const RegisterDeviceInfo &deviceInfo
                                                                           )
      {
        RegisterQueryPtr pThis(new RegisterQuery(UseStack::queueCore(), lock, delegate, deviceInfo));
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PushMessaging::RegisterQuery::attachMailbox(IServicePushMailboxSessionPtr mailbox)
      {
        typedef IServicePushMailboxSession::RegisterDeviceInfo UseMailboxRegisterDeviceInfo;

        ZS_LOG_DEBUG(log("attach mailbox called"))

        ZS_THROW_INVALID_ARGUMENT_IF(!mailbox)

        AutoRecursiveLock lock(*this);

        UseMailboxRegisterDeviceInfo info;
        info.mDeviceToken = mDeviceInfo.mDeviceToken;
        info.mFolder = UseSettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MAILBOX_FOLDER);
        info.mExpires = mDeviceInfo.mExpires;
        info.mMappedType = mDeviceInfo.mMappedType;
        info.mUnreadBadge = mDeviceInfo.mUnreadBadge;
        info.mSound = mDeviceInfo.mSound;
        info.mAction = mDeviceInfo.mAction;
        info.mLaunchImage = mDeviceInfo.mLaunchImage;
        info.mPriority = mDeviceInfo.mPriority;
        info.mValueNames = mDeviceInfo.mValueNames;

        mQuery = mailbox->registerDevice(mThisWeak.lock(), info);

        mHadQuery = true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging::RegisterQuery => IPushMessagingQuery
      #pragma mark

      //-----------------------------------------------------------------------
      void PushMessaging::RegisterQuery::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        AutoRecursiveLock lock(*this);

        if (mQuery) {
          mQuery->isComplete(&mLastErrorCode, &mLastErrorReason);
          mQuery.reset();
        }

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      bool PushMessaging::RegisterQuery::isComplete(
                                                    WORD *outErrorCode,
                                                    String *outErrorReason
                                                    ) const
      {
        ZS_LOG_TRACE("is complete called")

        AutoRecursiveLock lock(*this);

        if (outErrorCode)
          *outErrorCode = mLastErrorCode;
        if (outErrorReason)
          *outErrorReason = mLastErrorReason;

        if (!mQuery) return mHadQuery;

        return mQuery->isComplete(outErrorCode, outErrorReason);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging::RegisterQuery => IServicePushMailboxSendQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushMessaging::RegisterQuery::onPushMailboxRegisterQueryCompleted(IServicePushMailboxRegisterQueryPtr query)
      {
        ZS_LOG_DEBUG(log("notified complete"))

        AutoRecursiveLock lock(*this);

        if (!mDelegate) return;

        try {
          mDelegate->onPushMessagingRegisterQueryCompleted(mThisWeak.lock());
        } catch (IPushMessagingRegisterQueryDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging::RegisterQuery => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PushMessaging::RegisterQuery::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::PushMessaging::RegisterQuery");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

  }
}
