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
    #pragma mark IPushPresence
    #pragma mark

    interaction IPushPresence
    {
      enum PushPresenceStates
      {
        PushPresenceState_Pending,
        PushPresenceState_Ready,
        PushPresenceState_ShuttingDown,
        PushPresenceState_Shutdown,
      };
      static const char *toString(PushPresenceStates state);

      enum PresenceStatuses
      {
        PresenceStatus_None,

        PresenceStatus_Busy,        // user is busy and wishes not to be contacted at this time
        PresenceStatus_Away,        // user is not available
        PresenceStatus_Idle,        // user is around but not active
        PresenceStatus_Available,   // user is available
      };

      ZS_DECLARE_TYPEDEF_PTR(std::list<IContactPtr>, ContactList)

      struct Status
      {
        String mStatusID;                   // system will fill in this value

        PresenceStatuses mStatusType;       // basic status type
        String mStatusTypeExtended;         // extended information about status

        String mStatusMessage;              // user defined status message

        ElementPtr mLocation;

        Time mSent;                         // when was the status was sent, system will assign a value if not specified
        Time mExpires;                      // optional, system will assign a long life time if not specified

        IContactPtr mFrom;                  // what peer sent the status (system will fill in if sending a status out)
      };

      struct Location
      {
        String mLocationName;               // friendly name representing location, e.g. "Nat's Pup"
        String mAddress;                    // street address of where to find
        String mCity;                       // town or city
        String mTerritory;                  // state or province
        String mCountry;                    // location country

        double mLatitude;                   // degrees from equator; positive is north and negative is south
        double mLongitude;                  // degrees from zero meridian

        double mAltitude;                   // height above sea level as measured in meters

        double mDirection;                  // the direction being headed in degrees on a circle (0 = north, 90 = east, 180 = south, 270 = west)
        double mSpeed;                      // speed moving in direction (meters / second)
      };

      ZS_DECLARE_PTR(Status)
      ZS_DECLARE_PTR(Location)

      typedef String ValueName;
      typedef std::list<ValueName> ValueNameList;

      static ElementPtr toDebug(IPushPresencePtr push);

      //-----------------------------------------------------------------------
      // PURPOSE: create a connection to the push presence service
      static IPushPresencePtr create(
                                     IPushPresenceDelegatePtr delegate,
                                     IPushPresenceDatabaseAbstractionDelegatePtr databaseDelegate,
                                     IAccountPtr account
                                     );

      //-----------------------------------------------------------------------
      // PURPOSE: get the push presence object instance ID
      virtual PUID getID() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get the current state of the push presence service
      virtual PushPresenceStates getState(
                                          WORD *outErrorCode = NULL,
                                          String *outErrorReason = NULL
                                          ) const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: shutdown the connection to the push presence service
      virtual void shutdown() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: register or unregister for push presence status updates
      virtual IPushPresenceRegisterQueryPtr registerDevice(
                                                           IPushPresenceRegisterQueryDelegatePtr inDelegate,
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
      // PURPOSE: send a status message over the network
      virtual void send(
                        const ContactList &toContactList,
                        const Status &status
                        ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: cause a check to refresh data contained within the server
      virtual void recheckNow() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: extract a list of name / value pairs contained within
      //          a push info structure
      // RETURNS: a pointer to the name value map
      static LocationPtr getLocation(const Status &status);

      //-----------------------------------------------------------------------
      // PURPOSE: create a JSON blob compatible with the PushInfo.mValues
      //          based on a collection of name / value pairs.
      // RETURNS: a pointer to the values blob or null ElementPtr() if no
      //          values were found.
      static ElementPtr createLocation(const Location &location);
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushPresenceDelegate
    #pragma mark

    interaction IPushPresenceDelegate
    {
      typedef IPushPresence::PushPresenceStates PushPresenceStates;
      typedef IPushPresence::StatusPtr StatusPtr;

      virtual void onPushPresenceStateChanged(
                                              IPushPresencePtr presence,
                                              PushPresenceStates state
                                              ) = 0;

      virtual void onPushPresenceNewStatus(
                                           IPushPresencePtr presence,
                                           StatusPtr status
                                           ) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushPresenceRegisterQuery
    #pragma mark

    interaction IPushPresenceRegisterQuery
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
    #pragma mark IPushPresenceRegisterQueryDelegate
    #pragma mark

    interaction IPushPresenceRegisterQueryDelegate
    {
      virtual void onPushPresenceRegisterQueryCompleted(IPushPresenceRegisterQueryPtr query) = 0;
    };
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IPushPresenceDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushPresencePtr, IPushPresencePtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushPresence::PushPresenceStates, PushPresenceStates)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushPresence::StatusPtr, StatusPtr)
ZS_DECLARE_PROXY_METHOD_2(onPushPresenceStateChanged, IPushPresencePtr, PushPresenceStates)
ZS_DECLARE_PROXY_METHOD_2(onPushPresenceNewStatus, IPushPresencePtr, StatusPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IPushPresenceRegisterQueryDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IPushPresenceRegisterQueryPtr, IPushPresenceRegisterQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushPresenceRegisterQueryCompleted, IPushPresenceRegisterQueryPtr)
ZS_DECLARE_PROXY_END()
