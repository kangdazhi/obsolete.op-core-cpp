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

#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Identity.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_Helper.h>
#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IServiceLockbox.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPublication.h>
#include <openpeer/stack/IPublicationRepository.h>
#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IHTTP.h>

#include <zsLib/Stringize.h>
#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#define OPENPEER_PEER_SUBSCRIPTION_AUTO_CLOSE_TIMEOUT_IN_SECONDS (60*3)

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForAccount, UseConversationThread)

      typedef IStackForInternal UseStack;

      using services::IHelper;

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
        mID(zsLib::createPUID()),
        mDelegate(IAccountDelegateProxy::createWeak(UseStack::queueApplication(), delegate)),
        mConversationThreadDelegate(IConversationThreadDelegateProxy::createWeak(UseStack::queueApplication(), conversationThreadDelegate)),
        mCallDelegate(ICallDelegateProxy::createWeak(UseStack::queueApplication(), callDelegate)),
        mCurrentState(AccountState_Pending),
        mLastErrorCode(0),
        mLockboxForceCreateNewAccount(false)
      {
        ZS_LOG_BASIC(log("created"))
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
        AutoRecursiveLock lock(mLock);
        step();
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(IAccountPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForCallPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForContactPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForConversationThreadPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForIdentityPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForIdentityLookupPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
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

        String lockboxDomain;
        String accountID;
        String grantID;
        SecureByteBlockPtr lockboxKey;

        try {
          lockboxDomain = reloginInformation->findFirstChildElementChecked("lockboxDomain")->getTextDecoded();
          accountID = reloginInformation->findFirstChildElementChecked("accountID")->getTextDecoded();
          grantID = reloginInformation->findFirstChildElementChecked("grantID")->getTextDecoded();
          lockboxKey = IHelper::convertFromBase64(reloginInformation->findFirstChildElementChecked("lockboxKey")->getTextDecoded());
        } catch (CheckFailed &) {
          return AccountPtr();
        }

        IBootstrappedNetworkPtr lockboxNetwork = IBootstrappedNetwork::prepare(lockboxDomain);
        if (!lockboxNetwork) {
          ZS_LOG_ERROR(Detail, pThis->log("failed to prepare bootstrapped network for domain") + ZS_PARAM("domain", lockboxDomain))
          return AccountPtr();
        }

        if (IHelper::isEmpty(lockboxKey)) {
          ZS_LOG_ERROR(Detail, pThis->log("lockbox key specified in relogin information is not valid"))
          return AccountPtr();
        }

        pThis->mGrantSession = IServiceNamespaceGrantSession::create(pThis, namespaceGrantOuterFrameURLUponReload, grantID);
        ZS_THROW_BAD_STATE_IF(!pThis->mGrantSession)

        pThis->mLockboxService = IServiceLockbox::createServiceLockboxFrom(lockboxNetwork);
        ZS_THROW_BAD_STATE_IF(!pThis->mLockboxService)

        pThis->mLockboxForceCreateNewAccount = false;

        pThis->mLockboxSession = IServiceLockboxSession::relogin(pThis, pThis->mLockboxService, pThis->mGrantSession, accountID, *lockboxKey);
        pThis->init();

        if (!pThis->mLockboxSession) {
          ZS_LOG_ERROR(Detail, pThis->log("failed to create lockbox session from relogin information"))
          return AccountPtr();
        }
        return pThis;
      }

      //-----------------------------------------------------------------------
      IAccount::AccountStates Account::getState(
                                                WORD *outErrorCode,
                                                String *outErrorReason
                                                ) const
      {
        AutoRecursiveLock lock(getLock());

        if (outErrorCode) *outErrorCode = mLastErrorCode;
        if (outErrorReason) *outErrorReason = mLastErrorReason;

        ZS_LOG_DEBUG(debug("getting account state"))

        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::getReloginInformation() const
      {
        AutoRecursiveLock lock(getLock());

        if (!mLockboxService) {
          ZS_LOG_WARNING(Detail, log("missing lockbox domain information"))
          return ElementPtr();
        }

        String lockboxDomain = mLockboxService->getBootstrappedNetwork()->getDomain();
        if (lockboxDomain.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("missing lockbox domain information"))
          return ElementPtr();
        }

        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("missing namespace grant information"))
          return ElementPtr();
        }

        if (!mLockboxSession) {
          ZS_LOG_WARNING(Detail, log("missing lockbox session information"))
          return ElementPtr();
        }

        String accountID = mLockboxSession->getAccountID();
        if (accountID.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("missing account ID information"))
          return ElementPtr();
        }

        String grantID = mGrantSession->getGrantID();
        if (grantID.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("missing grant ID information"))
          return ElementPtr();
        }

        SecureByteBlockPtr lockboxKey = mLockboxSession->getLockboxKey();

        if (!lockboxKey) {
          ZS_LOG_WARNING(Detail, log("missing lockbox key information"))
          return ElementPtr();
        }

        ElementPtr reloginEl = Element::create("relogin");

        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("lockboxDomain", lockboxDomain));
        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("accountID", accountID));
        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("grantID", grantID));
        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("lockboxKey", services::IHelper::convertToBase64(*lockboxKey)));

        return reloginEl;
      }

      //-----------------------------------------------------------------------
      String Account::getStableID() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mLockboxSession) return String();
        return mLockboxSession->getStableID();
      }

      //-----------------------------------------------------------------------
      String Account::getLocationID() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mStackAccount) return String();

        ILocationPtr self(ILocation::getForLocal(mStackAccount));
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
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(debug("shutdown called"))
        cancel();
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::savePeerFilePrivate() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mLockboxSession) return ElementPtr();

        IPeerFilesPtr peerFiles = mLockboxSession->getPeerFiles();
        if (!peerFiles) {
          ZS_LOG_WARNING(Detail, debug("peer files are not available"))
          return ElementPtr();
        }

        return peerFiles->saveToPrivatePeerElement();
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr Account::getPeerFilePrivateSecret() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mLockboxSession) return SecureByteBlockPtr();

        IPeerFilesPtr peerFiles = mLockboxSession->getPeerFiles();
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
        AutoRecursiveLock lock(getLock());

        IdentityListPtr result(new IdentityList);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, debug("cannot get identities during shutdown"))
          return result;
        }

        ServiceIdentitySessionListPtr identities = mLockboxSession->getAssociatedIdentities();
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
        AutoRecursiveLock lock(getLock());

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, debug("cannot associate identities during shutdown"))
          return;
        }

        if (!mLockboxSession) {
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

        mLockboxSession->associateIdentities(add, remove);
      }

      //-----------------------------------------------------------------------
      String Account::getInnerBrowserWindowFrameURL() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return String();
        }
        return mGrantSession->getInnerBrowserWindowFrameURL();
      }

      //-----------------------------------------------------------------------
      void Account::notifyBrowserWindowVisible()
      {
        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return;
        }
        mGrantSession->notifyBrowserWindowVisible();
      }

      //-----------------------------------------------------------------------
      void Account::notifyBrowserWindowClosed()
      {
        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return;
        }
        mGrantSession->notifyBrowserWindowClosed();
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::getNextMessageForInnerBrowerWindowFrame()
      {
        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return ElementPtr();
        }
        DocumentPtr doc = mGrantSession->getNextMessageForInnerBrowerWindowFrame();
        if (!doc) {
          ZS_LOG_WARNING(Detail, log("lockbox has no message pending for inner browser window frame"))
          return ElementPtr();
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

        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return;
        }
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
        AutoRecursiveLock lock(mLock);
        return CallTransport::convert(mCallTransport);
      }

      //-----------------------------------------------------------------------
      ICallDelegatePtr Account::getCallDelegate() const
      {
        AutoRecursiveLock lock(mLock);
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

        AutoRecursiveLock lock(mLock);
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

        AutoRecursiveLock lock(mLock);
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

        AutoRecursiveLock lock(mLock);

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
        if (mStackAccount) {
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
        AutoRecursiveLock lock(mLock);
        return Contact::convert(mSelfContact);
      }

      //-----------------------------------------------------------------------
      ILocationPtr Account::getSelfLocation() const
      {
        AutoRecursiveLock lock(mLock);
        if (!mStackAccount) return ILocationPtr();

        return ILocation::getForLocal(mStackAccount);
      }

      //-----------------------------------------------------------------------
      stack::IAccountPtr Account::getStackAccount() const
      {
        AutoRecursiveLock lock(mLock);
        return mStackAccount;
      }

      //-----------------------------------------------------------------------
      IPublicationRepositoryPtr Account::getRepository() const
      {
        AutoRecursiveLock lock(mLock);
        if (!mStackAccount) return IPublicationRepositoryPtr();
        return IPublicationRepository::getFromAccount(mStackAccount);
      }

      //-----------------------------------------------------------------------
      IPeerFilesPtr Account::getPeerFiles() const
      {
        AutoRecursiveLock lock(mLock);
        if (!mLockboxSession) {
          ZS_LOG_WARNING(Detail, log("lockbox is not created yet thus peer files are not available yet"))
          return IPeerFilesPtr();
        }

        return mLockboxSession->getPeerFiles();
      }

      //-----------------------------------------------------------------------
      IConversationThreadDelegatePtr Account::getConversationThreadDelegate() const
      {
        AutoRecursiveLock lock(mLock);
        return mConversationThreadDelegate;
      }

      //-----------------------------------------------------------------------
      void Account::notifyConversationThreadCreated(ConversationThreadPtr inThread)
      {
        UseConversationThreadPtr thread = inThread;

        ZS_THROW_INVALID_ARGUMENT_IF(!thread)
        AutoRecursiveLock lock(mLock);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, debug("cannot remember new thread or notify about it during shutdown"))
          return;
        }

        mConversationThreads[thread->getThreadID()] = thread;

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
        AutoRecursiveLock lock(mLock);

        ConversationThreadMap::const_iterator found = mConversationThreads.find(threadID);
        if (found == mConversationThreads.end()) return ConversationThreadPtr();
        const UseConversationThreadPtr &thread = (*found).second;
        return ConversationThread::convert(thread);
      }

      //-----------------------------------------------------------------------
      void Account::getConversationThreads(ConversationThreadList &outConversationThreads) const
      {
        AutoRecursiveLock lock(mLock);

        for (ConversationThreadMap::const_iterator iter = mConversationThreads.begin(); iter != mConversationThreads.end(); ++iter)
        {
          const UseConversationThreadPtr &thread = (*iter).second;
          outConversationThreads.push_back(ConversationThread::convert(thread));
        }
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
        AutoRecursiveLock lock(mLock);
        return mGrantSession;
      }

      //-----------------------------------------------------------------------
      stack::IServiceLockboxSessionPtr Account::getLockboxSession() const
      {
        AutoRecursiveLock lock(mLock);
        return mLockboxSession;
      }

      //-----------------------------------------------------------------------
      void Account::associateIdentity(IdentityPtr inIdentity)
      {
        UseIdentityPtr identity = inIdentity;

        ZS_THROW_INVALID_ARGUMENT_IF(!identity)

        ZS_LOG_DEBUG(log("associating identity to account/lockbox"))

        AutoRecursiveLock lock(mLock);

        mIdentities[identity->getSession()->getID()] = identity;

        if (!mLockboxSession) {
          ZS_LOG_DEBUG(log("creating lockbox session"))
          mLockboxSession = IServiceLockboxSession::login(mThisWeak.lock(), mLockboxService, mGrantSession, identity->getSession(), mLockboxForceCreateNewAccount);
        } else {
          ZS_LOG_DEBUG(log("associating to existing lockbox session"))
          ServiceIdentitySessionList add;
          ServiceIdentitySessionList remove;

          add.push_back(identity->getSession());

          mLockboxSession->associateIdentities(add, remove);
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
      #pragma mark Account => ICallTransportDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onCallTransportStateChanged(
                                                ICallTransportPtr inTransport,
                                                CallTransportStates state
                                                )
      {
        ZS_LOG_DEBUG(log("notified call transport state changed"))

        AutoRecursiveLock lock(mLock);
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
        AutoRecursiveLock lock(mLock);
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
        AutoRecursiveLock lock(mLock);
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
        AutoRecursiveLock lock(mLock);

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
        AutoRecursiveLock lock(mLock);
        step();
      }

      //-----------------------------------------------------------------------
      void Account::onServiceLockboxSessionAssociatedIdentitiesChanged(IServiceLockboxSessionPtr session)
      {
        AutoRecursiveLock lock(mLock);

        if (session != mLockboxSession) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete peer contact session"))
          return;
        }

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("notified of association change during shutdown"))
          return;
        }

        ZS_THROW_BAD_STATE_IF(!mDelegate)

        try {
          mDelegate->onAccountAssociatedIdentitiesChanged(mThisWeak.lock());
        } catch(IAccountDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

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
        AutoRecursiveLock lock(mLock);
        step();
      }

      //-----------------------------------------------------------------------
      void Account::onServiceNamespaceGrantSessionPendingMessageForInnerBrowserWindowFrame(IServiceNamespaceGrantSessionPtr session)
      {
        AutoRecursiveLock lock(mLock);

        if (session != mGrantSession) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete namespace grant session"))
          return;
        }

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("notified pending messages during shutdown"))
          return;
        }

        ZS_THROW_BAD_STATE_IF(!mDelegate)

        try {
          mDelegate->onAccountPendingMessageForInnerBrowserWindowFrame(mThisWeak.lock());
        } catch(IAccountDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        step();
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
        AutoRecursiveLock lock(mLock);
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
        AutoRecursiveLock lock(mLock);
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

        String baseThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_BASE_THREAD_ID_INDEX);
        String hostThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX);
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
        String baseThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_BASE_THREAD_ID_INDEX);
        String hostThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX);
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
        IHelper::debugAppend(objectEl, "id", mID);
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
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("core::Account");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));
        IHelper::debugAppend(resultEl, "error code", mLastErrorCode);
        IHelper::debugAppend(resultEl, "error reason", mLastErrorReason);
        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);
        IHelper::debugAppend(resultEl, "conversation thread delegate", (bool)mConversationThreadDelegate);
        IHelper::debugAppend(resultEl, "call delegate", (bool)mCallDelegate);
        IHelper::debugAppend(resultEl, stack::IAccount::toDebug(mStackAccount));
        IHelper::debugAppend(resultEl, stack::IServiceNamespaceGrantSession::toDebug(mGrantSession));
        IHelper::debugAppend(resultEl, stack::IServiceLockboxSession::toDebug(mLockboxSession));
        IHelper::debugAppend(resultEl, "force new lockbox account", mLockboxForceCreateNewAccount ? String("true") : String());
        IHelper::debugAppend(resultEl, "identities", mIdentities.size());
        IHelper::debugAppend(resultEl, stack::IPeerSubscription::toDebug(mPeerSubscription));
        IHelper::debugAppend(resultEl, UseContact::toDebug(mSelfContact));
        IHelper::debugAppend(resultEl, "contacts", mContacts.size());
        IHelper::debugAppend(resultEl, "contact subscription", mContactSubscriptions.size());
        IHelper::debugAppend(resultEl, "conversations", mConversationThreads.size());
        IHelper::debugAppend(resultEl, "call transport", (bool)mCallTransport);
        IHelper::debugAppend(resultEl, "subscribers permission document", (bool)mSubscribersPermissionDocument);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void Account::cancel()
      {
        AutoRecursiveLock lock(mLock);  // just in case

        ZS_LOG_DEBUG(debug("cancel called"))

        if (isShutdown()) return;
        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(AccountState_ShuttingDown);

        if (mCallTransport) {
          ZS_LOG_DEBUG(log("shutting down call transport"))
          mCallTransport->shutdown();
        }

        if (mStackAccount) {
          ZS_LOG_DEBUG(log("shutting down stack account"))
          mStackAccount->shutdown();
        }

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
          mPeerSubscription.reset();
        }

        if (mGracefulShutdownReference) {
          if (mStackAccount) {
            if (stack::IAccount::AccountState_Shutdown != mStackAccount->getState()) {
              ZS_LOG_DEBUG(log("waiting for stack account to shutdown"))
              return;
            }
          }

          if (mCallTransport) {
            if (ICallTransport::CallTransportState_Shutdown != mCallTransport->getState()) {
              ZS_LOG_DEBUG(log("waiting for call transport to shutdown"))
              return;
            }
          }
        }

        setState(AccountState_Shutdown);

        if (mGrantSession) {
          mGrantSession->cancel();    // do not reset
        }

        if (mLockboxSession) {
          mLockboxSession->cancel();  // do not reset
        }

        mGracefulShutdownReference.reset();

        mDelegate.reset();
        mConversationThreadDelegate.reset();
        mCallDelegate.reset();

        mStackAccount.reset();
        mCallTransport.reset();

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
        if (mLockboxSession) {
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

        IServiceLockboxSession::SessionStates state = mLockboxSession->getState(&errorCode, &reason);
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
        ZS_THROW_BAD_STATE_IF(!mLockboxSession)

        if (mStackAccount) {
          ZS_LOG_TRACE(log("stack account already created"))
          return true;
        }

        ZS_LOG_DEBUG(log("creating stack account"))
        mStackAccount = stack::IAccount::create(mThisWeak.lock(), mLockboxSession);
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepLockboxSession()
      {
        ZS_THROW_BAD_STATE_IF(!mLockboxSession)

        WORD errorCode = 0;
        String reason;

        IServiceLockboxSession::SessionStates state = mLockboxSession->getState(&errorCode, &reason);

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
        ZS_THROW_BAD_STATE_IF(!mLockboxSession)
        ZS_THROW_BAD_STATE_IF(!mStackAccount)

        WORD errorCode = 0;
        String reason;

        stack::IAccount::AccountStates state = mStackAccount->getState(&errorCode, &reason);

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

        ILocationPtr selfLocation = ILocation::getForLocal(mStackAccount);
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
        if (mCallTransport) {
          ICallTransportForAccount::CallTransportStates state = mCallTransport->getState();
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
        mStackAccount->getNATServers(turnServers, stunServers);

        mCallTransport = ICallTransportForAccount::create(mThisWeak.lock(), turnServers, stunServers);

        if (!mCallTransport) {
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

        ILocationPtr selfLocation = ILocation::getForLocal(mStackAccount);

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

        mPeerSubscription = IPeerSubscription::subscribeAll(mStackAccount, mThisWeak.lock());

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
        if (ICallTransport::CallTransportState_Ready == mCallTransport->getState()) {
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
          try {
            mDelegate->onAccountStateChanged(mThisWeak.lock(), state);
          } catch (IAccountDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
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
      #pragma mark Account => ContactSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      const char *Account::ContactSubscription::toString(Account::ContactSubscription::ContactSubscriptionStates state)
      {
        switch (state) {
          case ContactSubscriptionState_Pending:      return "Pending";
          case ContactSubscriptionState_Ready:        return "Ready";
          case ContactSubscriptionState_ShuttingDown: return "Shutting down";
          case ContactSubscriptionState_Shutdown:     return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      Account::ContactSubscription::ContactSubscription(
                                                        AccountPtr outer,
                                                        UseContactPtr contact
                                                        ) :
        MessageQueueAssociator(outer->getAssociatedMessageQueue()),
        mID(zsLib::createPUID()),
        mOuter(outer),
        mContact(contact),
        mCurrentState(ContactSubscriptionState_Pending)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!outer)
        ZS_THROW_INVALID_ARGUMENT_IF(!contact)
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::init(ILocationPtr peerLocation)
      {
        if (!peerLocation) {
          ZS_LOG_DEBUG(log("creating a contact subscription to a hinted location") + UseContact::toDebug(mContact))

          // If there isn't a peer location then this contact subscription came
          // into being because this contact was hinted that it wants to connect
          // with this user. As such we will need to open a peer subscription
          // to the contact to cause locations to be found/opened. If there
          // are active conversation threads then the conversation threads will
          // open their own peer subscriptions and thus this peer subscription
          // can be shutdown after a reasonable amount of time has passed to
          // try to connect to the peer.
          AccountPtr outer = mOuter.lock();
          ZS_THROW_BAD_STATE_IF(!outer)

          stack::IAccountPtr stackAccount = outer->getStackAccount();
          if (!stackAccount) {
            ZS_LOG_WARNING(Detail, log("stack account is not available thus unable to create contact subscription"))
            goto step;
          }

          IPeerFilePublicPtr peerFilePublic = mContact->getPeerFilePublic();
          if (!peerFilePublic) {
            ZS_LOG_WARNING(Detail, log("public peer file for contact is not available"))
            goto step;
          }

          IPeerPtr peer = mContact->getPeer();

          mPeerSubscription = IPeerSubscription::subscribe(peer, mThisWeak.lock());
          ZS_THROW_BAD_STATE_IF(!mPeerSubscription)

          mPeerSubscriptionAutoCloseTimer = Timer::create(mThisWeak.lock(), Seconds(OPENPEER_PEER_SUBSCRIPTION_AUTO_CLOSE_TIMEOUT_IN_SECONDS), false);
        } else {
          ZS_LOG_DEBUG(log("creating location subscription to location") + ILocation::toDebug(peerLocation))
          mLocations[peerLocation->getLocationID()] = LocationSubscription::create(mThisWeak.lock(), peerLocation);
        }

      step:
        step();
      }

      //-----------------------------------------------------------------------
      Account::ContactSubscription::~ContactSubscription()
      {
        ZS_LOG_DEBUG(log("destructor called"))
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::ContactSubscription::toDebug(ContactSubscriptionPtr contactSubscription)
      {
        if (!contactSubscription) return ElementPtr();
        return contactSubscription->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => friend Account
      #pragma mark

      //-----------------------------------------------------------------------
      Account::ContactSubscriptionPtr Account::ContactSubscription::create(
                                                                           AccountPtr outer,
                                                                           UseContactPtr contact,
                                                                           ILocationPtr peerLocation
                                                                           )
      {
        ContactSubscriptionPtr pThis(new ContactSubscription(outer, contact));
        pThis->mThisWeak = pThis;
        pThis->init(peerLocation);
        return pThis;
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::notifyAboutLocationState(
                                                                  ILocationPtr location,
                                                                  ILocation::LocationConnectionStates state
                                                                  )
      {
        LocationSubscriptionMap::iterator found = mLocations.find(location->getLocationID());

        LocationSubscriptionPtr locationSubscription;
        if (found != mLocations.end()) {
          locationSubscription = (*found).second;
        }

        ZS_LOG_DEBUG(log("notifying about location state") + ZS_PARAM("state", ILocation::toString(state)) + ZS_PARAM("found", (found != mLocations.end())) + ILocation::toDebug(location))

        switch (state) {
          case ILocation::LocationConnectionState_Pending: {
            if (found == mLocations.end()) {
              ZS_LOG_DEBUG(log("pending state where location is not found thus do nothing"))
              return;  // only do something when its actually connected
            }

            ZS_LOG_DEBUG(log("pending state where location is found thus cancelling existing location"))

            // we must have had an old subscription laying around, kill it in favour of a new one that will come later...
            locationSubscription->cancel();
            break;
          }
          case ILocation::LocationConnectionState_Connected: {
            if (found != mLocations.end()) {
              // make sure the location that already exists isn't in the middle of a shutdown...
              if ((locationSubscription->isShuttingDown()) || (locationSubscription->isShutdown())) {
                ZS_LOG_WARNING(Debug, log("connected state where location subscription was shutting down thus forgetting location subscription early"))

                // forget about this location early since it must shutdown anyway...
                mLocations.erase(found);
                found = mLocations.end();
              }
            }

            if (found != mLocations.end()) {
              ZS_LOG_DEBUG(log("connected state where location subscription is pending or ready"))
              return;  // nothing to do since location already exists
            }

            ZS_LOG_DEBUG(log("creating location subscription for connected location"))

            // we have a new location, remember it...
            locationSubscription = LocationSubscription::create(mThisWeak.lock(), location);
            mLocations[location->getLocationID()] = locationSubscription;
            break;
          }
          case ILocation::LocationConnectionState_Disconnecting:
          case ILocation::LocationConnectionState_Disconnected:  {
            if (found == mLocations.end()) {
              ZS_LOG_DEBUG(log("ignoring disconnecting/disconnected state where there is no location subscription"))
              return;  // nothing to do as we don't have location anyway...
            }

            ZS_LOG_DEBUG(log("cancelling location subscription for disconnecting/disconnected location"))
            locationSubscription->cancel();
            break;
          }
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => IContactSubscriptionAsyncDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onWake()
      {
        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => IPeerSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(getLock());
        if (mPeerSubscription != subscription) {
          ZS_LOG_DEBUG(log("ignoring peer subscription shutdown for obslete subscription"))
          return;
        }

        ZS_LOG_DEBUG(log("peer subscription shutdown"))

        mPeerSubscription.reset();
        step();
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onPeerSubscriptionFindStateChanged(
                                                                            IPeerSubscriptionPtr subscription,
                                                                            IPeerPtr peer,
                                                                            PeerFindStates state
                                                                            )
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("peer subscription find state changed") + ZS_PARAM("state", IPeer::toString(state)) + IPeer::toDebug(peer))
        step();
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onPeerSubscriptionLocationConnectionStateChanged(
                                                                                          IPeerSubscriptionPtr subscription,
                                                                                          ILocationPtr location,
                                                                                          LocationConnectionStates state
                                                                                          )
      {
        AutoRecursiveLock lock(getLock());
        if (mPeerSubscription != subscription) {
          ZS_LOG_DEBUG(log("ignoring peer subscription shutdown for obslete subscription"))
          return;
        }

        ZS_LOG_DEBUG(log("peer subscription location state changed") + ZS_PARAM("state", ILocation::toString(state)) + ILocation::toDebug(location))
        step();
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onPeerSubscriptionMessageIncoming(
                                                                           IPeerSubscriptionPtr subscription,
                                                                           IMessageIncomingPtr incomingMessage
                                                                           )
      {
        //IGNORED
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onTimer(TimerPtr timer)
      {
        AutoRecursiveLock lock(getLock());
        if (timer != mPeerSubscriptionAutoCloseTimer) return;

        ZS_LOG_DEBUG(log("timer fired") + IPeerSubscription::toDebug(mPeerSubscription))

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
          mPeerSubscription.reset();
        }

        mPeerSubscriptionAutoCloseTimer->cancel();
        mPeerSubscriptionAutoCloseTimer.reset();

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => friend LocationSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      AccountPtr Account::ContactSubscription::getOuter() const
      {
        AutoRecursiveLock lock(getLock());
        return mOuter.lock();
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::notifyLocationShutdown(const String &locationID)
      {
        AutoRecursiveLock lock(getLock());

        LocationSubscriptionMap::iterator found = mLocations.find(locationID);
        if (found == mLocations.end()) {
          ZS_LOG_DEBUG(log("location subscription not found in connection subscription list") + ZS_PARAM("location ID", locationID))
          return;
        }

        ZS_LOG_DEBUG(log("erasing location subscription") + ZS_PARAM("location ID", locationID))
        mLocations.erase(found);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &Account::ContactSubscription::getLock() const
      {
        AccountPtr outer = mOuter.lock();
        if (!outer) return mBogusLock;
        return outer->getLock();
      }

      //-----------------------------------------------------------------------
      Log::Params Account::ContactSubscription::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("Account::ContactSubscription");
        IHelper::debugAppend(objectEl, "id", mID);
        IHelper::debugAppend(objectEl, "peer uri", mContact->getPeerURI());
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::ContactSubscription::toDebug() const
      {
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("core::Account::ContactSubscription");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));
        IHelper::debugAppend(resultEl, UseContact::toDebug(mContact));
        IHelper::debugAppend(resultEl, IPeerSubscription::toDebug(mPeerSubscription));
        IHelper::debugAppend(resultEl, "timer", (bool)mPeerSubscriptionAutoCloseTimer);
        IHelper::debugAppend(resultEl, "locations", mLocations.size() > 0);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::cancel()
      {
        if (isShutdown()) return;

        setState(ContactSubscriptionState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        bool locationsShutdown = true;

        // clear out locations
        {
          for (LocationSubscriptionMap::iterator locIter = mLocations.begin(); locIter != mLocations.end(); )
          {
            LocationSubscriptionMap::iterator current = locIter;
            ++locIter;

            LocationSubscriptionPtr &location = (*current).second;

            location->cancel();
            if (!location->isShutdown()) locationsShutdown = false;
          }

          mLocations.clear();
        }

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
        }

        if (mGracefulShutdownReference) {
          if (mPeerSubscription) {
            if (!mPeerSubscription->isShutdown()) {
              ZS_LOG_DEBUG(log("waiting for peer subscription to shutdown"))
              return;
            }
          }

          if (!locationsShutdown) {
            ZS_LOG_DEBUG(log("waiting for location to shutdown"))
            return;
          }
        }

        setState(ContactSubscriptionState_Shutdown);

        mGracefulShutdownReference.reset();

        if (mPeerSubscriptionAutoCloseTimer) {
          mPeerSubscriptionAutoCloseTimer->cancel();
          mPeerSubscriptionAutoCloseTimer.reset();
        }

        mLocations.clear();

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
          mPeerSubscription.reset();
        }

        AccountPtr outer = mOuter.lock();
        if (outer) {
          outer->notifyContactSubscriptionShutdown(mContact->getPeerURI());
        }
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
          cancel();
          return;
        }

        setState(ContactSubscriptionState_Ready);

        if (!mPeerSubscriptionAutoCloseTimer) {
          if (mLocations.size() < 1) {
            // there are no more locations... we should shut outselves down...
            cancel();
          }
        }
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::setState(ContactSubscriptionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_DETAIL(log("state changed") + ZS_PARAM("old state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))
        mCurrentState = state;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => LocationSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      const char *Account::LocationSubscription::toString(Account::LocationSubscription::LocationSubscriptionStates state)
      {
        switch (state) {
          case LocationSubscriptionState_Pending:      return "Pending";
          case LocationSubscriptionState_Ready:        return "Ready";
          case LocationSubscriptionState_ShuttingDown: return "Shutting down";
          case LocationSubscriptionState_Shutdown:     return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      Account::LocationSubscription::LocationSubscription(
                                                          ContactSubscriptionPtr outer,
                                                          ILocationPtr peerLocation
                                                          ) :
        MessageQueueAssociator(outer->getAssociatedMessageQueue()),
        mID(zsLib::createPUID()),
        mOuter(outer),
        mPeerLocation(peerLocation),
        mCurrentState(LocationSubscriptionState_Pending)
      {
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::init()
      {
        step();
      }

      //-----------------------------------------------------------------------
      Account::LocationSubscription::~LocationSubscription()
      {
        ZS_LOG_DEBUG(log("destructor called"))
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::LocationSubscription::toDebug(LocationSubscriptionPtr subscription)
      {
        if (!subscription) return ElementPtr();
        return subscription->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::LocationSubscription => friend ContactSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      Account::LocationSubscriptionPtr Account::LocationSubscription::create(
                                                                             ContactSubscriptionPtr outer,
                                                                             ILocationPtr peerLocation
                                                                             )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!peerLocation)
        LocationSubscriptionPtr pThis(new LocationSubscription(outer, peerLocation));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::LocationSubscription => IPublicationSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::onPublicationSubscriptionStateChanged(
                                                                                IPublicationSubscriptionPtr subscription,
                                                                                PublicationSubscriptionStates state
                                                                                )
      {
        AutoRecursiveLock lock(getLock());
        if (subscription != mPublicationSubscription) {
          ZS_LOG_DEBUG(log("ignoring publication subscription state change for obsolete subscription"))
          return;
        }

        ZS_LOG_DEBUG(log("publication subscription state change") + ZS_PARAM("state", IPublicationSubscription::toString(state)) + IPublicationSubscription::toDebug(subscription))

        if ((stack::IPublicationSubscription::PublicationSubscriptionState_ShuttingDown == mPublicationSubscription->getState()) ||
            (stack::IPublicationSubscription::PublicationSubscriptionState_ShuttingDown == mPublicationSubscription->getState())) {
          ZS_LOG_WARNING(Detail, log("failed to create a subscription to the peer"))
          mPublicationSubscription.reset();
          cancel();
          return;
        }

        step();
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::onPublicationSubscriptionPublicationUpdated(
                                                                                      IPublicationSubscriptionPtr subscription,
                                                                                      IPublicationMetaDataPtr metaData
                                                                                      )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!metaData)

        AutoRecursiveLock lock(getLock());
        if (subscription != mPublicationSubscription) {
          ZS_LOG_DEBUG(log("ignoring publication notification on obsolete publication subscription"))
          return;
        }

        String name = metaData->getName();

        SplitMap result;
        services::IHelper::split(name, result, '/');

        if (result.size() < 6) {
          ZS_LOG_WARNING(Debug, log("subscription path is too short") + IPublicationMetaData::toDebug(metaData))
          return;
        }

        ContactSubscriptionPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Debug, log("unable to locate contact subscription"))
          return;
        }

        AccountPtr account = outer->getOuter();
        if (!account) {
          ZS_LOG_WARNING(Debug, log("unable to locate account"))
          return;
        }

        UseConversationThreadPtr thread = account->notifyPublicationUpdated(mPeerLocation, metaData, result);
        if (!thread) {
          ZS_LOG_WARNING(Debug, log("publication did not result in a conversation thread"))
          return;
        }

        String threadID = thread->getThreadID();
        ConversationThreadMap::iterator found = mConversationThreads.find(threadID);
        if (found != mConversationThreads.end()) {
          ZS_LOG_DEBUG(log("already know about this conversation thread (thus nothing more to do)"))
          return;  // already know about this conversation thread
        }

        ZS_LOG_DEBUG(log("remembering converation thread for the future"))

        // remember this conversation thread is linked to this peer location
        mConversationThreads[threadID] = thread;
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::onPublicationSubscriptionPublicationGone(
                                                                                   IPublicationSubscriptionPtr subscription,
                                                                                   IPublicationMetaDataPtr metaData
                                                                                   )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!metaData)

        AutoRecursiveLock lock(getLock());
        if (subscription != mPublicationSubscription) {
          ZS_LOG_DEBUG(log("ignoring publication notification on obsolete publication subscription"))
          return;
        }

        String name = metaData->getName();

        SplitMap result;
        services::IHelper::split(name, result, '/');

        if (result.size() < 6) {
          ZS_LOG_WARNING(Debug, log("subscription path is too short") + ZS_PARAM("path", name))
          return;
        }

        ContactSubscriptionPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Debug, log("unable to locate contact subscription"))
          return;
        }

        AccountPtr account = outer->getOuter();
        if (!account) {
          ZS_LOG_WARNING(Debug, log("unable to locate account"))
          return;
        }

        account->notifyPublicationGone(mPeerLocation, metaData, result);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::LocationSubscription => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &Account::LocationSubscription::getLock() const
      {
        ContactSubscriptionPtr outer = mOuter.lock();
        if (!outer) return mBogusLock;
        return outer->getLock();
      }

      //-----------------------------------------------------------------------
      Log::Params Account::LocationSubscription::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Account::LocationSubscription");
        IHelper::debugAppend(objectEl, "id", mID);
        IHelper::debugAppend(objectEl, "peer uri", getPeerURI());
        IHelper::debugAppend(objectEl, "location id", getLocationID());
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::LocationSubscription::toDebug() const
      {
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("core::Account::LocationSubscription");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));
        IHelper::debugAppend(resultEl, ILocation::toDebug(mPeerLocation));
        IHelper::debugAppend(resultEl, IPublicationSubscription::toDebug(mPublicationSubscription));
        IHelper::debugAppend(resultEl, "conversation thread", mConversationThreads.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      String Account::LocationSubscription::getPeerURI() const
      {
        static String empty;
        ContactSubscriptionPtr outer = mOuter.lock();
        if (outer) return outer->getContact()->getPeerURI();
        return empty;
      }

      //-----------------------------------------------------------------------
      String Account::LocationSubscription::getLocationID() const
      {
        if (!mPeerLocation) return String();
        return mPeerLocation->getLocationID();
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::cancel()
      {
        if (isShutdown()) return;

        setState(LocationSubscriptionState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        // scope: notify all the conversation threads that the peer location is shutting down
        {
          for (ConversationThreadMap::iterator iter = mConversationThreads.begin(); iter != mConversationThreads.end(); ++iter)
          {
            UseConversationThreadPtr &thread = (*iter).second;
            thread->notifyPeerDisconnected(mPeerLocation);
          }
          mConversationThreads.clear();
        }

        if (mPublicationSubscription) {
          mPublicationSubscription->cancel();
        }

        if (mGracefulShutdownReference) {
          if (mPublicationSubscription) {
            if (stack::IPublicationSubscription::PublicationSubscriptionState_Shutdown != mPublicationSubscription->getState()) {
              ZS_LOG_DEBUG(log("waiting for publication subscription to shutdown"))
              return;
            }
          }
        }

        setState(LocationSubscriptionState_Shutdown);

        ContactSubscriptionPtr outer = mOuter.lock();
        if ((outer) &&
            (mPeerLocation)) {
          outer->notifyLocationShutdown(getLocationID());
        }

        mPublicationSubscription.reset();
        mPeerLocation.reset();
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
          cancel();
          return;
        }

        if (!mPublicationSubscription) {
          ContactSubscriptionPtr outer = mOuter.lock();
          if (!outer) {
            ZS_LOG_WARNING(Detail, log("failed to obtain contact subscription"))
            return;
          }

          AccountPtr account = outer->getOuter();
          if (!account) {
            ZS_LOG_WARNING(Detail, log("failed to obtain account"))
            return;
          }

          stack::IAccountPtr stackAccount = account->getStackAccount();
          if (!stackAccount) {
            ZS_LOG_WARNING(Detail, log("failed to obtain stack account"))
            return;
          }

          IPublicationRepositoryPtr repository = account->getRepository();
          if (!repository) {
            ZS_LOG_WARNING(Detail, log("failed to obtain stack publication respository"))
            return;
          }

          stack::IPublicationMetaData::PeerURIList empty;
          stack::IPublicationRepository::SubscribeToRelationshipsMap relationships;
          relationships["/threads/1.0/subscribers/permissions"] = IPublicationMetaData::PermissionAndPeerURIListPair(stack::IPublicationMetaData::Permission_All, empty);

          ZS_LOG_DEBUG(log("subscribing to peer thread publications"))
          mPublicationSubscription = repository->subscribe(mThisWeak.lock(), mPeerLocation, "/threads/1.0/", relationships);
        }

        if (!mPublicationSubscription) {
          ZS_LOG_WARNING(Detail, log("failed to create publication subscription"))
          cancel();
          return;
        }

        if (IPublicationSubscription::PublicationSubscriptionState_Established != mPublicationSubscription->getState()) {
          ZS_LOG_DEBUG(log("waiting for publication subscription to establish"))
          return;
        }

        setState(LocationSubscriptionState_Ready);
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::setState(LocationSubscriptionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_DETAIL(log("state changed") + ZS_PARAM("old state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;
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
