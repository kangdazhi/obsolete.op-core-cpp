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

#include <openpeer/core/internal/core_Account_ContactSubscription.h>
#include <openpeer/core/internal/core_Contact.h>

#include <zsLib/XML.h>

#define OPENPEER_PEER_SUBSCRIPTION_AUTO_CLOSE_TIMEOUT_IN_SECONDS (60*3)

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      using services::IHelper;

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
        AutoRecursiveLock lock(getLock());

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
    }

  }
}
