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

//#include <openpeer/core/IAccount.h>
//#include <openpeer/core/IPushPresence.h>
//
#include <openpeer/core/internal/core_Account.h>
//#include <openpeer/core/internal/core_Contact.h>

#include <openpeer/stack/IServicePushMailbox.h>

//#include <zsLib/MessageQueueAssociator.h>
//
//#define OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MAILBOX_FOLDER "openpeer/core/push-presence-default-push-mailbox-folder"
//#define OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_MESSAGE_TYPE "openpeer/core/push-presence-default-push-message-type"
//
//#define OPENPEER_CORE_SETTING_PUSH_PRESENCE_DEFAULT_PUSH_EXPIRES_IN_SECONDS "openpeer/core/push-presence-default-push-expires-in-seconds"
//
//#define OPENPEER_CORE_SETTING_PUSH_PRESENCE_LAST_DOWNLOADED_MESSAGE "openpeer/core/push-presence-last-downloaded-message"
//
//#define OPENPEER_CORE_PUSH_PRESENCE_JSON_MIME_TYPE "text/json"
//
//#define OPENPEER_CORE_PUSH_PRESENCE_PUSH_SERVICE_TYPE "all"

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
      #pragma mark IPushMailboxManagerForPushMessaging
      #pragma mark

      interaction IPushMailboxManagerForPushMessaging
      {
        ZS_DECLARE_TYPEDEF_PTR(IPushMailboxManagerForPushMessaging, ForPushMessaging)

        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession, IServicePushMailboxSession)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSessionDelegate, IServicePushMailboxSessionDelegate)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSessionSubscription, IServicePushMailboxSessionSubscription)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxDatabaseAbstractionDelegate, IServicePushMailboxDatabaseAbstractionDelegate)


        static IServicePushMailboxSessionPtr create(
                                                    IServicePushMailboxSessionDelegatePtr inDelegate,
                                                    IServicePushMailboxDatabaseAbstractionDelegatePtr inDatabaseDelegate,
                                                    IMessageQueuePtr inQueue,
                                                    IAccountPtr inAccount,
                                                    IServicePushMailboxSessionSubscriptionPtr &outSubscription
                                                    );

        virtual ~IPushMailboxManagerForPushMessaging() {} // make polymorphic
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPushMailboxManagerForPushPresence
      #pragma mark

      interaction IPushMailboxManagerForPushPresence
      {
        ZS_DECLARE_TYPEDEF_PTR(IPushMailboxManagerForPushPresence, ForPushPresence)

        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailbox, IServicePushMailbox)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession, IServicePushMailboxSession)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSessionDelegate, IServicePushMailboxSessionDelegate)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSessionSubscription, IServicePushMailboxSessionSubscription)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxDatabaseAbstractionDelegate, IServicePushMailboxDatabaseAbstractionDelegate)

        static IServicePushMailboxSessionPtr create(
                                                    IServicePushMailboxSessionDelegatePtr inDelegate,
                                                    IServicePushMailboxDatabaseAbstractionDelegatePtr inDatabaseDelegate,
                                                    IMessageQueuePtr inQueue,
                                                    IAccountPtr inAccount,
                                                    IServicePushMailboxSessionSubscriptionPtr &outSubscription
                                                    );

        virtual ~IPushMailboxManagerForPushPresence() {} // make polymorphic
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMailbox
      #pragma mark

      class PushMailboxManager : public Noop,
                                 public SharedRecursiveLock,
                                 public IPushMailboxManagerForPushMessaging,
                                 public IPushMailboxManagerForPushPresence
      {
      public:
        friend interaction IPushMailboxManagerForPushMessaging;
        friend interaction IPushMailboxManagerForPushPresence;
        friend interaction IPushMailboxManagerFactory;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForPushMailboxManager, UseAccount)

        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSession, IServicePushMailboxSession)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSessionDelegate, IServicePushMailboxSessionDelegate)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxSessionSubscription, IServicePushMailboxSessionSubscription)
        ZS_DECLARE_TYPEDEF_PTR(stack::IServicePushMailboxDatabaseAbstractionDelegate, IServicePushMailboxDatabaseAbstractionDelegate)

        struct Mailbox
        {
          UseAccountWeakPtr mAccount;
          IServicePushMailboxSessionWeakPtr mMailbox;
        };

        typedef std::list<Mailbox> MailboxList;

      protected:
        PushMailboxManager();

        PushMailboxManager(Noop) :
          Noop(true),
          SharedRecursiveLock(SharedRecursiveLock::create()) {}

        void init();

        static PushMailboxManagerPtr create();

      public:
        ~PushMailboxManager();

        PushMailboxManagerPtr convert(ForPushMessagingPtr object);
        PushMailboxManagerPtr convert(ForPushPresencePtr object);

      protected:
        static PushMailboxManagerPtr singleton();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMailboxManager => IPushMailboxManagerForPushMessaging
        #pragma mark

        virtual IServicePushMailboxSessionPtr create(
                                                    IServicePushMailboxSessionDelegatePtr inDelegate,
                                                    IServicePushMailboxDatabaseAbstractionDelegatePtr inDatabaseDelegate,
                                                    IMessageQueuePtr inQueue,
                                                    IAccountPtr inAccount,
                                                    IServicePushMailboxSessionSubscriptionPtr &outSubscription
                                                    );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMailboxManager => IPushMailboxManagerForPushPresence
        #pragma mark

        // (duplicate) virtual IServicePushMailboxSessionPtr create(
        //                                                          IServicePushMailboxSessionDelegatePtr inDelegate,
        //                                                          IServicePushMailboxDatabaseAbstractionDelegatePtr inDatabaseDelegate,
        //                                                          IMessageQueuePtr inQueue,
        //                                                          IAccountPtr inAccount,
        //                                                          IServicePushMailboxSessionSubscriptionPtr &outSubscription
        //                                                          );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMailboxManager => (internal)
        #pragma mark

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;
        ElementPtr toDebug() const;

        void cancel();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PushMailboxManager => (data)
        #pragma mark

        PushMailboxManagerWeakPtr mThisWeak;

        AutoPUID mID;
        MailboxList mMailboxes;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPushPresenceFactory
      #pragma mark

      interaction IPushMailboxManagerFactory
      {
        static IPushMailboxManagerFactory &singleton();

        virtual PushMailboxManagerPtr singletonManager();
      };

      class PushMailboxManagerFactory : public IFactory<IPushMailboxManagerFactory> {};
    }
  }
}
