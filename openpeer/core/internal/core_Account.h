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
#include <openpeer/core/internal/core_CallTransport.h>

#include <openpeer/core/IAccount.h>

#include <openpeer/stack/IAccount.h>
#include <openpeer/stack/IPeerSubscription.h>
#include <openpeer/stack/IPublicationRepository.h>
#include <openpeer/stack/IServiceLockbox.h>
#include <openpeer/stack/IServiceNamespaceGrant.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      interaction ICallTransportForAccount;
      interaction IContactForAccount;
      interaction IConversationThreadForAccount;
      interaction IIdentityForAccount;

      typedef services::IHelper::SplitMap SplitMap;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForCall
      #pragma mark

      interaction IAccountForCall
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForCall, ForCall)

        virtual CallTransportPtr getCallTransport() const = 0;
        virtual ICallDelegatePtr getCallDelegate() const = 0;

        virtual ContactPtr getSelfContact() const = 0;
        virtual ILocationPtr getSelfLocation() const = 0;

        virtual stack::IAccountPtr getStackAccount() const = 0;

        virtual IPeerFilesPtr getPeerFiles() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForContact
      #pragma mark

      interaction IAccountForContact
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForContact, ForContact)

        virtual RecursiveLock &getLock() const = 0;

        virtual ContactPtr getSelfContact() const = 0;

        virtual stack::IAccountPtr getStackAccount() const = 0;

        virtual ContactPtr findContact(const char *peerURI) const = 0;

        virtual void notifyAboutContact(ContactPtr contact) = 0;

        virtual void hintAboutContactLocation(
                                              ContactPtr contact,
                                              const char *locationID
                                              ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForConversationThread
      #pragma mark

      interaction IAccountForConversationThread
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForConversationThread, ForConversationThread)

        virtual IAccount::AccountStates getState(
                                                 WORD *outErrorCode = NULL,
                                                 String *outErrorReason = NULL
                                                 ) const = 0;

        virtual RecursiveLock &getLock() const = 0;

        virtual ContactPtr getSelfContact() const = 0;
        virtual ILocationPtr getSelfLocation() const = 0;

        virtual ContactPtr findContact(const char *peerURI) const = 0;

        virtual stack::IAccountPtr getStackAccount() const = 0;
        virtual IPublicationRepositoryPtr getRepository() const = 0;

        virtual IPeerFilesPtr getPeerFiles() const = 0;

        virtual IConversationThreadDelegatePtr getConversationThreadDelegate() const = 0;
        virtual void notifyConversationThreadCreated(ConversationThreadPtr thread) = 0;

        virtual ConversationThreadPtr getConversationThreadByID(const char *threadID) const = 0;
        virtual void getConversationThreads(ConversationThreadList &outConversationThreads) const = 0;

        virtual void notifyConversationThreadStateChanged() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForIdentity
      #pragma mark

      interaction IAccountForIdentity
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForIdentity, ForIdentity)

        virtual stack::IServiceNamespaceGrantSessionPtr getNamespaceGrantSession() const = 0;
        virtual stack::IServiceLockboxSessionPtr getLockboxSession() const = 0;

        virtual void associateIdentity(IdentityPtr identity) = 0;
      };
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForIdentityLookup
      #pragma mark

      interaction IAccountForIdentityLookup
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForIdentityLookup, ForIdentityLookup)

        virtual RecursiveLock &getLock() const = 0;

        virtual ContactPtr findContact(const char *peerURI) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account
      #pragma mark

      class Account : public Noop,
                      public MessageQueueAssociator,
                      public IAccount,
                      public IAccountForCall,
                      public IAccountForContact,
                      public IAccountForConversationThread,
                      public IAccountForIdentity,
                      public IAccountForIdentityLookup,
                      public ICallTransportDelegate,
                      public stack::IAccountDelegate,
                      public IPeerSubscriptionDelegate,
                      public IServiceLockboxSessionDelegate,
                      public IServiceNamespaceGrantSessionDelegate,
                      public IWakeDelegate
      {
      public:
        friend interaction IAccountFactory;
        friend interaction IAccount;

        ZS_DECLARE_TYPEDEF_PTR(ICallTransportForAccount, UseCallTransport)
        ZS_DECLARE_TYPEDEF_PTR(IContactForAccount, UseContact)
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForAccount, UseConversationThread)
        ZS_DECLARE_TYPEDEF_PTR(IIdentityForAccount, UseIdentity)

        ZS_DECLARE_CLASS_PTR(ContactSubscription)
        ZS_DECLARE_CLASS_PTR(LocationSubscription)

        friend class ContactSubscription;
        friend class LocationSubscription;

        typedef IAccount::AccountStates AccountStates;

        typedef IServiceLockboxSession::SessionStates LockboxSessionStates;
        typedef IServiceNamespaceGrantSession::SessionStates GrantSessionStates;

        typedef String PeerURI;
        typedef std::map<PeerURI, ContactSubscriptionPtr> ContactSubscriptionMap;

        typedef String BaseThreadID;
        typedef std::map<BaseThreadID, UseConversationThreadPtr> ConversationThreadMap;

        typedef std::map<PeerURI, UseContactPtr> ContactMap;

        typedef PUID ServiceIdentitySessionID;
        typedef std::map<ServiceIdentitySessionID, UseIdentityPtr> IdentityMap;

      protected:
        Account(
                IMessageQueuePtr queue,
                IAccountDelegatePtr delegate,
                IConversationThreadDelegatePtr conversationThreadDelegate,
                ICallDelegatePtr callDelegate
                );
        
        Account(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init();

      public:
        ~Account();

        static AccountPtr convert(IAccountPtr account);
        static AccountPtr convert(ForCallPtr account);
        static AccountPtr convert(ForContactPtr account);
        static AccountPtr convert(ForConversationThreadPtr account);
        static AccountPtr convert(ForIdentityPtr account);
        static AccountPtr convert(ForIdentityLookupPtr account);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccount
        #pragma mark

        static ElementPtr toDebug(IAccountPtr account);

        static AccountPtr login(
                                IAccountDelegatePtr delegate,
                                IConversationThreadDelegatePtr conversationThreadDelegate,
                                ICallDelegatePtr callDelegate,
                                const char *namespaceGrantOuterFrameURLUponReload,
                                const char *grantID,
                                const char *lockboxServiceDomain,
                                bool forceCreateNewLockboxAccount = false
                                );
        static AccountPtr relogin(
                                  IAccountDelegatePtr delegate,
                                  IConversationThreadDelegatePtr conversationThreadDelegate,
                                  ICallDelegatePtr callDelegate,
                                  const char *namespaceGrantOuterFrameURLUponReload,
                                  ElementPtr reloginInformation
                                  );

        virtual PUID getID() const {return mID;}

        virtual AccountStates getState(
                                       WORD *outErrorCode,
                                       String *outErrorReason
                                       ) const;

        virtual ElementPtr getReloginInformation() const;

        virtual String getStableID() const;

        virtual String getLocationID() const;

        virtual void shutdown();

        virtual ElementPtr savePeerFilePrivate() const;
        virtual SecureByteBlockPtr getPeerFilePrivateSecret() const;

        virtual IdentityListPtr getAssociatedIdentities() const;
        virtual void removeIdentities(const IdentityList &identitiesToRemove);

        virtual String getInnerBrowserWindowFrameURL() const;

        virtual void notifyBrowserWindowVisible();
        virtual void notifyBrowserWindowClosed();

        virtual ElementPtr getNextMessageForInnerBrowerWindowFrame();
        virtual void handleMessageFromInnerBrowserWindowFrame(ElementPtr unparsedMessage);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForCall
        #pragma mark

        virtual CallTransportPtr getCallTransport() const;
        virtual ICallDelegatePtr getCallDelegate() const;

        // (duplicate) virtual ContactPtr getSelfContact() const;
        // (duplicate) virtual ILocationPtr getSelfLocation() const;

        // (duplicate) virtual stack::IAccountPtr getStackAccount() const;

        // (duplicate) virtual IPeerFilesPtr getPeerFiles() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForContact
        #pragma mark

        // (duplicate) virtual RecursiveLock &getLock() const;

        // (duplicate) virtual ContactPtr getSelfContact() const;
        // (duplicate) virtual stack::IAccountPtr getStackAccount() const;

        virtual ContactPtr findContact(const char *peerURI) const;

        virtual void notifyAboutContact(ContactPtr contact);

        virtual void hintAboutContactLocation(
                                              ContactPtr contact,
                                              const char *locationID
                                              );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForConversationThread
        #pragma mark

        // (duplicate) virtual IAccount::AccountStates getState(
        //                                                      WORD *outErrorCode,
        //                                                      String *outErrorReason
        //                                                      ) const = 0;

        virtual RecursiveLock &getLock() const {return mLock;}

        virtual ContactPtr getSelfContact() const;
        virtual ILocationPtr getSelfLocation() const;

        // (duplicate) virtual ContactPtr findContact(const char *peerURI) const;

        virtual stack::IAccountPtr getStackAccount() const;
        virtual IPublicationRepositoryPtr getRepository() const;

        virtual IPeerFilesPtr getPeerFiles() const;

        virtual IConversationThreadDelegatePtr getConversationThreadDelegate() const;
        virtual void notifyConversationThreadCreated(ConversationThreadPtr thread);

        virtual ConversationThreadPtr getConversationThreadByID(const char *threadID) const;
        virtual void getConversationThreads(ConversationThreadList &outConversationThreads) const;

        virtual void notifyConversationThreadStateChanged();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForIdentity
        #pragma mark

        virtual IServiceNamespaceGrantSessionPtr getNamespaceGrantSession() const;
        virtual IServiceLockboxSessionPtr getLockboxSession() const;

        virtual void associateIdentity(IdentityPtr identity);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForIdentityLookup
        #pragma mark

        // (duplicate) virtual RecursiveLock &getLock() const;

        // (duplicate) virtual ContactPtr findContact(const char *peerURI) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => ICallTransportDelegate
        #pragma mark

        virtual void onCallTransportStateChanged(
                                                 ICallTransportPtr transport,
                                                 CallTransportStates state
                                                 );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => stack::IAccountDelegate
        #pragma mark

        virtual void onAccountStateChanged(
                                           stack::IAccountPtr account,
                                           stack::IAccount::AccountStates state
                                           );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IPeerSubscriptionDelegate
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

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IServiceLockboxSessionDelegate
        #pragma mark

        virtual void onServiceLockboxSessionStateChanged(
                                                         IServiceLockboxSessionPtr session,
                                                         LockboxSessionStates state
                                                         );
        virtual void onServiceLockboxSessionAssociatedIdentitiesChanged(IServiceLockboxSessionPtr session);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IServiceNamespaceGrantSessionDelegate
        #pragma mark

        virtual void onServiceNamespaceGrantSessionStateChanged(
                                                                IServiceNamespaceGrantSessionPtr session,
                                                                GrantSessionStates state
                                                                );
        virtual void onServiceNamespaceGrantSessionPendingMessageForInnerBrowserWindowFrame(IServiceNamespaceGrantSessionPtr session);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IWakeDelegate
        #pragma mark

        virtual void onWake();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => friend Account::ContactSubscription
        #pragma mark

        void notifyContactSubscriptionShutdown(const String &contactID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => friend Account::LocationSubscription
        #pragma mark

        UseConversationThreadPtr notifyPublicationUpdated(
                                                          ILocationPtr peerLocation,
                                                          IPublicationMetaDataPtr metaData,
                                                          const SplitMap &split
                                                          );

        void notifyPublicationGone(
                                   ILocationPtr peerLocation,
                                   IPublicationMetaDataPtr metaData,
                                   const SplitMap &split
                                   );

      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => (internal)
        #pragma mark

        bool isPending() const      {return AccountState_Pending == mCurrentState;}
        bool isReady() const        {return AccountState_Ready == mCurrentState;}
        bool isShuttingDown() const {return AccountState_ShuttingDown == mCurrentState;}
        bool isShutdown() const     {return AccountState_Shutdown == mCurrentState;}

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();

        void step();
        bool stepLoginIdentityAssociated();
        bool stepLockboxShutdownCheck();
        bool stepGrantSession();
        bool stepStackAccountCreation();
        bool stepLockboxSession();
        bool stepStackAccount();
        bool stepSelfContact();
        bool stepCallTransportSetup();
        bool stepSubscribersPermissionDocument();
        bool stepPeerSubscription();
        bool stepCallTransportFinalize();

        void setState(core::IAccount::AccountStates newState);
        void setError(
                      WORD errorCode,
                      const char *reason = NULL
                      );

      public:
#define OPENPEER_CORE_ACCOUNT_INCLUDE_CONTACT_SUBSCRIPTION
#include <openpeer/core/internal/core_Account_ContactSubscription.h>
#undef OPENPEER_CORE_ACCOUNT_INCLUDE_CONTACT_SUBSCRIPTION

#define OPENPEER_CORE_ACCOUNT_INCLUDE_LOCATION_SUBSCRIPTION
#include <openpeer/core/internal/core_Account_LocationSubscription.h>
#undef OPENPEER_CORE_ACCOUNT_INCLUDE_LOCATION_SUBSCRIPTION

      private:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => (data)
        #pragma mark

        mutable RecursiveLock mLock;
        PUID mID;
        AccountWeakPtr mThisWeak;
        AccountPtr mGracefulShutdownReference;

        IAccount::AccountStates mCurrentState;
        WORD mLastErrorCode;
        String mLastErrorReason;

        IAccountDelegatePtr mDelegate;

        IConversationThreadDelegatePtr mConversationThreadDelegate;
        ICallDelegatePtr mCallDelegate;

        stack::IAccountPtr mStackAccount;

        IServiceNamespaceGrantSessionPtr mGrantSession;

        IServiceLockboxSessionPtr mLockboxSession;
        IServiceLockboxPtr mLockboxService;
        bool mLockboxForceCreateNewAccount;

        mutable IdentityMap mIdentities;

        IPeerSubscriptionPtr mPeerSubscription;

        UseContactPtr mSelfContact;

        ContactMap mContacts;
        ContactSubscriptionMap mContactSubscriptions;

        ConversationThreadMap mConversationThreads;

        UseCallTransportPtr mCallTransport;

        IPublicationPtr mSubscribersPermissionDocument;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountFactory
      #pragma mark

      interaction IAccountFactory
      {
        static IAccountFactory &singleton();

        virtual AccountPtr login(
                                 IAccountDelegatePtr delegate,
                                 IConversationThreadDelegatePtr conversationThreadDelegate,
                                 ICallDelegatePtr callDelegate,
                                 const char *namespaceGrantOuterFrameURLUponReload,
                                 const char *grantID,
                                 const char *lockboxServiceDomain,
                                 bool forceCreateNewLockboxAccount = false
                                 );

        virtual AccountPtr relogin(
                                   IAccountDelegatePtr delegate,
                                   IConversationThreadDelegatePtr conversationThreadDelegate,
                                   ICallDelegatePtr callDelegate,
                                   const char *namespaceGrantOuterFrameURLUponReload,
                                   ElementPtr reloginInformation
                                   );
      };

    }
  }
}

