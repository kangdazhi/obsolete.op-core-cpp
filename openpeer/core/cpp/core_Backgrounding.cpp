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
      Backgrounding::Backgrounding()
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
        AutoRecursiveLock lock(IHelper::getGlobalLock());
        static BackgroundingPtr pThis = IBackgroundingFactory::singleton().createForBackgrounding();
        return pThis;
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

        BackgroundingPtr pThis = Backgrounding::convert(backgrounding);
        return pThis->toDebug();
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
        ElementPtr objectEl = Element::create("services::Backgrounding");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
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
      #pragma mark Backgrounding::Query
      #pragma mark

      //-----------------------------------------------------------------------
      Backgrounding::QueryPtr Backgrounding::Query::create()
      {
        return QueryPtr(new Query());
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
                                            QueryPtr query,
                                            IBackgroundingCompletionDelegatePtr delegate
                                            ) :
        mQuery(query),
        mDelegate(delegate)
      {
      }

      //-----------------------------------------------------------------------
      Backgrounding::CompletionPtr Backgrounding::Completion::create(
                                                                     QueryPtr query,
                                                                     IBackgroundingCompletionDelegatePtr delegate
                                                                     )
      {
        CompletionPtr pThis = CompletionPtr(new Completion(query, delegate));
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
        }

        mQuery.reset();
        mDelegate.reset();
        mThis.reset();
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
      return internal::Backgrounding::singleton()->notifyGoingToBackground(readyDelegate);
    }

    //-------------------------------------------------------------------------
    void IBackgrounding::notifyGoingToBackgroundNow()
    {
      return internal::Backgrounding::singleton()->notifyGoingToBackgroundNow();
    }

    //-------------------------------------------------------------------------
    void IBackgrounding::notifyReturningFromBackground()
    {
      return internal::Backgrounding::singleton()->notifyReturningFromBackground();
    }
  }
}

