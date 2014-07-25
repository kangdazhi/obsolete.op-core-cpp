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

#ifdef OPENPEER_CORE_PUSH_MESSAGING_PUSH_QUERY

#if 0
namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      class PushMessaging //...
      {
#endif //0

      public:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging::PushQuery
        #pragma mark

        class PushQuery : public MessageQueueAssociator,
                          public SharedRecursiveLock,
                          public IPushMessagingQuery
        {
        public:
          ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSendQuery, IServicePushMailboxSendQuery)

        protected:
          PushQuery(
                    IMessageQueuePtr queue,
                    const SharedRecursiveLock &lock,
                    IPushMessagingQueryDelegatePtr delegate
                    );
          void init();

        public:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushMessaging::PushQuery => friend PushMessaging
          #pragma mark

          PushQueryPtr create(
                              IMessageQueuePtr queue,
                              const SharedRecursiveLock &lock,
                              IPushMessagingQueryDelegatePtr delegate,
                              PushMessagePtr message
                              );

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushMessaging::PushQuery => IPushMessagingQuery
          #pragma mark

          virtual PUID getID() const {return mID;}

          virtual void cancel();

          virtual bool isUploaded() const;
          virtual PushMessagePtr getPushMessage();

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushMessaging::PushQuery => (data)
          #pragma mark

          AutoPUID mID;

          IPushMessagingDelegatePtr mDelegate;
          PushMessagePtr mMessage;

          IServicePushMailboxSendQueryPtr mQuery;
        };

#if 0
      };

    }
  }
}
#endif //0

#else
#include <openpeer/core/internal/core_PushMessaging.h>
#endif //OPENPEER_CORE_PUSH_MESSAGING_PUSH_QUERY
