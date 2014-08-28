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

#include <openpeer/core/IAccount.h>
#include <openpeer/core/IPushPresence.h>

#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_PushMailboxManager.h>

#include <openpeer/stack/IServicePushMailbox.h>

#include <zsLib/MessageQueueAssociator.h>

#define OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MAILBOX_FOLDER "openpeer/core/push-presence-default-push-mailbox-folder"
#define OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MESSAGE_TYPE "openpeer/core/push-presence-default-push-message-type"

#define OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_EXPIRES_IN_SECONDS "openpeer/core/push-presence-default-push-expires-in-seconds"

#define OPENPEER_CORE_SETTING_PUSH_PRESENCE_LAST_DOWNLOADED_MESSAGE "openpeer/core/push-presence-last-downloaded-message"

#define OPENPEER_CORE_PUSH_PRESENCE_JSON_MIME_TYPE "text/json"

#define OPENPEER_CORE_PUSH_PRESENCE_PUSH_SERVICE_TYPE "all"

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
      #pragma mark PushPresence
      #pragma mark

      class PushPresence : public Noop,
                           public MessageQueueAssociator,
                           public SharedRecursiveLock,
                           public IPushPresence,
                           public IWakeDelegate,
                           public IAccountDelegate,
                           public stack::IServicePushMailboxSessionDelegate
      {
      public:
        friend interaction IPushPresenceFactory;
        friend interaction IPushPresence;
        friend class RegisterQuery;

        ZS_DECLARE_CLASS_PTR(RegisterQuery)

        ZS_DECLARE_TYPEDEF_PTR(IPushMailboxManagerForPushPresence, UsePushMailboxManager)

        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession, IServicePushMailboxSession)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSessionSubscription, IServicePushMailboxSessionSubscription)
        ZS_DECLARE_TYPEDEF_PTR(IAccountForPushPresence, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(IContactForPushPresence, UseContact)

        typedef std::list<RegisterQueryPtr> RegisterQueryList;

        typedef String PeerOrIdentityURI;
        ZS_DECLARE_TYPEDEF_PTR(std::list<PeerOrIdentityURI>, PeerOrIdentityList)

        typedef String MessageID;
        typedef PeerOrIdentityListPtr ToListPtr;
        typedef std::pair<ToListPtr, StatusPtr> ToAndStatusPair;
        typedef std::list<ToAndStatusPair> StatusList;

      protected:
        PushPresence(
                      IMessageQueuePtr queue,
                      IPushPresenceDelegatePtr delegate,
                      IPushPresenceDatabaseAbstractionDelegatePtr databaseDelegate,
                      UseAccountPtr account
                      );
        
        PushPresence(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~PushPresence();

        static PushPresencePtr convert(IPushPresencePtr messaging);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushPresence => IPushPresence
        #pragma mark

        static ElementPtr toDebug(IPushPresencePtr identity);

        static PushPresencePtr create(
                                      IPushPresenceDelegatePtr delegate,
                                      IPushPresenceDatabaseAbstractionDelegatePtr databaseDelegate,
                                      IAccountPtr account
                                      );

        virtual PUID getID() const {return mID;}

        virtual PushPresenceStates getState(
                                            WORD *outErrorCode = NULL,
                                            String *outErrorReason = NULL
                                            ) const;

        virtual void shutdown();

        virtual IPushPresenceRegisterQueryPtr registerDevice(
                                                             IPushPresenceRegisterQueryDelegatePtr inDelegate,
                                                             const char *inDeviceToken,
                                                             Time inExpires,
                                                             const char *inMappedType,
                                                             bool inUnreadBadge,
                                                             const char *inSound,
                                                             const char *inAction,
                                                             const char *inLaunchImage,
                                                             unsigned int inPriority,
                                                             const ValueNameList &inValueNames
                                                             );

        virtual void send(
                          const ContactList &toContactList,
                          const Status &status
                          );

        virtual void recheckNow();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushPresence => friend RegisterQuery
        #pragma mark

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushPresence => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushPresence => IAccountDelegate
        #pragma mark

        virtual void onAccountStateChanged(
                                           IAccountPtr account,
                                           AccountStates state
                                           );

        virtual void onAccountAssociatedIdentitiesChanged(IAccountPtr account);

        virtual void onAccountPendingMessageForInnerBrowserWindowFrame(IAccountPtr account);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushPresence => IServicePushPresenceSessionDelegate
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
        #pragma mark PushPresence => (internal)
        #pragma mark

        bool isShutdown() const {return PushPresenceState_Shutdown == mCurrentState;}
        bool isShuttingDown() const {return PushPresenceState_ShuttingDown == mCurrentState;}

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();
        void setCurrentState(PushPresenceStates state);
        void setError(WORD errorCode, const char *inReason = NULL);

        void step();
        bool stepAccount();
        bool stepMailbox();
        bool stepAttach();
        bool stepGetMessages();
        bool stepDeliverMessages();

      public:

#define OPENPEER_CORE_PUSH_PRESENCE_REGISTER_QUERY
#include <openpeer/core/internal/core_PushPresence_RegisterQuery.h>
#undef OPENPEER_CORE_PUSH_PRESENCE_REGISTER_QUERY

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushPresence => (data)
        #pragma mark

        AutoPUID mID;
        PushPresenceWeakPtr mThisWeak;
        PushPresencePtr mGracefulShutdownReference;

        IPushPresenceDelegatePtr mDelegate;
        IPushPresenceDatabaseAbstractionDelegatePtr mDatabase;

        IServicePushMailboxSessionPtr mMailbox;
        IServicePushMailboxSessionSubscriptionPtr mMailboxSubscription;

        UseAccountPtr mAccount;
        IAccountSubscriptionPtr mAccountSubscription;
        bool mPreviouslyReady {};

        PushPresenceStates mCurrentState;
        WORD mLastError {};
        String mLastErrorReason;

        RegisterQueryList mPendingAttachmentRegisterQueries;

        String mLastVersionDownloaded;

        StatusList mPendingDelivery;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPushPresenceFactory
      #pragma mark

      interaction IPushPresenceFactory
      {
        static IPushPresenceFactory &singleton();

        virtual PushPresencePtr create(
                                       IPushPresenceDelegatePtr delegate,
                                       IPushPresenceDatabaseAbstractionDelegatePtr databaseDelegate,
                                       IAccountPtr account
                                       );
      };

      class PushPresenceFactory : public IFactory<IPushPresenceFactory> {};
    }
  }
}
