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

#include <openpeer/core/core.h>
#include <openpeer/core/internal/core.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/core/ComposingStatus.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_IMPLEMENT_SUBSYSTEM(openpeer_core) } }
namespace openpeer { namespace core { ZS_IMPLEMENT_SUBSYSTEM(openpeer_media) } }
namespace openpeer { namespace core { ZS_IMPLEMENT_SUBSYSTEM(openpeer_webrtc) } }
ZS_IMPLEMENT_SUBSYSTEM(openpeer_sdk)
namespace openpeer { namespace core { namespace application { ZS_IMPLEMENT_SUBSYSTEM(openpeer_application) } } }

namespace openpeer
{
  namespace core
  {
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(IHelperForInternal, UseHelper)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ContactStatusInfo
      #pragma mark

      //-----------------------------------------------------------------------
      ContactStatusInfo::ContactStatusInfo()
      {
      }

      //-----------------------------------------------------------------------
      ContactStatusInfo::ContactStatusInfo(const ElementPtr &statusEl)
      {
        mStatusEl = statusEl ? statusEl->clone()->toElement() : ElementPtr();
        if (mStatusEl) {
          mCreated = UseServicesHelper::stringToTime(statusEl->getAttributeValue("created"));
          if (Time() == mCreated) mCreated = zsLib::now();
        }
        mStatusHash = UseHelper::hash(statusEl);
      }

      //-----------------------------------------------------------------------
      ContactStatusInfo::ContactStatusInfo(const ContactStatusInfo &rValue)
      {
        mStatusEl = rValue.mStatusEl ? rValue.mStatusEl->clone()->toElement() : ElementPtr();
        mStatusHash = rValue.mStatusHash;
        mCreated = rValue.mCreated;
      }

      //-----------------------------------------------------------------------
      bool ContactStatusInfo::hasData() const
      {
        return ((Time() != mCreated) ||
                ((bool)mStatusEl) ||
                (mStatusHash.hasData()));
      }

      //-----------------------------------------------------------------------
      ElementPtr ContactStatusInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("core::ContactStatusInfo");

        UseServicesHelper::debugAppend(resultEl, "created", mCreated);
        UseServicesHelper::debugAppend(resultEl, "status", (bool)mStatusEl);
        ComposingStatusPtr status = ComposingStatus::extract(mStatusEl);
        UseServicesHelper::debugAppend(resultEl, "composing", status ? status->toDebug() : ElementPtr());
        UseServicesHelper::debugAppend(resultEl, "status hash", mStatusHash);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      bool ContactStatusInfo::operator==(const ContactStatusInfo &rValue) const
      {
        if (mCreated != rValue.mCreated) return false;
        if (mStatusHash != rValue.mStatusHash) return false;
        if (((bool)mStatusEl) != ((bool)rValue.mStatusEl)) return false;

        return true;
      }

      //-----------------------------------------------------------------------
      bool ContactStatusInfo::operator!=(const ContactStatusInfo &rValue) const
      {
        return !((*this) == rValue);
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark RolodexContact
    #pragma mark

    //-------------------------------------------------------------------------
    RolodexContact::RolodexContact() :
      mDisposition(Disposition_NA)
    {
    }

    //-------------------------------------------------------------------------
    bool RolodexContact::hasData() const
    {
      return ((Disposition_NA != mDisposition) ||
              (mIdentityURI.hasData()) ||
              (mIdentityProvider.hasData()) ||
              (mName.hasData()) ||
              (mProfileURL.hasData()) ||
              (mVProfileURL.hasData()) ||
              (mAvatars.size() > 0));
    }

    //-------------------------------------------------------------------------
    bool RolodexContact::Avatar::operator==(const Avatar &rValue) const
    {
      if (mName != rValue.mName) return false;
      if (mURL != rValue.mURL) return false;
      if (mWidth != rValue.mWidth) return false;
      if (mHeight != rValue.mHeight) return false;

      return true;
    }

    //-------------------------------------------------------------------------
    bool RolodexContact::Avatar::operator!=(const Avatar &rValue) const
    {
      return !(*this == rValue);
    }

    //-------------------------------------------------------------------------
    bool RolodexContact::operator==(const RolodexContact &rValue) const
    {
      if (mIdentityURI != rValue.mIdentityURI) return false;
      if (mIdentityProvider != rValue.mIdentityProvider) return false;
      if (mName != rValue.mName) return false;
      if (mProfileURL != rValue.mProfileURL) return false;
      if (mVProfileURL != rValue.mVProfileURL) return false;
      if (mAvatars.size() != rValue.mAvatars.size()) return false;

      for (AvatarList::const_iterator iter1 = mAvatars.begin(), iter2 = rValue.mAvatars.begin(); iter1 != mAvatars.end() && iter2 != rValue.mAvatars.end(); ++iter1, ++iter2)
      {
        const Avatar &av1 = (*iter1);
        const Avatar &av2 = (*iter1);
        if (av1 != av2) return false;
      }

      return true;
    }

    //-------------------------------------------------------------------------
    bool RolodexContact::operator!=(const RolodexContact &rValue) const
    {
      return !(*this == rValue);
    }
    

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IdentityContact
    #pragma mark

    //-------------------------------------------------------------------------
    IdentityContact::IdentityContact() :
      RolodexContact(),
      mPriority(0),
      mWeight(0)
    {
    }

    //-------------------------------------------------------------------------
    IdentityContact::IdentityContact(const RolodexContact &rolodexInfo) :
      mPriority(0),
      mWeight(0)
    {
      // rolodex disposition is "lost" as it has no meaning once translated into an actual identity structure
      mDisposition = rolodexInfo.mDisposition;
      mIdentityURI = rolodexInfo.mIdentityURI;
      mIdentityProvider = rolodexInfo.mIdentityProvider;

      mName = rolodexInfo.mName;
      mProfileURL = rolodexInfo.mProfileURL;
      mVProfileURL = rolodexInfo.mVProfileURL;
      mAvatars = rolodexInfo.mAvatars;
    }

    //-------------------------------------------------------------------------
    bool IdentityContact::hasData() const
    {
      return ((mIdentityURI.hasData()) ||
              (mIdentityProvider.hasData()) ||
              (mStableID.hasData()) ||
              (mPeerFilePublic) ||
              (mIdentityProofBundleEl) ||
              (0 != mPriority) ||
              (0 != mWeight) ||
              (Time() != mLastUpdated) ||
              (Time() != mExpires) ||
              (mName.hasData()) ||
              (mProfileURL.hasData()) ||
              (mVProfileURL.hasData()) ||
              (mAvatars.size() > 0));
    }

    //-------------------------------------------------------------------------
    bool IdentityContact::operator==(const IdentityContact &rValue) const
    {
      const RolodexContact &rolo1 = *this;
      const RolodexContact &rolo2 = rValue;

      if (rolo1 != rolo2) return false;

      if (mStableID != rValue.mStableID) return false;
      if (mPeerFilePublic != rValue.mPeerFilePublic) return false;
      if (mIdentityProofBundleEl != rValue.mIdentityProofBundleEl) return false;
      if (mPriority != rValue.mPriority) return false;
      if (mWeight != rValue.mWeight) return false;
      if (mLastUpdated != rValue.mLastUpdated) return false;
      if (mExpires != rValue.mExpires) return false;

      return true;
    }

    //-------------------------------------------------------------------------
    bool IdentityContact::operator!=(const IdentityContact &rValue) const
    {
      return !(*this == rValue);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IdentityContact
    #pragma mark

    //-------------------------------------------------------------------------
    bool ContactProfileInfo::hasData() const
    {
      return ((mContact) ||
              (mIdentityContacts.size() > 0));
    }

  }
}
