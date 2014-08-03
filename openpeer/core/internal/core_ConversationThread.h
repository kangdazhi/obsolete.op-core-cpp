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
#include <openpeer/core/IConversationThread.h>
#include <openpeer/core/internal/core_thread.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IWakeDelegate.h>

#define OPENPEER_CONVERSATION_THREAD_TYPE_INDEX (2)
#define OPENPEER_CONVERSATION_THREAD_BASE_THREAD_ID_INDEX (3)
#define OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX (4)

#define OPENPEER_CONVERSATION_THREAD_MAX_WAIT_DELIVERY_TIME_BEFORE_PUSH_IN_SECONDS (30)

#define OPENPEER_CORE_SETTINGS_CONVERSATION_THREAD_HOST_BACKGROUNDING_PHASE "openpeer/core/backgrounding-phase-conversation-thread"

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      interaction IAccountForConversationThread;
      interaction ICallForConversationThread;
      interaction IContactForConversationThread;
      interaction IConversationThreadHostForConversationThread;
      interaction IConversationThreadSlaveForConversationThread;

      typedef services::IHelper::SplitMap SplitMap;
      using thread::DialogPtr;
      using thread::ThreadContactMap;
      using thread::MessageList;
      using thread::MessagePtr;
      using thread::ContactURIList;

      // host publishes these documents:

      // /threads/1.0/host/base-thread-id/host-thread-id/state          - current state of the thread (includes list of all participants)
      // /threads/1.0/host/base-thread-id/host-thread-id/permissions    - all participant peer URIs that are part of the conversation thread

      // /threads/1.0/subscribers/permissions                           - peer URI of the self added to this document (only allow subscriber to fetch documents published by ourself)


      // slaves publishes these documents to their own machine:

      // /threads/1.0/slave/base-thread-id/host-thread-id/state
      // /threads/1.0/slave/base-thread-id/host-thread-id/permissions   - all who can receive this document (i.e. at minimal the current host)

      // /threads/1.0/subscribers/permissions                           - peer URI of the self added to this document (only allow subscriber to fetch documents published by ourself)

      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadForAccount
      #pragma mark

      interaction IConversationThreadForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForAccount, ForAccount)

        static ForAccountPtr create(
                                    AccountPtr account,
                                    ILocationPtr peerLocation,
                                    IPublicationMetaDataPtr metaData,
                                    const SplitMap &split
                                    );

        virtual String getThreadID() const = 0;

        virtual void notifyPublicationUpdated(
                                              ILocationPtr peerLocation,
                                              IPublicationMetaDataPtr metaData,
                                              const SplitMap &split
                                              ) = 0;

        virtual void notifyPublicationGone(
                                           ILocationPtr peerLocation,
                                           IPublicationMetaDataPtr metaData,
                                           const SplitMap &split
                                           ) = 0;

        virtual void notifyPeerDisconnected(ILocationPtr peerLocation) = 0;

        virtual void shutdown() = 0;
        virtual bool isShutdown() const = 0;
      };

      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadForCall
      #pragma mark

      interaction IConversationThreadForCall
      {
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForCall, ForCall)

        typedef String LocationID;
        typedef std::map<LocationID, DialogPtr> LocationDialogMap;

        virtual AccountPtr getAccount() const = 0;

        virtual bool placeCall(CallPtr call) = 0;
        virtual void notifyCallStateChanged(CallPtr call) = 0;
        virtual void notifyCallCleanup(CallPtr call) = 0;

        virtual void gatherDialogReplies(
                                         const char *callID,
                                         LocationDialogMap &outDialogs
                                         ) const = 0;
      };

      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadHostSlaveBase
      #pragma mark

      interaction IConversationThreadHostSlaveBase
      {
        ZS_DECLARE_TYPEDEF_PTR(ICallForConversationThread, UseCall)
        ZS_DECLARE_TYPEDEF_PTR(IContactForConversationThread, UseContact)

        typedef String CallID;
        typedef std::map<CallID, UseCallPtr> PendingCallMap;

        typedef IConversationThreadForCall::LocationDialogMap LocationDialogMap;
        typedef IConversationThread::ContactConnectionStates ContactConnectionStates;

        static ElementPtr toDebug(IConversationThreadHostSlaveBasePtr hostOrSlave);

        virtual String getThreadID() const = 0;

        virtual bool isHost() const = 0;
        virtual bool isSlave() const = 0;

        virtual ConversationThreadHostPtr toHost() const = 0;
        virtual ConversationThreadSlavePtr toSlave() const = 0;

        virtual void shutdown() = 0;
        virtual bool isShutdown() const = 0;

        virtual bool isHostThreadOpen() const = 0;

        virtual void notifyPublicationUpdated(
                                              ILocationPtr peerLocation,
                                              IPublicationMetaDataPtr metaData,
                                              const SplitMap &split
                                              ) = 0;

        virtual void notifyPublicationGone(
                                           ILocationPtr peerLocation,
                                           IPublicationMetaDataPtr metaData,
                                           const SplitMap &split
                                           ) = 0;

        virtual void notifyPeerDisconnected(ILocationPtr peerLocation) = 0;

        virtual Time getHostCreationTime() const = 0;
        virtual String getHostServerName() const = 0;

        virtual bool safeToChangeContacts() const = 0;

        virtual void getContacts(ThreadContactMap &outContacts) const = 0;
        virtual bool inConversation(UseContactPtr contact) const = 0;
        virtual void addContacts(const ContactProfileInfoList &contacts) = 0;
        virtual void removeContacts(const ContactList &contacts) = 0;

        virtual ContactConnectionStates getContactConnectionState(UseContactPtr contact) const = 0;

        virtual bool sendMessages(const MessageList &messages) = 0;

        virtual bool placeCalls(const PendingCallMap &pendingCalls) = 0;
        virtual void notifyCallStateChanged(UseCallPtr call) = 0;
        virtual void notifyCallCleanup(UseCallPtr call) = 0;

        virtual void gatherDialogReplies(
                                         const char *callID,
                                         LocationDialogMap &outDialogs
                                         ) const = 0;

        virtual void markAllMessagesRead() = 0;

        virtual void setStatusInThread(
                                       UseContactPtr selfContact,
                                       const IdentityContactList &selfIdentityContacts,
                                       const Time &contactStatusTime,
                                       const String &contactStatusInThreadOfSelfHash,
                                       ElementPtr contactStatusInThreadOfSelf
                                       ) = 0;
      };

      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadForHostOrSlave
      #pragma mark

      interaction IConversationThreadForHostOrSlave
      {
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForHostOrSlave, ForHostOrSlave)

        ZS_DECLARE_TYPEDEF_PTR(ICallForConversationThread, UseCall)
        ZS_DECLARE_TYPEDEF_PTR(IContactForConversationThread, UseContact)

        typedef IConversationThread::ContactConnectionStates ContactConnectionStates;

        virtual AccountPtr getAccount() const = 0;
        virtual stack::IPublicationRepositoryPtr getRepository() const = 0;

        virtual String getThreadID() const = 0;

        virtual void notifyStateChanged(IConversationThreadHostSlaveBasePtr thread) = 0;

        virtual void notifyContactConnectionState(
                                                  IConversationThreadHostSlaveBasePtr thread,
                                                  UseContactPtr contact,
                                                  ContactConnectionStates state
                                                  ) = 0;

        virtual void notifyContactStatus(
                                         IConversationThreadHostSlaveBasePtr thread,
                                         UseContactPtr contact,
                                         const Time &statusTime,
                                         const String &statusHash,
                                         ElementPtr status
                                         ) = 0;

        virtual bool getLastContactStatus(
                                          UseContactPtr contact,
                                          Time &outStatusTime,
                                          String &outStatusHash,
                                          ElementPtr &outStatus
                                          ) = 0;

        virtual void notifyMessageReceived(MessagePtr message) = 0;
        virtual void notifyMessageDeliveryStateChanged(
                                                       const char *messageID,
                                                       IConversationThread::MessageDeliveryStates state
                                                       ) = 0;
        virtual void notifyMessagePush(
                                       MessagePtr message,
                                       UseContactPtr toContact
                                       ) = 0;

        virtual void requestAddIncomingCallHandler(
                                                   const char *dialogID,
                                                   IConversationThreadHostSlaveBasePtr hostOrSlaveThread,
                                                   UseCallPtr newCall
                                                   ) = 0;
        virtual void requestRemoveIncomingCallHandler(const char *dialogID) = 0;

        virtual void notifyPossibleCallReplyStateChange(const char *dialogID) = 0;
      };

      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadForHost
      #pragma mark

      interaction IConversationThreadForHost : public IConversationThreadForHostOrSlave
      {
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForHost, ForHost)

        ZS_DECLARE_TYPEDEF_PTR(IContactForConversationThread, UseContact)

        virtual bool inConversation(UseContactPtr contact) const = 0;

        virtual void addContacts(const ContactProfileInfoList &contacts) = 0;
        virtual void removeContacts(const ContactURIList &contacts) = 0;
      };

      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadForSlave
      #pragma mark

      interaction IConversationThreadForSlave : public IConversationThreadForHostOrSlave
      {
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForSlave, ForSlave)

        typedef thread::ThreadPtr ThreadPtr;

        virtual void notifyAboutNewThreadNowIfNotNotified(ConversationThreadSlavePtr slave) = 0;

        virtual void convertSlaveToClosedHost(
                                              ConversationThreadSlavePtr slave,
                                              ThreadPtr originalHost,
                                              ThreadPtr originalSlave
                                              ) = 0;
      };

      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThread
      #pragma mark

      class ConversationThread  : public Noop,
                                  public MessageQueueAssociator,
                                  public SharedRecursiveLock,
                                  public IConversationThread,
                                  public IConversationThreadForAccount,
                                  public IConversationThreadForCall,
                                  public IConversationThreadForHost,
                                  public IConversationThreadForSlave,
                                  public IWakeDelegate
      {
      public:
        friend interaction IConversationThreadFactory;
        friend interaction IConversationThread;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForConversationThread, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(ICallForConversationThread, UseCall)
        ZS_DECLARE_TYPEDEF_PTR(IContactForConversationThread, UseContact)
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadHostForConversationThread, UseConversationThreadHost)
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadSlaveForConversationThread, UseConversationThreadSlave)

        enum ConversationThreadStates
        {
          ConversationThreadState_Pending,
          ConversationThreadState_Ready,
          ConversationThreadState_ShuttingDown,
          ConversationThreadState_Shutdown,
        };

        static const char *toString(ConversationThreadStates state);

        typedef IConversationThread::ContactConnectionStates ContactConnectionStates;

        typedef String MessageID;
        typedef std::map<MessageID, MessageDeliveryStates> MessageDeliveryStatesMap;
        typedef std::map<MessageID, MessagePtr> MessageReceivedMap;
        typedef std::list<MessagePtr> MessageList;

        typedef String HostThreadID;
        typedef std::map<HostThreadID, IConversationThreadHostSlaveBasePtr> ThreadMap;

        typedef String CallID;
        typedef std::map<CallID, UseCallPtr> PendingCallMap;

        typedef std::pair<IConversationThreadHostSlaveBasePtr, UseCallPtr> CallHandlerPair;
        typedef std::map<CallID, CallHandlerPair> CallHandlerMap;

        typedef String ContactID;
        typedef std::pair<UseContactPtr, ContactConnectionStates> ContactConnectionStatePair;
        typedef std::map<ContactID, ContactConnectionStatePair> ContactConnectionStateMap;

        struct ContactStatus
        {
          UseContactPtr mContact;
          Time mStatusTime;
          String mStatusHash;
          ElementPtr mStatus;
        };
        typedef std::map<ContactID, ContactStatus> ContactStatusMap;

      protected:
        ConversationThread(
                           IMessageQueuePtr queue,
                           AccountPtr account,
                           const char *threadID,
                           const char *serverName
                           );
        
        ConversationThread(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~ConversationThread();

        static ConversationThreadPtr convert(IConversationThreadPtr thread);
        static ConversationThreadPtr convert(ForAccountPtr thread);
        static ConversationThreadPtr convert(ForCallPtr thread);
        static ConversationThreadPtr convert(ForHostOrSlavePtr thread);
        static ConversationThreadPtr convert(ForHostPtr thread);
        static ConversationThreadPtr convert(ForSlavePtr thread);

      protected:
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThread => IConversationThread
        #pragma mark

        static ElementPtr toDebug(IConversationThreadPtr thread);

        static ConversationThreadPtr create(
                                            AccountPtr account,
                                            const IdentityContactList &identityContacts
                                            );

        static ConversationThreadListPtr getConversationThreads(IAccountPtr account);
        static ConversationThreadPtr getConversationThreadByID(
                                                               IAccountPtr account,
                                                               const char *threadID
                                                               );

        virtual PUID getID() const {return mID;}

        virtual String getThreadID() const;

        virtual bool amIHost() const;
        virtual IAccountPtr getAssociatedAccount() const;

        virtual ContactListPtr getContacts() const;
        virtual void addContacts(const ContactProfileInfoList &contactProfileInfos);
        virtual void removeContacts(const ContactList &contacts);

        virtual IdentityContactListPtr getIdentityContactList(IContactPtr contact) const;
        virtual ContactConnectionStates getContactConnectionState(IContactPtr contact) const;

        virtual ElementPtr getContactStatus(IContactPtr contact) const;

        virtual void setStatusInThread(ElementPtr contactStatusInThreadOfSelf);

        // sending a message will cause the message to be delivered to all the contacts currently in the conversation
        virtual void sendMessage(
                                 const char *messageID,
                                 const char *replacesMessageID,
                                 const char *messageType,
                                 const char *message,
                                 bool signMessage
                                 );

        // returns false if the message ID is not known
        virtual bool getMessage(
                                const char *messageID,
                                String &outReplacesMessageID,
                                IContactPtr &outFrom,
                                String &outMessageType,
                                String &outMessage,
                                Time &outTime,
                                bool &outValidated
                                ) const;

        // returns false if the message ID is not known
        virtual bool getMessageDeliveryState(
                                             const char *messageID,
                                             MessageDeliveryStates &outDeliveryState
                                             ) const;

        virtual void markAllMessagesRead();

        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThread => IConversationThreadForAccount
        #pragma mark

        static ConversationThreadPtr create(
                                            AccountPtr account,
                                            ILocationPtr peerLocation,
                                            IPublicationMetaDataPtr metaData,
                                            const SplitMap &split
                                            );

        // (duplicate) virtual String getThreadID() const;
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

        virtual void shutdown();
        // (duplicate) virtual bool isShutdown() const;

        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThread => IConversationThreadForHostOrSlave
        #pragma mark

      protected:
        virtual AccountPtr getAccount() const;
        virtual IPublicationRepositoryPtr getRepository() const;

        // (duplicate) virtual String getThreadID() const;

        virtual void notifyStateChanged(IConversationThreadHostSlaveBasePtr thread);

        virtual void notifyContactConnectionState(
                                                  IConversationThreadHostSlaveBasePtr thread,
                                                  UseContactPtr contact,
                                                  ContactConnectionStates state
                                                  );

        virtual void notifyContactStatus(
                                         IConversationThreadHostSlaveBasePtr thread,
                                         UseContactPtr contact,
                                         const Time &statusTime,
                                         const String &statusHash,
                                         ElementPtr status
                                         );

        virtual bool getLastContactStatus(
                                          UseContactPtr contact,
                                          Time &outStatusTime,
                                          String &outStatusHash,
                                          ElementPtr &outStatus
                                          );

        virtual void notifyMessageReceived(MessagePtr message);
        virtual void notifyMessageDeliveryStateChanged(
                                                       const char *messageID,
                                                       IConversationThread::MessageDeliveryStates state
                                                       );
        virtual void notifyMessagePush(
                                       MessagePtr message,
                                       UseContactPtr toContact
                                       );

        virtual void requestAddIncomingCallHandler(
                                                   const char *dialogID,
                                                   IConversationThreadHostSlaveBasePtr hostOrSlaveThread,
                                                   UseCallPtr newCall
                                                   );
        virtual void requestRemoveIncomingCallHandler(const char *dialogID);

        virtual void notifyPossibleCallReplyStateChange(const char *dialogID);

        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThread => IConversationThreadForHost
        #pragma mark

        virtual bool inConversation(UseContactPtr contact) const;
        // (duplicate) virtual void addContacts(const ContactList &contacts);
        virtual void removeContacts(const ContactURIList &contacts);

        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThread => IConversationThreadForSlave
        #pragma mark

        virtual void notifyAboutNewThreadNowIfNotNotified(ConversationThreadSlavePtr slave);

        virtual void convertSlaveToClosedHost(
                                              ConversationThreadSlavePtr slave,
                                              ThreadPtr originalHost,
                                              ThreadPtr originalSlave
                                              );

        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThread => IConversationThreadForCall
        #pragma mark

        // (duplicate) virtual IConversationThreadPtr convertIConversationThread() const;
        // (duplicate) virtual IAccountForConversationThreadPtr getAccount() const;
        virtual bool placeCall(CallPtr call);
        virtual void notifyCallStateChanged(CallPtr call);
        virtual void notifyCallCleanup(CallPtr call);

        virtual void gatherDialogReplies(
                                         const char *callID,
                                         LocationDialogMap &outDialogs
                                         ) const;

        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThread => IWakeDelegate
        #pragma mark

        virtual void onWake() {step();}

      protected:
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThread => (internal)
        #pragma mark

        bool isPending() const {return ConversationThreadState_Pending == mCurrentState;}
        bool isReady() const {return ConversationThreadState_Ready == mCurrentState;}
        bool isShuttingDown() const {return ConversationThreadState_ShuttingDown == mCurrentState;}
        virtual bool isShutdown() const {AutoRecursiveLock lock(*this); return ConversationThreadState_Shutdown == mCurrentState;}

        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();
        void step();

        void setState(ConversationThreadStates state);

        void handleLastOpenThreadChanged();
        void handleContactsChanged();

      protected:
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThread => (data)
        #pragma mark

        AutoPUID mID;
        ConversationThreadWeakPtr mThisWeak;
        ConversationThreadPtr mGracefulShutdownReference;

        IConversationThreadDelegatePtr mDelegate;

        UseAccountWeakPtr mAccount;

        String mThreadID;
        String mServerName;

        ConversationThreadStates mCurrentState;
        bool mMustNotifyAboutNewThread;

        IConversationThreadHostSlaveBasePtr mOpenThread;      // if there is an open thread, this is valid
        IConversationThreadHostSlaveBasePtr mLastOpenThread;  // if there is was an open thread, this is valid

        IConversationThreadHostSlaveBasePtr mHandleThreadChanged;
        DWORD mHandleContactsChangedCRC;

        ThreadMap mThreads;

        MessageReceivedMap mReceivedOrPushedMessages;   // remembered so the "get" of the message can be done later

        MessageDeliveryStatesMap mMessageDeliveryStates;
        MessageList mPendingDeliveryMessages;

        PendingCallMap mPendingCalls;

        CallHandlerMap mCallHandlers;

        IdentityContactList mSelfIdentityContacts;

        // used to remember the last notified state for a contact
        ContactConnectionStateMap mLastReportedContactConnectionStates;
        ContactStatusMap mLastReportedContactStatuses;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadFactory
      #pragma mark

      interaction IConversationThreadFactory
      {
        static IConversationThreadFactory &singleton();

        virtual ConversationThreadPtr createConversationThread(
                                                               AccountPtr account,
                                                               const IdentityContactList &identityContacts
                                                               );
        virtual ConversationThreadPtr createConversationThread(
                                                               AccountPtr account,
                                                               ILocationPtr peerLocation,
                                                               IPublicationMetaDataPtr metaData,
                                                               const SplitMap &split
                                                               );
      };
    }
  }
}
