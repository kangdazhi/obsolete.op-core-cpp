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

#ifdef OPENPEER_CORE_PUSH_MESSAGING_TRANSFER_NOTIFIER

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

        class TransferNotifier : public IPushMessagingTransferNotifier
        {
        public:
          ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSessionTransferNotifier, IServicePushMailboxSessionTransferNotifier)

        protected:
          TransferNotifier(IServicePushMailboxSessionTransferNotifierPtr notifier);

          void init();

        public:
          ~TransferNotifier();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushMessaging::TransferNotifier => friend PushMessaging
          #pragma mark

          static TransferNotifierPtr create(IServicePushMailboxSessionTransferNotifierPtr notifier);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushMessaging::TransferNotifier => IPushMessagingTransferNotifier
          #pragma mark

          virtual void notifyComplete(bool wasSuccessful);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushMessaging::TransferNotifier => (internal)
          #pragma mark

          Log::Params log(const char *message) const;

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushMessaging::TransferNotifier => (data)
          #pragma mark

          AutoPUID mID;
          TransferNotifierWeakPtr mThisWeak;

          IServicePushMailboxSessionTransferNotifierPtr mNotifier;
        };

#if 0
      };

    }
  }
}
#endif //0

#else
#include <openpeer/core/internal/core_PushMessaging.h>
#endif //OPENPEER_CORE_PUSH_MESSAGING_TRANSFER_NOTIFIER
