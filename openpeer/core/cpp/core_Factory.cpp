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

#include <openpeer/core/internal/core_Factory.h>

#include <zsLib/Log.h>
#include <zsLib/helpers.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helper)
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Factory
      #pragma mark

      //-----------------------------------------------------------------------
      void Factory::override(FactoryPtr override)
      {
        singleton().mOverride = override;
      }

      //-----------------------------------------------------------------------
      Factory &Factory::singleton()
      {
        static Singleton<Factory, false> factory;
        Factory &singleton = factory.singleton();
        if (singleton.mOverride) return (*singleton.mOverride);
        return singleton;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBackgroundingFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IBackgroundingFactory &IBackgroundingFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      BackgroundingPtr IBackgroundingFactory::createForBackgrounding()
      {
        if (this) {}
        return Backgrounding::create();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IAccountFactory &IAccountFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      AccountPtr IAccountFactory::login(
                                        IAccountDelegatePtr delegate,
                                        IConversationThreadDelegatePtr conversationThreadDelegate,
                                        ICallDelegatePtr callDelegate,
                                        const char *namespaceGrantOuterFrameURLUponReload,
                                        const char *grantID,
                                        const char *lockboxServiceDomain,
                                        bool forceCreateNewLockboxAccount
                                        )
      {
        if (this) {}
        return Account::login(delegate, conversationThreadDelegate, callDelegate, namespaceGrantOuterFrameURLUponReload, grantID, lockboxServiceDomain, forceCreateNewLockboxAccount);
      }

      //-----------------------------------------------------------------------
      AccountPtr IAccountFactory::relogin(
                                          IAccountDelegatePtr delegate,
                                          IConversationThreadDelegatePtr conversationThreadDelegate,
                                          ICallDelegatePtr callDelegate,
                                          const char *namespaceGrantOuterFrameURLUponReload,
                                          ElementPtr reloginInformation
                                          )
      {
        if (this) {}
        return Account::relogin(delegate, conversationThreadDelegate, callDelegate, namespaceGrantOuterFrameURLUponReload, reloginInformation);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountFactory
      #pragma mark

      //-----------------------------------------------------------------------
      ICallFactory &ICallFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      CallPtr ICallFactory::placeCall(
                                      ConversationThreadPtr conversationThread,
                                      IContactPtr toContact,
                                      bool includeAudio,
                                      bool includeVideo
                                      )
      {
        if (this) {}
        return Call::placeCall(conversationThread, toContact, includeAudio, includeVideo);
      }

      //-----------------------------------------------------------------------
      CallPtr ICallFactory::createForIncomingCall(
                                                  ConversationThreadPtr inConversationThread,
                                                  ContactPtr callerContact,
                                                  const DialogPtr &remoteDialog
                                                  )
      {
        if (this) {}
        return Call::createForIncomingCall(inConversationThread, callerContact, remoteDialog);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportFactory
      #pragma mark

      //-----------------------------------------------------------------------
      ICallTransportFactory &ICallTransportFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      CallTransportPtr ICallTransportFactory::create(
                                                     ICallTransportDelegatePtr delegate,
                                                     const IICESocket::TURNServerInfoList &turnServers,
                                                     const IICESocket::STUNServerInfoList &stunServers
                                                     )
      {
        if (this) {}
        return CallTransport::create(delegate, turnServers, stunServers);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IContactFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IContactFactory &IContactFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      ContactPtr IContactFactory::createFromPeer(
                                                 AccountPtr account,
                                                 IPeerPtr peer
                                                 )
      {
        if (this) {}
        return Contact::createFromPeer(account, peer);
      }

      //-----------------------------------------------------------------------
      ContactPtr IContactFactory::createFromPeerFilePublic(
                                                           AccountPtr account,
                                                           IPeerFilePublicPtr peerFilePublic
                                                           )
      {
        if (this) {}
        return Contact::createFromPeerFilePublic(account, peerFilePublic);
      }

      //-----------------------------------------------------------------------
      ContactPtr IContactFactory::getForSelf(IAccountPtr account)
      {
        if (this) {}
        return Contact::getForSelf(account);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IConversationThreadFactory &IConversationThreadFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr IConversationThreadFactory::createConversationThread(
                                                                                 AccountPtr account,
                                                                                 const IdentityContactList &identityContacts,
                                                                                 const ContactProfileInfoList &addContacts,
                                                                                 const char *threadID
                                                                                 )
      {
        if (this) {}
        return ConversationThread::create(account, identityContacts);
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr IConversationThreadFactory::createConversationThread(
                                                                                 AccountPtr account,
                                                                                 ILocationPtr peerLocation,
                                                                                 IPublicationMetaDataPtr metaData,
                                                                                 const SplitMap &split
                                                                                 )
      {
        if (this) {}
        return ConversationThread::create(account, peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadDocumentFetcherFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IConversationThreadDocumentFetcherFactory &IConversationThreadDocumentFetcherFactory::singleton()
      {
        return Factory::singleton();
      }

      ConversationThreadDocumentFetcherPtr IConversationThreadDocumentFetcherFactory::create(
                                                                                             IConversationThreadDocumentFetcherDelegatePtr delegate,
                                                                                             IPublicationRepositoryPtr repository
                                                                                             )
      {
        if (this) {}
        return ConversationThreadDocumentFetcher::create(delegate, repository);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadHostFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IConversationThreadHostFactory &IConversationThreadHostFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      ConversationThreadHostPtr IConversationThreadHostFactory::createConversationThreadHost(
                                                                                             ConversationThreadPtr baseThread,
                                                                                             const char *serverName,
                                                                                             thread::Details::ConversationThreadStates state
                                                                                             )
      {
        if (this) {}
        return ConversationThreadHost::create(baseThread, serverName, state);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadSlaveFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IConversationThreadSlaveFactory &IConversationThreadSlaveFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      ConversationThreadSlavePtr IConversationThreadSlaveFactory::createConversationThreadSlave(
                                                                                                ConversationThreadPtr baseThread,
                                                                                                ILocationPtr peerLocation,
                                                                                                IPublicationMetaDataPtr metaData,
                                                                                                const SplitMap &split,
                                                                                                const char *serverName
                                                                                                )
      {
        if (this) {}
        return ConversationThreadSlave::create(baseThread, peerLocation, metaData, split, serverName);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IIdentityFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IIdentityFactory &IIdentityFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      IdentityPtr IIdentityFactory::login(
                                          IAccountPtr account,
                                          IIdentityDelegatePtr delegate,
                                          const char *identityProviderDomain,
                                          const char *identityURI_or_identityBaseURI,
                                          const char *outerFrameURLUponReload
                                          )
      {
        if (this) {}
        return Identity::login(account, delegate, identityProviderDomain, identityURI_or_identityBaseURI, outerFrameURLUponReload);
      }

      //-----------------------------------------------------------------------
      IdentityPtr IIdentityFactory::loginWithIdentityPreauthorized(
                                                                   IAccountPtr account,
                                                                   IIdentityDelegatePtr delegate,
                                                                   const char *identityProviderDomain,
                                                                   const char *identityURI,
                                                                   const char *identityAccessToken,
                                                                   const char *identityAccessSecret,
                                                                   Time identityAccessSecretExpires
                                                                   )
      {
        if (this) {}
        return Identity::loginWithIdentityPreauthorized(account, delegate, identityProviderDomain, identityURI, identityAccessToken, identityAccessSecret, identityAccessSecretExpires);
      }

      //-----------------------------------------------------------------------
      IdentityPtr IIdentityFactory::createFromExistingSession(IServiceIdentitySessionPtr session)
      {
        if (this) {}
        return Identity::createFromExistingSession(session);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IIdentityLookupFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IIdentityLookupFactory &IIdentityLookupFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      IdentityLookupPtr IIdentityLookupFactory::create(
                                                       IAccountPtr account,
                                                       IIdentityLookupDelegatePtr delegate,
                                                       const IdentityLookupInfoList &identityLookupInfos,
                                                       const char *identityServiceDomain
                                                       )
      {
        if (this) {}
        return IdentityLookup::create(account, delegate, identityLookupInfos, identityServiceDomain);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMediaEngineFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IMediaEngineFactory &IMediaEngineFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      MediaEnginePtr IMediaEngineFactory::createMediaEngine(IMediaEngineDelegatePtr delegate)
      {
        if (this) {}
        return MediaEngine::create(delegate);
      }
    }
  }
}
