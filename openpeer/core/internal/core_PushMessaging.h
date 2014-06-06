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

#pragma once

#include <openpeer/core/internal/types.h>
#include <openpeer/core/IPushMessaging.h>

#include <openpeer/stack/IServicePushMailbox.h>

#include <zsLib/MessageQueueAssociator.h>

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
      #pragma mark PushMessaging
      #pragma mark

      class PushMessaging : public Noop,
                            public MessageQueueAssociator,
                            public IPushMessaging
      {
      public:
        friend interaction IPushMessagingFactory;
        friend interaction IPushMessaging;

      protected:
        PushMessaging(IMessageQueuePtr queue);
        
        PushMessaging(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init();

      public:
        ~PushMessaging();

        static PushMessagingPtr convert(IPushMessagingPtr messaging);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => IPushMessaging
        #pragma mark

        static ElementPtr toDebug(IPushMessagingPtr identity);

        static PushMessagingPtr create(
                                        IPushMessagingDelegatePtr delegate,
                                        IAccountPtr account
                                        );

        virtual PUID getID() const;

        virtual PushMessagingStates getState(
                                             WORD *outErrorCode,
                                             String *outErrorReason
                                             ) const;

        virtual void shutdown();

        virtual IPushMessagingRegisterQueryPtr registerDevice(
                                                              const char *deviceToken,
                                                              Time expires,
                                                              const char *mappedType,
                                                              bool unreadBadge,
                                                              const char *sound,
                                                              const char *action,
                                                              const char *launchImage,
                                                              unsigned int priority
                                                              );

        virtual IPushMessagingQueryPtr push(
                                            IPushMessagingQueryDelegatePtr delegate,
                                            const PeerOrIdentityURIList &toContactList,
                                            const PushMessage &message
                                            );

        virtual void recheckNow();

        virtual void markPushMessageRead(const char *messageID);
        virtual void deletePushMessage(const char *messageID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => IServicePushMessagingSessionDelegate
        #pragma mark

      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => (internal)
        #pragma mark

        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => (data)
        #pragma mark

        PUID mID;
        mutable RecursiveLock mLock;
        PushMessagingWeakPtr mThisWeak;

        IPushMessagingDelegatePtr mDelegate;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPushMessagingFactory
      #pragma mark

      interaction IPushMessagingFactory
      {
        static IPushMessagingFactory &singleton();

        virtual PushMessagingPtr create(
                                        IPushMessagingDelegatePtr delegate,
                                        IAccountPtr account
                                        );
      };

    }
  }
}
