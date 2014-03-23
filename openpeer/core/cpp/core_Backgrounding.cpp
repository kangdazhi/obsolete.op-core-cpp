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

#include <openpeer/core/internal/core_Backgrounding.h>
#include <openpeer/core/internal/core_Stack.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>

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
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding
      #pragma mark

      //-----------------------------------------------------------------------
      Backgrounding::Backgrounding() :
        MessageQueueAssociator(IStackForInternal::queueCore())
      {
        ZS_LOG_DETAIL(log("created"))
      }

      //-----------------------------------------------------------------------
      Backgrounding::~Backgrounding()
      {
        mThisWeak.reset();
        ZS_LOG_DETAIL(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      BackgroundingPtr Backgrounding::convert(IBackgroundingPtr backgrounding)
      {
        return dynamic_pointer_cast<Backgrounding>(backgrounding);
      }

      //-----------------------------------------------------------------------
      BackgroundingPtr Backgrounding::create()
      {
        BackgroundingPtr pThis(new Backgrounding());
        pThis->mThisWeak = pThis;
        return pThis;
      }

      //-----------------------------------------------------------------------
      BackgroundingPtr Backgrounding::singleton()
      {
        static SingletonLazySharedPtr<Backgrounding> singleton(IBackgroundingFactory::singleton().createForBackgrounding());
        BackgroundingPtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Detail, slog("singleton gone"))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      Backgrounding::QueryPtr Backgrounding::createDeadQuery(IBackgroundingCompletionDelegatePtr inReadyDelegate)
      {
        ZS_LOG_WARNING(Detail, slog("creating dead query") + ZS_PARAM("delegate", (bool)inReadyDelegate))

        QueryPtr query = Query::create();

        if (inReadyDelegate) {
          core::IBackgroundingCompletionDelegatePtr delegate = core::IBackgroundingCompletionDelegateProxy::createWeak(IStackForInternal::queueApplication(), inReadyDelegate);

          try {
            delegate->onBackgroundingReady(query);
          } catch(core::IBackgroundingCompletionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, slog("delegate gone"))
          }
        }

        return query;
      }
      
      //-----------------------------------------------------------------------
      Backgrounding::SubscriptionPtr Backgrounding::subscribe(
                                                              core::IBackgroundingDelegatePtr inDelegate,
                                                              ULONG phase
                                                              )
      {
        SubscriptionPtr subscription = Subscription::create(core::IBackgroundingDelegateProxy::createWeak(IStackForInternal::queueApplication(), inDelegate));
        subscription->subscribe(services::IBackgrounding::subscribe(subscription, phase));
        return subscription;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding => IBackgrounding
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Backgrounding::toDebug(BackgroundingPtr backgrounding)
      {
        if (!backgrounding) return ElementPtr();
        return backgrounding->toDebug();
      }

      //-----------------------------------------------------------------------
      IBackgroundingQueryPtr Backgrounding::notifyGoingToBackground(IBackgroundingCompletionDelegatePtr readyDelegate)
      {
        ZS_LOG_DETAIL(log("going to background") + ZS_PARAM("delegate", (bool)readyDelegate))

        services::IBackgroundingCompletionDelegatePtr completionDelegate;

        QueryPtr query = Query::create();

        if (readyDelegate) {
          CompletionPtr completion = Completion::create(query, IBackgroundingCompletionDelegateProxy::createWeak(IStackForInternal::queueApplication(), readyDelegate));

          completionDelegate = services::IBackgroundingCompletionDelegateProxy::create(IStackForInternal::queueCore(), completion);
        }

        query->setup(services::IBackgrounding::notifyGoingToBackground(completionDelegate));

        return query;
      }

      //-----------------------------------------------------------------------
      void Backgrounding::notifyGoingToBackgroundNow()
      {
        ZS_LOG_DETAIL(log("going to background now"))

        services::IBackgrounding::notifyGoingToBackgroundNow();
      }

      //-----------------------------------------------------------------------
      void Backgrounding::notifyReturningFromBackground()
      {
        ZS_LOG_DETAIL(log("returning from background"))

        services::IBackgrounding::notifyReturningFromBackground();
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Backgrounding::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Backgrounding");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params Backgrounding::slog(const char *message)
      {
        return Log::Params(message, "core::Backgrounding");
      }

      //-----------------------------------------------------------------------
      Log::Params Backgrounding::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr Backgrounding::toDebug() const
      {
        ElementPtr resultEl = Element::create("core::Backgrounding");

        IHelper::debugAppend(resultEl, "id", mID);
        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding::Completion
      #pragma mark

      //-----------------------------------------------------------------------
      Backgrounding::Completion::Completion(
                                            IMessageQueuePtr queue,
                                            QueryPtr query,
                                            core::IBackgroundingCompletionDelegatePtr delegate
                                            ) :
        MessageQueueAssociator(queue),
        mQuery(query),
        mDelegate(delegate)
      {
      }

      //-----------------------------------------------------------------------
      Backgrounding::CompletionPtr Backgrounding::Completion::create(
                                                                     QueryPtr query,
                                                                     core::IBackgroundingCompletionDelegatePtr delegate
                                                                     )
      {
        CompletionPtr pThis = CompletionPtr(new Completion(IStackForInternal::queueCore(), query, delegate));
        pThis->mThis = pThis;
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding::Completion => services::IBackgroundingCompletionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Backgrounding::Completion::onBackgroundingReady(services::IBackgroundingQueryPtr query)
      {
        try {
          mDelegate->onBackgroundingReady(mQuery);
        } catch(IBackgroundingCompletionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        mQuery.reset();
        mDelegate.reset();
        mThis.reset();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding::Completion => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Backgrounding::Completion::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Backgrounding::Completion");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding::Subscription
      #pragma mark

      //-----------------------------------------------------------------------
      Backgrounding::Subscription::Subscription(
                                                IMessageQueuePtr queue,
                                                core::IBackgroundingDelegatePtr delegate
                                                ) :
        MessageQueueAssociator(queue),
        mDelegate(delegate)
      {
      }

      //-----------------------------------------------------------------------
      Backgrounding::SubscriptionPtr Backgrounding::Subscription::create(core::IBackgroundingDelegatePtr delegate)
      {
        SubscriptionPtr pThis(new Subscription(IStackForInternal::queueCore(), delegate));
        pThis->mThisWeak = pThis;
        return pThis;
      }

      //-----------------------------------------------------------------------
      void Backgrounding::Subscription::subscribe(services::IBackgroundingSubscriptionPtr subscription)
      {
        mSubscription = subscription;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding::Subscription => IBackgroundingSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      void Backgrounding::Subscription::cancel()
      {
        mSubscription->cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding::Subscription => IBackgroundingSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      void Backgrounding::Subscription::onBackgroundingGoingToBackground(
                                                                         services::IBackgroundingSubscriptionPtr subscription,
                                                                         services::IBackgroundingNotifierPtr notifier
                                                                         )
      {
        try {
          mDelegate->onBackgroundingGoingToBackground(mThisWeak.lock(), Notifier::create(notifier));
        } catch(core::IBackgroundingDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void Backgrounding::Subscription::onBackgroundingGoingToBackgroundNow(services::IBackgroundingSubscriptionPtr subscription)
      {
        try {
          mDelegate->onBackgroundingGoingToBackgroundNow(mThisWeak.lock());
        } catch(core::IBackgroundingDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void Backgrounding::Subscription::onBackgroundingReturningFromBackground(services::IBackgroundingSubscriptionPtr subscription)
      {
        try {
          mDelegate->onBackgroundingReturningFromBackground(mThisWeak.lock());
        } catch(core::IBackgroundingDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void Backgrounding::Subscription::onBackgroundingApplicationWillQuit(services::IBackgroundingSubscriptionPtr subscription)
      {
        try {
          mDelegate->onBackgroundingApplicationWillQuit(mThisWeak.lock());
        } catch(core::IBackgroundingDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Backgrounding::Subscription => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Backgrounding::Subscription::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Backgrounding::Subscription");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IBackgrounding
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IBackgrounding::toDebug()
    {
      return internal::Backgrounding::toDebug(internal::Backgrounding::singleton());
    }

    //-------------------------------------------------------------------------
    IBackgroundingQueryPtr IBackgrounding::notifyGoingToBackground(IBackgroundingCompletionDelegatePtr readyDelegate)
    {
      internal::BackgroundingPtr singleton = internal::Backgrounding::singleton();
      if (!singleton) return internal::Backgrounding::createDeadQuery(readyDelegate);
      return singleton->notifyGoingToBackground(readyDelegate);
    }

    //-------------------------------------------------------------------------
    void IBackgrounding::notifyGoingToBackgroundNow()
    {
      internal::BackgroundingPtr singleton = internal::Backgrounding::singleton();
      singleton->notifyGoingToBackgroundNow();
    }

    //-------------------------------------------------------------------------
    void IBackgrounding::notifyReturningFromBackground()
    {
      internal::BackgroundingPtr singleton = internal::Backgrounding::singleton();
      singleton->notifyReturningFromBackground();
    }

    //-------------------------------------------------------------------------
    IBackgroundingSubscriptionPtr IBackgrounding::subscribe(
                                                            IBackgroundingDelegatePtr delegate,
                                                            ULONG phase
                                                            )
    {
      return internal::Backgrounding::subscribe(delegate, phase);
    }
  }
}
