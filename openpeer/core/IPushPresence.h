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

      ZS_DECLARE_TYPEDEF_PTR(std::list<IContactPtr>, ContactList)

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

      struct Status
      {
        String mStatusID;                         // system will fill in this value

        ElementPtr mPresenceEl;

        Time mSent;                               // when was the status was sent, system will assign a value if not specified
        Time mExpires;                            // optional, system will assign a long life time if not specified

        IContactPtr mFrom;                        // what peer sent the status (system will fill in if sending a status out)

        PushInfoList mPushInfos;                  // each service has its own push information

        static ElementPtr createEmptyPresence();  // create an emty status JSON object ready to be filled with presence data

        bool hasData() const;
        ElementPtr toDebug() const;
      };
      ZS_DECLARE_PTR(Status)

      typedef String ValueName;
      typedef std::list<ValueName> ValueNameList;

      static ElementPtr toDebug(IPushPresencePtr push);

      //-----------------------------------------------------------------------
      // PURPOSE: create a connection to the push presence service
      static IPushPresencePtr create(
                                     IPushPresenceDelegatePtr delegate,
                                     IPushPresenceTransferDelegatePtr transferDelegate,
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

      struct RegisterDeviceInfo
      {
        String  mDeviceToken;       // a token used for pushing to this particular service
        Time    mExpires;           // how long should the subscription for push messaging last; pass in Time() to remove a previous subscription
        String  mMappedType;        // for APNS maps to "loc-key"
        bool    mUnreadBadge {};    // true causes total unread messages to be displayed in badge
        String  mSound;             // what sound to play upon receiving a message. For APNS, maps to "sound" field
        String  mAction;            // for APNS, maps to "action-loc-key"
        String  mLaunchImage;       // for APNS, maps to "launch-image"
        UINT    mPriority {};       // for APNS, maps to push priority
        ValueNameList mValueNames;  // list of values requested from each push from the push server (in order they should be delivered); empty = all values

        bool hasData() const;
        ElementPtr toDebug() const;
      };

      //-----------------------------------------------------------------------
      // PURPOSE: register or unregister for push presence status updates
      virtual IPushPresenceRegisterQueryPtr registerDevice(
                                                           IPushPresenceRegisterQueryDelegatePtr inDelegate,
                                                           const RegisterDeviceInfo &inDeviceInfo
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
      static NameValueMapPtr getValues(const PushInfo &pushInfo);

      //-----------------------------------------------------------------------
      // PURPOSE: create a JSON blob compatible with the PushInfo.mValues
      //          based on a collection of name / value pairs.
      // RETURNS: a pointer to the values blob or null ElementPtr() if no
      //          values were found.
      static ElementPtr createValues(const NameValueMap &values);
      
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

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushPresenceTransferDelegate
    #pragma mark

    interaction IPushPresenceTransferDelegate
    {
      //-----------------------------------------------------------------------
      // PURPOSE: upload a file to a url
      // NOTES:   - this upload should occur even while the application goes
      //            to the background
      //          - this method is called asynchronously on the application's
      //            thread
      virtual void onPushPresenceTransferUploadFileDataToURL(
                                                             IPushPresencePtr session,
                                                             const char *postURL,
                                                             const char *fileNameContainingData,
                                                             ULONGEST totalFileSizeInBytes,            // the total bytes that exists within the file
                                                             ULONGEST remainingBytesToUpload,          // the file should be seeked to the position of (total size - remaining) and upload the remaining bytes from this position in the file
                                                             IPushPresenceTransferNotifierPtr notifier
                                                             ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: download a file from a URL
      // NOTES:   - this download should occur even while the application goes
      //            to the background
      //          - this method is called asynchronously on the application's
      //            thread
      virtual void onPushPresenceTransferDownloadDataFromURL(
                                                             IPushPresencePtr session,
                                                             const char *getURL,
                                                             const char *fileNameToAppendData,          // the existing file name to open and append
                                                             ULONGEST finalFileSizeInBytes,             // when the download completes the file size will be this size
                                                             ULONGEST remainingBytesToBeDownloaded,     // the downloaded data will be appended to the end of the existing file and this is the total bytes that are to be downloaded
                                                             IPushPresenceTransferNotifierPtr notifier
                                                             ) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPushPresenceTransferNotifier
    #pragma mark

    interaction IPushPresenceTransferNotifier
    {
      virtual void notifyComplete(bool wasSuccessful) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark struct PresenceStatus
    #pragma mark

    struct PresenceStatus
    {
      enum PresenceStatuses
      {
        PresenceStatus_None,

        PresenceStatus_Busy,        // user is busy and wishes not to be contacted at this time
        PresenceStatus_Away,        // user is not available
        PresenceStatus_Idle,        // user is around but not active
        PresenceStatus_Available,   // user is available
      };

      static const char *toString(PresenceStatuses state);
      static PresenceStatuses toPresenceStatus(const char *state);

      PresenceStatuses mStatus {PresenceStatus_None};   // basic status
      String mExtendedStatus;                           // extended status property related to status

      String mStatusMessage;                            // human readable message to display about status

      int mPriority {};                                 // relative priority of this status; higher value is a greater priority;

      PresenceStatus();
      PresenceStatus(const PresenceStatus &rValue);

      static PresenceStatusPtr extract(ElementPtr dataEl);
      void insert(ElementPtr dataEl) const;

      bool hasData() const;
      ElementPtr toDebug() const;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark struct PresenceTimeZoneLocation
    #pragma mark

    struct PresenceTimeZoneLocation
    {
      Seconds mOffset {};         // +/- offset from GMT / UTC to calculate local time from UTC time
      String mAbbreviation;       // time zone abbreviation for active time zone
      String mName;               // current time zone full name for active time zone

      String mCity;               // basing time zone off this city's location
      String mCountry;            // basing time zone within this country

      PresenceTimeZoneLocation();
      PresenceTimeZoneLocation(const PresenceTimeZoneLocation &rValue);

      static PresenceTimeZoneLocationPtr extract(ElementPtr dataEl);
      void insert(ElementPtr dataEl) const;

      bool hasData() const;
      ElementPtr toDebug() const;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark struct PresenceGeographicLocation
    #pragma mark

    struct PresenceGeographicLocation
    {
      double mLatitude {};                  // degrees from equator; positive is north and negative is south
      double mLongitude {};                 // degrees from zero meridian
      double mGeographicAccuracyRadius {};  // radious of accuracy for the latitude/longitude as expressed in meters; anegative value indicates an invalid geographic coordinate

      double mAltitude {};                  // height above sea level as measured in meters
      double mAltitudeAccuracy {};          // the absolute value of the + or - altitude accuracy in meters; a negative value indicates an invalid altitude

      double mDirection {};                 // the direction being headed in degrees on a circle (0 = north, 90 = east, 180 = south, 270 = west)
      double mSpeed {};                     // speed moving in direction (meters / second); a negative value indicates the speed/direction are invalid

      PresenceGeographicLocation();
      PresenceGeographicLocation(const PresenceGeographicLocation &rValue);

      static PresenceGeographicLocationPtr extract(ElementPtr dataEl);
      void insert(ElementPtr dataEl) const;

      bool hasData() const;
      ElementPtr toDebug() const;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark struct PresenceStreetLocation
    #pragma mark

    struct PresenceStreetLocation
    {
      String mFriendlyName;               // friendly name representing residence/business, e.g. "Nat's Pup"

      String mSuiteNumber;                // a suite number within a building
      String mBuildingFloor;              // the current floor of a building
      String mBuilding;                   // a building name/number when at an address with multiple buildings

      String mStreetNumber;               // the designated street number
      String mStreetNumberSuffix;         // the street number suffix

      String mStreetDirectionPrefix;      // N S E W NE NW SE SW if applicable
      String mStreetName;                 // name of the street
      String mStreetSuffix;               // E.g. "Ave" or "Dr"
      String mStreetDirectionSuffix;      // N S E W NE NW SE SW if applicable

      String mPostalCommunity;            // residence community, town, or city   (e.g. "Nepean")
      String mServiceCommunity;           // serviced by (typically greater) community, town, or city (e.g. "Ottawa")

      String mProvince;                   // state, province, or territory, (e.g. "Ontario")
      String mCountry;                    // e.g. "Canada"

      PresenceStreetLocation();
      PresenceStreetLocation(const PresenceStreetLocation &rValue);

      static PresenceStreetLocationPtr extract(ElementPtr dataEl);
      void insert(ElementPtr dataEl) const;

      bool hasData() const;
      ElementPtr toDebug() const;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark struct PresenceResources
    #pragma mark

    struct PresenceResources
    {
      struct Resource
      {
        String mID;                       // resources with same ID are alternative formats/dimensions of the same information
        String mRelatedID;                // alternative resources related to an existing ID
        String mType;                     // purpose of resource so remote party can know what to do with resource

        String mFriendlyName;             // human readable friendly name

        String mResourceURL;              // where to download resource
        String mMimeType;                 // mime type of resource
        size_t mSize {};                  // size in bytes of resource; 0 = unkonwn;

        int mWidth {};                    // width in pixels if known; negative means unknown
        int mHeight {};                   // height in pixels if known; negative means unknown
        Seconds mLength {};               // how long is audio/video

        String mExternalLinkURL;          // external link to resource

        String mEncoding;                 // if set, resource is encoded/encrypted using this algorithm/secret (use IEncryptor/IDecryptor)

        bool hasData() const;
      };

      ZS_DECLARE_TYPEDEF_PTR(std::list<Resource>, ResourceList)

      ResourceList mResources;

      PresenceResources();
      PresenceResources(const PresenceResources &rValue);

      static PresenceResourcesPtr extract(ElementPtr dataEl);
      void insert(ElementPtr dataEl) const;

      bool hasData() const;
      ElementPtr toDebug() const;
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

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IPushPresenceTransferDelegate)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::core::IPushPresencePtr, IPushPresencePtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::core::IPushPresenceTransferNotifierPtr, IPushPresenceTransferNotifierPtr)
ZS_DECLARE_PROXY_METHOD_6(onPushPresenceTransferUploadFileDataToURL, IPushPresencePtr, const char *, const char *, ULONGEST, ULONGEST, IPushPresenceTransferNotifierPtr)
ZS_DECLARE_PROXY_METHOD_6(onPushPresenceTransferDownloadDataFromURL, IPushPresencePtr, const char *, const char *, ULONGEST, ULONGEST, IPushPresenceTransferNotifierPtr)
ZS_DECLARE_PROXY_END()
