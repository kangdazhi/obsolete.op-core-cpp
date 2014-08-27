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

#ifdef OPENPEER_CORE_PUSH_PRESENCE_REGISTER_QUERY

#if 0
namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      class PushPresence //...
      {
#endif //0

      public:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushPresence::PushQuery
        #pragma mark

        class RegisterQuery : public MessageQueueAssociator,
                              public SharedRecursiveLock,
                              public IPushPresenceRegisterQuery,
                              public stack::IServicePushMailboxRegisterQueryDelegate
        {
        public:
          typedef IPushPresence::ValueNameList ValueNameList;
          ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxRegisterQuery, IServicePushMailboxRegisterQuery)

        protected:
          RegisterQuery(
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
                        );

          void init();

        public:
          ~RegisterQuery();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushPresence::RegisterQuery => friend PushPresence
          #pragma mark

          static RegisterQueryPtr create(
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
                                         );

          void attachMailbox(IServicePushMailboxSessionPtr mailbox);

          void cancel();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushPresence::RegisterQuery => IPushPresenceQuery
          #pragma mark

          virtual PUID getID() const {return mID;}

          virtual bool isComplete(
                                  WORD *outErrorCode = NULL,
                                  String *outErrorReason = NULL
                                  ) const;

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushPresence::RegisterQuery => IServicePushMailboxRegisterQueryDelegate
          #pragma mark

          virtual void onPushMailboxRegisterQueryCompleted(IServicePushMailboxRegisterQueryPtr query);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushPresence::PushQuery => (internal)
          #pragma mark

          Log::Params log(const char *message) const;

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PushPresence::PushQuery => (data)
          #pragma mark

          AutoPUID mID;
          RegisterQueryWeakPtr mThisWeak;

          IPushPresenceRegisterQueryDelegatePtr mDelegate;

          bool mHadQuery {};
          IServicePushMailboxRegisterQueryPtr mQuery;

          String mDeviceToken;
          Time mExpires;
          String mMappedType;
          bool mUnreadBadge;
          String mSound;
          String mAction;
          String mLaunchImage;
          unsigned int mPriority;
          ValueNameList mValueNames;

          WORD mLastErrorCode;
          String mLastErrorReason;
        };

#if 0
      };

    }
  }
}
#endif //0

#else
#include <openpeer/core/internal/core_PushPresence.h>
#endif //OPENPEER_CORE_PUSH_PRESENCE_REGISTER_QUERY
