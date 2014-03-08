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

#pragma once

#include <openpeer/core/types.h>

namespace openpeer
{
  namespace core
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IBackgrounding
    #pragma mark

    interaction IBackgrounding
    {
      //-----------------------------------------------------------------------
      // PURPOSE: returns a debug element containing internal object state
      static ElementPtr toDebug();

      //-----------------------------------------------------------------------
      // PURPOSE: Notifies the application is about to go into the background
      // PARAMS:  readyDelegate - pass in a delegate which will get a callback
      //                          when all backgrounding subscribers are ready
      //                          to go into the background
      // RETURNS: a query interface about the current backgrounding state
      static IBackgroundingQueryPtr notifyGoingToBackground(
                                                            IBackgroundingCompletionDelegatePtr readyDelegate = IBackgroundingCompletionDelegatePtr()
                                                            );

      //-----------------------------------------------------------------------
      // PURPOSE: Notifies the application is goinging to the background
      //          immediately
      static void notifyGoingToBackgroundNow();

      //-----------------------------------------------------------------------
      // PURPOSE: Notifies the application is returning from to the background
      static void notifyReturningFromBackground();

      virtual ~IBackgrounding() {}  // needed to ensure virtual table is created in order to use dynamic cast
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IBackgroundingQuery
    #pragma mark

    interaction IBackgroundingQuery
    {
      virtual PUID getID() const = 0;

      virtual bool isReady() const = 0;

      virtual size_t totalBackgroundingSubscribersStillPending() const = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IBackgroundingCompletionDelegate
    #pragma mark

    interaction IBackgroundingCompletionDelegate
    {
      virtual void onBackgroundingReady(IBackgroundingQueryPtr query) = 0;
    };

  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IBackgroundingCompletionDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IBackgroundingQueryPtr, IBackgroundingQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onBackgroundingReady, IBackgroundingQueryPtr)
ZS_DECLARE_PROXY_END()

