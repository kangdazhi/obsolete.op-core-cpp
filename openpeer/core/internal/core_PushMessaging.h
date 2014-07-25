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

#pragma once

#include <openpeer/core/internal/types.h>
#include <openpeer/core/IPushMessaging.h>

#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Contact.h>

#include <openpeer/stack/IServicePushMailbox.h>

#include <zsLib/MessageQueueAssociator.h>

#define OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MAILBOX_FOLDER "openpeer/core/push-messaging-default-push-mailbox-folder"
#define OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_MESSAGE_TYPE "openpeer/core/push-messaging-default-push-message-type"

#define OPENPEER_CORE_SETTING_PUSH_MESSAGING_DEFAULT_PUSH_EXPIRES_IN_SECONDS "openpeer/core/push-messaging-default-push-expires-in-seconds"

#define OPENPEER_CORE_PUSH_MESSAGING_MIMETYPE_FILTER_PREFIX "text/"

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging
      #pragma mark

      class PushMessaging : public Noop,
                            public MessageQueueAssociator,
                            public SharedRecursiveLock,
                            public IPushMessaging,
                            public IWakeDelegate,
                            public IAccountDelegate,
                            public stack::IServicePushMailboxSessionDelegate
      {
      public:
        friend interaction IPushMessagingFactory;
        friend interaction IPushMessaging;
        friend class PushQuery;
        friend class RegisterQuery;

        ZS_DECLARE_CLASS_PTR(RegisterQuery)
        ZS_DECLARE_CLASS_PTR(PushQuery)

        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession, IServicePushMailboxSession)
        ZS_DECLARE_TYPEDEF_PTR(IAccountForPushMessaging, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(IContactForPushMessaging, UseContact)

        typedef std::list<PushQueryPtr> PushQueryList;
        typedef std::list<RegisterQueryPtr> RegisterQueryList;

        typedef String MessageID;
        typedef std::list<MessageID> MessageIDList;

      protected:
        PushMessaging(
                      IMessageQueuePtr queue,
                      IPushMessagingDelegatePtr delegate,
                      IPushMessagingDatabaseAbstractionDelegatePtr databaseDelegate,
                      UseAccountPtr account
                      );
        
        PushMessaging(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~PushMessaging();

        static PushMessagingPtr convert(IPushMessagingPtr messaging);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => IPushMessaging
        #pragma mark

        static ElementPtr toDebug(IPushMessagingPtr identity);

        static PushMessagingPtr create(
                                        IPushMessagingDelegatePtr delegate,
                                        IPushMessagingDatabaseAbstractionDelegatePtr databaseDelegate,
                                        IAccountPtr account
                                        );

        virtual PUID getID() const {return mID;}

        virtual PushMessagingStates getState(
                                             WORD *outErrorCode = NULL,
                                             String *outErrorReason = NULL
                                             ) const;

        virtual void shutdown();

        virtual IPushMessagingRegisterQueryPtr registerDevice(
                                                              IPushMessagingRegisterQueryDelegatePtr delegate,
                                                              const char *deviceToken,
                                                              Time expires,
                                                              const char *mappedType,
                                                              bool unreadBadge,
                                                              const char *sound,
                                                              const char *action,
                                                              const char *launchImage,
                                                              unsigned int priority
                                                              );

        virtual IPushMessagingQueryPtr push(
                                            IPushMessagingQueryDelegatePtr delegate,
                                            const ContactList &toContactList,
                                            const PushMessage &message
                                            );

        virtual void recheckNow();

        virtual bool getMessagesUpdates(
                                        const char *inLastVersionDownloaded,
                                        String &outUpdatedToVersion,
                                        PushMessageList &outNewMessages
                                        );

        virtual void markPushMessageRead(const char *messageID);
        virtual void deletePushMessage(const char *messageID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => friend PushQuery
        #pragma mark

        // (duplicate) static void copy(
        //                              UseAccountPtr account,
        //                              const IServicePushMailboxSession::PushMessage &source,
        //                              PushMessage &dest
        //                              );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => friend RegisterQuery
        #pragma mark

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => IAccountDelegate
        #pragma mark

        virtual void onAccountStateChanged(
                                           IAccountPtr account,
                                           AccountStates state
                                           );

        virtual void onAccountAssociatedIdentitiesChanged(IAccountPtr account);

        virtual void onAccountPendingMessageForInnerBrowserWindowFrame(IAccountPtr account);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => IServicePushMessagingSessionDelegate
        #pragma mark

        virtual void onServicePushMailboxSessionStateChanged(
                                                             IServicePushMailboxSessionPtr session,
                                                             SessionStates state
                                                             );

        virtual void onServicePushMailboxSessionFolderChanged(
                                                              IServicePushMailboxSessionPtr session,
                                                              const char *folder
                                                              );

      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => (internal)
        #pragma mark

        bool isShutdown() const {return PushMessagingStates_Shutdown == mCurrentState;}
        bool isShuttingDown() const {return PushMessagingStates_ShuttingDown == mCurrentState;}

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();
        void setCurrentState(PushMessagingStates state);
        void setError(WORD errorCode, const char *inReason = NULL);

        void step();
        bool stepAccount();
        bool stepMailbox();
        bool stepAttach();
        bool stepMarkReadAndDelete();

        static void copy(
                         UseAccountPtr account,
                         const IServicePushMailboxSession::PushMessage &source,
                         PushMessage &dest
                         );

        static void copy(
                         const PushMessage &source,
                         IServicePushMailboxSession::PushMessage &dest
                         );

      public:

#define OPENPEER_CORE_PUSH_MESSAGING_PUSH_QUERY
#include <openpeer/core/internal/core_PushMessaging_PushQuery.h>
#undef OPENPEER_CORE_PUSH_MESSAGING_PUSH_QUERY

#define OPENPEER_CORE_PUSH_MESSAGING_REGISTER_QUERY
#include <openpeer/core/internal/core_PushMessaging_RegisterQuery.h>
#undef OPENPEER_CORE_PUSH_MESSAGING_REGISTER_QUERY

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMessaging => (data)
        #pragma mark

        AutoPUID mID;
        PushMessagingWeakPtr mThisWeak;
        PushMessagingPtr mGracefulShutdownReference;

        IPushMessagingDelegatePtr mDelegate;
        IPushMessagingDatabaseAbstractionDelegatePtr mDatabase;

        IServicePushMailboxSessionPtr mMailbox;

        UseAccountPtr mAccount;
        IAccountSubscriptionPtr mAccountSubscription;
        AutoBool mPreviouslyReady;

        PushMessagingStates mCurrentState;
        AutoWORD mLastError;
        String mLastErrorReason;

        PushQueryList mPendingAttachmentPushQueries;
        RegisterQueryList mPendingAttachmentRegisterQueries;

        MessageIDList mMarkReadMessages;
        MessageIDList mDeleteMessages;

        String mLastVersionDownloaded;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPushMessagingFactory
      #pragma mark

      interaction IPushMessagingFactory
      {
        static IPushMessagingFactory &singleton();

        virtual PushMessagingPtr create(
                                        IPushMessagingDelegatePtr delegate,
                                        IPushMessagingDatabaseAbstractionDelegatePtr databaseDelegate,
                                        IAccountPtr account
                                        );
      };

    }
  }
}
