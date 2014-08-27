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

#include <openpeer/core/internal/core_PushPresence_Status.h>
#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Stringize.h>
#include <zsLib/XML.h>
#include <zsLib/Numeric.h>
#include <zsLib/Log.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    using zsLib::Numeric;
    using zsLib::Log;
    using zsLib::string;

    ZS_DECLARE_TYPEDEF_PTR(stack::message::IMessageHelper, UseMessageHelper)
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //---------------------------------------------------------------------
      static Log::Params PresenceStatus_slog(const char *message)
      {
        return Log::Params(message, "core::PresenceStatus");
      }

      //---------------------------------------------------------------------
      static Log::Params PresenceTimeZoneLocation_slog(const char *message)
      {
        return Log::Params(message, "core::PresenceTimeZoneLocation");
      }

      //---------------------------------------------------------------------
      static Log::Params PresenceGeographicLocation_slog(const char *message)
      {
        return Log::Params(message, "core::PresenceGeographicLocation");
      }

      //---------------------------------------------------------------------
      static double PresenceGeographicLocation_convert(const String &str)
      {
        if (str.isEmpty()) return (double {});

        try {
          return Numeric<double>(str);
        } catch(Numeric<double>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, PresenceGeographicLocation_slog("value out of range"))
        }
        return (double {});
      }

      //---------------------------------------------------------------------
      static Log::Params PresenceResources_slog(const char *message)
      {
        return Log::Params(message, "core::PresenceResources");
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
    #pragma mark PresenceStatus
    #pragma mark

    //-------------------------------------------------------------------------
    const char *PresenceStatus::toString(PresenceStatuses state)
    {
      switch (state) {
        case PresenceStatus_None:       return "none";
        case PresenceStatus_Busy:       return "busy";
        case PresenceStatus_Away:       return "away";
        case PresenceStatus_Idle:       return "idle";
        case PresenceStatus_Available:  return "available";
      }
      return "undefined";
    }

    //-------------------------------------------------------------------------
    PresenceStatus::PresenceStatuses PresenceStatus::toPresenceStatus(const char *inState)
    {
      String state(inState);

      static PresenceStatuses statuses[] = {
        PresenceStatus_Busy,
        PresenceStatus_Away,
        PresenceStatus_Idle,
        PresenceStatus_Available,
        PresenceStatus_None
      };

      for (int index = 0; PresenceStatus_None != statuses[index]; ++index) {
        if (state != toString(statuses[index])) continue;
        return statuses[index];
      }
      return PresenceStatus_None;
    }

    //-------------------------------------------------------------------------
    PresenceStatus::PresenceStatus()
    {
    }

    //-------------------------------------------------------------------------
    PresenceStatus::PresenceStatus(const PresenceStatus &rValue)
    {
      mStatus = rValue.mStatus;
      mExtendedStatus = rValue.mExtendedStatus;

      mStatusMessage = rValue.mStatusMessage;

      mPriority = rValue.mPriority;
    }

    //-------------------------------------------------------------------------
    PresenceStatusPtr PresenceStatus::extract(ElementPtr dataEl)
    {
      if (!dataEl) return PresenceStatusPtr();

      ElementPtr extractEl = dataEl->findFirstChildElement("status");
      if (!extractEl) return PresenceStatusPtr();

      PresenceStatusPtr pThis(new PresenceStatus);
      pThis->mStatus = toPresenceStatus(UseMessageHelper::getElementText(extractEl->findFirstChildElement("status")));
      pThis->mExtendedStatus = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("extended"));
      pThis->mStatusMessage = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("message"));

      String priorityStr = UseMessageHelper::getElementText(extractEl->findFirstChildElement("priority"));

      if (priorityStr.hasData()) {
        try {
          pThis->mPriority = Numeric<decltype(pThis->mPriority)>(priorityStr);
        } catch(Numeric<decltype(pThis->mPriority)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, internal::PresenceStatus_slog("value out of range"))
        }
      }

      return pThis;
    }

    //-------------------------------------------------------------------------
    void PresenceStatus::insert(ElementPtr dataEl) const
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!dataEl)

      ElementPtr existingEl = dataEl->findFirstChildElement("status");

      ElementPtr insertEl = Element::create("status");

      if (PresenceStatus_None != mStatus) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithText("status", toString(mStatus)));
      }

      if (mExtendedStatus.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("extended", mExtendedStatus));
      }

      if (mStatusMessage.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("message", mStatusMessage));
      }

      if (0 != mPriority) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("priority", string(mPriority)));
      }

      if (existingEl) {
        if (insertEl->hasChildren()) existingEl->adoptAsNextSibling(insertEl);
        existingEl->orphan();
      } else {
        if (insertEl->hasChildren()) dataEl->adoptAsLastChild(insertEl);
      }
    }

    //-------------------------------------------------------------------------
    bool PresenceStatus::hasData() const
    {
      return ((PresenceStatus_None != mStatus) ||
              (mExtendedStatus.hasData()) ||
              (mStatusMessage.hasData()) ||
              (0 != mPriority));
    }

    //-------------------------------------------------------------------------
    ElementPtr PresenceStatus::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::PresenceStatus");

      UseServicesHelper::debugAppend(resultEl, "status", toString(mStatus));
      UseServicesHelper::debugAppend(resultEl, "extended", mExtendedStatus);
      UseServicesHelper::debugAppend(resultEl, "message", mStatusMessage);
      UseServicesHelper::debugAppend(resultEl, "priority", mPriority);

      return resultEl;
    }


    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark PresenceTimeZoneLocation
    #pragma mark

    //-------------------------------------------------------------------------
    PresenceTimeZoneLocation::PresenceTimeZoneLocation()
    {
    }

    //-------------------------------------------------------------------------
    PresenceTimeZoneLocation::PresenceTimeZoneLocation(const PresenceTimeZoneLocation &rValue)
    {
      mOffset = rValue.mOffset;
      mAbbreviation = rValue.mAbbreviation;
      mName = rValue.mName;

      mCity = rValue.mCity;
      mCountry = rValue.mCountry;
    }

    //-------------------------------------------------------------------------
    PresenceTimeZoneLocationPtr PresenceTimeZoneLocation::extract(ElementPtr dataEl)
    {
      if (!dataEl) return PresenceTimeZoneLocationPtr();

      ElementPtr extractEl = dataEl->findFirstChildElement("timeZone");
      if (!extractEl) return PresenceTimeZoneLocationPtr();

      PresenceTimeZoneLocationPtr pThis(new PresenceTimeZoneLocation);

      String offsetStr = UseMessageHelper::getElementText(extractEl->findFirstChildElement("offset"));
      if (offsetStr.hasData()) {
        try {
          pThis->mOffset = Seconds(Numeric<Duration::sec_type>(offsetStr));
        } catch(Numeric<Duration::sec_type>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, internal::PresenceTimeZoneLocation_slog("value out of range"))
        }
      }

      pThis->mAbbreviation = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("abbreviation"));
      pThis->mName = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("name"));

      pThis->mCity = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("city"));
      pThis->mCountry = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("country"));

      return pThis;
    }

    //-------------------------------------------------------------------------
    void PresenceTimeZoneLocation::insert(ElementPtr dataEl) const
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!dataEl)

      ElementPtr existingEl = dataEl->findFirstChildElement("timeZone");

      ElementPtr insertEl = Element::create("timeZone");

      if (Duration() != mOffset) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("priority", string(mOffset.total_seconds())));
      }

      if (mAbbreviation.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("abbreviation", mAbbreviation));
      }

      if (mName.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("name", mName));
      }

      if (mCity.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("city", mCity));
      }

      if (mCountry.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("country", mCountry));
      }

      if (existingEl) {
        if (insertEl->hasChildren()) existingEl->adoptAsNextSibling(insertEl);
        existingEl->orphan();
      } else {
        if (insertEl->hasChildren()) dataEl->adoptAsLastChild(insertEl);
      }
    }

    //-------------------------------------------------------------------------
    bool PresenceTimeZoneLocation::hasData() const
    {
      return ((Duration() != mOffset) ||
              (mAbbreviation.hasData()) ||
              (mName.hasData()) ||

              (mCity.hasData()) ||
              (mCountry.hasData()));
    }

    //-------------------------------------------------------------------------
    ElementPtr PresenceTimeZoneLocation::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::PresenceTimeZoneLocation");

      UseServicesHelper::debugAppend(resultEl, "offset", mOffset);
      UseServicesHelper::debugAppend(resultEl, "abbreviation", mAbbreviation);
      UseServicesHelper::debugAppend(resultEl, "name", mName);

      UseServicesHelper::debugAppend(resultEl, "city", mCity);
      UseServicesHelper::debugAppend(resultEl, "city", mCountry);

      return resultEl;
    }


    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark PresenceGeographicLocation
    #pragma mark

    //-------------------------------------------------------------------------
    PresenceGeographicLocation::PresenceGeographicLocation()
    {
    }

    //-------------------------------------------------------------------------
    PresenceGeographicLocation::PresenceGeographicLocation(const PresenceGeographicLocation &rValue)
    {
      mLatitude = rValue.mLatitude;
      mLongitude = rValue.mLongitude;
      mGeographicAccuracyRadius = rValue.mGeographicAccuracyRadius;

      mAltitude = rValue.mAltitude;
      mAltitudeAccuracy = rValue.mAltitudeAccuracy;

      mDirection = rValue.mDirection;
      mSpeed = rValue.mSpeed;
    }

    //-------------------------------------------------------------------------
    PresenceGeographicLocationPtr PresenceGeographicLocation::extract(ElementPtr dataEl)
    {
      if (!dataEl) return PresenceGeographicLocationPtr();

      ElementPtr extractEl = dataEl->findFirstChildElement("geographicLocation");
      if (!extractEl) return PresenceGeographicLocationPtr();

      PresenceGeographicLocationPtr pThis(new PresenceGeographicLocation);

      pThis->mLatitude = internal::PresenceGeographicLocation_convert(UseMessageHelper::getElementText(extractEl->findFirstChildElement("latitude")));
      pThis->mLongitude = internal::PresenceGeographicLocation_convert(UseMessageHelper::getElementText(extractEl->findFirstChildElement("longitude")));
      pThis->mGeographicAccuracyRadius = internal::PresenceGeographicLocation_convert(UseMessageHelper::getElementText(extractEl->findFirstChildElement("geographicAccuracyRadius")));

      pThis->mAltitude = internal::PresenceGeographicLocation_convert(UseMessageHelper::getElementText(extractEl->findFirstChildElement("altitude")));
      pThis->mAltitudeAccuracy = internal::PresenceGeographicLocation_convert(UseMessageHelper::getElementText(extractEl->findFirstChildElement("altitudeAccuracy")));

      pThis->mDirection = internal::PresenceGeographicLocation_convert(UseMessageHelper::getElementText(extractEl->findFirstChildElement("direction")));
      pThis->mSpeed = internal::PresenceGeographicLocation_convert(UseMessageHelper::getElementText(extractEl->findFirstChildElement("speed")));

      return pThis;
    }

    //-------------------------------------------------------------------------
    void PresenceGeographicLocation::insert(ElementPtr dataEl) const
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!dataEl)

      ElementPtr existingEl = dataEl->findFirstChildElement("geographicLocation");

      ElementPtr insertEl = Element::create("geographicLocation");

      if (0.0L != mLatitude) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("latitude", string(mLatitude)));
      }
      if (0.0L != mLongitude) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("longitude", string(mLongitude)));
      }
      if (0.0L != mGeographicAccuracyRadius) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("geographicAccuracyRadius", string(mGeographicAccuracyRadius)));
      }

      if (0.0L != mAltitude) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("altitude", string(mAltitude)));
      }
      if (0.0L != mAltitudeAccuracy) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("altitudeAccuracy", string(mAltitudeAccuracy)));
      }

      if (0.0L != mDirection) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("direction", string(mDirection)));
      }
      if (0.0L != mSpeed) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("speed", string(mSpeed)));
      }

      if (existingEl) {
        if (insertEl->hasChildren()) existingEl->adoptAsNextSibling(insertEl);
        existingEl->orphan();
      } else {
        if (insertEl->hasChildren()) dataEl->adoptAsLastChild(insertEl);
      }
    }

    //-------------------------------------------------------------------------
    bool PresenceGeographicLocation::hasData() const
    {
      return ((0.0L != mLatitude) ||
              (0.0L != mLongitude) ||
              (0.0L != mGeographicAccuracyRadius) ||

              (0.0L != mAltitude) ||
              (0.0L != mAltitudeAccuracy) ||

              (0.0L != mDirection) ||
              (0.0L != mSpeed));
    }

    //-------------------------------------------------------------------------
    ElementPtr PresenceGeographicLocation::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::PresenceGeographicLocation");

      UseServicesHelper::debugAppend(resultEl, "latitude", mLatitude);
      UseServicesHelper::debugAppend(resultEl, "longitude", mLongitude);
      UseServicesHelper::debugAppend(resultEl, "geographic accuracy radius", mGeographicAccuracyRadius);

      UseServicesHelper::debugAppend(resultEl, "altitude", mAltitude);
      UseServicesHelper::debugAppend(resultEl, "altitude accuracy", mAltitudeAccuracy);

      UseServicesHelper::debugAppend(resultEl, "direction", mDirection);
      UseServicesHelper::debugAppend(resultEl, "speed", mSpeed);

      return resultEl;
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark PresenceStreetLocation
    #pragma mark

    //-------------------------------------------------------------------------
    PresenceStreetLocation::PresenceStreetLocation()
    {
    }

    //-------------------------------------------------------------------------
    PresenceStreetLocation::PresenceStreetLocation(const PresenceStreetLocation &rValue)
    {
      mFriendlyName = rValue.mFriendlyName;

      mSuiteNumber = rValue.mSuiteNumber;
      mBuildingFloor = rValue.mBuildingFloor;
      mBuilding = rValue.mBuilding;

      mStreetNumber = rValue.mStreetNumber;
      mStreetNumberSuffix = rValue.mStreetNumberSuffix;

      mStreetDirectionPrefix = rValue.mStreetDirectionPrefix;
      mStreetName = rValue.mStreetName;
      mStreetSuffix = rValue.mStreetSuffix;
      mStreetDirectionSuffix = rValue.mStreetDirectionSuffix;

      mPostalCommunity = rValue.mPostalCommunity;
      mServiceCommunity = rValue.mServiceCommunity;

      mProvince = rValue.mProvince;
      mCountry = rValue. mCountry;
    }

    //-------------------------------------------------------------------------
    PresenceStreetLocationPtr PresenceStreetLocation::extract(ElementPtr dataEl)
    {
      if (!dataEl) return PresenceStreetLocationPtr();

      ElementPtr extractEl = dataEl->findFirstChildElement("streetLocation");
      if (!extractEl) return PresenceStreetLocationPtr();

      PresenceStreetLocationPtr pThis(new PresenceStreetLocation);

      pThis->mFriendlyName = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("friendlyName"));

      pThis->mSuiteNumber = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("suiteNumber"));
      pThis->mBuildingFloor = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("buildingFloor"));
      pThis->mBuilding = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("building"));

      pThis->mStreetNumber = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("streetNumber"));
      pThis->mStreetNumberSuffix = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("streetNumberSuffix"));

      pThis->mStreetDirectionPrefix = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("streetDirectionPrefix"));
      pThis->mStreetName = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("streetName"));
      pThis->mStreetSuffix = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("streetSuffix"));
      pThis->mStreetDirectionSuffix = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("streetDirectionSuffix"));

      pThis->mPostalCommunity = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("postalCommunity"));
      pThis->mServiceCommunity = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("serviceCommunity"));

      pThis->mProvince = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("province"));
      pThis->mCountry = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("country"));

      return pThis;
    }

    //-------------------------------------------------------------------------
    void PresenceStreetLocation::insert(ElementPtr dataEl) const
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!dataEl)

      ElementPtr existingEl = dataEl->findFirstChildElement("streetLocation");

      ElementPtr insertEl = Element::create("streetLocation");

      if (mFriendlyName.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("friendlyName", mFriendlyName));
      }

      if (mSuiteNumber.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("suiteNumber", mSuiteNumber));
      }
      if (mBuildingFloor.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("buildingFloor", mBuildingFloor));
      }
      if (mBuilding.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("building", mBuilding));
      }

      if (mStreetNumber.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("streetNumber", mStreetNumber));
      }
      if (mStreetNumberSuffix.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("streetNumberSuffix", mStreetNumberSuffix));
      }

      if (mStreetDirectionPrefix.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("streetDirectionPrefix", mStreetDirectionPrefix));
      }
      if (mStreetName.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("streetName", mStreetName));
      }
      if (mStreetSuffix.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("streetSuffix", mStreetSuffix));
      }
      if (mStreetDirectionSuffix.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("streetDirectionSuffix", mStreetDirectionSuffix));
      }

      if (mPostalCommunity.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("postalCommunity", mPostalCommunity));
      }
      if (mServiceCommunity.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("serviceCommunity", mServiceCommunity));
      }

      if (mProvince.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("province", mProvince));
      }
      if (mCountry.hasData()) {
        insertEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("country", mCountry));
      }

      if (existingEl) {
        if (insertEl->hasChildren()) existingEl->adoptAsNextSibling(insertEl);
        existingEl->orphan();
      } else {
        if (insertEl->hasChildren()) dataEl->adoptAsLastChild(insertEl);
      }
    }

    //-------------------------------------------------------------------------
    bool PresenceStreetLocation::hasData() const
    {
      return ((mFriendlyName.hasData()) ||

              (mSuiteNumber.hasData()) ||
              (mBuildingFloor.hasData()) ||
              (mBuilding.hasData()) ||

              (mStreetNumber.hasData()) ||
              (mStreetNumberSuffix.hasData()) ||

              (mStreetDirectionPrefix.hasData()) ||
              (mStreetName.hasData()) ||
              (mStreetSuffix.hasData()) ||
              (mStreetDirectionSuffix.hasData()) ||

              (mPostalCommunity.hasData()) ||
              (mServiceCommunity.hasData()) ||

              (mProvince.hasData()) ||
              (mCountry.hasData()));
    }

    //-------------------------------------------------------------------------
    ElementPtr PresenceStreetLocation::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::PresenceStreetLocation");

      UseServicesHelper::debugAppend(resultEl, "friendly name", mFriendlyName);

      UseServicesHelper::debugAppend(resultEl, "suite number", mSuiteNumber);
      UseServicesHelper::debugAppend(resultEl, "building floor", mBuildingFloor);
      UseServicesHelper::debugAppend(resultEl, "building", mBuilding);

      UseServicesHelper::debugAppend(resultEl, "street number", mStreetNumber);
      UseServicesHelper::debugAppend(resultEl, "street number suffix", mStreetNumberSuffix);

      UseServicesHelper::debugAppend(resultEl, "street direction prefix", mStreetDirectionPrefix);
      UseServicesHelper::debugAppend(resultEl, "street name", mStreetName);
      UseServicesHelper::debugAppend(resultEl, "street suffix", mStreetSuffix);
      UseServicesHelper::debugAppend(resultEl, "street direction suffix", mStreetDirectionSuffix);

      UseServicesHelper::debugAppend(resultEl, "postal community", mPostalCommunity);
      UseServicesHelper::debugAppend(resultEl, "service community", mServiceCommunity);

      UseServicesHelper::debugAppend(resultEl, "province", mProvince);
      UseServicesHelper::debugAppend(resultEl, "country", mCountry);

      return resultEl;
    }


    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark PresenceResources
    #pragma mark

    //-------------------------------------------------------------------------
    PresenceResources::PresenceResources()
    {
    }

    //-------------------------------------------------------------------------
    PresenceResources::PresenceResources(const PresenceResources &rValue)
    {
      mResources = rValue.mResources;
    }

    //-------------------------------------------------------------------------
    PresenceResourcesPtr PresenceResources::extract(ElementPtr dataEl)
    {
      if (!dataEl) return PresenceResourcesPtr();

      ElementPtr extractEl = dataEl->findFirstChildElement("resources");
      if (!extractEl) return PresenceResourcesPtr();

      PresenceResourcesPtr pThis(new PresenceResources);

      ElementPtr resourceEl = extractEl->findFirstChildElement("resource");
      while (resourceEl) {
        Resource info;

        info.mID = UseMessageHelper::getAttributeID(resourceEl);
        info.mRelatedID = UseMessageHelper::getAttribute(resourceEl, "related");
        info.mType = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("type"));

        info.mFriendlyName = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("friendlyName"));

        info.mResourceURL = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("resourceURL"));
        info.mMimeType = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("mimeType"));

        String sizeStr = UseMessageHelper::getElementText(extractEl->findFirstChildElement("size"));
        if (sizeStr.hasData()) {
          try {
            info.mSize = Numeric<decltype(info.mSize)>(sizeStr);
          } catch(Numeric<decltype(info.mSize)>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, internal::PresenceResources_slog("value out of range"))
          }
        }

        String widthStr = UseMessageHelper::getElementText(extractEl->findFirstChildElement("width"));
        if (widthStr.hasData()) {
          try {
            info.mSize = Numeric<decltype(info.mWidth)>(widthStr);
          } catch(Numeric<decltype(info.mWidth)>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, internal::PresenceResources_slog("value out of range"))
          }
        }
        String heightStr = UseMessageHelper::getElementText(extractEl->findFirstChildElement("height"));
        if (heightStr.hasData()) {
          try {
            info.mSize = Numeric<decltype(info.mHeight)>(heightStr);
          } catch(Numeric<decltype(info.mHeight)>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, internal::PresenceResources_slog("value out of range"))
          }
        }
        String lengthStr = UseMessageHelper::getElementText(extractEl->findFirstChildElement("length"));
        if (lengthStr.hasData()) {
          try {
            info.mSize = Numeric<Duration::tick_type>(lengthStr);
          } catch(Numeric<Duration::tick_type>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, internal::PresenceResources_slog("value out of range"))
          }
        }

        info.mExternalLinkURL = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("externalLinkURL"));
        info.mEncoding = UseMessageHelper::getElementTextAndDecode(extractEl->findFirstChildElement("encoding"));

        if (info.hasData()) {
          pThis->mResources.push_back(info);
        }

        resourceEl = resourceEl->findNextSiblingElement("resource");
      }

      return pThis;
    }

    //-------------------------------------------------------------------------
    void PresenceResources::insert(ElementPtr dataEl) const
    {
      ElementPtr existingEl = dataEl->findFirstChildElement("resources");

      ElementPtr insertEl = Element::create("resources");

      for (ResourceList::const_iterator iter = mResources.begin(); iter != mResources.end(); ++iter) {
        const Resource &info = (*iter);

        ElementPtr resourceEl = UseMessageHelper::createElementWithID("resource", info.mID);

        if (info.mRelatedID) {
          resourceEl->setAttribute("related", info.mRelatedID);
        }

        if (info.mType.hasData()) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("type", info.mType));
        }

        if (info.mFriendlyName.hasData()) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("friendlyName", info.mFriendlyName));
        }

        if (info.mResourceURL.hasData()) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("resourceURL", info.mResourceURL));
        }
        if (info.mMimeType.hasData()) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("mimeType", info.mMimeType));
        }
        if (0 != info.mSize) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("size", string(info.mSize)));
        }

        if (0 != info.mWidth) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("width", string(info.mWidth)));
        }
        if (0 != info.mHeight) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("height", string(info.mHeight)));
        }
        if (Duration() != info.mLength) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("length", string(info.mLength.total_milliseconds())));
        }

        if (info.mExternalLinkURL.hasData()) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("externalLinkURL", info.mExternalLinkURL));
        }
        if (info.mEncoding.hasData()) {
          resourceEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode("encoding", info.mEncoding));
        }

        if (resourceEl->hasChildren()) {
          insertEl->adoptAsLastChild(resourceEl);
        }
      }

      if (existingEl) {
        if (insertEl->hasChildren()) existingEl->adoptAsNextSibling(insertEl);
        existingEl->orphan();
      } else {
        if (insertEl->hasChildren()) dataEl->adoptAsLastChild(insertEl);
      }
    }

    //-------------------------------------------------------------------------
    bool PresenceResources::hasData() const
    {
      return (mResources.size() > 0);
    }

    //-------------------------------------------------------------------------
    ElementPtr PresenceResources::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::PresenceResources");

      UseServicesHelper::debugAppend(resultEl, "resources", mResources.size());

      return resultEl;
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark PresenceResources::Resource
    #pragma mark

    //-------------------------------------------------------------------------
    bool PresenceResources::Resource::hasData() const
    {
      return ((mID.hasData()) ||
              (mRelatedID.hasData()) ||
              (mType.hasData()) ||

              (mFriendlyName.hasData()) ||

              (mResourceURL.hasData()) ||
              (mMimeType.hasData()) ||
              (0 != mSize) ||

              (0 != mWidth) ||
              (0 != mHeight) ||
              (Duration() != mLength) ||

              (mExternalLinkURL.hasData()) ||
              (mEncoding.hasData()));
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
