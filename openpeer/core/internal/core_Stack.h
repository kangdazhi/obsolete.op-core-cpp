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

#pragma once

#include <openpeer/core/IStack.h>
#include <openpeer/core/internal/types.h>

#define OPENPEER_CORE_SETTING_STACK_CORE_THREAD_PRIORITY "openpeer/core/core-thread-priority"
#define OPENPEER_CORE_SETTING_STACK_MEDIA_THREAD_PRIORITY "openpeer/core/media-thread-priority"

#define OPENPEER_CORE_SETTING_STACK_AUTHORIZED_APPLICATION_ID_SPLIT_CHAR "openpeer/core/authorized-application-id-split-char"


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IStackShutdownCheckAgain
      #pragma mark

      interaction IStackShutdownCheckAgain
      {
        virtual void notifyShutdownCheckAgain() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IStackForInternal
      #pragma mark

      interaction IStackForInternal
      {
        static const String &userAgent();
        static const String &deviceID();
        static const String &os();
        static const String &system();

        static IMessageQueuePtr queueApplication();
        static IMessageQueuePtr queueCore();
        static IMessageQueuePtr queueMedia();
        static IMessageQueuePtr queueServices();
        static IMessageQueuePtr queueKeyGeneration();

        static IMediaEngineDelegatePtr        mediaEngineDelegate();
        static IConversationThreadDelegatePtr conversationThreadDelegate();
        static ICallDelegatePtr               callDelegate();

        static void finalShutdown();
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack
      #pragma mark

      class Stack : public IStack,
                    public IStackForInternal,
                    public IStackShutdownCheckAgain,
                    public IStackMessageQueue
      {
      public:
        friend interaction IStack;
        friend interaction IStackMessageQueue;
        friend interaction IStackForInternal;
        friend interaction IStackShutdownCheckAgain;

      protected:
        Stack();

        static StackPtr convert(IStackPtr stack);

        static StackPtr create();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => IStack
        #pragma mark

        static StackPtr singleton();

        virtual void setup(
                           IStackDelegatePtr stackDelegate,
                           IMediaEngineDelegatePtr mediaEngineDelegate
                           );

        virtual void shutdown();

        static String createAuthorizedApplicationID(
                                                    const char *applicationID,
                                                    const char *applicationIDSharedSecret,
                                                    Time expires
                                                    );

        static Time getAuthorizedApplicationIDExpiry(
                                                     const char *authorizedApplicationID,
                                                     Duration *outRemainingDurationAvailable = NULL
                                                     );

        static bool isAuthorizedApplicationIDExpiryWindowStillValid(
                                                                    const char *authorizedApplicationID,
                                                                    Duration minimumValidityWindowRequired
                                                                    );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => IStackMessageQueue
        #pragma mark

        // static IStackMessageQueuePtr singleton();

        virtual void interceptProcessing(IStackMessageQueueDelegatePtr delegate);

        virtual void notifyProcessMessageFromCustomThread();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => IStackShutdownCheckAgain
        #pragma mark

        void notifyShutdownCheckAgain();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => IStackForInternal
        #pragma mark

        virtual IMessageQueuePtr getQueueApplication();
        virtual IMessageQueuePtr getQueueCore();
        virtual IMessageQueuePtr getQueueMedia();
        virtual IMessageQueuePtr getQueueServices() const;
        virtual IMessageQueuePtr getQueueKeyGeneration() const;

        virtual IMediaEngineDelegatePtr getMediaEngineDelegate() const;

        virtual void finalShutdown();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => (internal)
        #pragma mark

        void makeReady();

        static Log::Params slog(const char *message);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => (data)
        #pragma mark

        mutable RecursiveLock mLock;
        PUID mID;
        StackWeakPtr mThisWeak;

        IShutdownCheckAgainDelegatePtr mShutdownCheckAgainDelegate;

        IMessageQueuePtr mApplicationQueue;
        IMessageQueuePtr mCoreQueue;
        IMessageQueuePtr mMediaQueue;

        IStackDelegatePtr              mStackDelegate;
        IMediaEngineDelegatePtr        mMediaEngineDelegate;

        IStackMessageQueueDelegatePtr  mStackMessageQueueDelegate;
      };
    }
  }
}
