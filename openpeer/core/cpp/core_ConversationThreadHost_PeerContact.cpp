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


#include <openpeer/core/internal/core_ConversationThreadHost_PeerContact.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Contact.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/XML.h>


namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::UseAccount, UseAccount)
      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::UseContact, UseContact)
      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::UseConversationThread, UseConversationThread)

      using services::IHelper;
      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)

      using namespace core::internal::thread;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact
      #pragma mark

      //-----------------------------------------------------------------------
      const char *ConversationThreadHost::PeerContact::toString(PeerContactStates state)
      {
        switch (state)
        {
          case PeerContactState_Pending:      return "Pending";
          case PeerContactState_Ready:        return "Ready";
          case PeerContactState_ShuttingDown: return "Shutting down";
          case PeerContactState_Shutdown:     return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerContact::PeerContact(
                                                       IMessageQueuePtr queue,
                                                       ConversationThreadHostPtr host,
                                                       UseContactPtr contact,
                                                       const IdentityContactList &identityContacts
                                                       ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*host),
        mCurrentState(PeerContactState_Pending),
        mOuter(host),
        mContact(contact),
        mIdentityContacts(identityContacts)
      {
        ZS_LOG_DETAIL(log("created") + UseContact::toDebug(contact))
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::init()
      {
        AutoRecursiveLock lock(*this);

        mBackgroundingSubscription = IBackgrounding::subscribe(mThisWeak.lock(), UseSettings::getUInt(OPENPEER_CORE_SETTINGS_CONVERSATION_THREAD_HOST_BACKGROUNDING_PHASE));

        ULONG autoFindSeconds = services::ISettings::getUInt(OPENPEER_CORE_SETTING_CONVERSATION_THREAD_HOST_PEER_CONTACT);

        if (0 != autoFindSeconds) {
          Duration timeout = Seconds(autoFindSeconds);
          ZS_LOG_DEBUG(log("will perform autofind") + ZS_PARAM("duration (s)", timeout))
          mAutoFindTimer = Timer::create(mThisWeak.lock(), timeout, false);
        }
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerContact::~PeerContact()
      {
        mThisWeak.reset();
        ZS_LOG_DETAIL(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadHost::PeerContact::toDebug(PeerContactPtr contact)
      {
        if (!contact) return ElementPtr();
        return contact->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => friend ConversationThreadHost
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerContactPtr ConversationThreadHost::PeerContact::create(
                                                                                         IMessageQueuePtr queue,
                                                                                         ConversationThreadHostPtr host,
                                                                                         UseContactPtr contact,
                                                                                         const IdentityContactList &identityContacts
                                                                                         )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!host)
        ZS_THROW_INVALID_ARGUMENT_IF(!contact)
        PeerContactPtr pThis(new PeerContact(queue, host, contact, identityContacts));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyPublicationUpdated(
                                                                         ILocationPtr peerLocation,
                                                                         IPublicationMetaDataPtr metaData,
                                                                         const SplitMap &split
                                                                         )
      {
        AutoRecursiveLock lock(*this);
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("received publication update notification after shutdown"))
          return;
        }

        PeerLocationPtr location = findPeerLocation(peerLocation);
        if (!location) {
          location = PeerLocation::create(getAssociatedMessageQueue(), mThisWeak.lock(), peerLocation);
          if (!location) {
            ZS_LOG_WARNING(Detail, log("failed to create peer location") + ILocation::toDebug(peerLocation))
            return;
          }
          ZS_LOG_DEBUG(log("created new oeer location") + UseContact::toDebug(mContact))
          mPeerLocations[location->getLocationID()] = location;
        }

        location->notifyPublicationUpdated(peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyPublicationGone(
                                                                      ILocationPtr peerLocation,
                                                                      IPublicationMetaDataPtr metaData,
                                                                      const SplitMap &split
                                                                      )
      {
        AutoRecursiveLock lock(*this);
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("received publication gone notification after shutdown"))
          return;
        }
        PeerLocationPtr location = findPeerLocation(peerLocation);
        if (!location) {
          ZS_LOG_WARNING(Detail, log("location was not found to pass on publication gone notification") + ILocation::toDebug(peerLocation))
          return;
        }
        location->notifyPublicationGone(peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyPeerDisconnected(ILocationPtr peerLocation)
      {
        AutoRecursiveLock lock(*this);
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("received notification of peer disconnection after shutdown"))
          return;
        }

        PeerLocationPtr location = findPeerLocation(peerLocation);
        if (!location) {
          ZS_LOG_WARNING(Detail, log("unable to pass on peer disconnection notification as location is not available") + ILocation::toDebug(peerLocation))
          return;
        }
        location->notifyPeerDisconnected(peerLocation);
      }

      //-----------------------------------------------------------------------
      UseContactPtr ConversationThreadHost::PeerContact::getContact() const
      {
        AutoRecursiveLock lock(*this);
        return mContact;
      }

      //-----------------------------------------------------------------------
      const IdentityContactList &ConversationThreadHost::PeerContact::getIdentityContacts() const
      {
        AutoRecursiveLock lock(*this);
        return mIdentityContacts;
      }

      //-----------------------------------------------------------------------
      IConversationThread::ContactConnectionStates ConversationThreadHost::PeerContact::getContactConnectionState() const
      {
        AutoRecursiveLock lock(*this);

        // first check to see if any locations are connected...
        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          if (peerLocation->isConnected()) {
            return IConversationThread::ContactConnectionState_Connected;
          }
        }

        if (mContact) {
          switch (mContact->getPeer()->getFindState()) {
            case IPeer::PeerFindState_Pending:    return IConversationThread::ContactConnectionState_NotApplicable;
            case IPeer::PeerFindState_Idle:       return (mPeerLocations.size() > 0 ? IConversationThread::ContactConnectionState_Disconnected : IConversationThread::ContactConnectionState_NotApplicable);
            case IPeer::PeerFindState_Finding:    return IConversationThread::ContactConnectionState_Finding;
            case IPeer::PeerFindState_Completed:  return (mPeerLocations.size() > 0 ? IConversationThread::ContactConnectionState_Disconnected : IConversationThread::ContactConnectionState_NotApplicable);
          }
        }

        return IConversationThread::ContactConnectionState_NotApplicable;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::gatherMessagesDelivered(MessageReceiptMap &delivered) const
      {
        AutoRecursiveLock lock(*this);
        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          peerLocation->gatherMessagesDelivered(delivered);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::gatherContactsToAdd(ThreadContactMap &contacts) const
      {
        AutoRecursiveLock lock(*this);
        if (!isStillPartOfCurrentConversation(mContact)) {
          ZS_LOG_WARNING(Debug, log("ignoring request to add contacts as peer contact is not part of current conversation"))
          return;
        }

        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          peerLocation->gatherContactsToAdd(contacts);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::gatherContactsToRemove(ContactURIList &contacts) const
      {
        AutoRecursiveLock lock(*this);
        if (!isStillPartOfCurrentConversation(mContact)) {
          ZS_LOG_WARNING(Debug, log("ignoring request to remove contacts as peer contact is not part of current conversation"))
          return;
        }

        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          peerLocation->gatherContactsToRemove(contacts);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::gatherDialogReplies(
                                                                    const char *callID,
                                                                    LocationDialogMap &outDialogs
                                                                    ) const
      {
        AutoRecursiveLock lock(*this);
        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          peerLocation->gatherDialogReplies(callID, outDialogs);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyStep(bool performStepAsync)
      {
        if (performStepAsync) {
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return;
        }

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => IPeerSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(*this);
        if (subscription != mSlaveSubscription) {
          ZS_LOG_WARNING(Trace, log("ignoring shutdown notification on obsolete slave peer subscription (probably okay)"))
          return;
        }

        mSlaveSubscription.reset();
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onPeerSubscriptionFindStateChanged(
                                                                                   IPeerSubscriptionPtr subscription,
                                                                                   IPeerPtr peer,
                                                                                   PeerFindStates state
                                                                                   )
      {
        AutoRecursiveLock lock(*this);
        if (subscription != mSlaveSubscription) {
          ZS_LOG_WARNING(Detail, log("notified of subscription state from obsolete subscription (probably okay)"))
          return;
        }
        ZS_LOG_DEBUG(log("notified peer subscription state changed") + ZS_PARAM("state", IPeer::toString(state)))
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onPeerSubscriptionLocationConnectionStateChanged(
                                                                                                 IPeerSubscriptionPtr subscription,
                                                                                                 ILocationPtr location,
                                                                                                 LocationConnectionStates state
                                                                                                 )
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onPeerSubscriptionMessageIncoming(
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
      #pragma mark ConversationThreadHost::PeerContact => IBackgroundingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onBackgroundingGoingToBackground(
                                                                                 IBackgroundingSubscriptionPtr subscription,
                                                                                 IBackgroundingNotifierPtr notifier
                                                                                 )
      {
        ZS_LOG_DEBUG(log("notified going to background") + ZS_PARAM("subscription id", subscription->getID()) + ZS_PARAM("notifier", notifier->getID()))

        AutoRecursiveLock lock(*this);

        mBackgroundingNotifier = notifier;
        get(mBackgroundingNow) = false;
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onBackgroundingGoingToBackgroundNow(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("notified going to background now") + ZS_PARAM("subscription id", subscription->getID()))

        AutoRecursiveLock lock(*this);

        get(mBackgroundingNow) = true;
        step();

        mBackgroundingNotifier.reset();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onBackgroundingReturningFromBackground(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("notified returning from background now") + ZS_PARAM("subscription id", subscription->getID()))

        AutoRecursiveLock lock(*this);

        mBackgroundingNotifier.reset();

        get(mBackgroundingNow) = false;
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onBackgroundingApplicationWillQuit(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("notified application will quit") + ZS_PARAM("subscription id", subscription->getID()))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onWake()
      {
        ZS_LOG_DEBUG(log("on wake"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onTimer(TimerPtr timer)
      {
        ZS_LOG_DEBUG(log("timer"))

        AutoRecursiveLock lock(*this);

        if (timer == mAutoFindTimer) {
          ZS_LOG_DEBUG(log("auto find timer is no longer needed"))

          mAutoFindTimer->cancel();
          mAutoFindTimer.reset();
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => friend ConversationThreadHost::PeerLocation
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadHostPtr ConversationThreadHost::PeerContact::getOuter() const
      {
        AutoRecursiveLock lock(*this);
        return mOuter.lock();
      }

      //-----------------------------------------------------------------------
      UseConversationThreadPtr ConversationThreadHost::PeerContact::getBaseThread() const
      {
        AutoRecursiveLock lock(*this);
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("failed to obtain conversation thread because conversation thread host object is gone"))
          return ConversationThreadPtr();
        }
        return outer->getBaseThread();
      }

      //-----------------------------------------------------------------------
      ThreadPtr ConversationThreadHost::PeerContact::getHostThread() const
      {
        AutoRecursiveLock lock(*this);
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("failed to obtain repository because conversation thread host object is gone"))
          return ThreadPtr();
        }
        return outer->getHostThread();
      }

      //-----------------------------------------------------------------------
      UseAccountPtr ConversationThreadHost::PeerContact::getAccount() const
      {
        AutoRecursiveLock lock(*this);
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) return AccountPtr();
        return outer->getAccount();
      }

      //-----------------------------------------------------------------------
      IPublicationRepositoryPtr ConversationThreadHost::PeerContact::getRepository() const
      {
        AutoRecursiveLock lock(*this);
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("failed to obtain repository because conversation thread host object is gone"))
          return IPublicationRepositoryPtr();
        }
        return outer->getRepository();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyMessagesReceived(const MessageList &messages)
      {
        AutoRecursiveLock lock(*this);
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_DEBUG(log("unable to notify of messages received since conversation thread host object is gone"))
          return;
        }
        return outer->notifyMessagesReceived(messages);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyMessageDeliveryStateChanged(
                                                                                  const String &messageID,
                                                                                  IConversationThread::MessageDeliveryStates state
                                                                                  )
      {
        ZS_LOG_DEBUG(log("notified delivery state changed") + ZS_PARAM("message id", messageID) + ZS_PARAM("state", IConversationThread::toString(state)))

        AutoRecursiveLock lock(*this);

        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_DEBUG(log("unable to notify of messages received since conversation thread host object is gone"))
          return;
        }

        MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(messageID);
        if (found != mMessageDeliveryStates.end()) {
          MessageDeliveryStatePtr &deliveryState = (*found).second;
          if (state <= deliveryState->mState) {
            ZS_LOG_DEBUG(log("no need to change delievery state") + ZS_PARAM("current state", IConversationThread::toString(state)) + ZS_PARAM("reported state", IConversationThread::toString(deliveryState->mState)))
            return;
          }

          deliveryState->setState(state);
        } else {
          mMessageDeliveryStates[messageID] = MessageDeliveryState::create(mThisWeak.lock(), state);
        }

        // cause step to happen to ensure peer subscription is proper
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return outer->notifyMessageDeliveryStateChanged(messageID, state);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyStateChanged(PeerLocationPtr peerLocation)
      {
        AutoRecursiveLock lock(*this);
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_DEBUG(log("unable to notify conversation thread host of state change"))
          return;
        }
        return outer->notifyStateChanged(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyPeerLocationShutdown(PeerLocationPtr location)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("notified peer location shutdown") + UseContact::toDebug(mContact) + PeerLocation::toDebug(location))

        PeerLocationMap::iterator found = mPeerLocations.find(location->getLocationID());
        if (found != mPeerLocations.end()) {
          ZS_LOG_TRACE(log("peer location removed from map") + UseContact::toDebug(mContact) + PeerLocation::toDebug(location))
          mPeerLocations.erase(found);
        }

        PeerContactPtr pThis = mThisWeak.lock();
        if (pThis) {
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ConversationThreadHost::PeerContact::log(const char *message) const
      {
        String peerURI;
        if (mContact) {
          peerURI = mContact->getPeerURI();
        }

        ElementPtr objectEl = Element::create("core::ConversationThreadHost::PeerContact");
        IHelper::debugAppend(objectEl, "id", mID);
        IHelper::debugAppend(objectEl, "peer uri", peerURI);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadHost::PeerContact::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::ConversationThreadHost::PeerContact");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));

        IHelper::debugAppend(resultEl, UseContact::toDebug(mContact));

        IHelper::debugAppend(resultEl, "identity contacts", mIdentityContacts.size());

        IHelper::debugAppend(resultEl, "backgrounding subsscription id", mBackgroundingSubscription ? mBackgroundingSubscription->getID() : 0);
        IHelper::debugAppend(resultEl, "backgrounding notifier id", mBackgroundingNotifier ? mBackgroundingNotifier->getID() : 0);
        IHelper::debugAppend(resultEl, "backgrounding now", mBackgroundingNow);

        IHelper::debugAppend(resultEl, IPeerSubscription::toDebug(mSlaveSubscription));

        IHelper::debugAppend(resultEl, "locations", mPeerLocations.size());

        IHelper::debugAppend(resultEl, "delivery states", mMessageDeliveryStates.size());

        IHelper::debugAppend(resultEl, "auto find timer", (bool)mAutoFindTimer);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::cancel()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("cancel called"))

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(PeerContactState_ShuttingDown);

        if (mSlaveSubscription) {
          mSlaveSubscription->cancel();
          mSlaveSubscription.reset();
        }

        if (mBackgroundingSubscription) {
          mBackgroundingSubscription->cancel();
          mBackgroundingSubscription.reset();
        }

        mBackgroundingNotifier.reset();

        // cancel all the locations
        for (PeerLocationMap::iterator peerIter_doNotUse = mPeerLocations.begin(); peerIter_doNotUse != mPeerLocations.end(); )
        {
          PeerLocationMap::iterator current = peerIter_doNotUse;
          ++peerIter_doNotUse;

          PeerLocationPtr location = (*current).second;
          location->cancel();
        }

        setState(PeerContactState_Shutdown);

        mGracefulShutdownReference.reset();

        // mContact.reset();  // DO NOT RESET -- LEAVE THIS TO THE DESTRUCTOR

        mPeerLocations.clear();
        mMessageDeliveryStates.clear();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::step()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(log("step") + toDebug())

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_DEBUG(log("step forwarding to cancel"))
          cancel();
          return;
        }

        // check to see if there are any undelivered messages
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("shutting down peer contact since conversation thread host is shutdown"))
          cancel();
          return;
        }

        ThreadPtr hostThread = outer->getHostThread();
        if (!hostThread) {
          ZS_LOG_WARNING(Detail, log("host thread not available"))
          return;
        }

        UseAccountPtr account = getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is not available thus shutting down"))
          cancel();
          return;
        }

        stack::IAccountPtr stackAccount = account->getStackAccount();
        if (!stackAccount) {
          ZS_LOG_WARNING(Detail, log("stack account is not available thus shutting down"))
          cancel();
          return;
        }

        setState(PeerContactState_Ready);

        for (PeerLocationMap::iterator peerIter = mPeerLocations.begin(); peerIter != mPeerLocations.end(); )
        {
          PeerLocationMap::iterator current = peerIter;
          ++peerIter;

          PeerLocationPtr &location = (*current).second;
          location->step();
        }

        // check to see if there are outstanding messages needing to be delivered to this contact
        bool requiresSubscription = (bool)mAutoFindTimer;

        // check to see if there are undelivered messages, if so we will need a subscription...
        const MessageList &messages = hostThread->messages();
        MessageList::const_reverse_iterator last = messages.rbegin();
        if (last != messages.rend()) {
          const MessagePtr &message = (*last);

          // check to see if this message has been acknowledged before...
          MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(message->messageID());
          if (found != mMessageDeliveryStates.end()) {
            MessageDeliveryStatePtr &deliveryState = (*found).second;
            if (IConversationThread::MessageDeliveryState_Delivered > deliveryState->mState) {
              ZS_LOG_DEBUG(log("requires subscription because of undelivered message") + message->toDebug() + ZS_PARAM("was in delivery state", IConversationThread::toString(deliveryState->mState)))
              requiresSubscription = true;
            }
          } else {
            ZS_LOG_DEBUG(log("requires subscription because of undelivered message") + message->toDebug())
            requiresSubscription = true;
          }
        }

        if (!requiresSubscription) {
          ZS_LOG_DEBUG(log("no outstanding undelivered messages thus no need to prevent backgrounding") + ZS_PARAM("notifier", mBackgroundingNotifier ? mBackgroundingNotifier->getID() : 0))
          mBackgroundingNotifier.reset();
        }

        if (outer->hasCallPlacedTo(mContact)) {
          ZS_LOG_DEBUG(log("calls are being placed to this slave contact thus subscription is required"))
          // as there is a call being placed to this contact thus a subscription is required
          requiresSubscription = true;
        }

        if (requiresSubscription) {
          ZS_LOG_DEBUG(log("subscription to slave is required") + toDebug())
          if (!mSlaveSubscription) {
            mSlaveSubscription = IPeerSubscription::subscribe(mContact->getPeer(), mThisWeak.lock());
          }
        } else {
          ZS_LOG_DEBUG(log("subscription to slave is NOT required") + toDebug())
          if (mSlaveSubscription) {
            mSlaveSubscription->cancel();
            mSlaveSubscription.reset();
          }
        }

        // scope: fix the state of pending messages...
        if (mSlaveSubscription) {
          IPeerPtr peer = mContact->getPeer();
          IPeer::PeerFindStates state = peer->getFindState();
          ZS_LOG_DEBUG(log("slave subscription state") + ZS_PARAM("state", IPeer::toString(state)))

          LocationListPtr peerLocations = peer->getLocationsForPeer(false);

          const MessageList &messages = hostThread->messages();

          // Search from the back of the list to the front for messages that
          // aren't delivered as they need to be marked as undeliverable
          // since there are no peer locations available for this user...
          for (MessageList::const_reverse_iterator iter = messages.rbegin(); iter != messages.rend(); ++iter) {
            const MessagePtr &message = (*iter);
            MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(message->messageID());

            MessageDeliveryStatePtr deliveryState;

            if (found != mMessageDeliveryStates.end()) {
              bool stopProcessing = false;
              deliveryState = (*found).second;

              switch (deliveryState->mState) {
                case IConversationThread::MessageDeliveryState_Discovering:   {
                  break;
                }
                case IConversationThread::MessageDeliveryState_UserNotAvailable:
                case IConversationThread::MessageDeliveryState_Delivered:
                case IConversationThread::MessageDeliveryState_Read:          {
                  stopProcessing = true;
                  break;
                }
              }

              if (stopProcessing) {
                ZS_LOG_DEBUG(log("processing undeliverable messages stopped since message already has a delivery state") + message->toDebug())
                break;
              }
            } else {
              deliveryState = MessageDeliveryState::create(mThisWeak.lock(), IConversationThread::MessageDeliveryState_Discovering);
              mMessageDeliveryStates[message->messageID()] = deliveryState;
            }

            if (( ((IPeer::PeerFindState_Idle == state) ||
                   (IPeer::PeerFindState_Completed == state)) &&
                 (peerLocations->size() < 1)) ||
                (deliveryState->shouldPush(mBackgroundingNow))) {
              ZS_LOG_DEBUG(log("state must now be set to undeliverable") + ZS_PARAM("message", message->toDebug()) + ZS_PARAM("peer find state", IPeer::toString(state)))
              deliveryState->setState(IConversationThread::MessageDeliveryState_UserNotAvailable);
              outer->notifyMessageDeliveryStateChanged(message->messageID(), IConversationThread::MessageDeliveryState_UserNotAvailable);

              // tell the application to push this message out as a push notification
              outer->notifyMessagePush(message, mContact);
            }
          }
        }

        outer->notifyContactState(mContact, getContactConnectionState());
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::setState(PeerContactStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ZS_PARAM("previous state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerLocationPtr ConversationThreadHost::PeerContact::findPeerLocation(ILocationPtr peerLocation) const
      {
        AutoRecursiveLock lock(*this);
        PeerLocationMap::const_iterator found = mPeerLocations.find(peerLocation->getLocationID());
        if (found == mPeerLocations.end()) return PeerLocationPtr();
        const PeerLocationPtr &location = (*found).second;
        return location;
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::PeerContact::isStillPartOfCurrentConversation(UseContactPtr contact) const
      {
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("cannot tell if still part of current conversation because conversation thread host is gone"))
          return false;
        }

        // only respond to contacts to add if this contact is part of the current "live" thread...
        UseConversationThreadPtr baseThread = outer->getBaseThread();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("cannot tell if still part of current conversation because conversation thread is gone"))
          return false;
        }

        // check the base thread to see if this contact is still part of the conversation
        return baseThread->inConversation(contact);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact::MessageDeliveryState
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerContact::MessageDeliveryState::~MessageDeliveryState()
      {
        mOuter.reset();
        if (mPushTimer) {
          mPushTimer->cancel();
          mPushTimer.reset();
        }
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerContact::MessageDeliveryStatePtr ConversationThreadHost::PeerContact::MessageDeliveryState::create(
                                                                                                                                     PeerContactPtr owner,
                                                                                                                                     MessageDeliveryStates state
                                                                                                                                     )
      {
        MessageDeliveryStatePtr pThis(new MessageDeliveryState);
        pThis->mOuter = owner;
        pThis->mState = state;
        pThis->setState(state);
        return pThis;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::MessageDeliveryState::setState(MessageDeliveryStates state)
      {
        mLastStateChanged = zsLib::now();
        mState = state;

        if (IConversationThread::MessageDeliveryState_Discovering == state) {
          if (mPushTimer) return;


          PeerContactPtr outer = mOuter.lock();
          if (!outer) return; // no point adding a timer if the outer object is gone

          mPushTimer = Timer::create(outer, Seconds(OPENPEER_CONVERSATION_THREAD_MAX_WAIT_DELIVERY_TIME_BEFORE_PUSH_IN_SECONDS), false);
          mPushTime = mLastStateChanged + Seconds(OPENPEER_CONVERSATION_THREAD_MAX_WAIT_DELIVERY_TIME_BEFORE_PUSH_IN_SECONDS);
          return;
        }

        if (!mPushTimer) return;

        mPushTimer->cancel();
        mPushTimer.reset();

        mPushTime = Time();

        switch (mState) {
          case IConversationThread::MessageDeliveryState_Discovering:       break;  // not possible anyway
          case IConversationThread::MessageDeliveryState_UserNotAvailable:
          case IConversationThread::MessageDeliveryState_Delivered:
          case IConversationThread::MessageDeliveryState_Read:              mOuter.reset(); break;  // no longer require link to outer
        }
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::PeerContact::MessageDeliveryState::shouldPush(bool backgroundingNow) const
      {
        if (IConversationThread::MessageDeliveryState_Discovering != mState) return false;
        if (Time() == mPushTime) return false;
        if (backgroundingNow) return true;

        return (zsLib::now() >= mPushTime);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }
  }
}
