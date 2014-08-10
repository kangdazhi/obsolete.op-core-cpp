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

#include <openpeer/core/IBackgrounding.h>
#include <openpeer/core/internal/types.h>

#include <openpeer/services/IBackgrounding.h>

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
      #pragma mark Backgrounding
      #pragma mark

      class Backgrounding : public zsLib::MessageQueueAssociator,
                            public core::IBackgrounding
      {
      public:
        friend interaction IBackgroundingFactory;
        friend interaction core::IBackgrounding;

        ZS_DECLARE_CLASS_PTR(Query)
        ZS_DECLARE_CLASS_PTR(Completion)
        ZS_DECLARE_CLASS_PTR(Subscription)

      protected:
        Backgrounding();

        static BackgroundingPtr create();

      public:
        ~Backgrounding();

      public:
        static BackgroundingPtr convert(IBackgroundingPtr backgrounding);

        static BackgroundingPtr singleton();

        static QueryPtr createDeadQuery(core::IBackgroundingCompletionDelegatePtr readyDelegate);

        static SubscriptionPtr subscribe(
                                         core::IBackgroundingDelegatePtr delegate,
                                         ULONG phase
                                         );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Backgrounding => IBackgrounding
        #pragma mark

        static ElementPtr toDebug(BackgroundingPtr backgrounding);

        virtual IBackgroundingQueryPtr notifyGoingToBackground(
                                                               IBackgroundingCompletionDelegatePtr readyDelegate = IBackgroundingCompletionDelegatePtr()
                                                               );

        virtual void notifyGoingToBackgroundNow();

        virtual void notifyReturningFromBackground();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Backgrounding => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        static Log::Params slog(const char *message);
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

      public:

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Backgrounding::Query
        #pragma mark

        class Query : public core::IBackgroundingQuery
        {
          friend class Backgrounding;

        protected:
          Query() {}

        public:
          static QueryPtr create() {return QueryPtr(new Query());}

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Backgrounding::Query => friend Backgrounding
          #pragma mark

          void setup(services::IBackgroundingQueryPtr query) {mQuery = query;}

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Backgrounding::Query => IBackgroundingQuery
          #pragma mark

          virtual PUID getID() const {return mQuery ? mQuery->getID() : mID;}

          virtual bool isReady() const {return mQuery ? mQuery->isReady() : true;}

          virtual size_t totalBackgroundingSubscribersStillPending() const {return mQuery ? mQuery->totalBackgroundingSubscribersStillPending() : 0;}

        protected:
          AutoPUID mID;
          services::IBackgroundingQueryPtr mQuery;
        };

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Backgrounding::Completion
        #pragma mark

        class Completion : public MessageQueueAssociator,
                           public services::IBackgroundingCompletionDelegate
        {
        protected:
          Completion(
                     IMessageQueuePtr queue,
                     QueryPtr query,
                     core::IBackgroundingCompletionDelegatePtr delegate
                     );

        public:
          static CompletionPtr create(
                                      QueryPtr query,
                                      core::IBackgroundingCompletionDelegatePtr delegate
                                      );

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Backgrounding::Completion => services::IBackgroundingCompletionDelegate
          #pragma mark

          virtual void onBackgroundingReady(services::IBackgroundingQueryPtr query);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Backgrounding::Completion => (internal)
          #pragma mark

          Log::Params log(const char *message) const;

        protected:
          AutoPUID mID;
          CompletionPtr mThis;

          QueryPtr mQuery;
          IBackgroundingCompletionDelegatePtr mDelegate;
        };

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Backgrounding::Subscription
        #pragma mark

        class Subscription : public MessageQueueAssociator,
                             public core::IBackgroundingSubscription,
                             public services::IBackgroundingDelegate
        {
        public:
          ZS_DECLARE_CLASS_PTR(Notifier)

        protected:
          Subscription(
                       IMessageQueuePtr queue,
                       core::IBackgroundingDelegatePtr delegate
                       );

        public:
          static SubscriptionPtr create(core::IBackgroundingDelegatePtr delegate);

          void subscribe(services::IBackgroundingSubscriptionPtr subscription);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Backgrounding::Subscription => IBackgroundingSubscription
          #pragma mark

          virtual PUID getID() const {return mID;}

          virtual void cancel();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Backgrounding::Subscription => services::IBackgroundingDelegate
          #pragma mark

          virtual void onBackgroundingGoingToBackground(
                                                        services::IBackgroundingSubscriptionPtr subscription,
                                                        services::IBackgroundingNotifierPtr notifier
                                                        );

          virtual void onBackgroundingGoingToBackgroundNow(services::IBackgroundingSubscriptionPtr subscription);

          virtual void onBackgroundingReturningFromBackground(services::IBackgroundingSubscriptionPtr subscription);

          virtual void onBackgroundingApplicationWillQuit(services::IBackgroundingSubscriptionPtr subscription);

        public:
          class Notifier : public core::IBackgroundingNotifier
          {
          protected:
            Notifier(services::IBackgroundingNotifierPtr notifier) : mNotifier(notifier) {}

          public:
            static NotifierPtr create(services::IBackgroundingNotifierPtr notifier)
            {
              NotifierPtr pThis(new Notifier(notifier));
              return pThis;
            }

          protected:
            virtual PUID getID() const {return mNotifier->getID();}

            virtual void ready() {mNotifier->ready();}

          protected:
            services::IBackgroundingNotifierPtr mNotifier;
          };

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Backgrounding::Subscription => (internal)
          #pragma mark

          Log::Params log(const char *message) const;

        protected:
          AutoPUID mID;
          SubscriptionWeakPtr mThisWeak;

          services::IBackgroundingSubscriptionPtr mSubscription;
          core::IBackgroundingDelegatePtr mDelegate;
        };

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Backgrounding => (data)
        #pragma mark

        AutoPUID mID;
        BackgroundingWeakPtr mThisWeak;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBackgroundingFactory
      #pragma mark

      interaction IBackgroundingFactory
      {
        static IBackgroundingFactory &singleton();

        virtual BackgroundingPtr createForBackgrounding();
      };

      class BackgroundingFactory : public IFactory<IBackgroundingFactory> {};
    }
  }
}
