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
        PushMessagingState_Pending,
        PushMessagingState_Ready,
        PushMessagingState_ShuttingDown,
        PushMessagingState_Shutdown,
      };
      static const char *toString(PushMessagingStates state);

      enum PushStates
      {
        PushState_None,

        PushState_Read,
        PushState_Answered,
        PushState_Flagged,
        PushState_Deleted,
        PushState_Draft,
        PushState_Recent,
        PushState_Delivered,
        PushState_Sent,
        PushState_Pushed,
        PushState_Error,
      };

      static const char *toString(PushStates state);
      static PushStates toPushState(const char *state);

      ZS_DECLARE_TYPEDEF_PTR(std::list<IContactPtr>, ContactList)

      struct PushStateContactDetail
      {
        IContactPtr mRemotePeer;

        WORD mErrorCode;
        String mErrorReason;
      };

      typedef std::list<PushStateContactDetail> PushStateContactDetailList;
      typedef std::map<PushStates, PushStateContactDetailList> PushStateDetailMap;
      
      typedef String ValueName;
      typedef std::list<ValueName> ValueNameList;

      typedef String Name;
      typedef String Value;
      typedef std::map<Name, Value> NameValueMap;
      ZS_DECLARE_PTR(NameValueMap)

      struct PushInfo
      {
        String mServiceType;  // e.g. "apns", "gcm", or all
        ElementPtr mValues;   // "values" data associateed with push messages (use "getValues(...)" to extract data
        ElementPtr mCustom;   // extended push related custom push data
      };

      typedef std::list<PushInfo> PushInfoList;

      struct PushMessage
      {
        String mMessageID;                  // system will fill in this value

        String mMimeType;
        String mFullMessage;                // only valid for "text/<sub-type>" mime types
        SecureByteBlockPtr mRawFullMessage; // raw version of the message (only supply if full message is empty and sending binary)

        String mPushType;                   // worked with registration "mappedType" to filter out push message types
        PushInfoList mPushInfos;            // each service has its own push information

        Time mSent;                         // when was the message sent, system will assign a value if not specified
        Time mExpires;                      // optional, system will assign a long life time if not specified

        IContactPtr mFrom;                  // what peer sent the message (system will fill in if sending a message out)

        PushStateDetailMap mPushStateDetails;
      };

      ZS_DECLARE_PTR(PushMessage)

      typedef std::list<PushMessagePtr> PushMessageList;

      typedef String MessageID;
      typedef std::list<MessageID> MessageIDList;

      static ElementPtr toDebug(IPushMessagingPtr push);

      //-----------------------------------------------------------------------
      // PURPOSE: create a connection to the push messaging service
      static IPushMessagingPtr create(
                                      IPushMessagingDelegatePtr delegate,
                                      IPushMessagingDatabaseAbstractionDelegatePtr databaseDelegate,
                                      IAccountPtr account
                                      );

      //-----------------------------------------------------------------------
      // PURPOSE: get the push messaging object instance ID
      virtual PUID getID() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get the current state of the push messaging service
      virtual PushMessagingStates getState(
                                           WORD *outErrorCode = NULL,
                                           String *outErrorReason = NULL
                                           ) const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: shutdown the connection to the push messaging service
      virtual void shutdown() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: register or unregister for push messages
      virtual IPushMessagingRegisterQueryPtr registerDevice(
                                                            IPushMessagingRegisterQueryDelegatePtr inDelegate,
                                                            const char *inDeviceToken,        // a token used for pushing to this particular service
                                                            Time inExpires,                   // how long should the subscription for push messaging last; pass in Time() to remove a previous subscription
                                                            const char *inMappedType,         // for APNS maps to "loc-key"
                                                            bool inUnreadBadge,               // true causes total unread messages to be displayed in badge
                                                            const char *inSound,              // what sound to play upon receiving a message. For APNS, maps to "sound" field
                                                            const char *inAction,             // for APNS, maps to "action-loc-key"
                                                            const char *inLaunchImage,        // for APNS, maps to "launch-image"
                                                            unsigned int inPriority,          // for APNS, maps to push priority
                                                            const ValueNameList &inValueNames // list of values requested from each push from the push server (in order they should be delivered); empty = all values
                                                            ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: send a push message to a contact list
      virtual IPushMessagingQueryPtr push(
                                          IPushMessagingQueryDelegatePtr delegate,
                                          const ContactList &toContactList,
                                          const PushMessage &message
                                          ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: cause a check to refresh data contained within the server
      virtual void recheckNow() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get delta list of messages that have changed since last
      //          fetch of messages
      // RETURNS: true if call was successful otherwise false
      // NOTES:   If false is returned the current list of messages must be
      //          flushed and all messages must be downloaded again (i.e.
      //          a version conflict was detected). Pass in NULL
      //          for "inLastVersionDownloaded" if false is returned. If false
      //          is still returning then the messages cannot be fetched.
      //
      //          The resultant list can be empty even if the method
      //          returns true. This could mean all the downloaded messages
      //          were filtererd out because they were not compatible push
      //          messages.
      virtual bool getMessagesUpdates(
                                      const char *inLastVersionDownloaded,  // pass in NULL if no previous version known
                                      String &outUpdatedToVersion,          // updated to this version (if same as passed in then no change available)
                                      PushMessageList &outNewMessages
                                      ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: extract a list of name / value pairs contained within
      //          a push info structure
      // RETURNS: a pointer to the name value map
      static NameValueMapPtr getValues(const PushInfo &pushInfo);

      //-----------------------------------------------------------------------
      // PURPOSE: create a JSON blob compatible with the PushInfo.mValues
      //          based on a collection of name / value pairs.
      // RETURNS: a pointer to the values blob or null ElementPtr() if no
      //          values were found.
      static ElementPtr createValues(const NameValueMap &values);

      //-----------------------------------------------------------------------
      // PURPOSE: mark an individual message as having been read
      virtual void markPushMessageRead(const char *messageID) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: delete an individual message
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

      virtual void onPushMessagingNewMessages(IPushMessagingPtr messaging) = 0;
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

      virtual bool isUploaded() const = 0;
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
      virtual void onPushMessagingQueryUploaded(IPushMessagingQueryPtr query) = 0;
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
                              ) const = 0;
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
ZS_DECLARE_PROXY_METHOD_1(onPushMessagingNewMessages, IPushMessagingPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IPushMessagingQueryDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushMessagingQueryPtr, IPushMessagingQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushMessagingQueryUploaded, IPushMessagingQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushMessagingQueryPushStatesChanged, IPushMessagingQueryPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IPushMessagingRegisterQueryDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushMessagingRegisterQueryPtr, IPushMessagingRegisterQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushMessagingRegisterQueryCompleted, IPushMessagingRegisterQueryPtr)
ZS_DECLARE_PROXY_END()
