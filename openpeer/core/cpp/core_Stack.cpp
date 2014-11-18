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

#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_MediaEngine.h>
#include <openpeer/core/internal/core_Settings.h>
#include <openpeer/core/IConversationThread.h>
#include <openpeer/core/ICall.h>

#include <openpeer/stack/IStack.h>
#include <openpeer/stack/IHelper.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ILogger.h>
#include <openpeer/services/ISettings.h>
#include <openpeer/services/IMessageQueueManager.h>

#include <zsLib/helpers.h>
#include <zsLib/MessageQueueThread.h>
#include <zsLib/Socket.h>

#define OPENPEER_CORE_STACK_CORE_THREAD_QUEUE_NAME  "org.openpeer.core.coreThread"
#define OPENPEER_CORE_STACK_MEDIA_THREAD_QUEUE_NAME "org.openpeer.core.mediaThread"


namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      interaction IShutdownCheckAgainDelegate
      {
        virtual void onShutdownCheckAgain() = 0;
      };
    }
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::core::internal::IShutdownCheckAgainDelegate)
ZS_DECLARE_PROXY_METHOD_0(onShutdownCheckAgain)
ZS_DECLARE_PROXY_END()

namespace openpeer
{
  namespace core
  {
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
    using services::IMessageQueueManager;

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IStackAutoCleanup
    #pragma mark

    interaction IStackAutoCleanup
    {
    };

    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(IMediaEngineForStack, UseMediaEngine)

      typedef IStackForInternal UseStack;

      using zsLib::IMessageQueue;

      ZS_DECLARE_CLASS_PTR(ShutdownCheckAgain)
      ZS_DECLARE_CLASS_PTR(StackAutoCleanup)

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
      #pragma mark ShutdownCheckAgain
      #pragma mark

      class ShutdownCheckAgain : public IShutdownCheckAgainDelegate,
                                 public zsLib::MessageQueueAssociator
      {
      protected:
        //---------------------------------------------------------------------
        ShutdownCheckAgain(
                           IMessageQueuePtr queue,
                           IStackShutdownCheckAgainPtr stack
                           ) :
          zsLib::MessageQueueAssociator(queue),
          mStack(stack)
        {}

      public:
        //---------------------------------------------------------------------
        static ShutdownCheckAgainPtr create(
                                            IMessageQueuePtr queue,
                                            IStackShutdownCheckAgainPtr stack
                                            )
        {
          return ShutdownCheckAgainPtr(new ShutdownCheckAgain(queue, stack));
        }

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ShutdownCheckAgain => IShutdownCheckAgainDelegate
        #pragma mark

        //---------------------------------------------------------------------
        virtual void onShutdownCheckAgain()
        {
          mStack->notifyShutdownCheckAgain();
        }

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ShutdownCheckAgain => (data)
        #pragma mark

        IStackShutdownCheckAgainPtr mStack;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark StackAutoCleanup
      #pragma mark

      class StackAutoCleanup : public IStackAutoCleanup
      {
      protected:
        StackAutoCleanup() {}

      public:
        ~StackAutoCleanup()
        {
          UseStack::finalShutdown();
        }

        static StackAutoCleanupPtr create()
        {
          return StackAutoCleanupPtr(new StackAutoCleanup);
        }
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack
      #pragma mark

      ZS_DECLARE_CLASS_PTR(InterceptApplicationThread)

      class InterceptApplicationThread : public IMessageQueueThread,
                                         public IMessageQueueNotify
      {
      public:

      protected:
        //---------------------------------------------------------------------
        InterceptApplicationThread(IStackMessageQueueDelegatePtr delegate) :
          mDelegate(delegate)
        {
        }

      public:
        //---------------------------------------------------------------------
        static InterceptApplicationThreadPtr create(IStackMessageQueueDelegatePtr delegate) {
          InterceptApplicationThreadPtr pThis(new InterceptApplicationThread(delegate));
          pThis->mThisWeak = pThis;
          pThis->mQueue = MessageQueue::create(pThis);
          return pThis;
        }

        //---------------------------------------------------------------------
        virtual void post(IMessageQueueMessagePtr message)
        {
          mQueue->post(message);
        }

        //---------------------------------------------------------------------
        virtual IMessageQueue::size_type getTotalUnprocessedMessages() const
        {
          AutoLock lock(mLock);
          return mQueue->getTotalUnprocessedMessages();
        }

        //---------------------------------------------------------------------
        virtual void waitForShutdown()
        {
          IStackMessageQueueDelegatePtr delegate;

          {
            AutoLock lock(mLock);
            delegate = mDelegate;

            mDelegate.reset();
          }
        }

        //---------------------------------------------------------------------
        virtual void setThreadPriority(zsLib::ThreadPriorities priority)
        {
          // no-op
        }

        //---------------------------------------------------------------------
        virtual void processMessagesFromThread()
        {
          mQueue->process();
        }

        //---------------------------------------------------------------------
        virtual void notifyMessagePosted()
        {
          IStackMessageQueueDelegatePtr delegate;
          {
            AutoLock lock(mLock);
            delegate = mDelegate;
          }

          ZS_THROW_CUSTOM_MSG_IF(IMessageQueue::Exceptions::MessageQueueGone, !delegate, "message posted to message queue after queue was deleted.")

          delegate->onStackMessageQueueWakeUpCustomThreadAndProcessOnCustomThread();
        }

        //---------------------------------------------------------------------
        void processMessage()
        {
          mQueue->processOnlyOneMessage();
        }

      protected:
        mutable Lock mLock;
        InterceptApplicationThreadPtr mThisWeak;
        IStackMessageQueueDelegatePtr mDelegate;
        MessageQueuePtr mQueue;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IStackForInternal
      #pragma mark


      //-----------------------------------------------------------------------
      IMessageQueuePtr IStackForInternal::queueApplication()
      {
        StackPtr singleton = Stack::singleton();
        if (!singleton) {
          return services::IMessageQueueManager::getMessageQueueForGUIThread();
        }
        return singleton->getQueueApplication();
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr IStackForInternal::queueCore()
      {
        StackPtr singleton = Stack::singleton();
        if (!singleton) {
          return services::IMessageQueueManager::getMessageQueue(OPENPEER_CORE_STACK_CORE_THREAD_QUEUE_NAME);
        }
        return singleton->getQueueCore();
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr IStackForInternal::queueMedia()
      {
        StackPtr singleton = Stack::singleton();
        if (!singleton) {
          return services::IMessageQueueManager::getMessageQueue(OPENPEER_CORE_STACK_MEDIA_THREAD_QUEUE_NAME);
        }
        return singleton->getQueueMedia();
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr IStackForInternal::queueServices()
      {
        StackPtr singleton = Stack::singleton();
        if (!singleton) {
          return UseServicesHelper::getServiceQueue();
        }
        return singleton->getQueueServices();
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr IStackForInternal::queueKeyGeneration()
      {
        StackPtr singleton = Stack::singleton();
        if (!singleton) {
          return stack::IStack::getKeyGenerationQueue();
        }
        return singleton->getQueueKeyGeneration();
      }

      //-----------------------------------------------------------------------
      IMediaEngineDelegatePtr IStackForInternal::mediaEngineDelegate()
      {
        StackPtr singleton = Stack::singleton();
        if (!singleton) return IMediaEngineDelegatePtr();
        return singleton->getMediaEngineDelegate();
      }

      //-----------------------------------------------------------------------
      void IStackForInternal::finalShutdown()
      {
        StackPtr singleton = Stack::singleton();
        if (!singleton) return;
        return singleton->finalShutdown();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack
      #pragma mark

      //-----------------------------------------------------------------------
      Stack::Stack() :
        mID(zsLib::createPUID())
      {
      }

      //-----------------------------------------------------------------------
      StackPtr Stack::convert(IStackPtr stack)
      {
        return ZS_DYNAMIC_PTR_CAST(Stack, stack);
      }

      //-----------------------------------------------------------------------
      StackPtr Stack::create()
      {
        StackPtr pThis(new Stack());
        pThis->mThisWeak = pThis;
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack => IStack
      #pragma mark

      //-----------------------------------------------------------------------
      StackPtr Stack::singleton()
      {
        static SingletonLazySharedPtr<Stack> singleton(create());
        StackPtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Detail, slog("singleton gone"))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      void Stack::setup(
                        IStackDelegatePtr stackDelegate,
                        IMediaEngineDelegatePtr mediaEngineDelegate
                        )
      {
        UseServicesHelper::setSocketThreadPriority();
        UseServicesHelper::setTimerThreadPriority();

        AutoRecursiveLock lock(mLock);

        ISettingsForStack::applyDefaultsIfNoDelegatePresent();

        String deviceID = services::ISettings::getString(OPENPEER_COMMON_SETTING_DEVICE_ID);
        String instanceID = services::ISettings::getString(OPENPEER_COMMON_SETTING_INSTANCE_ID);
        String authorizedAppId = services::ISettings::getString(OPENPEER_COMMON_SETTING_APPLICATION_AUTHORIZATION_ID);

        ZS_LOG_FORCED(Informational, Basic, slog("instance information") + ZS_PARAM("device id", deviceID) + ZS_PARAM("instance id", instanceID) + ZS_PARAM("authorized application id", authorizedAppId))

        IMessageQueueManager::registerMessageQueueThreadPriority(OPENPEER_CORE_STACK_CORE_THREAD_QUEUE_NAME, zsLib::threadPriorityFromString(services::ISettings::getString(OPENPEER_CORE_SETTING_STACK_CORE_THREAD_PRIORITY)));
        IMessageQueueManager::registerMessageQueueThreadPriority(OPENPEER_CORE_STACK_MEDIA_THREAD_QUEUE_NAME, zsLib::threadPriorityFromString(services::ISettings::getString(OPENPEER_CORE_SETTING_STACK_MEDIA_THREAD_PRIORITY)));

        makeReady();

        if (stackDelegate) {
          mStackDelegate = IStackDelegateProxy::create(getQueueApplication(), stackDelegate);
        }

        if (mediaEngineDelegate) {
          mMediaEngineDelegate = IMediaEngineDelegateProxy::create(getQueueApplication(), mediaEngineDelegate);
          UseMediaEngine::setup(mMediaEngineDelegate);
        }

        if (!isAuthorizedApplicationIDExpiryWindowStillValid(authorizedAppId, Seconds(1))) {
          ZS_LOG_WARNING(Basic, slog("application id is not valid") + ZS_PARAM("authorized application id", authorizedAppId))
        }

        stack::IStack::setup(queueApplication());
      }

      //-----------------------------------------------------------------------
      void Stack::shutdown()
      {
        AutoRecursiveLock lock(mLock);

        if (!mApplicationQueue) {
          // already shutdown...
          return;
        }

        if (mShutdownCheckAgainDelegate) {
          // already shutting down...
          return;
        }

        ShutdownCheckAgainPtr checkAgain = ShutdownCheckAgain::create(getQueueApplication(), mThisWeak.lock());
        mShutdownCheckAgainDelegate = IShutdownCheckAgainDelegateProxy::create(checkAgain);

        mShutdownCheckAgainDelegate->onShutdownCheckAgain();
      }

      //-----------------------------------------------------------------------
      String Stack::createAuthorizedApplicationID(
                                                  const char *applicationID,
                                                  const char *applicationIDSharedSecret,
                                                  Time expires
                                                  )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!applicationID)
        ZS_THROW_INVALID_ARGUMENT_IF(!applicationIDSharedSecret)
        ZS_THROW_INVALID_ARGUMENT_IF(Time() == expires)

        String fakeDomain = String(applicationID) + ".com";

        String splitChar = services::ISettings::getString(OPENPEER_CORE_SETTING_STACK_AUTHORIZED_APPLICATION_ID_SPLIT_CHAR);
        if (splitChar.isEmpty()) {
          splitChar = ":";
        }

        if (!UseServicesHelper::isValidDomain(fakeDomain)) {
          // if you are hitting this it's because your app ID value was set wrong
          ZS_LOG_WARNING(Basic, slog("illegal application ID value") + ZS_PARAM("application ID", applicationID))
          ZS_THROW_INVALID_ARGUMENT(slog("Illegal application ID value"))
        }

        String appID(applicationID);
        String random = UseServicesHelper::randomString(20);
        String time = UseServicesHelper::timeToString(expires);

        String merged = appID + splitChar + random + splitChar + time;

        String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::convertToBuffer(applicationIDSharedSecret), merged));

        String final = merged + splitChar + hash;

        ZS_LOG_WARNING(Basic, slog("method should only be called during development") + ZS_PARAM("authorized application ID", final))

        return final;
      }

      //-----------------------------------------------------------------------
      Time Stack::getAuthorizedApplicationIDExpiry(
                                                   const char *authorizedApplicationID,
                                                   Seconds *outRemainingDurationAvailable
                                                   )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!authorizedApplicationID)

        if (outRemainingDurationAvailable) {
          *outRemainingDurationAvailable = Seconds(0);
        }

        String splitChar = services::ISettings::getString(OPENPEER_CORE_SETTING_STACK_AUTHORIZED_APPLICATION_ID_SPLIT_CHAR);
        if (splitChar.isEmpty()) {
          splitChar = ":";
        }

        char splitter = *splitChar.c_str();

        UseServicesHelper::SplitMap split;
        UseServicesHelper::split(authorizedApplicationID, split, splitter);

        if (split.size() < 3) {
          ZS_LOG_WARNING(Detail, slog("authorized application id is not in a valid format") + ZS_PARAM("authorized application id", authorizedApplicationID))
          return Time();
        }

        String timeStr = (*(split.find(split.size()-2))).second;

        Time expires = UseServicesHelper::stringToTime(timeStr);
        if (Time() == expires) {
          ZS_LOG_WARNING(Detail, slog("authorized application id time segment is not in a valid format") + ZS_PARAM("authorized application id", authorizedApplicationID) + ZS_PARAM("time str", timeStr))
          return Time();
        }

        Time now = zsLib::now();
        if (now < expires) {
          if (outRemainingDurationAvailable) {
            *outRemainingDurationAvailable = zsLib::toSeconds(expires - now);
          }
        }

        return expires;
      }

      //-----------------------------------------------------------------------
      bool Stack::isAuthorizedApplicationIDExpiryWindowStillValid(
                                                                  const char *authorizedApplicationID,
                                                                  Seconds minimumValidityWindowRequired
                                                                  )
      {
        Seconds available;
        Time expires = getAuthorizedApplicationIDExpiry(authorizedApplicationID, &available);

        if (available < minimumValidityWindowRequired) {
          ZS_LOG_BASIC(slog("authorized application id will expire") + ZS_PARAM("authorized application id", authorizedApplicationID) + ZS_PARAM("expires at", expires) + ZS_PARAM("window (s)", minimumValidityWindowRequired) + ZS_PARAM("available (s)", available))
          return false;
        }

        ZS_LOG_TRACE(slog("authorized application id is still valid") + ZS_PARAM("authorized application id", authorizedApplicationID) + ZS_PARAM("expires at", expires) + ZS_PARAM("window (s)", minimumValidityWindowRequired) + ZS_PARAM("available (s)", available))
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack => IStackMessageQueue
      #pragma mark

      //-----------------------------------------------------------------------
      void Stack::interceptProcessing(IStackMessageQueueDelegatePtr delegate)
      {
        ZS_THROW_INVALID_USAGE_IF(mApplicationQueue)
        mStackMessageQueueDelegate = delegate;
      }

      //-----------------------------------------------------------------------
      void Stack::notifyProcessMessageFromCustomThread()
      {
        InterceptApplicationThreadPtr thread;
        {
          AutoRecursiveLock lock(mLock);
          thread = ZS_DYNAMIC_PTR_CAST(InterceptApplicationThread, mApplicationQueue);
          ZS_THROW_INVALID_USAGE_IF(!thread)  // you can only call this method if you specified a delegate upon setup and have not already finalized the shutdown
        }

        thread->processMessage();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack => IStackShutdownCheckAgain
      #pragma mark

      //-----------------------------------------------------------------------
      void Stack::notifyShutdownCheckAgain()
      {
        ZS_TRACE_THIS()

        AutoRecursiveLock lock(mLock);

        ZS_THROW_BAD_STATE_IF(!mShutdownCheckAgainDelegate)

        size_t total = IMessageQueueManager::getTotalUnprocessedMessages();

        if (total > 0) {
          mShutdownCheckAgainDelegate->onShutdownCheckAgain();
          return;
        }

        // all activity has ceased on the threads so clean out the delegates remaining in this class
        mMediaEngineDelegate.reset();

        // cleaning the delegates could cause more activity to start
        total = IMessageQueueManager::getTotalUnprocessedMessages();

        if (total > 0) {
          mShutdownCheckAgainDelegate->onShutdownCheckAgain();
          return;
        }

        // delegates are now gone and all activity has stopped, stopped sending notifications to self via the GUI thread
        mShutdownCheckAgainDelegate.reset();

        IStackAutoCleanupPtr autoCleanup = StackAutoCleanup::create();

        // notify the GUI thread it is now safe to finalize the shutdown
        mStackDelegate->onStackShutdown(autoCleanup);
        mStackDelegate.reset();

        // the telnet logger must disconnect here before anything can continue
        services::ILogger::uninstallTelnetLogger();

        // at this point all proxies to delegates should be completely destroyed - if they are not then someone forgot to do some clean-up!
        zsLib::proxyDump();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack => IStackForInternal
      #pragma mark

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueApplication()
      {
        AutoRecursiveLock lock(mLock);
        if (!mApplicationQueue) {
          mApplicationQueue = IMessageQueueManager::getMessageQueueForGUIThread();
        }
        return mApplicationQueue;
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueCore()
      {
        AutoRecursiveLock lock(mLock);
        ZS_THROW_INVALID_USAGE_IF(!mApplicationQueue) // set-up was not called
        if (!mCoreQueue) {
          mCoreQueue = IMessageQueueManager::getMessageQueue(OPENPEER_CORE_STACK_CORE_THREAD_QUEUE_NAME);
        }
        return mCoreQueue;
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueMedia()
      {
        AutoRecursiveLock lock(mLock);
        ZS_THROW_INVALID_USAGE_IF(!mApplicationQueue) // set-up was not called
        if (!mMediaQueue) {
          mMediaQueue = IMessageQueueManager::getMessageQueue(OPENPEER_CORE_STACK_MEDIA_THREAD_QUEUE_NAME);
        }
        return mMediaQueue;
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueServices() const
      {
        AutoRecursiveLock lock(mLock);
        return UseServicesHelper::getServiceQueue();
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueKeyGeneration() const
      {
        AutoRecursiveLock lock(mLock);
        return stack::IStack::getKeyGenerationQueue();
      }

      //-----------------------------------------------------------------------
      IMediaEngineDelegatePtr Stack::getMediaEngineDelegate() const
      {
        AutoRecursiveLock lock(mLock);
        return mMediaEngineDelegate;
      }

      //-----------------------------------------------------------------------
      void Stack::finalShutdown()
      {
        IMessageQueueThreadPtr applicationThread;
        MessageQueueThreadPtr  coreThread;
        MessageQueueThreadPtr  mediaThread;
        MessageQueueThreadPtr  servicesThread;
        MessageQueueThreadPtr  keyGenerationThread;
        IStackMessageQueueDelegatePtr stackMessage;

        {
          AutoRecursiveLock lock(mLock);
          mApplicationQueue.reset();
          mCoreQueue.reset();
          mMediaQueue.reset();
          mStackMessageQueueDelegate.reset();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      void Stack::makeReady()
      {
        if (mApplicationQueue) return;

        Socket::ignoreSIGPIPEOnThisThread();

        if (!mStackMessageQueueDelegate) {
          getQueueApplication();
        } else {
          mApplicationQueue = InterceptApplicationThread::create(mStackMessageQueueDelegate);
          mStackMessageQueueDelegate.reset();
        }
      }

      //-----------------------------------------------------------------------
      Log::Params Stack::slog(const char *message)
      {
        return Log::Params(message, "core::Stack");
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IStack
    #pragma mark

    //-------------------------------------------------------------------------
    IStackPtr IStack::singleton()
    {
      return internal::Stack::singleton();
    }

    //-------------------------------------------------------------------------
    String IStack::createAuthorizedApplicationID(
                                                 const char *applicationID,
                                                 const char *applicationIDSharedSecret,
                                                 Time expires
                                                 )
    {
      return internal::Stack::createAuthorizedApplicationID(applicationID, applicationIDSharedSecret, expires);
    }

    //-------------------------------------------------------------------------
    Time IStack::getAuthorizedApplicationIDExpiry(
                                                  const char *authorizedApplicationID,
                                                  Seconds *outRemainingDurationAvailable
                                                  )
    {
      return internal::Stack::getAuthorizedApplicationIDExpiry(authorizedApplicationID, outRemainingDurationAvailable);
    }

    //-------------------------------------------------------------------------
    bool IStack::isAuthorizedApplicationIDExpiryWindowStillValid(
                                                               const char *authorizedApplicationID,
                                                               Seconds minimumValidityWindowRequired
                                                               )
    {
      return internal::Stack::isAuthorizedApplicationIDExpiryWindowStillValid(authorizedApplicationID, minimumValidityWindowRequired);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IStackMessageQueue
    #pragma mark

    //-------------------------------------------------------------------------
    IStackMessageQueuePtr IStackMessageQueue::singleton()
    {
      return internal::Stack::singleton();
    }
  }
}
