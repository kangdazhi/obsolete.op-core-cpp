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

#include <openpeer/stack/types.h>
#include <openpeer/stack/message/types.h>

#include <zsLib/types.h>
#include <zsLib/Proxy.h>

#include <list>

namespace openpeer
{
  namespace core
  {
    using zsLib::PUID;
    using zsLib::WORD;
    using zsLib::UINT;
    using zsLib::LONG;
    using zsLib::ULONG;
    using zsLib::LONGEST;
    using zsLib::ULONGEST;
    using zsLib::BYTE;
    using zsLib::Time;
    using zsLib::String;
    using zsLib::PTRNUMBER;
    typedef PTRNUMBER SubsystemID;
    using zsLib::Hours;
    using zsLib::Minutes;
    using zsLib::Seconds;
    using zsLib::Seconds;
    using zsLib::Milliseconds;
    using zsLib::Microseconds;
    using zsLib::Nanoseconds;

    ZS_DECLARE_USING_PTR(zsLib::XML, Element)

    using openpeer::services::SharedRecursiveLock;
    using openpeer::services::LockedValue;

    ZS_DECLARE_USING_PTR(openpeer::services, SecureByteBlock)

    ZS_DECLARE_USING_PTR(openpeer::stack, IPeerFilePublic)

    ZS_DECLARE_USING_PTR(openpeer::stack::message, IMessageHelper)

    // other types
    struct ContactProfileInfo;
    struct IdentityContact;
    struct RolodexContact;

    ZS_DECLARE_USING_PTR(services, RecursiveLock)

    ZS_DECLARE_INTERACTION_PTR(IContact)

    struct CallSystemMessage;
    ZS_DECLARE_PTR(CallSystemMessage)

    struct ComposingStatus;
    ZS_DECLARE_PTR(ComposingStatus)

    struct PresenceStatus;
    struct PresenceTimeZoneLocation;
    struct PresenceGeographicLocation;
    struct PresenceStreetLocation;
    struct PresenceResources;

    ZS_DECLARE_PTR(PresenceStatus)
    ZS_DECLARE_PTR(PresenceTimeZoneLocation)
    ZS_DECLARE_PTR(PresenceGeographicLocation)
    ZS_DECLARE_PTR(PresenceStreetLocation)
    ZS_DECLARE_PTR(PresenceResources)

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark RolodexContact
    #pragma mark

    struct RolodexContact
    {
      struct Avatar
      {
        String mName;
        String mURL;
        int mWidth;
        int mHeight;

        bool operator==(const Avatar &rValue) const;
        bool operator!=(const Avatar &rValue) const;
      };
      typedef std::list<Avatar> AvatarList;

      enum Dispositions
      {
        Disposition_NA,
        Disposition_Update,
        Disposition_Remove,
      };

      Dispositions mDisposition;
      String mIdentityURI;
      String mIdentityProvider;

      String mName;
      String mProfileURL;
      String mVProfileURL;

      AvatarList mAvatars;

      RolodexContact();
      bool hasData() const;

      bool operator==(const RolodexContact &rValue) const;
      bool operator!=(const RolodexContact &rValue) const;
    };
    
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IdentityContact
    #pragma mark

    struct IdentityContact : public RolodexContact
    {
      String mStableID;

      IPeerFilePublicPtr mPeerFilePublic;
      ElementPtr mIdentityProofBundleEl;

      WORD mPriority;
      WORD mWeight;

      Time mLastUpdated;
      Time mExpires;

      IdentityContact();
      IdentityContact(const RolodexContact &);
      bool hasData() const;

      bool operator==(const IdentityContact &rValue) const;
      bool operator!=(const IdentityContact &rValue) const;
    };

    ZS_DECLARE_TYPEDEF_PTR(std::list<IdentityContact>, IdentityContactList)

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ContactProfileInfo
    #pragma mark

    struct ContactProfileInfo
    {
      IContactPtr mContact;
      IdentityContactList mIdentityContacts;

      bool hasData() const;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark (other)
    #pragma mark

    ZS_DECLARE_INTERACTION_PTR(IAccount)
    ZS_DECLARE_INTERACTION_PTR(IBackgrounding)
    ZS_DECLARE_INTERACTION_PTR(IBackgroundingNotifier)
    ZS_DECLARE_INTERACTION_PTR(IBackgroundingQuery)
    ZS_DECLARE_INTERACTION_PTR(IBackgroundingSubscription)
    ZS_DECLARE_INTERACTION_PTR(ICache)
    ZS_DECLARE_INTERACTION_PTR(ICacheDelegate)
    ZS_DECLARE_INTERACTION_PTR(ICall)
    ZS_DECLARE_INTERACTION_PTR(IConversationThread)
    ZS_DECLARE_INTERACTION_PTR(IConversationThreadComposingStatus)
    ZS_DECLARE_INTERACTION_PTR(IConversationThreadSystemMessage)
    ZS_DECLARE_INTERACTION_PTR(IContactPeerFilePublicLookup)
    ZS_DECLARE_INTERACTION_PTR(IDecryptor)
    ZS_DECLARE_INTERACTION_PTR(IEncryptor)
    ZS_DECLARE_INTERACTION_PTR(IHelper)
    ZS_DECLARE_INTERACTION_PTR(IIdentity)
    ZS_DECLARE_INTERACTION_PTR(IIdentityLookup)
    ZS_DECLARE_INTERACTION_PTR(ILoggerDelegate)
    ZS_DECLARE_INTERACTION_PTR(IMediaEngine)
    ZS_DECLARE_INTERACTION_PTR(IPushMessaging)
    ZS_DECLARE_INTERACTION_PTR(IPushMessagingQuery)
    ZS_DECLARE_INTERACTION_PTR(IPushMessagingRegisterQuery)
    ZS_DECLARE_INTERACTION_PTR(IPushMessagingTransferNotifier)
    ZS_DECLARE_INTERACTION_PTR(IPushPresence)
    ZS_DECLARE_INTERACTION_PTR(IPushPresenceRegisterQuery)
    ZS_DECLARE_INTERACTION_PTR(IPushPresenceTransferNotifier)
    ZS_DECLARE_INTERACTION_PTR(ISettings)
    ZS_DECLARE_INTERACTION_PTR(ISettingsDelegate)
    ZS_DECLARE_INTERACTION_PTR(ISystemMessage)
    ZS_DECLARE_INTERACTION_PTR(IStack)
    ZS_DECLARE_INTERACTION_PTR(IStackAutoCleanup)
    ZS_DECLARE_INTERACTION_PTR(IStackMessageQueue)
    ZS_DECLARE_INTERACTION_PTR(IStackMessageQueueDelegate)

    ZS_DECLARE_INTERACTION_PROXY(IAccountDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IBackgroundingCompletionDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IBackgroundingDelegate)
    ZS_DECLARE_INTERACTION_PROXY(ICallDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IConversationThreadDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IContactPeerFilePublicLookupDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IIdentityDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IIdentityLookupDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IMediaEngineDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPushMessagingDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPushMessagingTransferDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPushMessagingQueryDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPushMessagingRegisterQueryDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPushPresenceDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPushPresenceTransferDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPushPresenceRegisterQueryDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IStackDelegate)

    ZS_DECLARE_TYPEDEF_PTR(std::list<IContactPtr>, ContactList)
    ZS_DECLARE_TYPEDEF_PTR(std::list<ContactProfileInfo>, ContactProfileInfoList)
    ZS_DECLARE_TYPEDEF_PTR(std::list<IConversationThreadPtr>, ConversationThreadList)
    ZS_DECLARE_TYPEDEF_PTR(std::list<IIdentityPtr>, IdentityList)
    ZS_DECLARE_TYPEDEF_PTR(std::list<RolodexContact>, RolodexContactList)

    ZS_DECLARE_INTERACTION_PROXY_SUBSCRIPTION(IAccountSubscription, IAccountDelegate)
  }
}
