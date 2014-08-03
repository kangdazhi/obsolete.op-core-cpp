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

#include <openpeer/services/IBackgrounding.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>
#include <zsLib/String.h>
#include <zsLib/Timer.h>

#define OPENPEER_CORE_SETTING_CONVERSATION_THREAD_HOST_PEER_CONTACT "openpeer/core/auto-find-peers-added-to-conversation-in-seconds"

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
                                               const char *serverName,
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
                                      public SharedRecursiveLock,
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

        typedef core::internal::thread::ThreadPtr ThreadPtr;

        ZS_DECLARE_CLASS_PTR(PeerContact)
        ZS_DECLARE_CLASS_PTR(PeerLocation)

        typedef String MessageID;
        typedef std::map<MessageID, IConversationThread::MessageDeliveryStates> MessageDeliveryStatesMap;

        typedef String PeerURI;
        typedef std::map<PeerURI, PeerContactPtr> PeerContactMap;

      protected:
        ConversationThreadHost(
                               IMessageQueuePtr queue,
                               AccountPtr account,
                               ConversationThreadPtr baseThread,
                               const char *threadID,
                               const char *serverName
                               );

        ConversationThreadHost(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

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
                                                const char *serverName,
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
        virtual String getHostServerName() const;

        virtual bool safeToChangeContacts() const;

        virtual void getContacts(ThreadContactMap &outContacts) const;
        virtual bool inConversation(UseContactPtr contact) const;
        virtual void addContacts(const ContactProfileInfoList &contacts);
        virtual void removeContacts(const ContactList &contacts);

        virtual ContactConnectionStates getContactConnectionState(UseContactPtr contact) const;

        virtual bool placeCalls(const PendingCallMap &pendingCalls);
        virtual void notifyCallStateChanged(UseCallPtr call);
        virtual void notifyCallCleanup(UseCallPtr call);

        virtual void gatherDialogReplies(
                                         const char *callID,
                                         LocationDialogMap &outDialogs
                                         ) const;

        virtual void markAllMessagesRead();

        virtual void setStatusInThread(
                                       UseContactPtr selfContact,
                                       const IdentityContactList &selfIdentityContacts,
                                       const String &contactStatusInThreadOfSelfHash,
                                       ElementPtr contactStatusInThreadOfSelf
                                       );

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

        ThreadPtr getHostThread() const;
        UseAccountPtr getAccount() const;
        IPublicationRepositoryPtr getRepository() const;
        UseConversationThreadPtr getBaseThread() const;

        void notifyMessagesReceived(const MessageList &messages);
        void notifyMessageDeliveryStateChanged(
                                               const String &messageID,
                                               IConversationThread::MessageDeliveryStates state
                                               );
        void notifyContactStatus(
                                 UseContactPtr contact,
                                 const String &statusHash,
                                 ElementPtr status
                                 );
        virtual void notifyMessagePush(
                                       MessagePtr message,
                                       UseContactPtr toContact
                                       );

        void notifyStateChanged(PeerContactPtr peerContact);
        void notifyContactState(
                                UseContactPtr contact,
                                ContactConnectionStates state
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
        virtual bool isShutdown() const {AutoRecursiveLock(*this); return ConversationThreadHostState_Shutdown == mCurrentState;}

        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug() const;

      private:
        void cancel();
        void step();

        void setState(ConversationThreadHostStates state);

        IPublicationRepositoryPtr getPublicationRepostiory();

        void removeContacts(const ContactURIList &contacts);

        PeerContactPtr findContact(ILocationPtr peerLocation) const;

      public:

#define OPENPEER_CORE_CONVERSATION_THREAD_HOST_INCLUDE_PEER_CONTACT
#include <openpeer/core/internal/core_ConversationThreadHost_PeerContact.h>
#undef OPENPEER_CORE_CONVERSATION_THREAD_HOST_INCLUDE_PEER_CONTACT

#define OPENPEER_CORE_CONVERSATION_THREAD_HOST_INCLUDE_PEER_LOCATION
#include <openpeer/core/internal/core_ConversationThreadHost_PeerLocation.h>
#undef OPENPEER_CORE_CONVERSATION_THREAD_HOST_INCLUDE_PEER_LOCATION

      protected:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost => (data)
        #pragma mark

        AutoPUID mID;
        ConversationThreadHostWeakPtr mThisWeak;
        ConversationThreadHostPtr mGracefulShutdownReference;

        UseConversationThreadWeakPtr mBaseThread;
        UseAccountWeakPtr mAccount;

        UseContactPtr mSelfContact;

        String mThreadID;
        String mServerName;

        ConversationThreadHostStates mCurrentState;

        ThreadPtr mHostThread;

        MessageDeliveryStatesMap mMessageDeliveryStates;

        AutoBool mMarkAllRead;
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
                                                                       const char *serverName,
                                                                       thread::Details::ConversationThreadStates state = thread::Details::ConversationThreadState_Open
                                                                       );
      };
    }
  }
}
