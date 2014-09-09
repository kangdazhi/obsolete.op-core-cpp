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

#include <openpeer/core/internal/core_Helper.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IHelperForInternal
      #pragma mark

      String IHelperForInternal::hash(ElementPtr element)
      {
        if (!element) return String();

        String result = Helper::convertToString(element);
        return UseServicesHelper::convertToHex(*UseServicesHelper::hash(result));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Helper
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Helper::createElement(const String &elementStr)
      {
        return UseServicesHelper::toJSON(elementStr);
      }

      //-----------------------------------------------------------------------
      String Helper::convertToString(const ElementPtr &element)
      {
        return UseServicesHelper::toString(element);
      }

      //-----------------------------------------------------------------------
      ElementPtr Helper::clone(const ElementPtr &element)
      {
        if (!element) return ElementPtr();
        return element->clone()->toElement();
      }

      //-----------------------------------------------------------------------
      String Helper::getPeerURI(IPeerFilePublicPtr peerFilePublic)
      {
        if (!peerFilePublic) return String();
        return peerFilePublic->getPeerURI();
      }

      //-----------------------------------------------------------------------
      IPeerFilePublicPtr Helper::createPeerFilePublic(const ElementPtr &element)
      {
        if (!element) return IPeerFilePublicPtr();
        return IPeerFilePublic::loadFromElement(element);
      }

      //-----------------------------------------------------------------------
      ElementPtr Helper::convertToElement(IPeerFilePublicPtr peerFilePublic)
      {
        if (!peerFilePublic) return ElementPtr();
        return peerFilePublic->saveToElement();
      }

      //-----------------------------------------------------------------------
      void Helper::convert(
                           const IdentityInfo &source,
                           IdentityContact &outContact
                           )
      {
        outContact.mIdentityURI = source.mURI;
        outContact.mIdentityProvider = source.mProvider;
        outContact.mStableID = source.mStableID;

        outContact.mPeerFilePublic = source.mPeerFilePublic;
        outContact.mIdentityProofBundleEl = source.mIdentityProofBundle;

        outContact.mPriority = source.mPriority;
        outContact.mWeight = source.mWeight;

        outContact.mLastUpdated = source.mUpdated;
        outContact.mExpires = source.mExpires;

        outContact.mName = source.mName;
        outContact.mProfileURL = source.mProfile;
        outContact.mVProfileURL = source.mVProfile;

        for (IdentityInfo::AvatarList::const_iterator avIter = source.mAvatars.begin();  avIter != source.mAvatars.end(); ++avIter)
        {
          const IdentityInfo::Avatar &resultAvatar = (*avIter);
          IdentityContact::Avatar avatar;

          avatar.mName = resultAvatar.mName;
          avatar.mURL = resultAvatar.mURL;
          avatar.mWidth = resultAvatar.mWidth;
          avatar.mHeight = resultAvatar.mHeight;
          outContact.mAvatars.push_back(avatar);
        }
      }

      //-----------------------------------------------------------------------
      void Helper::convert(
                           const IdentityContact &source,
                           IdentityInfo &outContact
                           )
      {
        outContact.mURI = source.mIdentityURI;
        outContact.mProvider = source.mIdentityProvider;
        outContact.mStableID = source.mStableID;

        outContact.mPeerFilePublic = source.mPeerFilePublic;
        outContact.mIdentityProofBundle = source.mIdentityProofBundleEl;

        outContact.mPriority = source.mPriority;
        outContact.mWeight = source.mWeight;

        outContact.mUpdated = source.mLastUpdated;
        outContact.mExpires = source.mExpires;

        outContact.mName = source.mName;
        outContact.mProfile = source.mProfileURL;
        outContact.mVProfile = source.mVProfileURL;

        for (IdentityContact::AvatarList::const_iterator avIter = source.mAvatars.begin();  avIter != source.mAvatars.end(); ++avIter)
        {
          const IdentityContact::Avatar &resultAvatar = (*avIter);
          IdentityInfo::Avatar avatar;

          avatar.mName = resultAvatar.mName;
          avatar.mURL = resultAvatar.mURL;
          avatar.mWidth = resultAvatar.mWidth;
          avatar.mHeight = resultAvatar.mHeight;
          outContact.mAvatars.push_back(avatar);
        }
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IHelper
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IHelper::createElement(const String &elementStr)
    {
      return internal::Helper::createElement(elementStr);
    }

    //-------------------------------------------------------------------------
    String IHelper::convertToString(const ElementPtr &element)
    {
      return internal::Helper::convertToString(element);
    }

    //-------------------------------------------------------------------------
    ElementPtr IHelper::clone(const ElementPtr &element)
    {
      return internal::Helper::clone(element);
    }
    
    //-------------------------------------------------------------------------
    String IHelper::getPeerURI(IPeerFilePublicPtr peerFilePublic)
    {
      return internal::Helper::getPeerURI(peerFilePublic);
    }
      
    //-------------------------------------------------------------------------
    IPeerFilePublicPtr IHelper::createPeerFilePublic(const ElementPtr &element)
    {
      return internal::Helper::createPeerFilePublic(element);
    }
     
    //-------------------------------------------------------------------------  
    ElementPtr IHelper::convertToElement(IPeerFilePublicPtr peerFilePublic)
    {
      return internal::Helper::convertToElement(peerFilePublic);
    }
  }
}
