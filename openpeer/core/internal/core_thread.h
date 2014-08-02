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
#include <openpeer/core/ICall.h>

#include <openpeer/services/IICESocket.h>

#include <zsLib/Exception.h>
#include <zsLib/Timer.h>

#define OPENPEER_CONVESATION_THREAD_BASE_THREAD_INDEX (3)

#define OPENPEER_CORE_SETTING_THREAD_MOVE_MESSAGE_TO_CACHE_TIME_IN_SECONDS "openpeer/core/move-message-to-cache-time-in-seconds"

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      interaction IAccountForConversationThread;
      interaction IContactForConversationThread;

      namespace thread
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForConversationThread, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(IContactForConversationThread, UseContact)

        ZS_DECLARE_CLASS_PTR(Thread)
        ZS_DECLARE_CLASS_PTR(Message)
        ZS_DECLARE_CLASS_PTR(MessageReceipts)
        ZS_DECLARE_CLASS_PTR(ThreadContact)
        ZS_DECLARE_CLASS_PTR(ThreadContacts)
        ZS_DECLARE_CLASS_PTR(Dialog)
        ZS_DECLARE_CLASS_PTR(Details)

        typedef zsLib::Exceptions::InvalidArgument InvalidArgument;
        
        typedef String MessageID;
        typedef Time ReceiptTime;
        typedef std::map<MessageID, ReceiptTime> MessageReceiptMap;
        typedef std::list<MessageID> MessageIDList;

        typedef String PeerURI;
        typedef PeerURI ContactURI;

        typedef std::list<ThreadContactPtr> ThreadContactList;
        typedef std::list<ContactURI> ContactURIList;
        typedef std::map<ContactURI, ThreadContactPtr> ThreadContactMap;

        typedef std::map<ContactURI, IPublicationPtr> ContactPublicationMap;

        typedef stack::CandidateList CandidateList;

        typedef String DialogID;
        typedef std::map<DialogID, DialogPtr> DialogMap;
        typedef std::list<DialogPtr> DialogList;
        typedef std::list<DialogID> DialogIDList;

        typedef std::list<MessagePtr> MessageList;
        typedef std::map<MessageID, MessagePtr> MessageMap;

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Message
        #pragma mark

        class Message : public SharedRecursiveLock,
                        public ITimerDelegate
        {
        private:
          struct ManagedMessageData;
          ZS_DECLARE_TYPEDEF_PTR(ManagedMessageData, MessageData)

          enum Flags
          {
            Flag_Cached = 1,
            Flag_Validated = 2,
          };

        protected:
          Message();

        public:
          static MessagePtr create(
                                   const char *messageID,
                                   const char *replacesMessageID,
                                   const char *fromPeerURI,
                                   const char *mimeType,
                                   const char *body,
                                   Time sent,
                                   IPeerFilesPtr signer
                                   );

          static MessagePtr create(
                                   UseAccountPtr account,
                                   ElementPtr messageBundleEl
                                   );

          static ElementPtr toDebug(MessagePtr message);

          ElementPtr messageBundleElement() const;

          String messageID() const;
          String replacesMessageID() const;
          String fromPeerURI() const;
          String mimeType() const;
          String body() const;
          Time sent() const;
          bool validated() const;

          ElementPtr toDebug() const;

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Message => ITimerDelegate
          #pragma mark

          void onTimer(TimerPtr timer);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Message => (internal)
          #pragma mark

          Log::Params log(const char *message) const;

          ElementPtr constructBundleElement(IPeerFilesPtr signer) const;

          String getCookieName() const;

          void moveToCache();
          void restoreFromCache() const;
          void scheduleCaching() const;

          MessageDataPtr parseFromElement(
                                          UseAccountPtr account,
                                          ElementPtr messageBundleEl
                                          ) const;

        private:
          AutoPUID mID;
          MessageWeakPtr mThisWeak;
          int mFlags;

          struct ManagedMessageData
          {
            ElementPtr mBundleEl;

            String mMessageID;
            String mReplacesMessageID;
            String mFromPeerURI;
            String mMimeType;
            String mBody;
            Time mSent;
            bool mValidated;

            TimerPtr mTimer;
            Time mScheduledAt;

            ManagedMessageData();
          };

          mutable MessageDataPtr mData;
        };
        
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageReceipts
        #pragma mark

        class MessageReceipts
        {
        public:
          static ElementPtr toDebug(MessageReceiptsPtr receipts);

          static MessageReceiptsPtr create(
                                           const char *receiptsElementName,
                                           UINT version
                                           );
          static MessageReceiptsPtr create(
                                           const char *receiptsElementName,
                                           UINT version,
                                           const String &messageID
                                           );
          static MessageReceiptsPtr create(
                                           const char *receiptsElementName,
                                           UINT version,
                                           const MessageIDList &messageIDs
                                           );
          static MessageReceiptsPtr create(
                                           const char *receiptsElementName,
                                           UINT version,
                                           const MessageReceiptMap &messageReceipts
                                           );

          static MessageReceiptsPtr create(ElementPtr messageReceiptsEl);

          ElementPtr receiptsElement() const          {return constructReceiptsElement();}

          UINT version() const                        {return mVersion;}
          const MessageReceiptMap &receipts() const   {return mReceipts;}

          ElementPtr toDebug() const;

        protected:
          Log::Params log(const char *message) const;

          ElementPtr constructReceiptsElement() const;

        protected:
          MessageReceiptsWeakPtr mThisWeak;

          AutoPUID mID;

          String mReceiptsElementName;

          UINT mVersion;
          MessageReceiptMap mReceipts;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ThreadContacts
        #pragma mark

        class ThreadContact
        {
        public:
          static ElementPtr toDebug(ThreadContactPtr contact);

          static ThreadContactPtr create(
                                         UseContactPtr contact,
                                         const IdentityContactList &identityContacts
                                         );

          static ThreadContactPtr create(
                                         UseAccountPtr account,
                                         ElementPtr contactEl
                                         );

          UseContactPtr contact() const                       {return mContact;}
          const IdentityContactList &identityContacts() const {return mIdentityContacts;}

          ElementPtr contactElement() const                   {return constructContactElement();}

          ElementPtr toDebug() const;

        protected:
          ElementPtr constructContactElement() const;

        protected:
          AutoPUID mID;
          UseContactPtr mContact;
          IdentityContactList mIdentityContacts;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ThreadContacts
        #pragma mark

        class ThreadContacts
        {
        public:
          static ElementPtr toDebug(ThreadContactsPtr threadContacts);

          static ThreadContactsPtr create(
                                          UINT version,
                                          const ThreadContactList &contacts,
                                          const ThreadContactList &addContacts,
                                          const ContactURIList &removeContacts
                                          );

          static ThreadContactsPtr create(
                                          UseAccountPtr account,
                                          ElementPtr contactsEl
                                          );

          ElementPtr contactsElement() const            {return mContactsEl;}

          UINT version() const                          {return mVersion;}
          const ThreadContactMap &contacts() const      {return mContacts;}
          const ThreadContactMap &addContacts() const   {return mAddContacts;}
          const ContactURIList &removeContacts() const  {return mRemoveContacts;}

          ElementPtr toDebug() const;

        protected:
          Log::Params log(const char *message) const;

        protected:
          ThreadContactsWeakPtr mThisWeak;
          ElementPtr mContactsEl;

          AutoPUID mID;
          UINT mVersion;
          ThreadContactMap mContacts;
          ThreadContactMap mAddContacts;
          ContactURIList mRemoveContacts;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Dialog
        #pragma mark

        class Dialog
        {
        protected:
          Dialog();

        public:
          enum DialogStates
          {
            DialogState_None            = ICall::CallState_None,
            DialogState_Preparing       = ICall::CallState_Preparing,
            DialogState_Placed          = ICall::CallState_Placed,
            DialogState_Incoming        = ICall::CallState_Incoming,
            DialogState_Early           = ICall::CallState_Early,
            DialogState_Ringing         = ICall::CallState_Ringing,
            DialogState_Ringback        = ICall::CallState_Ringback,
            DialogState_Open            = ICall::CallState_Open,
            DialogState_Active          = ICall::CallState_Active,
            DialogState_Inactive        = ICall::CallState_Inactive,
            DialogState_Hold            = ICall::CallState_Hold,
            DialogState_Closing         = ICall::CallState_Closing,
            DialogState_Closed          = ICall::CallState_Closed,

            DialogState_First = DialogState_None,
            DialogState_Last = DialogState_Closed,
          };

          enum DialogClosedReasons
          {
            DialogClosedReason_None                     = ICall::CallClosedReason_None,

            DialogClosedReason_User                     = ICall::CallClosedReason_User,
            DialogClosedReason_RequestTimeout           = ICall::CallClosedReason_RequestTimeout,
            DialogClosedReason_TemporarilyUnavailable   = ICall::CallClosedReason_TemporarilyUnavailable,
            DialogClosedReason_Busy                     = ICall::CallClosedReason_Busy,
            DialogClosedReason_RequestTerminated        = ICall::CallClosedReason_RequestTerminated,
            DialogClosedReason_NotAcceptableHere        = ICall::CallClosedReason_NotAcceptableHere,
            DialogClosedReason_ServerInternalError      = ICall::CallClosedReason_ServerInternalError,
            DialogClosedReason_Decline                  = ICall::CallClosedReason_Decline,
          };

          static const char *toString(DialogStates state);
          static DialogStates toDialogStates(const char *state);

          static const char *toString(DialogClosedReasons closedReason);

          struct Codec;

          ZS_DECLARE_STRUCT_PTR(Description)

          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark Dialog::Codec
          #pragma mark

          struct Codec
          {
            BYTE mCodecID;

            String mName;
            DWORD mPTime;
            DWORD mRate;
            DWORD mChannels;

            Codec() : mCodecID(0), mPTime(0), mRate(0), mChannels(0) {}

            ElementPtr toDebug() const;
          };
          typedef std::list<Codec> CodecList;

          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark Dialog::Description
          #pragma mark

          struct Description
          {
            static DescriptionPtr create();

            UINT mVersion;
            String mDescriptionID;
            String mType;
            DWORD mSSRC;

            String mSecurityCipher;
            String mSecuritySecret;
            String mSecuritySalt;

            CodecList mCodecs;

            String mICEUsernameFrag;
            String mICEPassword;
            CandidateList mCandidates;
            bool mFinal;

            Description() :
              mVersion(0),
              mSSRC(0),
              mFinal(false) {}

            ElementPtr toDebug() const;
          };
          typedef std::list<DescriptionPtr> DescriptionList;

          static ElementPtr toDebug(DialogPtr dialog);

          static DialogPtr create(
                                  UINT version,
                                  const char *dialogID,
                                  DialogStates state,
                                  DialogClosedReasons closedReason,
                                  const char *callerContactURI,
                                  const char *callerLocationID,
                                  const char *calleeContactURI,
                                  const char *calleeLocationID,
                                  const char *replacesDialogID,
                                  const DescriptionList &descriptions,
                                  IPeerFilesPtr signer
                                  );

          static DialogPtr create(ElementPtr dialogBundleEl);

          ElementPtr dialogBundleElement() const      {return mDialogBundleEl;}
          UINT version() const                        {return mVersion;}
          const String &dialogID() const              {return mDialogID;}
          DialogStates dialogState() const            {return mState;}
          DialogClosedReasons closedReason() const    {return mClosedReason;}
          const String &closedReasonMessage() const   {return mClosedReasonMessage;}
          const String &callerPeerURI() const         {return mCallerContactURI;}
          const String &callerLocationID() const      {return mCallerLocationID;}
          const String &calleePeerURI() const         {return mCalleeContactURI;}
          const String &calleeLocationID() const      {return mCalleeLocationID;}
          const String &replacesDialogID() const      {return mReplacesDialogID;}
          const DescriptionList &descriptions() const {return mDescriptions;}

          ElementPtr toDebug() const;

        protected:
          Log::Params log(const char *message) const;

        private:
          DialogWeakPtr mThisWeak;
          ElementPtr mDialogBundleEl;

          AutoPUID mID;
          UINT mVersion;
          String mDialogID;
          DialogStates mState;
          DialogClosedReasons mClosedReason;
          String mClosedReasonMessage;
          String mCallerContactURI;
          String mCallerLocationID;
          String mCalleeContactURI;
          String mCalleeLocationID;
          String mReplacesDialogID;
          DescriptionList mDescriptions;
        };

        typedef Dialog::DialogStates DialogStates;

        typedef Dialog::Description Description;
        typedef Dialog::DescriptionPtr DescriptionPtr;

        typedef String DescriptionID;
        typedef std::pair<DialogID, DescriptionPtr> ChangedDescription;
        typedef std::map<DescriptionID, ChangedDescription> ChangedDescriptionMap;
        typedef std::list<DescriptionPtr> DescriptionList;
        typedef std::list<DescriptionID> DescriptionIDList;

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Details
        #pragma mark

        class Details
        {
        public:
          enum ConversationThreadStates
          {
            ConversationThreadState_None,     // slaves do not have state
            ConversationThreadState_Closed,
            ConversationThreadState_Open,

            ConversationThreadState_First = ConversationThreadState_None,
            ConversationThreadState_Last = ConversationThreadState_Open,
          };

          static const char *toString(ConversationThreadStates state);
          static ConversationThreadStates toConversationThreadState(const char *state);

          static ElementPtr toDebug(DetailsPtr details);

          static DetailsPtr create(
                                   UINT version,
                                   const char *baseThreadID,
                                   const char *hostThreadID,
                                   const char *topic,
                                   const char *replaces,
                                   ConversationThreadStates state
                                   );

          static DetailsPtr create(ElementPtr detailsEl);

          ElementPtr detailsElement() const       {return mDetailsEl;}
          UINT version() const                    {return mVersion;}
          const String &baseThreadID() const      {return mBaseThreadID;}
          const String &hostThreadID() const      {return mHostThreadID;}
          const String &replacesThreadID() const  {return mReplacesThreadID;}
          ConversationThreadStates state() const  {return mState;}
          const String &topic() const             {return mTopic;}
          Time created() const                    {return mCreated;}

          ElementPtr toDebug() const;

        protected:
          Log::Params log(const char *message) const;

        protected:
          AutoPUID mID;
          DetailsWeakPtr mThisWeak;
          ElementPtr mDetailsEl;

          UINT mVersion;
          String mBaseThreadID;
          String mHostThreadID;
          String mReplacesThreadID;
          ConversationThreadStates mState;
          String mTopic;
          Time mCreated;
        };

        typedef Details::ConversationThreadStates ConversationThreadStates;

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Thread
        #pragma mark

        class Thread
        {
        public:
          enum ThreadTypes
          {
            ThreadType_Host,
            ThreadType_Slave,
          };

          static const char *toString(ThreadTypes type);
          static ThreadTypes toThreadTypes(const char *type) throw (InvalidArgument);

        protected:
          Thread();

        public:
          ~Thread();

          static ElementPtr toDebug(ThreadPtr thread);

          static ThreadPtr create(
                                  UseAccountPtr account,
                                  IPublicationPtr publication
                                  );

          bool updateFrom(
                          UseAccountPtr account,
                          IPublicationPtr publication
                          );

          static ThreadPtr create(
                                  UseAccountPtr account,
                                  ThreadTypes threadType,         // needed for document name
                                  ILocationPtr creatorLocation,
                                  const char *baseThreadID,
                                  const char *hostThreadID,
                                  const char *topic,
                                  const char *replaces,
                                  ConversationThreadStates state,
                                  ILocationPtr peerHostLocation = ILocationPtr()
                                  );

          void updateBegin();
          bool updateEnd();

          void setState(Details::ConversationThreadStates state);

          void setContacts(const ThreadContactMap &contacts);
          void setContactsToAdd(const ThreadContactMap &contactsToAdd);
          void setContactsToRemove(const ContactURIList &contactsToRemoved);

          void addMessage(MessagePtr message);
          void addMessages(const MessageList &messages);

          void setDelivered(MessagePtr message);
          void setDelivered(const MessageReceiptMap &messages);

          void setRead(MessagePtr message);
          void setRead(const MessageReceiptMap &messages);

          void addDialogs(const DialogList &dialogs);
          void updateDialogs(const DialogList &dialogs);
          void removeDialogs(const DialogIDList &dialogs);

          IPublicationPtr publication() const                       {return mPublication;}
          IPublicationPtr permissionPublication() const             {return mPermissionPublication;}
          DetailsPtr details() const                                {return mDetails;}
          ThreadContactsPtr contacts() const                        {return mContacts;}
          const MessageList &messages() const                       {return mMessageList;}
          const MessageMap &messagesAsMap() const                   {return mMessageMap;}
          MessageReceiptsPtr messagesDelivered() const              {return mMessagesDelivered;}
          MessageReceiptsPtr messagesRead() const                   {return mMessagesRead;}
          const DialogMap &dialogs() const                          {return mDialogs;}

          // obtain a list of changes since the last updateFrom was called
          bool detailsChanged() const                               {return mDetailsChanged;}
          const ThreadContactMap &contactsChanged() const           {return mContactsChanged;}
          const ContactURIList &contactsRemoved() const             {return mContactsRemoved;}
          const ThreadContactMap &contactsToAddChanged() const      {return mContactsToAddChanged;}
          const ContactURIList &contactsToAddRemoved() const        {return mContactsToAddRemoved;}
          const ContactURIList &contactsToRemoveChanged() const     {return mContactsToRemoveChanged;}
          const ContactURIList &contactsToRemoveRemoved() const     {return mContactsToRemoveRemoved;}
          const MessageList &messagedChanged() const                {return mMessagesChanged;}
          Time messagedChangedTime() const                          {return mMessagesChangedTime;}
          const MessageReceiptMap &messagesDeliveredChanged() const {return mMessagesDeliveredChanged;}
          const MessageReceiptMap &messagesReadChanged() const      {return mMessagesReadChanged;}
          const DialogMap &dialogsChanged() const                   {return mDialogsChanged;}
          const DialogIDList &dialogsRemoved() const                {return mDialogsRemoved;}
          const ChangedDescriptionMap &descriptionsChanged() const  {return mDescriptionsChanged;}
          const DescriptionIDList &descriptionsRemoved() const      {return mDescriptionsRemoved;}

          void getContactPublicationsToPublish(ContactPublicationMap &outContactPublications);

          String getContactDocumentName(UseContactPtr contact) const;

          ElementPtr toDebug() const;

        protected:
          Log::Params log(const char *message) const;

          void resetChanged();
          void publishContact(UseContactPtr contact);

          static void mergedChanged(
                                    MessageReceiptsPtr oldReceipts,
                                    MessageReceiptsPtr newReceipts,
                                    MessageReceiptMap &ioChanged
                                    );

          void createReceiptDiffs(
                                  ElementPtr inReceiptsEl,
                                  const char *inSubElementName,
                                  const MessageReceiptMap &inChanged,
                                  MessageReceiptsPtr &ioReceipts
                                  );

          void setReceipts(
                           MessageReceiptsPtr receipts,
                           MessagePtr message,
                           MessageReceiptMap &ioChanged
                           );
          void setReceipts(
                           MessageReceiptsPtr receipts,
                           const MessageReceiptMap &messages,
                           MessageReceiptMap &ioChanged
                           );

        protected:
          AutoPUID mID;
          ThreadWeakPtr mThisWeak;
          ThreadTypes mType;
          bool mCanModify;
          bool mModifying;

          IPublicationPtr mPublication;
          IPublicationPtr mPermissionPublication;
          ContactPublicationMap mContactPublications;

          DetailsPtr mDetails;
          ThreadContactsPtr mContacts;
          UINT mMessagesVersion;
          MessageList mMessageList;
          MessageMap mMessageMap;
          MessageReceiptsPtr mMessagesDelivered;
          MessageReceiptsPtr mMessagesRead;
          UINT mDialogsVersion;
          DialogMap mDialogs;

          DocumentPtr mChangesDoc;

          bool mDetailsChanged;
          ThreadContactMap mContactsChanged;
          ContactURIList mContactsRemoved;
          ThreadContactMap mContactsToAddChanged;
          ContactURIList mContactsToAddRemoved;
          ContactURIList mContactsToRemoveChanged;
          ContactURIList mContactsToRemoveRemoved;
          MessageList mMessagesChanged;
          Time mMessagesChangedTime;
          MessageReceiptMap mMessagesDeliveredChanged;
          MessageReceiptMap mMessagesReadChanged;
          DialogMap mDialogsChanged;
          DialogIDList mDialogsRemoved;
          ChangedDescriptionMap mDescriptionsChanged;
          DescriptionIDList mDescriptionsRemoved;
        };
      };
    }
  }
}
