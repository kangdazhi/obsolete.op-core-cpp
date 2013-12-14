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
      interaction IConversationThreadForHost;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadHostForConversationThread
      #pragma mark

      interaction IConversationThreadHostForConversationThread : public IConversationThreadHostSlaveBase
      {
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadHostForConversationThread, ForConversationThread)

        static ElementPtr toDebug(ForConversationThreadPtr host);

        static ForConversationThreadPtr create(
                                               ConversationThreadPtr baseThread,
                                               thread::Details::ConversationThreadStates state = thread::Details::ConversationThreadState_Open
                                               );

        virtual void close() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost
      #pragma mark

      class ConversationThreadHost  : public Noop,
                                      public MessageQueueAssociator,
                                      public IConversationThreadHostForConversationThread,
                                      public IWakeDelegate
      {
      public:
        friend interaction IConversationThreadHostFactory;
        friend interaction IConversationThreadHostForConversationThread;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForConversationThread, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(ICallForConversationThread, UseCall)
        ZS_DECLARE_TYPEDEF_PTR(IContactForConversationThread, UseContact)
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForHost, UseConversationThread)

        enum ConversationThreadHostStates
        {
          ConversationThreadHostState_Pending,
          ConversationThreadHostState_Ready,
          ConversationThreadHostState_ShuttingDown,
          ConversationThreadHostState_Shutdown,
        };

        static const char *toString(ConversationThreadHostStates state);

        typedef thread::ThreadPtr ThreadPtr;

        class PeerContact;
        typedef boost::shared_ptr<PeerContact> PeerContactPtr;
        typedef boost::weak_ptr<PeerContact> PeerContactWeakPtr;

        class PeerLocation;
        typedef boost::shared_ptr<PeerLocation> PeerLocationPtr;
        typedef boost::weak_ptr<PeerLocation> PeerLocationWeakPtr;

        typedef String MessageID;
        typedef std::map<MessageID, IConversationThread::MessageDeliveryStates> MessageDeliveryStatesMap;

        typedef String PeerURI;
        typedef std::map<PeerURI, PeerContactPtr> PeerContactMap;

      protected:
        ConversationThreadHost(
                               IMessageQueuePtr queue,
                               UseAccountPtr account,
                               UseConversationThreadPtr baseThread,
                               const char *threadID
                               );
        
        ConversationThreadHost(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init(thread::Details::ConversationThreadStates state);

      public:
        ~ConversationThreadHost();

        static ConversationThreadHostPtr convert(ForConversationThreadPtr object);

      protected:
        static ElementPtr toDebug(ConversationThreadHostPtr host);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost => IConversationThreadHostSlaveBase
        #pragma mark

        static ConversationThreadHostPtr create(
                                                ConversationThreadPtr baseThread,
                                                thread::Details::ConversationThreadStates state = thread::Details::ConversationThreadState_Open
                                                );

        virtual String getThreadID() const;

        virtual bool isHost() const {return true;}
        virtual bool isSlave() const {return false;}

        virtual void shutdown();
        // (duplicate) virtual bool isShutdown() const;

        virtual ConversationThreadHostPtr toHost() const {return mThisWeak.lock();}
        virtual ConversationThreadSlavePtr toSlave() const {return ConversationThreadSlavePtr();}

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
        #pragma mark ConversationThreadHost => IConversationThreadHostForConversationThread
        #pragma mark

        virtual void close();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost => IWakeDelegate
        #pragma mark

        virtual void onWake() {step();}

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost => friend PeerContact
        #pragma mark

        // (duplicate) RecursiveLock &getLock() const;

        ThreadPtr getHostThread() const;
        UseAccountPtr getAccount() const;
        IPublicationRepositoryPtr getRepository() const;
        UseConversationThreadPtr getBaseThread() const;

        void notifyMessagesReceived(const MessageList &messages);
        void notifyMessageDeliveryStateChanged(
                                               const String &messageID,
                                               IConversationThread::MessageDeliveryStates state
                                               );
        virtual void notifyMessagePush(
                                       MessagePtr message,
                                       UseContactPtr toContact
                                       );

        void notifyStateChanged(PeerContactPtr peerContact);
        void notifyContactState(
                                UseContactPtr contact,
                                ContactStates state
                                );

        bool hasCallPlacedTo(UseContactPtr toContact);

      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost => (internal)
        #pragma mark

        bool isPending() const {return ConversationThreadHostState_Pending == mCurrentState;}
        bool isReady() const {return ConversationThreadHostState_Ready == mCurrentState;}
        bool isShuttingDown() const {return ConversationThreadHostState_ShuttingDown == mCurrentState;}
        virtual bool isShutdown() const {AutoRecursiveLock(getLock()); return ConversationThreadHostState_Shutdown == mCurrentState;}

        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug() const;

      protected:
        RecursiveLock &getLock() const;

      private:
        void cancel();
        void step();

        void setState(ConversationThreadHostStates state);

        void publish(
                     bool publishHostPublication,
                     bool publishHostPermissionPublication
                     ) const;

        void removeContacts(const ContactURIList &contacts);

        PeerContactPtr findContact(ILocationPtr peerLocation) const;

      public:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost::PeerContact
        #pragma mark

        class PeerContact : public MessageQueueAssociator,
                            public IPeerSubscriptionDelegate,
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

          typedef String MessageID;
          typedef Time StateChangedTime;
          typedef std::pair<StateChangedTime, IConversationThread::MessageDeliveryStates> DeliveryStatePair;
          typedef std::map<MessageID, DeliveryStatePair> MessageDeliveryStatesMap;

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

        public:
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
          #pragma mark ConversationThreadHost::PeerContact => IWakeDelegate
          #pragma mark

          virtual void onWake() {step();}

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => ITimerDelegate
          #pragma mark

          virtual void onTimer(TimerPtr timer);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => friend ConversationThreadHost::PeerLocation
          #pragma mark

          // (duplicate) RecursiveLock &getLock() const;

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

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerContact => (internal)
          #pragma mark

          RecursiveLock &getLock() const;
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

          PUID mID;
          mutable RecursiveLock mBogusLock;
          PeerContactWeakPtr mThisWeak;
          PeerContactPtr mGracefulShutdownReference;
          ConversationThreadHostWeakPtr mOuter;

          PeerContactStates mCurrentState;

          UseContactPtr mContact;
          ElementPtr mProfileBundleEl;

          IPeerSubscriptionPtr mSlaveSubscription;
          TimerPtr mSlaveMessageDeliveryTimer;

          PeerLocationMap mPeerLocations;

          MessageDeliveryStatesMap mMessageDeliveryStates;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost::PeerLocation
        #pragma mark

        class PeerLocation : public MessageQueueAssociator,
                             public IConversationThreadDocumentFetcherDelegate
        {
        public:
          typedef thread::MessageReceiptMap MessageReceiptMap;

          typedef String MessageID;
          typedef std::map<MessageID, IConversationThread::MessageDeliveryStates> MessageDeliveryStatesMap;

          typedef String CallID;
          typedef std::map<CallID, UseCallPtr> CallHandlers;

          friend class PeerContact;

        protected:
          PeerLocation(
                       IMessageQueuePtr queue,
                       PeerContactPtr peerContact,
                       ILocationPtr peerLocation
                       );

          void init();

        public:
          ~PeerLocation();

          static ElementPtr toDebug(PeerLocationPtr contact);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerLocation => friend ConversationThreadHost::PeerContact
          #pragma mark

          static PeerLocationPtr create(
                                        IMessageQueuePtr queue,
                                        PeerContactPtr peerContact,
                                        ILocationPtr peerLocation
                                        );

          PUID getID() const {return mID;}
          String getLocationID() const;

          bool isConnected() const;

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

          void gatherMessageReceipts(MessageReceiptMap &receipts) const;

          void gatherContactsToAdd(ThreadContactMap &contacts) const;
          void gatherContactsToRemove(ContactURIList &contacts) const;

          void gatherDialogReplies(
                                   const char *callID,
                                   LocationDialogMap &outDialogs
                                   ) const;

          // (duplicate) void cancel();
          // (duplicate) void step();

        public:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerLocation => IConversationThreadDocumentFetcherDelegate
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

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerLocation => (internal)
          #pragma mark

        private:
          Log::Params log(const char *message) const;

          bool isShutdown() {return mShutdown;}

        protected:
          RecursiveLock &getLock() const;

          virtual ElementPtr toDebug() const;

          void cancel();
          void step();

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerLocation => (data)
          #pragma mark

          PUID mID;
          mutable RecursiveLock mBogusLock;
          PeerLocationWeakPtr mThisWeak;
          PeerContactWeakPtr mOuter;
          bool mShutdown;

          ILocationPtr mPeerLocation;

          ThreadPtr mSlaveThread;

          IConversationThreadDocumentFetcherPtr mFetcher;

          MessageDeliveryStatesMap mMessageDeliveryStates;

          CallHandlers mIncomingCallHandlers;
        };

      protected:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost => (data)
        #pragma mark

        mutable RecursiveLock mBogusLock;
        PUID mID;
        ConversationThreadHostWeakPtr mThisWeak;
        ConversationThreadHostPtr mGracefulShutdownReference;

        UseConversationThreadWeakPtr mBaseThread;
        UseAccountWeakPtr mAccount;

        UseContactPtr mSelfContact;

        String mThreadID;

        ConversationThreadHostStates mCurrentState;

        ThreadPtr mHostThread;

        MessageDeliveryStatesMap mMessageDeliveryStates;

        PeerContactMap mPeerContacts;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadHostFactory
      #pragma mark

      interaction IConversationThreadHostFactory
      {
        static IConversationThreadHostFactory &singleton();

        virtual ConversationThreadHostPtr createConversationThreadHost(
                                                                       ConversationThreadPtr baseThread,
                                                                       thread::Details::ConversationThreadStates state = thread::Details::ConversationThreadState_Open
                                                                       );
      };
    }
  }
}
