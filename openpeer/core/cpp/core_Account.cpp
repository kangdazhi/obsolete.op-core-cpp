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

#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Identity.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_ConversationThread.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IServiceIdentity.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPublication.h>
#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/ISettings.h>

#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForAccount, UseConversationThread)

      typedef IStackForInternal UseStack;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForCall
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForContact
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account
      #pragma mark

      //-----------------------------------------------------------------------
      Account::Account(
                       IMessageQueuePtr queue,
                       IAccountDelegatePtr delegate,
                       IConversationThreadDelegatePtr conversationThreadDelegate,
                       ICallDelegatePtr callDelegate
                       ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mConversationThreadDelegate(IConversationThreadDelegateProxy::createWeak(UseStack::queueApplication(), conversationThreadDelegate)),
        mCallDelegate(ICallDelegateProxy::createWeak(UseStack::queueApplication(), callDelegate)),
        mCurrentState(AccountState_Pending),
        mLastErrorCode(0),
        mLockboxForceCreateNewAccount(false),
        mTotalPendingMessages(0)
      {
        ZS_LOG_BASIC(log("created"))

        mDefaultSubscription = mSubscriptions.subscribe(delegate, UseStack::queueApplication());
      }

      //-----------------------------------------------------------------------
      Account::~Account()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void Account::init()
      {
        AutoRecursiveLock lock(*this);

        mDelegateFilter = DelegateFilter::create(mThisWeak.lock(), mConversationThreadDelegate, mCallDelegate);

        // replace conversation thread delegate / call delegate with intercepted delegate
        mConversationThreadDelegate = IConversationThreadDelegateProxy::create(mDelegateFilter);
        mCallDelegate = ICallDelegateProxy::create(mDelegateFilter);

        mBackgroundingSubscription = IBackgrounding::subscribe(mThisWeak.lock(), services::ISettings::getUInt(OPENPEER_CORE_SETTING_ACCOUNT_BACKGROUNDING_PHASE));

        step();
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(IAccountPtr account)
      {
        return ZS_DYNAMIC_PTR_CAST(Account, account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForCallPtr account)
      {
        return ZS_DYNAMIC_PTR_CAST(Account, account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForContactPtr account)
      {
        return ZS_DYNAMIC_PTR_CAST(Account, account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForConversationThreadPtr account)
      {
        return ZS_DYNAMIC_PTR_CAST(Account, account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForIdentityPtr account)
      {
        return ZS_DYNAMIC_PTR_CAST(Account, account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForIdentityLookupPtr account)
      {
        return ZS_DYNAMIC_PTR_CAST(Account, account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForPushMailboxManagerPtr account)
      {
        return ZS_DYNAMIC_PTR_CAST(Account, account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForPushMessagingPtr account)
      {
        return ZS_DYNAMIC_PTR_CAST(Account, account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForPushPresencePtr account)
      {
        return ZS_DYNAMIC_PTR_CAST(Account, account);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Account::toDebug(IAccountPtr account)
      {
        if (!account) return ElementPtr();
        return Account::convert(account)->toDebug();
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::login(
                                IAccountDelegatePtr delegate,
                                IConversationThreadDelegatePtr conversationThreadDelegate,
                                ICallDelegatePtr callDelegate,
                                const char *namespaceGrantOuterFrameURLUponReload,
                                const char *grantID,
                                const char *lockboxServiceDomain,
                                bool forceCreateNewLockboxAccount
                                )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!namespaceGrantOuterFrameURLUponReload)
        ZS_THROW_INVALID_ARGUMENT_IF(!grantID)
        ZS_THROW_INVALID_ARGUMENT_IF(!lockboxServiceDomain)

        AccountPtr pThis(new Account(UseStack::queueCore(), delegate, conversationThreadDelegate, callDelegate));
        pThis->mThisWeak = pThis;

        AutoRecursiveLock lock(*pThis);

        String lockboxDomain(lockboxServiceDomain);
        IBootstrappedNetworkPtr lockboxNetwork = IBootstrappedNetwork::prepare(lockboxDomain);
        if (!lockboxNetwork) {
          ZS_LOG_ERROR(Detail, pThis->log("failed to prepare bootstrapped network for domain") + ZS_PARAM("domain", lockboxDomain))
          return AccountPtr();
        }

        pThis->mGrantSession = IServiceNamespaceGrantSession::create(pThis, namespaceGrantOuterFrameURLUponReload, grantID);
        ZS_THROW_BAD_STATE_IF(!pThis->mGrantSession)

        pThis->mLockboxService = IServiceLockbox::createServiceLockboxFrom(lockboxNetwork);
        ZS_THROW_BAD_STATE_IF(!pThis->mLockboxService)

        pThis->mLockboxForceCreateNewAccount = forceCreateNewLockboxAccount;

        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::relogin(
                                  IAccountDelegatePtr delegate,
                                  IConversationThreadDelegatePtr conversationThreadDelegate,
                                  ICallDelegatePtr callDelegate,
                                  const char *namespaceGrantOuterFrameURLUponReload,
                                  ElementPtr reloginInformation
                                  )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!namespaceGrantOuterFrameURLUponReload)
        ZS_THROW_INVALID_ARGUMENT_IF(!reloginInformation)

        AccountPtr pThis(new Account(UseStack::queueCore(), delegate, conversationThreadDelegate, callDelegate));
        pThis->mThisWeak = pThis;

        AutoRecursiveLock lock(*pThis);

        String lockboxDomain;
        String accountID;
        String grantID;
        SecureByteBlockPtr lockboxKey;

        try {
          lockboxDomain = reloginInformation->findFirstChildElementChecked("lockboxDomain")->getTextDecoded();
          accountID = reloginInformation->findFirstChildElementChecked("accountID")->getTextDecoded();
          grantID = reloginInformation->findFirstChildElementChecked("grantID")->getTextDecoded();
          lockboxKey = UseServicesHelper::convertFromBase64(reloginInformation->findFirstChildElementChecked("lockboxKey")->getTextDecoded());
        } catch (CheckFailed &) {
          return AccountPtr();
        }

        IBootstrappedNetworkPtr lockboxNetwork = IBootstrappedNetwork::prepare(lockboxDomain);
        if (!lockboxNetwork) {
          ZS_LOG_ERROR(Detail, pThis->log("failed to prepare bootstrapped network for domain") + ZS_PARAM("domain", lockboxDomain))
          return AccountPtr();
        }

        if (UseServicesHelper::isEmpty(lockboxKey)) {
          ZS_LOG_ERROR(Detail, pThis->log("lockbox key specified in relogin information is not valid"))
          return AccountPtr();
        }

        pThis->mGrantSession = IServiceNamespaceGrantSession::create(pThis, namespaceGrantOuterFrameURLUponReload, grantID);
        ZS_THROW_BAD_STATE_IF(!pThis->mGrantSession)

        pThis->mLockboxService = IServiceLockbox::createServiceLockboxFrom(lockboxNetwork);
        ZS_THROW_BAD_STATE_IF(!pThis->mLockboxService)

        pThis->mLockboxForceCreateNewAccount = false;

        pThis->mLockboxSession.set(IServiceLockboxSession::relogin(pThis, pThis->mLockboxService, pThis->mGrantSession, accountID, *lockboxKey));
        pThis->init();

        if (!pThis->mLockboxSession.get()) {
          ZS_LOG_ERROR(Detail, pThis->log("failed to create lockbox session from relogin information"))
          return AccountPtr();
        }
        return pThis;
      }

      //-----------------------------------------------------------------------
      IAccountSubscriptionPtr Account::subscribe(IAccountDelegatePtr originalDelegate)
      {
        ZS_LOG_DETAIL(log("subscribing to account state"))

        AutoRecursiveLock lock(*this);
        if (!originalDelegate) return mDefaultSubscription;

        IAccountSubscriptionPtr subscription = mSubscriptions.subscribe(originalDelegate, UseStack::queueApplication());

        IAccountDelegatePtr delegate = mSubscriptions.delegate(subscription, true);

        if (delegate) {
          AccountPtr pThis = mThisWeak.lock();

          if (AccountState_Pending != mCurrentState) {
            delegate->onAccountStateChanged(pThis, mCurrentState);
          }

          if (mAssociatedIdentitiesChanged) {
            delegate->onAccountAssociatedIdentitiesChanged(pThis);
          }

          for (ULONG index = 0; index < mTotalPendingMessages; ++index) {
            delegate->onAccountPendingMessageForInnerBrowserWindowFrame(pThis);
          }
        }

        if (isShutdown()) {
          mSubscriptions.clear();
        }
        
        return subscription;
      }
      
      //-----------------------------------------------------------------------
      IAccount::AccountStates Account::getState(
                                                WORD *outErrorCode,
                                                String *outErrorReason
                                                ) const
      {
        AutoRecursiveLock lock(*this);

        if (outErrorCode) *outErrorCode = mLastErrorCode;
        if (outErrorReason) *outErrorReason = mLastErrorReason;

        ZS_LOG_DEBUG(debug("getting account state"))

        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::getReloginInformation() const
      {
        AutoRecursiveLock lock(*this);

        ZS_THROW_BAD_STATE_IF(!mLockboxService)

        String lockboxDomain = mLockboxService->getBootstrappedNetwork()->getDomain();
        if (lockboxDomain.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("missing lockbox domain information"))
          return ElementPtr();
        }

        if (!mLockboxSession.get()) {
          ZS_LOG_WARNING(Detail, log("missing lockbox session information"))
          return ElementPtr();
        }

        String accountID = mLockboxSession.get()->getAccountID();
        if (accountID.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("missing account ID information"))
          return ElementPtr();
        }

        ZS_THROW_BAD_STATE_IF(!mGrantSession)
        String grantID = mGrantSession->getGrantID();
        if (grantID.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("missing grant ID information"))
          return ElementPtr();
        }

        SecureByteBlockPtr lockboxKey = mLockboxSession.get()->getLockboxKey();

        if (!lockboxKey) {
          ZS_LOG_WARNING(Detail, log("missing lockbox key information"))
          return ElementPtr();
        }

        ElementPtr reloginEl = Element::create("relogin");

        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("lockboxDomain", lockboxDomain));
        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("accountID", accountID));
        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("grantID", grantID));
        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("lockboxKey", UseServicesHelper::convertToBase64(*lockboxKey)));

        return reloginEl;
      }

      //-----------------------------------------------------------------------
      String Account::getStableID() const
      {
        if (!mLockboxSession.get()) return String();
        return mLockboxSession.get()->getStableID();
      }

      //-----------------------------------------------------------------------
      String Account::getLocationID() const
      {
        if (!mStackAccount.get()) return String();

        ILocationPtr self(ILocation::getForLocal(mStackAccount.get()));
        if (!self) {
          ZS_LOG_WARNING(Detail, debug("location ID is not available yet"))
          return String();
        }

        ZS_LOG_DEBUG(log("getting location") + ILocation::toDebug(self))
        return self->getLocationID();
      }

      //-----------------------------------------------------------------------
      void Account::shutdown()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(debug("shutdown called"))
        cancel();
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::savePeerFilePrivate() const
      {
        // look ma - no lock

        if (!mLockboxSession.get()) return ElementPtr();

        IPeerFilesPtr peerFiles = mLockboxSession.get()->getPeerFiles();
        if (!peerFiles) {
          ZS_LOG_WARNING(Detail, debug("peer files are not available"))
          return ElementPtr();
        }

        return peerFiles->saveToPrivatePeerElement();
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr Account::getPeerFilePrivateSecret() const
      {
        // look ma - no lock

        if (!mLockboxSession.get()) return SecureByteBlockPtr();

        IPeerFilesPtr peerFiles = mLockboxSession.get()->getPeerFiles();
        if (!peerFiles) {
          ZS_LOG_WARNING(Detail, debug("peer files are not available"))
          return SecureByteBlockPtr();
        }

        IPeerFilePrivatePtr peerFilePrivate = peerFiles->getPeerFilePrivate();
        return peerFilePrivate->getPassword();
      }

      //-----------------------------------------------------------------------
      IdentityListPtr Account::getAssociatedIdentities() const
      {
        AutoRecursiveLock lock(*this);

        IdentityListPtr result(new IdentityList);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, debug("cannot get identities during shutdown"))
          return result;
        }

        ServiceIdentitySessionListPtr identities = mLockboxSession.get()->getAssociatedIdentities();
        ZS_THROW_BAD_STATE_IF(!identities)

        for (ServiceIdentitySessionList::iterator iter = identities->begin(); iter != identities->end(); ++iter)
        {
          IServiceIdentitySessionPtr session = (*iter);
          IdentityMap::const_iterator found = mIdentities.find(session->getID());
          if (found != mIdentities.end()) {
            UseIdentityPtr identity = (*found).second;
            ZS_LOG_DEBUG(log("found existing identity") + UseIdentity::toDebug(identity))
            result->push_back(Identity::convert(identity));
            continue;
          }

          UseIdentityPtr identity = IIdentityForAccount::createFromExistingSession(session);
          ZS_LOG_DEBUG(log("new identity found") + UseIdentity::toDebug(identity))
          mIdentities[identity->getSession()->getID()] = identity;
          result->push_back(Identity::convert(identity));
        }

        ZS_LOG_DEBUG(log("get associated identities complete") + ZS_PARAM("total", result->size()))

        return result;
      }

      //-----------------------------------------------------------------------
      void Account::removeIdentities(const IdentityList &identitiesToRemove)
      {
        AutoRecursiveLock lock(*this);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, debug("cannot associate identities during shutdown"))
          return;
        }

        if (!mLockboxSession.get()) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return;
        }

        ServiceIdentitySessionList add;
        ServiceIdentitySessionList remove;

        for (IdentityList::const_iterator iter = identitiesToRemove.begin(); iter != identitiesToRemove.end(); ++iter)
        {
          UseIdentityPtr identity = Identity::convert(*iter);
          mIdentities[identity->getSession()->getID()] = identity;
          remove.push_back(identity->getSession());
        }

        mLockboxSession.get()->associateIdentities(add, remove);
      }

      //-----------------------------------------------------------------------
      String Account::getInnerBrowserWindowFrameURL() const
      {
        ZS_THROW_BAD_STATE_IF(!mGrantSession)
        return mGrantSession->getInnerBrowserWindowFrameURL();
      }

      //-----------------------------------------------------------------------
      String Account::getBrowserWindowRedirectURL() const
      {
        ZS_THROW_BAD_STATE_IF(!mGrantSession)
        return mGrantSession->getBrowserWindowRedirectURL();
      }

      //-----------------------------------------------------------------------
      void Account::notifyBrowserWindowVisible()
      {
        ZS_THROW_BAD_STATE_IF(!mGrantSession)
        mGrantSession->notifyBrowserWindowVisible();
      }

      //-----------------------------------------------------------------------
      void Account::notifyBrowserWindowRedirected()
      {
        ZS_THROW_BAD_STATE_IF(!mGrantSession)
        mGrantSession->notifyBrowserWindowRedirected();
      }

      //-----------------------------------------------------------------------
      void Account::notifyBrowserWindowClosed()
      {
        ZS_THROW_BAD_STATE_IF(!mGrantSession)
        mGrantSession->notifyBrowserWindowClosed();
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::getNextMessageForInnerBrowerWindowFrame()
      {
        AutoRecursiveLock lock(*this);
        ZS_THROW_BAD_STATE_IF(!mGrantSession)

        DocumentPtr doc = mGrantSession->getNextMessageForInnerBrowerWindowFrame();
        if (!doc) {
          ZS_LOG_WARNING(Detail, log("lockbox has no message pending for inner browser window frame"))
          return ElementPtr();
        }
        if (mTotalPendingMessages > 0) {
          --mTotalPendingMessages;
        }
        ElementPtr root = doc->getFirstChildElement();
        ZS_THROW_BAD_STATE_IF(!root)
        root->orphan();
        return root;
      }

      //-----------------------------------------------------------------------
      void Account::handleMessageFromInnerBrowserWindowFrame(ElementPtr unparsedMessage)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!unparsedMessage)

        ZS_THROW_BAD_STATE_IF(!mGrantSession)

        DocumentPtr doc = Document::create();
        doc->adoptAsLastChild(unparsedMessage);
        mGrantSession->handleMessageFromInnerBrowserWindowFrame(doc);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForCall
      #pragma mark

      //-----------------------------------------------------------------------
      CallTransportPtr Account::getCallTransport() const
      {
        // look ma - no lock
        return CallTransport::convert(mCallTransport.get());
      }

      //-----------------------------------------------------------------------
      ICallDelegatePtr Account::getCallDelegate() const
      {
        // look ma - no lock
        return mCallDelegate;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForContact
      #pragma mark

      //-----------------------------------------------------------------------
      ContactPtr Account::findContact(const char *peerURI) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!peerURI)

        AutoRecursiveLock lock(*this);
        ContactMap::const_iterator found = mContacts.find(peerURI);
        if (found == mContacts.end()) {
          ZS_LOG_DEBUG(log("contact was not found for peer URI") + ZS_PARAM("uri", peerURI))
          return ContactPtr();
        }
        const UseContactPtr &contact = (*found).second;
        return Contact::convert(contact);
      }

      //-----------------------------------------------------------------------
      void Account::notifyAboutContact(ContactPtr inContact)
      {
        UseContactPtr contact = inContact;

        ZS_THROW_INVALID_ARGUMENT_IF(!contact)

        AutoRecursiveLock lock(*this);
        String peerURI = contact->getPeerURI();
        mContacts[peerURI] = contact;
      }

      //-----------------------------------------------------------------------
      void Account::hintAboutContactLocation(
                                             ContactPtr inContact,
                                             const char *locationID
                                             )
      {
        UseContactPtr contact = inContact;

        ZS_THROW_INVALID_ARGUMENT_IF(!contact)
        ZS_THROW_INVALID_ARGUMENT_IF(!locationID)

        AutoRecursiveLock lock(*this);

        ContactSubscriptionMap::iterator found = mContactSubscriptions.find(contact->getPeerURI());
        if (found != mContactSubscriptions.end()) {
          ContactSubscriptionPtr contactSubscription = (*found).second;

          if ((contactSubscription->isShuttingDown()) ||
              (contactSubscription->isShutdown())) {
            // the contact subscription is dying, need to create a new one to replace the existing
            mContactSubscriptions.erase(found);
            found = mContactSubscriptions.end();
          }
        }

        if (found == mContactSubscriptions.end()) {
          // In this scenario we need to subscribe to this peer since we
          // do not have a connection established to this peer as of yet.
          ContactSubscriptionPtr contactSubscription = ContactSubscription::create(mThisWeak.lock(), contact);
          mContactSubscriptions[contact->getPeerURI()] = contactSubscription;
        }

        // We need to hint about the contact location to the stack just in case
        // the stack does not know about this location.
        if (mStackAccount.get()) {
          ILocationPtr location = ILocation::getForPeer(contact->getPeer(), locationID);
          ZS_THROW_BAD_STATE_IF(!location)
          location->hintNowAvailable();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      ContactPtr Account::getSelfContact() const
      {
        AutoRecursiveLock lock(*this);
        return Contact::convert(mSelfContact);
      }

      //-----------------------------------------------------------------------
      ILocationPtr Account::getSelfLocation() const
      {
        // look ma - no lock
        if (!mStackAccount.get()) return ILocationPtr();

        return ILocation::getForLocal(mStackAccount.get());
      }

      //-----------------------------------------------------------------------
      stack::IAccountPtr Account::getStackAccount() const
      {
        // look ma - no lock
        return mStackAccount.get();
      }

      //-----------------------------------------------------------------------
      IPublicationRepositoryPtr Account::getRepository() const
      {
        // look ma - no lock
        if (!mStackAccount.get()) return IPublicationRepositoryPtr();
        return IPublicationRepository::getFromAccount(mStackAccount.get());
      }

      //-----------------------------------------------------------------------
      IPeerFilesPtr Account::getPeerFiles() const
      {
        // look ma - no lock

        if (!mLockboxSession.get()) {
          ZS_LOG_WARNING(Detail, log("lockbox is not created yet thus peer files are not available yet"))
          return IPeerFilesPtr();
        }

        return mLockboxSession.get()->getPeerFiles();
      }

      //-----------------------------------------------------------------------
      IConversationThreadDelegatePtr Account::getConversationThreadDelegate() const
      {
        // look ma - no lock
        return mConversationThreadDelegate;
      }

      //-----------------------------------------------------------------------
      void Account::notifyConversationThreadCreated(
                                                    ConversationThreadPtr inThread,
                                                    bool notifyDelegate
                                                    )
      {
        UseConversationThreadPtr thread = inThread;

        ZS_THROW_INVALID_ARGUMENT_IF(!thread)
        AutoRecursiveLock lock(*this);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, debug("cannot remember new thread or notify about it during shutdown"))
          return;
        }

        mConversationThreads[thread->getThreadID()] = thread;

        if (!notifyDelegate) {
          ZS_LOG_DEBUG(log("no need to notifify delegate"))
          return;
        }

        try {
          mConversationThreadDelegate->onConversationThreadNew(ConversationThread::convert(thread));
        } catch (IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("could not notify of new conversation thread - conversation thread delegate is gone"))
        }
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr Account::getConversationThreadByID(const char *threadID) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!threadID)
        AutoRecursiveLock lock(*this);

        ConversationThreadMap::const_iterator found = mConversationThreads.find(threadID);
        if (found == mConversationThreads.end()) return ConversationThreadPtr();
        const UseConversationThreadPtr &thread = (*found).second;
        return ConversationThread::convert(thread);
      }

      //-----------------------------------------------------------------------
      void Account::getConversationThreads(ConversationThreadList &outConversationThreads) const
      {
        AutoRecursiveLock lock(*this);

        for (ConversationThreadMap::const_iterator iter = mConversationThreads.begin(); iter != mConversationThreads.end(); ++iter)
        {
          const UseConversationThreadPtr &thread = (*iter).second;
          outConversationThreads.push_back(ConversationThread::convert(thread));
        }
      }

      //-----------------------------------------------------------------------
      void Account::notifyConversationThreadStateChanged()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("notified conversation thread state changed"))

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForIdentity
      #pragma mark

      //-----------------------------------------------------------------------
      stack::IServiceNamespaceGrantSessionPtr Account::getNamespaceGrantSession() const
      {
        return mGrantSession;
      }

      //-----------------------------------------------------------------------
      stack::IServiceLockboxSessionPtr Account::getLockboxSession() const
      {
        return mLockboxSession.get();
      }

      //-----------------------------------------------------------------------
      void Account::associateIdentity(IdentityPtr inIdentity)
      {
        UseIdentityPtr identity = inIdentity;

        ZS_THROW_INVALID_ARGUMENT_IF(!identity)

        ZS_LOG_DEBUG(log("associating identity to account/lockbox"))

        AutoRecursiveLock lock(*this);

        mIdentities[identity->getSession()->getID()] = identity;

        if (!mLockboxSession.get()) {
          ZS_LOG_DEBUG(log("creating lockbox session"))
          mLockboxSession.set(IServiceLockboxSession::login(mThisWeak.lock(), mLockboxService, mGrantSession, identity->getSession(), mLockboxForceCreateNewAccount));
        } else {
          ZS_LOG_DEBUG(log("associating to existing lockbox session"))
          ServiceIdentitySessionList add;
          ServiceIdentitySessionList remove;

          add.push_back(identity->getSession());

          mLockboxSession.get()->associateIdentities(add, remove);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForIdentityLookup
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForPushMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      stack::IBootstrappedNetworkPtr Account::getLockboxBootstrapper() const
      {
        return mLockboxService->getBootstrappedNetwork();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => ICallTransportDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onCallTransportStateChanged(
                                                ICallTransportPtr inTransport,
                                                CallTransportStates state
                                                )
      {
        ZS_LOG_DEBUG(log("notified call transport state changed"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => stack::IAccountDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onAccountStateChanged(
                                          stack::IAccountPtr account,
                                          stack::IAccount::AccountStates state
                                          )
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IPeerSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(*this);
        if (subscription != mPeerSubscription) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete subscription"))
          return;
        }

        mPeerSubscription.reset();
        step();
      }

      //-----------------------------------------------------------------------
      void Account::onPeerSubscriptionFindStateChanged(
                                                       IPeerSubscriptionPtr subscription,
                                                       IPeerPtr peer,
                                                       PeerFindStates state
                                                       )
      {
        // IGNORED
      }

      //-----------------------------------------------------------------------
      void Account::onPeerSubscriptionLocationConnectionStateChanged(
                                                                     IPeerSubscriptionPtr subscription,
                                                                     ILocationPtr location,
                                                                     LocationConnectionStates state
                                                                     )
      {
        AutoRecursiveLock lock(*this);

        if (subscription != mPeerSubscription) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete subscription (thus ignoring)") + ZS_PARAM("subscription ID", subscription->getID()))
          return;
        }

        IPeerPtr peer = location->getPeer();
        if (!peer) {
          if (location->getLocationType() == ILocation::LocationType_Finder) {
            ZS_LOG_TRACE(log("notified about location finder location (thus ignoring)") + ILocation::toDebug(location))
            return;
          }
          ZS_LOG_WARNING(Detail, log("notified about location which is not a peer") + ILocation::toDebug(location))
          return;
        }

        String peerURI = peer->getPeerURI();

        ZS_LOG_TRACE(log("notified peer location state changed") + ZS_PARAM("state", ILocation::toString(state)) + ILocation::toDebug(location))

        // see if there is a local contact with this peer URI
        ContactMap::iterator foundContact = mContacts.find(peerURI);
        if (foundContact == mContacts.end()) {
          // did not find a contact with this peer URI - thus we need to create one
          IPeerFilePublicPtr peerFilePublic = peer->getPeerFilePublic();
          if (!peerFilePublic) {
            ZS_LOG_ERROR(Detail, log("no public peer file for location provided") + ILocation::toDebug(location))
            return;
          }

          // create and remember this contact for the future
          UseContactPtr contact = IContactForAccount::createFromPeer(mThisWeak.lock(), peer);

          // attempt find once more as contact might now be registered
          foundContact = mContacts.find(peerURI);
          ZS_THROW_BAD_STATE_IF(foundContact == mContacts.end())
        }

        UseContactPtr contact = (*foundContact).second;

        ContactSubscriptionMap::iterator foundContactSubscription = mContactSubscriptions.find(peerURI);
        ContactSubscriptionPtr contactSubscription;
        if (foundContactSubscription == mContactSubscriptions.end()) {
          switch (state) {
            case ILocation::LocationConnectionState_Pending:
            case ILocation::LocationConnectionState_Disconnecting:
            case ILocation::LocationConnectionState_Disconnected:   {
              ZS_LOG_DEBUG(log("no need to create contact subscription when the connection is not ready") + ILocation::toDebug(location))
              return;
            }
            case ILocation::LocationConnectionState_Connected: break;
          }

          ZS_LOG_DEBUG(log("creating a new contact subscription") + ILocation::toDebug(location))
          contactSubscription = ContactSubscription::create(mThisWeak.lock(), contact, location);
          mContactSubscriptions[peerURI] = contactSubscription;
        } else {
          contactSubscription = (*foundContactSubscription).second;
        }

        ZS_LOG_DEBUG(log("notifying contact subscription about state") + ILocation::toDebug(location))
        contactSubscription->notifyAboutLocationState(location, state);
      }

      //-----------------------------------------------------------------------
      void Account::onPeerSubscriptionMessageIncoming(
                                                      IPeerSubscriptionPtr subscription,
                                                      IMessageIncomingPtr message
                                                      )
      {
        // IGNORED
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IServiceLockboxSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onServiceLockboxSessionStateChanged(
                                                        IServiceLockboxSessionPtr session,
                                                        LockboxSessionStates state
                                                        )
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void Account::onServiceLockboxSessionAssociatedIdentitiesChanged(IServiceLockboxSessionPtr session)
      {
        AutoRecursiveLock lock(*this);

        if (session != mLockboxSession.get()) {
          ZS_LOG_WARNING(Detail, log("notified about unknown lockbox session"))
          return;
        }

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("notified of association change during shutdown"))
          return;
        }

        mAssociatedIdentitiesChanged = true;

        mSubscriptions.delegate()->onAccountAssociatedIdentitiesChanged(mThisWeak.lock());

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IServiceNamespaceGrantSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onServiceNamespaceGrantSessionStateChanged(
                                                               IServiceNamespaceGrantSessionPtr session,
                                                               GrantSessionStates state
                                                               )
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void Account::onServiceNamespaceGrantSessionPendingMessageForInnerBrowserWindowFrame(IServiceNamespaceGrantSessionPtr session)
      {
        AutoRecursiveLock lock(*this);

        ZS_THROW_UNEXPECTED_ERROR_IF(session != mGrantSession)

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("notified pending messages during shutdown"))
          return;
        }

        ++mTotalPendingMessages;

        mSubscriptions.delegate()->onAccountPendingMessageForInnerBrowserWindowFrame(mThisWeak.lock());

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IBackgroundingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onBackgroundingGoingToBackground(
                                                     IBackgroundingSubscriptionPtr subscription,
                                                     IBackgroundingNotifierPtr notifier
                                                     )
      {
        ZS_LOG_DEBUG(log("going to background"))
      }

      //-----------------------------------------------------------------------
      void Account::onBackgroundingGoingToBackgroundNow(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("going to background now"))
      }

      //-----------------------------------------------------------------------
      void Account::onBackgroundingReturningFromBackground(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("returning from background"))
      }

      //-----------------------------------------------------------------------
      void Account::onBackgroundingApplicationWillQuit(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("application will quit"))

        AutoRecursiveLock lock(*this);

        setError(IHTTP::HTTPStatusCode_ClientClosedRequest, "application is quitting");
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onWake()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("on wake"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => friend Account::ContactSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::notifyContactSubscriptionShutdown(const String &peerURI)
      {
        AutoRecursiveLock lock(*this);
        ContactSubscriptionMap::iterator found = mContactSubscriptions.find(peerURI);
        if (found == mContactSubscriptions.end()) return;

        mContactSubscriptions.erase(found);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => friend Account::LocationSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      UseConversationThreadPtr Account::notifyPublicationUpdated(
                                                                 ILocationPtr peerLocation,
                                                                 IPublicationMetaDataPtr metaData,
                                                                 const SplitMap &split
                                                                 )
      {
        if (isShutdown()) {
          ZS_LOG_WARNING(Debug, log("received updated publication document after account was shutdown thus ignoring"))
          return ConversationThreadPtr();
        }

        String baseThreadID = UseServicesHelper::get(split, OPENPEER_CONVERSATION_THREAD_BASE_THREAD_ID_INDEX);
        String hostThreadID = UseServicesHelper::get(split, OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX);
        if ((baseThreadID.size() < 1) ||
            (hostThreadID.size() < 1)) {
          ZS_LOG_WARNING(Debug, log("converation thread publication did not have a thread ID") + IPublicationMetaData::toDebug(metaData))
          return ConversationThreadPtr();
        }

        ConversationThreadMap::iterator found = mConversationThreads.find(baseThreadID);
        if (found != mConversationThreads.end()) {
          ZS_LOG_DEBUG(log("notify publication updated for existing thread") + ZS_PARAM("thread ID", baseThreadID) + IPublicationMetaData::toDebug(metaData))
          UseConversationThreadPtr thread = (*found).second;
          thread->notifyPublicationUpdated(peerLocation, metaData, split);
          return thread;
        }

        ZS_LOG_DEBUG(log("notify publication for new thread") + ZS_PARAM("thread ID", baseThreadID) + IPublicationMetaData::toDebug(metaData))
        UseConversationThreadPtr thread = IConversationThreadForAccount::create(mThisWeak.lock(), peerLocation, metaData, split);
        if (!thread) {
          ZS_LOG_WARNING(Debug, log("notify publication for new thread aborted"))
          return UseConversationThreadPtr();
        }

        return thread;
      }

      //-----------------------------------------------------------------------
      void Account::notifyPublicationGone(
                                          ILocationPtr peerLocation,
                                          IPublicationMetaDataPtr metaData,
                                          const SplitMap &split
                                          )
      {
        String baseThreadID = UseServicesHelper::get(split, OPENPEER_CONVERSATION_THREAD_BASE_THREAD_ID_INDEX);
        String hostThreadID = UseServicesHelper::get(split, OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX);
        if ((baseThreadID.size() < 1) ||
            (hostThreadID.size() < 1)) {
          ZS_LOG_WARNING(Debug, log("converation thread publication did not have a thread ID") + IPublicationMetaData::toDebug(metaData))
          return;
        }

        ConversationThreadMap::iterator found = mConversationThreads.find(baseThreadID);
        if (found == mConversationThreads.end()) {
          ZS_LOG_WARNING(Debug, log("notify publication gone for thread that did not exist") + ZS_PARAM("thread ID", baseThreadID) + IPublicationMetaData::toDebug(metaData))
          return;
        }

        ZS_LOG_DEBUG(log("notify publication gone for existing thread") + ZS_PARAM("thread ID", baseThreadID) + IPublicationMetaData::toDebug(metaData))
        UseConversationThreadPtr thread = (*found).second;
        thread->notifyPublicationGone(peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => internal
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Account::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Account");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params Account::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::Account");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "state", toString(mCurrentState));
        UseServicesHelper::debugAppend(resultEl, "error code", mLastErrorCode);
        UseServicesHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        UseServicesHelper::debugAppend(resultEl, "delegate", mSubscriptions.size());

        UseServicesHelper::debugAppend(resultEl, "conversation thread delegate", (bool)mConversationThreadDelegate);
        UseServicesHelper::debugAppend(resultEl, "call delegate", (bool)mCallDelegate);

        UseServicesHelper::debugAppend(resultEl, "backgrounding subscription", mBackgroundingSubscription ? mBackgroundingSubscription->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, stack::IAccount::toDebug(mStackAccount.get()));

        UseServicesHelper::debugAppend(resultEl, stack::IServiceNamespaceGrantSession::toDebug(mGrantSession));

        UseServicesHelper::debugAppend(resultEl, stack::IServiceLockboxSession::toDebug(mLockboxSession.get()));
        UseServicesHelper::debugAppend(resultEl, "force new lockbox account", mLockboxForceCreateNewAccount ? String("true") : String());
        UseServicesHelper::debugAppend(resultEl, "identities", mIdentities.size());

        UseServicesHelper::debugAppend(resultEl, stack::IPeerSubscription::toDebug(mPeerSubscription));

        UseServicesHelper::debugAppend(resultEl, UseContact::toDebug(mSelfContact));

        UseServicesHelper::debugAppend(resultEl, "contacts", mContacts.size());
        UseServicesHelper::debugAppend(resultEl, "contact subscription", mContactSubscriptions.size());

        UseServicesHelper::debugAppend(resultEl, "conversations", mConversationThreads.size());

        UseServicesHelper::debugAppend(resultEl, "call transport", (bool)mCallTransport.get());

        UseServicesHelper::debugAppend(resultEl, "subscribers permission document", (bool)mSubscribersPermissionDocument);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void Account::cancel()
      {
        AutoRecursiveLock lock(*this);  // just in case

        ZS_LOG_DEBUG(debug("cancel called"))

        if (isShutdown()) return;
        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(AccountState_ShuttingDown);

        if (mCallTransport.get()) {
          ZS_LOG_DEBUG(log("shutting down call transport"))
          mCallTransport.get()->shutdown();
        }

        if (mStackAccount.get()) {
          ZS_LOG_DEBUG(log("shutting down stack account"))
          mStackAccount.get()->shutdown();  // do not reset
        }

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
          mPeerSubscription.reset();
        }

        for (ConversationThreadMap::iterator iter_doNotUse = mConversationThreads.begin(); iter_doNotUse != mConversationThreads.end(); )
        {
          ConversationThreadMap::iterator current = iter_doNotUse; ++iter_doNotUse;
          UseConversationThreadPtr thread = (*current).second;
          thread->shutdown();
        }

        if (mGracefulShutdownReference) {

          for (ConversationThreadMap::iterator iter_doNotUse = mConversationThreads.begin(); iter_doNotUse != mConversationThreads.end(); )
          {
            ConversationThreadMap::iterator current = iter_doNotUse; ++iter_doNotUse;
            const BaseThreadID &threadI = (*current).first;
            UseConversationThreadPtr thread = (*current).second;

            if (!thread->isShutdown()) {
              ZS_LOG_DEBUG(log("waiting for conversation thread to shutdown") + ZS_PARAM("base thread id", threadI))
              return;
            }
          }

          if (mStackAccount.get()) {
            if (stack::IAccount::AccountState_Shutdown != mStackAccount.get()->getState()) {
              ZS_LOG_DEBUG(log("waiting for stack account to shutdown"))
              return;
            }
          }

          if (mCallTransport.get()) {
            if (ICallTransport::CallTransportState_Shutdown != mCallTransport.get()->getState()) {
              ZS_LOG_DEBUG(log("waiting for call transport to shutdown"))
              return;
            }
          }
        }

        setState(AccountState_Shutdown);

        if (mBackgroundingSubscription) {
          mBackgroundingSubscription->cancel();
          mBackgroundingSubscription.reset();
        }

        if (mGrantSession) {
          mGrantSession->cancel();    // do not reset
        }

        if (mLockboxSession.get()) {
          mLockboxSession.get()->cancel();  // do not reset
        }

        mGracefulShutdownReference.reset();

        mSubscriptions.clear();

        mConversationThreads.clear();

        ZS_LOG_DEBUG(log("shutdown complete"))
      }

      //-----------------------------------------------------------------------
      void Account::step()
      {
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("step forwarding to cancel"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        if (!stepLoginIdentityAssociated()) return;
        if (!stepLockboxShutdownCheck()) return;
        if (!stepStackAccountCreation()) return;
        if (!stepGrantSession()) return;
        if (!stepLockboxSession()) return;
        if (!stepStackAccount()) return;
        if (!stepSelfContact()) return;
        if (!stepCallTransportSetup()) return;
        if (!stepSubscribersPermissionDocument()) return;
        if (!stepPeerSubscription()) return;
        if (!stepCallTransportFinalize()) return;

        setState(AccountState_Ready);

        ZS_LOG_TRACE(debug("step complete"))
      }

      //-----------------------------------------------------------------------
      bool Account::stepLoginIdentityAssociated()
      {
        if (mLockboxSession.get()) {
          ZS_LOG_DEBUG(log("lockbox is already created thus login identity associate is not needed"))
          return true;
        }

        ZS_LOG_DEBUG(log("waiting for account to be associated to an identity"))

        setState(AccountState_WaitingForAssociationToIdentity);
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepLockboxShutdownCheck()
      {
        WORD errorCode = 0;
        String reason;

        IServiceLockboxSession::SessionStates state = mLockboxSession.get()->getState(&errorCode, &reason);
        if (IServiceLockboxSession::SessionState_Shutdown == state) {
          ZS_LOG_ERROR(Detail, log("lockbox session shutdown"))
          setError(errorCode, reason);
          cancel();
          return false;
        }

        ZS_LOG_TRACE(log("lockbox is not shutdown thus allowing to continue"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepGrantSession()
      {
        WORD errorCode = 0;
        String reason;

        IServiceNamespaceGrantSession::SessionStates state = mGrantSession->getState(&errorCode, &reason);

        switch (state) {
          case IServiceNamespaceGrantSession::SessionState_Pending:
          {
            ZS_LOG_TRACE(log("namespace grant session is pending"))
            setState(AccountState_Pending);
            return false;
          }
          case IServiceNamespaceGrantSession::SessionState_WaitingForBrowserWindowToBeLoaded:
          {
            ZS_LOG_TRACE(log("namespace grant is waiting for the browser window to be loaded"))
            setState(AccountState_WaitingForBrowserWindowToBeLoaded);
            return false;
          }
          case IServiceNamespaceGrantSession::SessionState_WaitingForBrowserWindowToBeMadeVisible:
          {
            ZS_LOG_TRACE(log("namespace grant is waiting for browser window to be made visible"))
            setState(AccountState_WaitingForBrowserWindowToBeMadeVisible);
            return false;
          }
          case IServiceNamespaceGrantSession::SessionState_WaitingForBrowserWindowToBeRedirected:
          {
            ZS_LOG_TRACE(log("namespace grant is waiting for browser window to close"))
            setState(AccountState_WaitingForBrowserWindowToBeRedirected);
            return false;
          }
          case IServiceNamespaceGrantSession::SessionState_WaitingForBrowserWindowToClose:
          {
            ZS_LOG_TRACE(log("namespace grant is waiting for browser window to close"))
            setState(AccountState_WaitingForBrowserWindowToClose);
            return false;
          }
          case IServiceNamespaceGrantSession::SessionState_Ready: {
            ZS_LOG_TRACE(log("namespace grant is ready"))
            return true;
          }
          case IServiceNamespaceGrantSession::SessionState_Shutdown:  {
            ZS_LOG_ERROR(Detail, log("namespace grant is session shutdown"))
            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        ZS_LOG_DEBUG(log("waiting for lockbox session to be ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepStackAccountCreation()
      {
        ZS_THROW_BAD_STATE_IF(!mLockboxSession.get())

        if (mStackAccount.get()) {
          ZS_LOG_TRACE(log("stack account already created"))
          return true;
        }

        ZS_LOG_DEBUG(log("creating stack account"))
        mStackAccount.set(stack::IAccount::create(mThisWeak.lock(), mLockboxSession.get()));
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepLockboxSession()
      {
        ZS_THROW_BAD_STATE_IF(!mLockboxSession.get())

        WORD errorCode = 0;
        String reason;

        IServiceLockboxSession::SessionStates state = mLockboxSession.get()->getState(&errorCode, &reason);

        switch (state) {
          case IServiceLockboxSession::SessionState_Pending:
          {
            ZS_LOG_DEBUG(log("lockbox is pending"))
            setState(AccountState_Pending);
            return false;
          }
          case IServiceLockboxSession::SessionState_PendingPeerFilesGeneration:
          {
            ZS_LOG_DEBUG(log("lockbox is pending and generating peer files"))
            setState(AccountState_PendingPeerFilesGeneration);
            return false;
          }
          case IServiceLockboxSession::SessionState_Ready: {
            ZS_LOG_DEBUG(log("lockbox session is ready"))
            return true;
          }
          case IServiceLockboxSession::SessionState_Shutdown:  {
            ZS_LOG_ERROR(Detail, log("lockbox session shutdown"))
            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        ZS_LOG_DEBUG(log("waiting for lockbox session to be ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepStackAccount()
      {
        ZS_THROW_BAD_STATE_IF(!mLockboxSession.get())
        ZS_THROW_BAD_STATE_IF(!mStackAccount.get())

        WORD errorCode = 0;
        String reason;

        stack::IAccount::AccountStates state = mStackAccount.get()->getState(&errorCode, &reason);

        if (stack::IAccount::AccountState_Ready == state) {
          ZS_LOG_DEBUG(log("step peer contact completed"))
          return true;
        }

        if ((stack::IAccount::AccountState_ShuttingDown == state) ||
            (stack::IAccount::AccountState_Shutdown == state)) {
          ZS_LOG_ERROR(Detail, log("peer contact session shutdown"))
          setError(errorCode, reason);
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("waiting for stack account session to be ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepSelfContact()
      {
        if (mSelfContact) {
          ZS_LOG_DEBUG(log("contact self ready"))
          return true;
        }

        ILocationPtr selfLocation = ILocation::getForLocal(mStackAccount.get());
        if (!selfLocation) {
          ZS_LOG_ERROR(Detail, log("could not obtain self location"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Could not obtain location for self");
          cancel();
          return false;
        }

        mSelfContact = IContactForAccount::createFromPeer(mThisWeak.lock(), selfLocation->getPeer());
        ZS_THROW_BAD_STATE_IF(!mSelfContact)
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepCallTransportSetup()
      {
        if (mCallTransport.get()) {
          ICallTransportForAccount::CallTransportStates state = mCallTransport.get()->getState();
          if ((ICallTransport::CallTransportState_ShuttingDown == state) ||
              (ICallTransport::CallTransportState_Shutdown == state)){
            ZS_LOG_ERROR(Detail, log("premature shutdown of transport object (something is wrong)"))
            setError(IHTTP::HTTPStatusCode_InternalServerError, "Call transport shutdown unexpectedly");
            cancel();
            return false;
          }

          ZS_LOG_DEBUG(log("call transport ready"))
          return true;
        }

        IICESocket::TURNServerInfoList turnServers;
        IICESocket::STUNServerInfoList stunServers;
        mStackAccount.get()->getNATServers(turnServers, stunServers);

        mCallTransport.set(ICallTransportForAccount::create(mThisWeak.lock(), turnServers, stunServers));

        if (!mCallTransport.get()) {
          ZS_LOG_ERROR(Detail, log("failed to create call transport object thus shutting down"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Call transport failed to create");
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("call transport is setup"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepSubscribersPermissionDocument()
      {
        if (mSubscribersPermissionDocument) {
          ZS_LOG_DEBUG(log("permission document ready"))
          return true;
        }

        IPublicationRepositoryPtr repository = getRepository();
        if (!repository) {
          ZS_LOG_ERROR(Detail, log("repository on stack account is not valid thus account must shutdown"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Repository object is missing");
          cancel();
          return false;
        }

        IPublication::RelationshipList relationships;
        relationships.push_back(mSelfContact->getPeerURI());

        ILocationPtr selfLocation = ILocation::getForLocal(mStackAccount.get());

        stack::IPublicationMetaData::PublishToRelationshipsMap empty;
        mSubscribersPermissionDocument = stack::IPublication::create(selfLocation, "/threads/1.0/subscribers/permissions", "text/x-json-openpeer-permissions", relationships, empty, selfLocation);
        if (!mSubscribersPermissionDocument) {
          ZS_LOG_ERROR(Detail, log("unable to create subscription permission document thus shutting down"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to create subscribers document");
          cancel();
          return false;
        }

        IPublicationPublisherPtr publisher = repository->publish(IPublicationPublisherDelegateProxy::createNoop(getAssociatedMessageQueue()), mSubscribersPermissionDocument);
        if (!publisher->isComplete()) {
          ZS_LOG_ERROR(Detail, log("unable to publish local subscription permission document which should have happened instantly"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to publish document to self");
          cancel();
          return false;
        }
        ZS_LOG_DEBUG(log("subscribers permission document created"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepPeerSubscription()
      {
        if (mPeerSubscription) {
          ZS_LOG_DEBUG(log("peer subscription ready"))
          return true;
        }

        mPeerSubscription = IPeerSubscription::subscribeAll(mStackAccount.get(), mThisWeak.lock());

        if (!mPeerSubscription) {
          ZS_LOG_ERROR(Detail, log("unable to create a subscription to all connections"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to create peer subscription");
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("peer subscription created"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepCallTransportFinalize()
      {
        if (ICallTransport::CallTransportState_Ready == mCallTransport.get()->getState()) {
          ZS_LOG_DEBUG(log("call transport is finalized"))
          return true;
        }
        ZS_LOG_DEBUG(log("waiting on call transport to be ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      void Account::setState(IAccount::AccountStates state)
      {
        if (mCurrentState == state) return;

        ZS_LOG_BASIC(debug("state changed") + ZS_PARAM("new state", toString(state)))
        mCurrentState = state;

        AccountPtr pThis = mThisWeak.lock();

        if (pThis) {
          mSubscriptions.delegate()->onAccountStateChanged(pThis, state);
        }
      }

      //-----------------------------------------------------------------------
      void Account::setError(
                             WORD errorCode,
                             const char *inReason
                             )
      {
        if (0 == errorCode) {
          ZS_LOG_DEBUG(log("no error specified"))
          return;
        }

        String reason(inReason ? String(inReason) : String());
        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if (0 != mLastErrorCode) {
          ZS_LOG_WARNING(Detail, debug("error was already set (thus ignoring)") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        mLastErrorCode = errorCode;
        mLastErrorReason = reason;
        ZS_LOG_ERROR(Detail, debug("account error"))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IAccountFactory &IAccountFactory::singleton()
      {
        return AccountFactory::singleton();
      }

      //-----------------------------------------------------------------------
      AccountPtr IAccountFactory::login(
                                        IAccountDelegatePtr delegate,
                                        IConversationThreadDelegatePtr conversationThreadDelegate,
                                        ICallDelegatePtr callDelegate,
                                        const char *namespaceGrantOuterFrameURLUponReload,
                                        const char *grantID,
                                        const char *lockboxServiceDomain,
                                        bool forceCreateNewLockboxAccount
                                        )
      {
        if (this) {}
        return Account::login(delegate, conversationThreadDelegate, callDelegate, namespaceGrantOuterFrameURLUponReload, grantID, lockboxServiceDomain, forceCreateNewLockboxAccount);
      }

      //-----------------------------------------------------------------------
      AccountPtr IAccountFactory::relogin(
                                          IAccountDelegatePtr delegate,
                                          IConversationThreadDelegatePtr conversationThreadDelegate,
                                          ICallDelegatePtr callDelegate,
                                          const char *namespaceGrantOuterFrameURLUponReload,
                                          ElementPtr reloginInformation
                                          )
      {
        if (this) {}
        return Account::relogin(delegate, conversationThreadDelegate, callDelegate, namespaceGrantOuterFrameURLUponReload, reloginInformation);
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IAccount
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IAccount::toString(AccountStates state)
    {
      switch (state) {
        case AccountState_Pending:                                return "Pending";
        case AccountState_PendingPeerFilesGeneration:             return "Pending Peer File Generation";
        case AccountState_WaitingForAssociationToIdentity:        return "Waiting for Association to Identity";
        case AccountState_WaitingForBrowserWindowToBeLoaded:      return "Waiting for Browser Window to be Loaded";
        case AccountState_WaitingForBrowserWindowToBeMadeVisible: return "Waiting for Browser Window to be made Visible";
        case AccountState_WaitingForBrowserWindowToBeRedirected:  return "Waiting for Browser Window to be Redirected";
        case AccountState_WaitingForBrowserWindowToClose:         return "Waiting for Browser Window to Close";
        case AccountState_Ready:                                  return "Ready";
        case AccountState_ShuttingDown:                           return "Shutting down";
        case AccountState_Shutdown:                               return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    ElementPtr IAccount::toDebug(IAccountPtr account)
    {
      return internal::Account::toDebug(account);
    }

    //-------------------------------------------------------------------------
    IAccountPtr IAccount::login(
                                IAccountDelegatePtr delegate,
                                IConversationThreadDelegatePtr conversationThreadDelegate,
                                ICallDelegatePtr callDelegate,
                                const char *namespaceGrantOuterFrameURLUponReload,
                                const char *grantID,
                                const char *lockboxServiceDomain,
                                bool forceCreateNewLockboxAccount
                                )
    {
      return internal::IAccountFactory::singleton().login(delegate, conversationThreadDelegate, callDelegate, namespaceGrantOuterFrameURLUponReload, grantID, lockboxServiceDomain, forceCreateNewLockboxAccount);
    }

    //-------------------------------------------------------------------------
    IAccountPtr IAccount::relogin(
                                  IAccountDelegatePtr delegate,
                                  IConversationThreadDelegatePtr conversationThreadDelegate,
                                  ICallDelegatePtr callDelegate,
                                  const char *namespaceGrantOuterFrameURLUponReload,
                                  ElementPtr reloginInformation
                                  )
    {
      return internal::IAccountFactory::singleton().relogin(delegate, conversationThreadDelegate, callDelegate, namespaceGrantOuterFrameURLUponReload, reloginInformation);
    }
  }
}
