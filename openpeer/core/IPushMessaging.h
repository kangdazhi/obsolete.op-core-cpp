/*

 Copyright (c) 2013, Hookflash Inc.
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
    #pragma mark IPushMessaging
    #pragma mark

    interaction IPushMessaging
    {
      enum PushMessagingStates
      {
        PushMessagingStates_Pending,
        PushMessagingStates_Ready,
        PushMessagingStates_ShuttingDown,
        PushMessagingStates_Shutdown,
      };
      static const char *toString(PushMessagingStates state);

      enum PushStates
      {
        PushState_Read,
        PushState_Delivered,
        PushState_Sent,
        PushState_Pushed,
        PushState_Error,
      };

      static const char *toString(PushStates state);

      typedef String PeerOrIdentityURI;
      ZS_DECLARE_TYPEDEF_PTR(std::list<PeerOrIdentityURI>, PeerOrIdentityURIList)

      struct PushStateContactDetail
      {
        PeerOrIdentityURI mURI;

        WORD mErrorCode;
        String mErrorReason;
      };

      ZS_DECLARE_PTR(PushStateContactDetail)

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushStateContactDetailPtr>, PushStateContactDetailList)

      struct PushStateDetail
      {
        PushStates mState;
        PushStateContactDetailList mRelatedContacts;
      };

      ZS_DECLARE_PTR(PushStateDetail)

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushStateDetailPtr>, PushStateDetailList)
      
      typedef String ValueType;
      typedef std::list<ValueType> ValueList;

      struct PushMessage
      {
        String mMessageID;          // system will fill in this value
        String mFullMessage;
        ValueList mValues;          // values related to mapped type
        ElementPtr mCustomPushData; // extended push related custom data

        Time mSent;                 // when was the message sent, system will assign a value if not specified
        Time mExpires;              // optional, system will assign a long life time if not specified

        IContactPtr mFrom;          // what peer sent the message (system will fill in if sending a message out)

        PushStateDetailList mPushStateDetails;
      };

      ZS_DECLARE_PTR(PushMessage)

      static ElementPtr toDebug(IPushMessagingPtr push);

      static IPushMessagingPtr create(
                                      IPushMessagingDelegatePtr delegate,
                                      IAccountPtr account
                                      );

      virtual PUID getID() const = 0;

      virtual PushMessagingStates getState(
                                           WORD *outErrorCode,
                                           String *outErrorReason
                                           ) const = 0;

      virtual void shutdown() = 0;

      virtual IPushMessagingRegisterQueryPtr registerDevice(
                                                            const char *deviceToken,
                                                            const char *mappedType,   // for APNS maps to "loc-key"
                                                            bool unreadBadge,         // true causes total unread messages to be displayed in badge
                                                            const char *sound,        // what sound to play upon receiving a message. For APNS, maps to "sound" field
                                                            const char *action,       // for APNS, maps to "action-loc-key"
                                                            const char *launchImage,  // for APNS, maps to "launch-image"
                                                            unsigned int priority     // for APNS, maps to push priority
                                                            ) = 0;

      virtual IPushMessagingQueryPtr push(
                                          IPushMessagingQueryDelegatePtr delegate,
                                          const PeerOrIdentityURIList &toContactList,
                                          const PushMessage &message
                                          ) = 0;

      virtual void markPushMessageRead(const char *messageID) = 0;
      virtual void deletePushMessage(const char *messageID) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushMessagingDelegate
    #pragma mark

    interaction IPushMessagingDelegate
    {
      typedef IPushMessaging::PushMessagingStates PushMessagingStates;
      typedef IPushMessaging::PushMessagePtr PushMessagePtr;

      virtual void onPushMessagingStateChanged(
                                               IPushMessagingPtr messaging,
                                               PushMessagingStates state
                                               ) = 0;

      virtual void onPushMessagingDeviceRegistered(IPushMessagingPtr messaging) = 0;

      virtual void onPushMessagingNewMessage(
                                             IPushMessagingPtr messaging,
                                             PushMessagePtr message
                                             ) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushMessagingQuery
    #pragma mark

    interaction IPushMessagingQuery
    {
      ZS_DECLARE_TYPEDEF_PTR(IPushMessaging::PushMessage, PushMessage)

      virtual PUID getID() const = 0;

      virtual void cancel() = 0;

      virtual PushMessagePtr getPushMessage() = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushMessagingQueryDelegate
    #pragma mark

    interaction IPushMessagingQueryDelegate
    {
      virtual void onPushMessagingQueryPushStatesChanged(IPushMessagingQueryPtr query) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushMessagingRegisterQuery
    #pragma mark

    interaction IPushMessagingRegisterQuery
    {
      virtual PUID getID() const = 0;

      virtual bool isComplete(
                              WORD *outErrorCode = NULL,
                              String *outErrorReason = NULL
                              ) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushMessagingRegisterQueryDelegate
    #pragma mark

    interaction IPushMessagingRegisterQueryDelegate
    {
      virtual void onPushMessagingRegisterQueryCompleted(IPushMessagingRegisterQueryPtr query) = 0;
    };
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IPushMessagingDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushMessagingPtr, IPushMessagingPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushMessaging::PushMessagingStates, PushMessagingStates)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushMessaging::PushMessagePtr, PushMessagePtr)
ZS_DECLARE_PROXY_METHOD_2(onPushMessagingStateChanged, IPushMessagingPtr, PushMessagingStates)
ZS_DECLARE_PROXY_METHOD_1(onPushMessagingDeviceRegistered, IPushMessagingPtr)
ZS_DECLARE_PROXY_METHOD_2(onPushMessagingNewMessage, IPushMessagingPtr, PushMessagePtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IPushMessagingQueryDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushMessagingQueryPtr, IPushMessagingQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushMessagingQueryPushStatesChanged, IPushMessagingQueryPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IPushMessagingRegisterQueryDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushMessagingRegisterQueryPtr, IPushMessagingRegisterQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushMessagingRegisterQueryCompleted, IPushMessagingRegisterQueryPtr)
ZS_DECLARE_PROXY_END()
