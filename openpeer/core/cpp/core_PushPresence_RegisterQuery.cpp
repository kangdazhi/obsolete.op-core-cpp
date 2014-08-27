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
      #pragma mark PushPresence::RegisterQuery
      #pragma mark

      //-----------------------------------------------------------------------
      PushPresence::RegisterQuery::RegisterQuery(
                                                  IMessageQueuePtr queue,
                                                  const SharedRecursiveLock &lock,
                                                  IPushPresenceRegisterQueryDelegatePtr delegate,
                                                  const char *deviceToken,
                                                  Time expires,
                                                  const char *mappedType,
                                                  bool unreadBadge,
                                                  const char *sound,
                                                  const char *action,
                                                  const char *launchImage,
                                                  unsigned int priority,
                                                  const ValueNameList &valueNames
                                                  ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDelegate(IPushPresenceRegisterQueryDelegateProxy::createWeak(UseStack::queueApplication(), delegate)),
        mDeviceToken(deviceToken),
        mExpires(expires),
        mMappedType(mappedType),
        mUnreadBadge(unreadBadge),
        mSound(sound),
        mAction(action),
        mLaunchImage(launchImage),
        mPriority(priority),
        mValueNames(valueNames),

        mLastErrorCode(0)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void PushPresence::RegisterQuery::init()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("init called"))
      }

      //-----------------------------------------------------------------------
      PushPresence::RegisterQuery::~RegisterQuery()
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
      #pragma mark PushPresence::RegisterQuery => friend PushPresence
      #pragma mark

      //-----------------------------------------------------------------------
      PushPresence::RegisterQueryPtr PushPresence::RegisterQuery::create(
                                                                           IMessageQueuePtr queue,
                                                                           const SharedRecursiveLock &lock,
                                                                           IPushPresenceRegisterQueryDelegatePtr delegate,
                                                                           const char *deviceToken,
                                                                           Time expires,
                                                                           const char *mappedType,
                                                                           bool unreadBadge,
                                                                           const char *sound,
                                                                           const char *action,
                                                                           const char *launchImage,
                                                                           unsigned int priority,
                                                                           const ValueNameList &valueNames
                                                                           )
      {
        RegisterQueryPtr pThis(new RegisterQuery(UseStack::queueCore(), lock, delegate, deviceToken, expires, mappedType, unreadBadge, sound, action, launchImage, priority, valueNames));
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PushPresence::RegisterQuery::attachMailbox(IServicePushMailboxSessionPtr mailbox)
      {
        ZS_LOG_DEBUG(log("attach mailbox called"))

        ZS_THROW_INVALID_ARGUMENT_IF(!mailbox)

        AutoRecursiveLock lock(*this);

        mQuery = mailbox->registerDevice(mThisWeak.lock(), mDeviceToken, services::ISettings::getString(OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MAILBOX_FOLDER), mExpires, mMappedType, mUnreadBadge, mSound, mAction, mLaunchImage, mPriority, mValueNames);

        mHadQuery = true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushPresence::RegisterQuery => IPushPresenceQuery
      #pragma mark

      //-----------------------------------------------------------------------
      void PushPresence::RegisterQuery::cancel()
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
      bool PushPresence::RegisterQuery::isComplete(
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
      #pragma mark PushPresence::RegisterQuery => IServicePushMailboxSendQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PushPresence::RegisterQuery::onPushMailboxRegisterQueryCompleted(IServicePushMailboxRegisterQueryPtr query)
      {
        ZS_LOG_DEBUG(log("notified complete"))

        AutoRecursiveLock lock(*this);

        if (!mDelegate) return;

        try {
          mDelegate->onPushPresenceRegisterQueryCompleted(mThisWeak.lock());
        } catch (IPushPresenceRegisterQueryDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushPresence::RegisterQuery => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PushPresence::RegisterQuery::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::PushPresence::RegisterQuery");
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
