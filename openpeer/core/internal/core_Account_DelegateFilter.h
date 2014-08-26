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

#ifndef OPENPEER_CORE_ACCOUNT_INCLUDE_DELEGATE_FILTER
#include <openpeer/core/internal/core_Account.h>
#else

#if 0

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      class Account : ...
      {

#endif //0

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account::DelegateFilter
        #pragma mark

        class DelegateFilter : public MessageQueueAssociator,
                               public SharedRecursiveLock,
                               public IConversationThreadDelegate,
                               public ICallDelegate
        {
        public:
          friend class Account;

          typedef std::pair<IContactPtr, ContactConnectionStates> ContactConnectionStateChangedEvent;

          typedef std::list<ContactConnectionStateChangedEvent> ContactConnectionStateChangedList;
          typedef std::list<IContactPtr> ContactStatusChangedList;

          struct FilteredEvents
          {
            IConversationThreadPtr mConversationThread;
            bool mFiredContactsChanged {};
            ContactConnectionStateChangedList mFiredContactConnectionStateChanges;
            ContactStatusChangedList mFiredContactStatusChanged;
          };

          typedef std::map<PUID, FilteredEvents> FilteredEventMap;

        protected:
          DelegateFilter(
                         AccountPtr outer,
                         IConversationThreadDelegatePtr conversationThreadDelegate,
                         ICallDelegatePtr callDelegate
                         );

          void init();

        public:
          ~DelegateFilter();

          static ElementPtr toDebug(DelegateFilterPtr subscription);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::DelegateFilter => friend Account
          #pragma mark

          static DelegateFilterPtr create(
                                          AccountPtr outer,
                                          IConversationThreadDelegatePtr conversationThreadDelegate,
                                          ICallDelegatePtr callDelegate
                                          );

        public:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::DelegateFilter => IConversationThreadDelegate
          #pragma mark

          virtual void onConversationThreadNew(IConversationThreadPtr conversationThread);

          virtual void onConversationThreadContactsChanged(IConversationThreadPtr conversationThread);
          virtual void onConversationThreadContactConnectionStateChanged(
                                                                         IConversationThreadPtr conversationThread,
                                                                         IContactPtr contact,
                                                                         ContactConnectionStates state
                                                                         );

          virtual void onConversationThreadContactStatusChanged(
                                                                IConversationThreadPtr conversationThread,
                                                                IContactPtr contact
                                                                );

          virtual void onConversationThreadMessage(
                                                   IConversationThreadPtr conversationThread,
                                                   const char *messageID
                                                   );

          virtual void onConversationThreadMessageDeliveryStateChanged(
                                                                       IConversationThreadPtr conversationThread,
                                                                       const char *messageID,
                                                                       MessageDeliveryStates state
                                                                       );

          virtual void onConversationThreadPushMessage(
                                                       IConversationThreadPtr conversationThread,
                                                       const char *messageID,
                                                       IContactPtr contact
                                                       );

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::DelegateFilter => ICallDelegate
          #pragma mark

          virtual void onCallStateChanged(
                                          ICallPtr call,
                                          CallStates state
                                          );

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::DelegateFilter => (internal)
          #pragma mark

        private:
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          void cancel();

          FilteredEvents *find(PUID conversationThreadID);
          void fireNow(PUID conversationThreadID);

        private:
          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::DelegateFilter => (data)
          #pragma mark

          AutoPUID mID;
          DelegateFilterWeakPtr mThisWeak;
          DelegateFilterPtr mGracefulShutdownReference;

          AccountWeakPtr mOuter;

          IConversationThreadDelegatePtr mConversationThreadDelegate;
          ICallDelegatePtr mCallDelegate;

          FilteredEventMap mFilteredEvents;
        };

#if 0
      };

    }
  }
}
#endif //0

#endif //OPENPEER_CORE_ACCOUNT_INCLUDE_DELEGATE_FILTER
