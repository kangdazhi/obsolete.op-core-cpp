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

#include <openpeer/core/internal/core_Account_LocationSubscription.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_ConversationThread.h>

#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

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
        SharedRecursiveLock(*outer),
        mOuter(outer),
        mPeerLocation(peerLocation),
        mCurrentState(LocationSubscriptionState_Pending)
      {
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::init()
      {
        AutoRecursiveLock lock(*this);
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
        AutoRecursiveLock lock(*this);
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

        AutoRecursiveLock lock(*this);
        if (subscription != mPublicationSubscription) {
          ZS_LOG_DEBUG(log("ignoring publication notification on obsolete publication subscription"))
          return;
        }

        String name = metaData->getName();

        SplitMap result;
        UseServicesHelper::split(name, result, '/');

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

        AutoRecursiveLock lock(*this);
        if (subscription != mPublicationSubscription) {
          ZS_LOG_DEBUG(log("ignoring publication notification on obsolete publication subscription"))
          return;
        }

        String name = metaData->getName();

        SplitMap result;
        UseServicesHelper::split(name, result, '/');

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
      Log::Params Account::LocationSubscription::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Account::LocationSubscription");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        UseServicesHelper::debugAppend(objectEl, "peer uri", getPeerURI());
        UseServicesHelper::debugAppend(objectEl, "location id", getLocationID());
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::LocationSubscription::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::Account::LocationSubscription");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "state", toString(mCurrentState));
        UseServicesHelper::debugAppend(resultEl, ILocation::toDebug(mPeerLocation));
        UseServicesHelper::debugAppend(resultEl, IPublicationSubscription::toDebug(mPublicationSubscription));
        UseServicesHelper::debugAppend(resultEl, "conversation thread", mConversationThreads.size());

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
          mPublicationSubscription = repository->subscribe(mThisWeak.lock(), mPeerLocation, "^\\/threads\\/1\\.0\\/.*$", relationships);
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
  }
}
