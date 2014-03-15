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

#ifndef OPENPEER_CORE_ACCOUNT_INCLUDE_CONTACT_SUBSCRIPTION
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
        #pragma mark Account::ContactSubscription
        #pragma mark

        class ContactSubscription : public MessageQueueAssociator,
                                    public SharedRecursiveLock,
                                    public IWakeDelegate,
                                    public IPeerSubscriptionDelegate,
                                    public ITimerDelegate
        {
        public:
          enum ContactSubscriptionStates
          {
            ContactSubscriptionState_Pending,
            ContactSubscriptionState_Ready,
            ContactSubscriptionState_ShuttingDown,
            ContactSubscriptionState_Shutdown,
          };

          static const char *toString(ContactSubscriptionStates state);

          friend class Account;
          friend class Account::LocationSubscription;

          typedef String LocationID;
          typedef std::map<LocationID, LocationSubscriptionPtr> LocationSubscriptionMap;

        protected:
          ContactSubscription(
                              AccountPtr outer,
                              UseContactPtr contact
                              );

          void init(ILocationPtr peerLocation);

        public:
          ~ContactSubscription();

          static ElementPtr toDebug(ContactSubscriptionPtr contactSubscription);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::ContactSubscription => friend Account
          #pragma mark

          static ContactSubscriptionPtr create(
                                               AccountPtr outer,
                                               UseContactPtr contact,
                                               ILocationPtr peerLocation = ILocationPtr()
                                               );

          // (duplicate) bool isShuttingDown() const;
          // (duplicate) bool isShutdown() const;

          void notifyAboutLocationState(
                                        ILocationPtr location,
                                        ILocation::LocationConnectionStates state
                                        );

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::ContactSubscription => IWakeDelegate
          #pragma mark

          virtual void onWake();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::ContactSubscription => IPeerSubscriptionDelegate
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

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::ContactSubscription => ITimerDelegate
          #pragma mark

          virtual void onTimer(TimerPtr timer);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::ContactSubscription => friend LocationSubscription
          #pragma mark

          AccountPtr getOuter() const;

          UseContactPtr getContact() const {return mContact;}

          void notifyLocationShutdown(const String &locationID);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::ContactSubscription => (internal)
          #pragma mark

          bool isPending() const {return ContactSubscriptionState_Pending == mCurrentState;}
          bool isReady() const {return ContactSubscriptionState_Ready == mCurrentState;}
        protected:
          bool isShuttingDown() const {return ContactSubscriptionState_ShuttingDown == mCurrentState;}
          bool isShutdown() const {return ContactSubscriptionState_Shutdown == mCurrentState;}

        private:
          virtual PUID getID() const {return mID;}

        private:
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          void cancel();
          void step();
          void setState(ContactSubscriptionStates state);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Account::ContactSubscription => (data)
          #pragma mark

          AutoPUID mID;
          ContactSubscriptionWeakPtr mThisWeak;
          ContactSubscriptionPtr mGracefulShutdownReference;

          AccountWeakPtr mOuter;

          ContactSubscriptionStates mCurrentState;

          UseContactPtr mContact;
          IPeerSubscriptionPtr mPeerSubscription;
          TimerPtr mPeerSubscriptionAutoCloseTimer;

          LocationSubscriptionMap mLocations;
        };

#if 0
      };
    }
  }
}
#endif //0

#endif //OPENPEER_CORE_ACCOUNT_INCLUDE_CONTACT_SUBSCRIPTION
