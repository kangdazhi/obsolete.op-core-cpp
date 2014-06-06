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

#ifndef OPENPEER_CORE_ACCOUNT_INCLUDE_LOCATION_SUBSCRIPTION
#include <openpeer/core/internal/core_Account.h>
#else

#if 0

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      class Account : ...
      {

#endif //0

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account::LocationSubscription
        #pragma mark

        class LocationSubscription : public MessageQueueAssociator,
                                     public SharedRecursiveLock,
                                     public IPublicationSubscriptionDelegate
        {
        public:
          enum LocationSubscriptionStates
          {
            LocationSubscriptionState_Pending,
            LocationSubscriptionState_Ready,
            LocationSubscriptionState_ShuttingDown,
            LocationSubscriptionState_Shutdown,
          };

          static const char *toString(LocationSubscriptionStates state);

          friend class Account::ContactSubscription;

          typedef String ThreadID;
          typedef std::map<ThreadID, UseConversationThreadPtr> ConversationThreadMap;

        protected:
          LocationSubscription(
                               ContactSubscriptionPtr outer,
                               ILocationPtr peerLocation
                               );

          void init();

        public:
          ~LocationSubscription();

          static ElementPtr toDebug(LocationSubscriptionPtr subscription);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::LocationSubscription => friend ContactSubscription
          #pragma mark

          static LocationSubscriptionPtr create(
                                                ContactSubscriptionPtr outer,
                                                ILocationPtr peerLocation
                                                );

          // (duplicate) bool isShuttingDown() const;
          // (duplicate) bool isShutdown() const;

          // (duplicate) void cancel();

        public:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::LocationSubscription => IPublicationSubscriptionDelegate
          #pragma mark

          virtual void onPublicationSubscriptionStateChanged(
                                                             IPublicationSubscriptionPtr subcription,
                                                             PublicationSubscriptionStates state
                                                             );

          virtual void onPublicationSubscriptionPublicationUpdated(
                                                                   IPublicationSubscriptionPtr subscription,
                                                                   IPublicationMetaDataPtr metaData
                                                                   );

          virtual void onPublicationSubscriptionPublicationGone(
                                                                IPublicationSubscriptionPtr subscription,
                                                                IPublicationMetaDataPtr metaData
                                                                );

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::LocationSubscription => (internal)
          #pragma mark

          bool isPending() const {return LocationSubscriptionState_Pending == mCurrentState;}
          bool isReady() const {return LocationSubscriptionState_Ready == mCurrentState;}
        protected:
          bool isShuttingDown() const {return LocationSubscriptionState_ShuttingDown == mCurrentState;}
          bool isShutdown() const {return LocationSubscriptionState_Shutdown == mCurrentState;}

        private:
          virtual PUID getID() const {return mID;}

          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          String getPeerURI() const;
          String getLocationID() const;

        protected:
          void cancel();

        private:
          void step();
          void setState(LocationSubscriptionStates state);

        private:
          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::LocationSubscription => (data)
          #pragma mark

          AutoPUID mID;
          LocationSubscriptionWeakPtr mThisWeak;
          LocationSubscriptionPtr mGracefulShutdownReference;

          ContactSubscriptionWeakPtr mOuter;

          LocationSubscriptionStates mCurrentState;

          ILocationPtr mPeerLocation;

          IPublicationSubscriptionPtr mPublicationSubscription;

          ConversationThreadMap mConversationThreads;             // all the conversations which have been attached to this location
        };

#if 0
      };

    }
  }
}
#endif //0

#endif //OPENPEER_CORE_ACCOUNT_INCLUDE_LOCATION_SUBSCRIPTION
