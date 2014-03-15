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

#include <openpeer/core/internal/types.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_ConversationThreadDocumentFetcher.h>
#include <openpeer/core/internal/core_thread.h>

#include <openpeer/stack/IPeerSubscription.h>

#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>
#include <zsLib/String.h>
#include <zsLib/Timer.h>

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      interaction IAccountForConversationThread;
      interaction ICallForConversationThread;
      interaction IContactForConversationThread;
      interaction IConversationThreadForSlave;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadSlaveForConversationThread
      #pragma mark

      interaction IConversationThreadSlaveForConversationThread : public IConversationThreadHostSlaveBase
      {
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadSlaveForConversationThread, ForConversationThread)

        static ElementPtr toDebug(ForConversationThreadPtr thread);

        static ForConversationThreadPtr create(
                                               ConversationThreadPtr baseThread,
                                               ILocationPtr peerLocation,
                                               IPublicationMetaDataPtr metaData,
                                               const SplitMap &split
                                               );
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave
      #pragma mark

      class ConversationThreadSlave  : public Noop,
                                       public MessageQueueAssociator,
                                       public SharedRecursiveLock,
                                       public IConversationThreadSlaveForConversationThread,
                                       public IConversationThreadDocumentFetcherDelegate,
                                       public IWakeDelegate,
                                       public ITimerDelegate,
                                       public IPeerSubscriptionDelegate
      {
      public:
        friend interaction IConversationThreadSlaveFactory;
        friend interaction IConversationThreadSlaveForConversationThread;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForConversationThread, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(ICallForConversationThread, UseCall)
        ZS_DECLARE_TYPEDEF_PTR(IContactForConversationThread, UseContact)
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForSlave, UseConversationThread)

        ZS_DECLARE_STRUCT_PTR(MessageDeliveryState)

        enum ConversationThreadSlaveStates
        {
          ConversationThreadSlaveState_Pending,
          ConversationThreadSlaveState_Ready,
          ConversationThreadSlaveState_ShuttingDown,
          ConversationThreadSlaveState_Shutdown,
        };

        static const char *toString(ConversationThreadSlaveStates state);

        typedef thread::ThreadPtr ThreadPtr;

        typedef String MessageID;
        typedef IConversationThread::MessageDeliveryStates MessageDeliveryStates;

        typedef std::map<MessageID, MessageDeliveryStatePtr> MessageDeliveryStatesMap;

        typedef String CallID;
        typedef std::map<CallID, UseCallPtr> CallHandlers;

      protected:
        ConversationThreadSlave(
                                IMessageQueuePtr queue,
                                AccountPtr account,
                                ILocationPtr peerLocation,
                                ConversationThreadPtr baseThread,
                                const char *threadID
                                );
        
        ConversationThreadSlave(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~ConversationThreadSlave();

        static ConversationThreadSlavePtr convert(ForConversationThreadPtr object);

      protected:
        static ElementPtr toDebug(ConversationThreadSlavePtr thread);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadSlave => IConversationThreadHostSlaveBase
        #pragma mark

        virtual String getThreadID() const;

        virtual bool isHost() const {return false;}
        virtual bool isSlave() const {return true;}

        virtual void shutdown();
        // (duplicate) virtual bool isShutdown() const;

        virtual ConversationThreadHostPtr toHost() const {return ConversationThreadHostPtr();}
        virtual ConversationThreadSlavePtr toSlave() const {return mThisWeak.lock();}

        virtual bool isHostThreadOpen() const;

        virtual void notifyPublicationUpdated(
                                              ILocationPtr peerLocation,
                                              IPublicationMetaDataPtr metaData,
                                              const SplitMap &split
                                              );

        virtual void notifyPublicationGone(
                                           ILocationPtr peerLocation,
                                           IPublicationMetaDataPtr metaData,
                                           const SplitMap &split
                                           );

        virtual void notifyPeerDisconnected(ILocationPtr peerLocation);

        virtual bool sendMessages(const MessageList &messages);

        virtual Time getHostCreationTime() const;

        virtual bool safeToChangeContacts() const;

        virtual void getContacts(ThreadContactMap &outContacts) const;
        virtual bool inConversation(UseContactPtr contact) const;
        virtual void addContacts(const ContactProfileInfoList &contacts);
        virtual void removeContacts(const ContactList &contacts);

        virtual ContactStates getContactState(UseContactPtr contact) const;

        virtual bool placeCalls(const PendingCallMap &pendingCalls);
        virtual void notifyCallStateChanged(UseCallPtr call);
        virtual void notifyCallCleanup(UseCallPtr call);

        virtual void gatherDialogReplies(
                                         const char *callID,
                                         LocationDialogMap &outDialogs
                                         ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadSlave => IConversationThreadSlaveForConversationThread
        #pragma mark

        static ConversationThreadSlavePtr create(
                                                 ConversationThreadPtr baseThread,
                                                 ILocationPtr peerLocation,
                                                 IPublicationMetaDataPtr metaData,
                                                 const SplitMap &split
                                                 );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadSlave => IConversationThreadDocumentFetcherDelegate
        #pragma mark

        virtual void onConversationThreadDocumentFetcherPublicationUpdated(
                                                                           IConversationThreadDocumentFetcherPtr fetcher,
                                                                           ILocationPtr peerLocation,
                                                                           IPublicationPtr publication
                                                                           );

        virtual void onConversationThreadDocumentFetcherPublicationGone(
                                                                        IConversationThreadDocumentFetcherPtr fetcher,
                                                                        ILocationPtr peerLocation,
                                                                        IPublicationMetaDataPtr metaData
                                                                        );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadSlave => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadSlave => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadSlave => IPeerSubscriptionDelegate
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

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadSlave => (internal)
        #pragma mark

        bool isPending() const {return ConversationThreadSlaveState_Pending == mCurrentState;}
        bool isReady() const {return ConversationThreadSlaveState_Ready == mCurrentState;}
        bool isShuttingDown() const {return ConversationThreadSlaveState_ShuttingDown == mCurrentState;}
        virtual bool isShutdown() const {AutoRecursiveLock lock(*this); return ConversationThreadSlaveState_Shutdown == mCurrentState;}

        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();
        void step();

        void setState(ConversationThreadSlaveStates state);

        UseContactPtr getHostContact() const;
        void publish(
                     bool publishSlavePublication,
                     bool publishSlavePermissionPublication
                     ) const;

      public:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadSlave::MessageDeliveryState
        #pragma mark

        struct MessageDeliveryState
        {
          MessageDeliveryStates mState;
          Time                  mLastStateChanged;
          Time                  mPushTime;

          TimerPtr              mPushTimer;

          ~MessageDeliveryState();

          static MessageDeliveryStatePtr create(
                                                ConversationThreadSlavePtr owner,
                                                MessageDeliveryStates state
                                                );

          void setState(MessageDeliveryStates state);

          bool shouldPush() const;

        protected:
          ConversationThreadSlaveWeakPtr mOuter;

          MessageDeliveryState() {}
          MessageDeliveryState(const MessageDeliveryState &) {}
        };

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadSlave => (data)
        #pragma mark

        AutoPUID mID;
        ConversationThreadSlaveWeakPtr mThisWeak;
        ConversationThreadSlavePtr mGracefulShutdownReference;
        ConversationThreadSlavePtr mSelfHoldingStartupReferenceUntilPublicationFetchCompletes;

        UseConversationThreadWeakPtr mBaseThread;
        UseAccountWeakPtr mAccount;

        String mThreadID;
        ILocationPtr mPeerLocation;

        ConversationThreadSlaveStates mCurrentState;

        IConversationThreadDocumentFetcherPtr mFetcher;

        ThreadPtr mHostThread;
        ThreadPtr mSlaveThread;

        IPeerSubscriptionPtr mHostSubscription;

        bool mConvertedToHostBecauseOriginalHostLikelyGoneForever;

        MessageDeliveryStatesMap mMessageDeliveryStates;

        CallHandlers mIncomingCallHandlers;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadHostFactory
      #pragma mark

      interaction IConversationThreadSlaveFactory
      {
        static IConversationThreadSlaveFactory &singleton();

        virtual ConversationThreadSlavePtr createConversationThreadSlave(
                                                                         ConversationThreadPtr baseThread,
                                                                         ILocationPtr peerLocation,
                                                                         IPublicationMetaDataPtr metaData,
                                                                         const SplitMap &split
                                                                         );
      };

    }
  }
}
