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

#ifndef OPENPEER_CORE_CONVERSATION_THREAD_HOST_INCLUDE_PEER_CONTACT
#include <openpeer/core/internal/core_ConversationThreadHost.h>
#else

#if 0
namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      class ConversationThreadHost
      {
        // ...
#endif //0

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost::PeerContact
        #pragma mark

        class PeerContact : public MessageQueueAssociator,
                            public SharedRecursiveLock,
                            public IPeerSubscriptionDelegate,
                            public IBackgroundingDelegate,
                            public IWakeDelegate,
                            public ITimerDelegate
        {
        public:
          enum PeerContactStates
          {
            PeerContactState_Pending,
            PeerContactState_Ready,
            PeerContactState_ShuttingDown,
            PeerContactState_Shutdown,
          };

          static const char *toString(PeerContactStates state);

          typedef thread::MessageReceiptMap MessageReceiptMap;

          friend class ConversationThreadHost;
          friend class PeerLocation;

          typedef String LocationID;
          typedef std::map<LocationID, PeerLocationPtr> PeerLocationMap;

          ZS_DECLARE_STRUCT_PTR(MessageDeliveryState)

          typedef String MessageID;
          typedef IConversationThread::MessageDeliveryStates MessageDeliveryStates;

          typedef std::map<MessageID, MessageDeliveryStatePtr> MessageDeliveryStatesMap;

        private:
          PeerContact(
                      IMessageQueuePtr queue,
                      ConversationThreadHostPtr host,
                      UseContactPtr contact,
                      ElementPtr profileBundleEl
                      );

          void init();

        public:
          ~PeerContact();

          static ElementPtr toDebug(PeerContactPtr contact);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => friend ConversationThreadHost
          #pragma mark

          static PeerContactPtr create(
                                       IMessageQueuePtr queue,
                                       ConversationThreadHostPtr host,
                                       UseContactPtr contact,
                                       ElementPtr profileBundleEL
                                       );

          void notifyPublicationUpdated(
                                        ILocationPtr peerLocation,
                                        IPublicationMetaDataPtr metaData,
                                        const SplitMap &split
                                        );
          void notifyPublicationGone(
                                     ILocationPtr peerLocation,
                                     IPublicationMetaDataPtr metaData,
                                     const SplitMap &split
                                     );
          void notifyPeerDisconnected(ILocationPtr peerLocation);

          UseContactPtr getContact() const;
          const ElementPtr &getProfileBundle() const;

          ContactStates getContactState() const;

          void gatherMessageReceipts(MessageReceiptMap &receipts) const;

          void gatherContactsToAdd(ThreadContactMap &contacts) const;
          void gatherContactsToRemove(ContactURIList &contacts) const;

          void gatherDialogReplies(
                                   const char *callID,
                                   LocationDialogMap &outDialogs
                                   ) const;

          void notifyStep(bool performStepAsync = true);

          // (duplicate) void cancel();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => IPeerSubscriptionDelegate
          #pragma mark

          virtual void onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription);

          virtual void onPeerSubscriptionFindStateChanged(
                                                          IPeerSubscriptionPtr subscription,
                                                          IPeerPtr peer,
                                                          PeerFindStates state
                                                          );

          virtual void onPeerSubscriptionLocationConnectionStateChanged(
                                                                        IPeerSubscriptionPtr subscription,
                                                                        ILocationPtr location,
                                                                        LocationConnectionStates state
                                                                        );

          virtual void onPeerSubscriptionMessageIncoming(
                                                         IPeerSubscriptionPtr subscription,
                                                         IMessageIncomingPtr message
                                                         );

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => IBackgroundingDelegate
          #pragma mark

          virtual void onBackgroundingGoingToBackground(
                                                        IBackgroundingSubscriptionPtr subscription,
                                                        IBackgroundingNotifierPtr notifier
                                                        );

          virtual void onBackgroundingGoingToBackgroundNow(IBackgroundingSubscriptionPtr subscription);

          virtual void onBackgroundingReturningFromBackground(IBackgroundingSubscriptionPtr subscription);

          virtual void onBackgroundingApplicationWillQuit(IBackgroundingSubscriptionPtr subscription);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => IWakeDelegate
          #pragma mark

          virtual void onWake();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => ITimerDelegate
          #pragma mark

          virtual void onTimer(TimerPtr timer);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => friend ConversationThreadHost::PeerLocation
          #pragma mark

          ConversationThreadHostPtr getOuter() const;
          UseConversationThreadPtr getBaseThread() const;
          ThreadPtr getHostThread() const;
          UseAccountPtr getAccount() const;
          IPublicationRepositoryPtr getRepository() const;

          void notifyMessagesReceived(const MessageList &messages);
          void notifyMessageDeliveryStateChanged(
                                                 const String &messageID,
                                                 IConversationThread::MessageDeliveryStates state
                                                 );

          void notifyStateChanged(PeerLocationPtr peerLocation);

          void notifyPeerLocationShutdown(PeerLocationPtr location);

        public:
          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerLocation::MessageDeliveryState
          #pragma mark

          struct MessageDeliveryState
          {
            MessageDeliveryStates mState;
            Time                  mLastStateChanged;
            Time                  mPushTime;

            TimerPtr              mPushTimer;

            ~MessageDeliveryState();

            static MessageDeliveryStatePtr create(
                                                  PeerContactPtr owner,
                                                  MessageDeliveryStates state
                                                  );

            void setState(MessageDeliveryStates state);

            bool shouldPush(bool backgroundingNow) const;

          protected:
            PeerContactWeakPtr mOuter;

            MessageDeliveryState() {}
            MessageDeliveryState(const MessageDeliveryState &) {}
          };
          
        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => (internal)
          #pragma mark

          PUID getID() const {return mID;}

          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          bool isPending() const {return PeerContactState_Pending == mCurrentState;}
          bool isReady() const {return PeerContactState_Ready == mCurrentState;}
          bool isShuttingDown() const {return PeerContactState_ShuttingDown == mCurrentState;}
          bool isShutdown() const {return PeerContactState_Shutdown == mCurrentState;}

          void cancel();
          void step();
          void setState(PeerContactStates state);

          PeerLocationPtr findPeerLocation(ILocationPtr peerLocation) const;

          bool isStillPartOfCurrentConversation(UseContactPtr contact) const;

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => (data)
          #pragma mark

          AutoPUID mID;
          PeerContactWeakPtr mThisWeak;
          PeerContactPtr mGracefulShutdownReference;
          ConversationThreadHostWeakPtr mOuter;

          PeerContactStates mCurrentState;

          UseContactPtr mContact;
          ElementPtr mProfileBundleEl;

          IBackgroundingSubscriptionPtr mBackgroundingSubscription;
          IBackgroundingNotifierPtr mBackgroundingNotifier;
          AutoBool mBackgroundingNow;

          IPeerSubscriptionPtr mSlaveSubscription;

          PeerLocationMap mPeerLocations;

          MessageDeliveryStatesMap mMessageDeliveryStates;
        };
#if 0

      }
    }
  }
}

#endif //0

#endif //OPENPEER_CORE_CONVERSATION_THREAD_HOST_INCLUDE_PEER_CONTACT
