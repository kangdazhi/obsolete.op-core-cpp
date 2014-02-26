/*

 Copyright (c) 2013, SMB Phone Inc.
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

#include <zsLib/helpers.h>
#include <zsLib/MessageQueueThread.h>
#include <zsLib/Socket.h>


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
    using services::IHelper;

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

      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "core::Stack");
      }
      
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
          MessageQueuePtr queue;
          {
            AutoLock lock(mLock);
            queue = mQueue;
          }
          queue->post(message);
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
          MessageQueuePtr queue;

          {
            AutoLock lock(mLock);
            delegate = mDelegate;
            queue = mQueue;

            mDelegate.reset();
            mQueue.reset();
          }
        }

        //---------------------------------------------------------------------
        virtual void notifyMessagePosted()
        {
          IStackMessageQueueDelegatePtr delegate;
          {
            AutoLock lock(mLock);
            delegate = mDelegate;
          }
          delegate->onStackMessageQueueWakeUpCustomThreadAndProcessOnCustomThread();
        }

        //---------------------------------------------------------------------
        void processMessage()
        {
          MessageQueuePtr queue;
          {
            AutoLock lock(mLock);
            queue = mQueue;
          }
          queue->processOnlyOneMessage();
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
        return (Stack::singleton())->getQueueApplication();
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr IStackForInternal::queueCore()
      {
        return (Stack::singleton())->getQueueCore();
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr IStackForInternal::queueMedia()
      {
        return (Stack::singleton())->getQueueMedia();
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr IStackForInternal::queueServices()
      {
        return (Stack::singleton())->getQueueServices();
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr IStackForInternal::queueKeyGeneration()
      {
        return (Stack::singleton())->getQueueKeyGeneration();
      }

      //-----------------------------------------------------------------------
      IMediaEngineDelegatePtr IStackForInternal::mediaEngineDelegate()
      {
        return (Stack::singleton())->getMediaEngineDelegate();
      }

      //-----------------------------------------------------------------------
      void IStackForInternal::finalShutdown()
      {
        return (Stack::singleton())->finalShutdown();
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
        return dynamic_pointer_cast<Stack>(stack);
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
        static StackPtr singleton = Stack::create();
        return singleton;
      }

      //-----------------------------------------------------------------------
      void Stack::setup(
                        IStackDelegatePtr stackDelegate,
                        IMediaEngineDelegatePtr mediaEngineDelegate
                        )
      {
        AutoRecursiveLock lock(mLock);

        makeReady();

        if (stackDelegate) {
          mStackDelegate = IStackDelegateProxy::create(getQueueApplication(), stackDelegate);
        }

        if (mediaEngineDelegate) {
          mMediaEngineDelegate = IMediaEngineDelegateProxy::create(getQueueApplication(), mediaEngineDelegate);
          UseMediaEngine::setup(mMediaEngineDelegate);
        }

        ISettingsForStack::applyDefaultsIfNoDelegatePresent();

        String authorizedAppId = services::ISettings::getString(OPENPEER_COMMON_SETTING_APPLICATION_AUTHORIZATION_ID);

        if (!isAuthorizedApplicationExpiryWindowStillValid(authorizedAppId, Seconds(1))) {
          ZS_LOG_WARNING(Basic, slog("application id is not valid") + ZS_PARAM("authorized application id", authorizedAppId))
        }

        stack::IStack::setup(mApplicationThreadQueue, mCoreThreadQueue, mServicesThreadQueue, mKeyGenerationThreadQueue);
      }

      //-----------------------------------------------------------------------
      void Stack::shutdown()
      {
        AutoRecursiveLock lock(mLock);

        if (!mApplicationThreadQueue) {
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

        if (!services::IHelper::isValidDomain(fakeDomain)) {
          // if you are hitting this it's because your app ID value was set wrong
          ZS_LOG_WARNING(Basic, slog("illegal application ID value") + ZS_PARAM("application ID", applicationID))
          ZS_THROW_INVALID_ARGUMENT(slog("Illegal application ID value"))
        }

        String appID(applicationID);
        String random = services::IHelper::randomString(20);
        String time = services::IHelper::timeToString(expires);

        String merged = appID + "-" + random + "-" + time;

        String hash = services::IHelper::convertToHex(*services::IHelper::hmac(*services::IHelper::convertToBuffer(applicationIDSharedSecret), merged));

        String final = merged + "-" + hash;

        ZS_LOG_WARNING(Basic, slog("method should only be called during development") + ZS_PARAM("authorized application ID", final))

        return final;
      }

      //-----------------------------------------------------------------------
      bool Stack::isAuthorizedApplicationExpiryWindowStillValid(
                                                                const char *authorizedApplicationID,
                                                                Duration minimumValidityWindowRequired
                                                                )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!authorizedApplicationID)

        IHelper::SplitMap split;

        IHelper::split(authorizedApplicationID, split, '-');

        if (split.size() < 3) {
          ZS_LOG_WARNING(Detail, slog("authorized application id is not in a valid format") + ZS_PARAM("authorized application id", authorizedApplicationID) + ZS_PARAM("window (s)", minimumValidityWindowRequired))
          return false;
        }

        String timeStr = (*(split.find(split.size()-1))).second;

        Time expires = IHelper::stringToTime(timeStr);

        Time now = zsLib::now();

        if (now + minimumValidityWindowRequired > expires) {
          ZS_LOG_BASIC(slog("authorized application id will expire") + ZS_PARAM("authorized application id", authorizedApplicationID) + ZS_PARAM("expires at", expires) + ZS_PARAM("now", now) + ZS_PARAM("window (s)", minimumValidityWindowRequired))
          return false;
        }

        ZS_LOG_TRACE(slog("authorized application id is still valid") + ZS_PARAM("authorized application id", authorizedApplicationID) + ZS_PARAM("expires at", expires) + ZS_PARAM("now", now) + ZS_PARAM("window (s)", minimumValidityWindowRequired))
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
        ZS_THROW_INVALID_USAGE_IF(mApplicationThreadQueue)
        mStackMessageQueueDelegate = delegate;
      }

      //-----------------------------------------------------------------------
      void Stack::notifyProcessMessageFromCustomThread()
      {
        InterceptApplicationThreadPtr thread;
        {
          AutoRecursiveLock lock(mLock);
          thread = dynamic_pointer_cast<InterceptApplicationThread>(mApplicationThreadQueue);
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

        IMessageQueue::size_type total = 0;

        total = mApplicationThreadQueue->getTotalUnprocessedMessages() +
                mCoreThreadQueue->getTotalUnprocessedMessages() +
                mMediaThreadQueue->getTotalUnprocessedMessages() +
                mServicesThreadQueue->getTotalUnprocessedMessages() +
                mKeyGenerationThreadQueue->getTotalUnprocessedMessages();

        if (total > 0) {
          mShutdownCheckAgainDelegate->onShutdownCheckAgain();
          return;
        }

        // all activity has ceased on the threads so clean out the delegates remaining in this class
        mMediaEngineDelegate.reset();

        // cleaning the delegates could cause more activity to start
        total = mApplicationThreadQueue->getTotalUnprocessedMessages() +
                mCoreThreadQueue->getTotalUnprocessedMessages() +
                mMediaThreadQueue->getTotalUnprocessedMessages() +
                mServicesThreadQueue->getTotalUnprocessedMessages() +
                mKeyGenerationThreadQueue->getTotalUnprocessedMessages();

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
        ULONG totalProxiesCreated = zsLib::proxyGetTotalConstructed();
        zsLib::proxyDump();
        ZS_THROW_BAD_STATE_IF(totalProxiesCreated > 0)  // DO NOT COMMENT THIS LINE AS A SOLUTION INSTEAD OF FINDING OUT WHERE YOU DID NOT SHUTDOWN/CLEANUP PROPERLY
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack => IStackForInternal
      #pragma mark

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueApplication() const
      {
        AutoRecursiveLock lock(mLock);
        return mApplicationThreadQueue;
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueCore() const
      {
        AutoRecursiveLock lock(mLock);
        return mCoreThreadQueue;
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueMedia() const
      {
        AutoRecursiveLock lock(mLock);
        return mMediaThreadQueue;
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueServices() const
      {
        AutoRecursiveLock lock(mLock);
        return mServicesThreadQueue;
      }

      //-----------------------------------------------------------------------
      IMessageQueuePtr Stack::getQueueKeyGeneration() const
      {
        AutoRecursiveLock lock(mLock);
        return mKeyGenerationThreadQueue;
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
          applicationThread = mApplicationThreadQueue;
          coreThread = mCoreThreadQueue;
          mediaThread = mMediaThreadQueue;
          servicesThread = mServicesThreadQueue;
          stackMessage = mStackMessageQueueDelegate;
          keyGenerationThread = mKeyGenerationThreadQueue;
        }

        applicationThread->waitForShutdown();
        coreThread->waitForShutdown();
        mediaThread->waitForShutdown();
        servicesThread->waitForShutdown();
        keyGenerationThread->waitForShutdown();

        {
          AutoRecursiveLock lock(mLock);
          mApplicationThreadQueue.reset();
          mCoreThreadQueue.reset();
          mMediaThreadQueue.reset();
          mServicesThreadQueue.reset();
          mKeyGenerationThreadQueue.reset();
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
        if (mCoreThreadQueue) return;

        Socket::ignoreSIGPIPEOnThisThread();

        mCoreThreadQueue = MessageQueueThread::createBasic("org.openpeer.core.mainThread");
        mMediaThreadQueue = MessageQueueThread::createBasic("org.openpeer.core.mediaThread", zsLib::ThreadPriority_RealtimePriority);
        mServicesThreadQueue = MessageQueueThread::createBasic("org.openpeer.core.servicesThread", zsLib::ThreadPriority_HighPriority);
        mKeyGenerationThreadQueue = MessageQueueThread::createBasic("org.openpeer.core.keyGenerationThread", zsLib::ThreadPriority_LowPriority);
        if (!mStackMessageQueueDelegate) {
          mApplicationThreadQueue = MessageQueueThread::singletonUsingCurrentGUIThreadsMessageQueue();
        } else {
          mApplicationThreadQueue = InterceptApplicationThread::create(mStackMessageQueueDelegate);
          mStackMessageQueueDelegate.reset();
        }
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
    bool IStack::isAuthorizedApplicationExpiryWindowStillValid(
                                                               const char *authorizedApplicationID,
                                                               Duration minimumValidityWindowRequired
                                                               )
    {
      return internal::Stack::isAuthorizedApplicationExpiryWindowStillValid(authorizedApplicationID, minimumValidityWindowRequired);
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
