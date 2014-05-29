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

#include <openpeer/core/internal/core_PushMessaging.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IServicePushMailbox.h>
#include <openpeer/stack/IHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Stringize.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    using stack::IServicePushMailboxSession;

    namespace internal
    {
      typedef IStackForInternal UseStack;

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
      #pragma mark PushMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      PushMessaging::PushMessaging(IMessageQueuePtr queue) :
        MessageQueueAssociator(queue),
        mID(zsLib::createPUID())
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void PushMessaging::init()
      {
        AutoRecursiveLock lock(mLock);
        ZS_LOG_DEBUG(log("init called"))
      }

      //-----------------------------------------------------------------------
      PushMessaging::~PushMessaging()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      PushMessagingPtr PushMessaging::convert(IPushMessagingPtr contact)
      {
        return dynamic_pointer_cast<PushMessaging>(contact);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging => IPushMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PushMessaging::toDebug(IPushMessagingPtr identity)
      {
        if (!identity) return ElementPtr();
        return PushMessaging::convert(identity)->toDebug();
      }

      //-----------------------------------------------------------------------
      PushMessagingPtr PushMessaging::create(
                                             IPushMessagingDelegatePtr delegate,
                                             IAccountPtr account
                                             )
      {
        return PushMessagingPtr();
      }

      //-----------------------------------------------------------------------
      PUID PushMessaging::getID() const
      {
        return mID;
      }

      //-----------------------------------------------------------------------
      IPushMessaging::PushMessagingStates PushMessaging::getState(
                                                                  WORD *outErrorCode,
                                                                  String *outErrorReason
                                                                  ) const
      {
        return PushMessagingStates_Pending;
      }

      //-----------------------------------------------------------------------
      void PushMessaging::shutdown()
      {
      }

      //-----------------------------------------------------------------------
      IPushMessagingRegisterQueryPtr PushMessaging::registerDevice(
                                                                   const char *deviceToken,
                                                                   const char *mappedType,
                                                                   bool unreadBadge,
                                                                   const char *sound,
                                                                   const char *action,
                                                                   const char *launchImage,
                                                                   unsigned int priority
                                                                   )
      {
        return IPushMessagingRegisterQueryPtr();
      }

      //-----------------------------------------------------------------------
      IPushMessagingQueryPtr PushMessaging::push(
                                                 IPushMessagingQueryDelegatePtr delegate,
                                                 const PeerOrIdentityURIList &toContactList,
                                                 const PushMessage &message
                                                 )
      {
        return IPushMessagingQueryPtr();
      }

      //-----------------------------------------------------------------------
      void PushMessaging::recheckNow()
      {
      }

      //-----------------------------------------------------------------------
      void PushMessaging::markPushMessageRead(const char *messageID)
      {
      }

      //-----------------------------------------------------------------------
      void PushMessaging::deletePushMessage(const char *messageID)
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessaging => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PushMessaging::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::PushMessaging");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PushMessaging::toDebug() const
      {
        return ElementPtr();
      }

      //-----------------------------------------------------------------------
      void PushMessaging::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushMessaging
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IPushMessaging::toDebug(IPushMessagingPtr identity)
    {
      return internal::PushMessaging::toDebug(identity);
    }

    //-------------------------------------------------------------------------
    const char *IPushMessaging::toString(PushMessagingStates state)
    {
      return NULL;
    }

    //-------------------------------------------------------------------------
    IPushMessagingPtr IPushMessaging::create(
                                             IPushMessagingDelegatePtr delegate,
                                             IAccountPtr account
                                             )
    {
      return internal::IPushMessagingFactory::singleton().create(delegate, account);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
