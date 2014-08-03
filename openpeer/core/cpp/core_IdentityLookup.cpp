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

#include <openpeer/core/internal/core_IdentityLookup.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Helper.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Contact.h>

#include <openpeer/stack/IServiceIdentity.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Stringize.h>
#include <zsLib/helpers.h>
#include <zsLib/Log.h>
#include <zsLib/XML.h>

#define OPENPEER_CORE_IDENTITY_LOOK_REQUEST_TIMEOUT_SECONDS (60)

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

      typedef stack::message::IdentityInfoList StackIdentityInfoList;
      typedef stack::message::IdentityInfo StackIdentityInfo;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static char getSafeSplitChar(const String &identifier)
      {
        const char *testChars = ",; :./\\*#!$%&@?~+=-_|^<>[]{}()";

        while (*testChars) {
          if (String::npos == identifier.find(*testChars)) {
            return *testChars;
          }

          ++testChars;
        }

        return 0;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityLookup
      #pragma mark

      //-----------------------------------------------------------------------
      IdentityLookup::IdentityLookup(
                                     IMessageQueuePtr queue,
                                     AccountPtr account,
                                     IIdentityLookupDelegatePtr delegate,
                                     const char *identityServiceDomain
                                     ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*account),
        mAccount(account),
        mDelegate(IIdentityLookupDelegateProxy::createWeak(UseStack::queueApplication(), delegate)),
        mErrorCode(0),
        mIdentityServiceDomain(identityServiceDomain),
        mAlreadyIssuedForProviderDomain(false)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void IdentityLookup::init(const IdentityLookupInfoList &identities)
      {
        AutoRecursiveLock lock(*this);

        for (IdentityLookupInfoList::const_iterator iter = identities.begin(); iter != identities.end(); ++iter) {
          const String &identityURI = (*iter).mIdentityURI;
          const Time &lastUpdated = (*iter).mLastUpdated;

          if (!IServiceIdentity::isValid(identityURI)) {
            ZS_LOG_WARNING(Detail, log("identity not valid") + ZS_PARAM("identity", identityURI))
            continue;
          }

          String domainOrType;
          String identifier;
          if (!IServiceIdentity::splitURI(identityURI, domainOrType, identifier)) {
            ZS_LOG_WARNING(Detail, log("failed to parse identity") + ZS_PARAM("identity", identityURI))
            continue;
          }

          if (identifier.isEmpty()) {
            ZS_LOG_WARNING(Detail, log("failed to obtain identifier for identity") + ZS_PARAM("identity", identityURI))
            continue;
          }

          if (IServiceIdentity::isLegacy(identityURI)) {
            prepareIdentity(mIdentityServiceDomain, domainOrType, identifier, lastUpdated);
          } else {
            prepareIdentity(domainOrType, domainOrType, identifier, lastUpdated);
          }
        }

        // all identities should be prepared so not figure out which character can safely be used to split the string into parts
        for (IdentifierSafeCharDomainLegacyTypeMap::iterator iter = mConcatDomains.begin(); iter != mConcatDomains.end(); )
        {
          IdentifierSafeCharDomainLegacyTypeMap::iterator current = iter;
          ++iter;

          const String &type = (*current).first;
          String &concat = (*current).second;

          char safeChar = getSafeSplitChar(concat);
          if (0 == safeChar) {
            ZS_LOG_WARNING(Detail, log("failed to obain a safe char to split for domain or legacy type") + ZS_PARAM("domain or type", type))
            mConcatDomains.erase(current);
            mDomainOrLegacyTypeIdentifiers.erase(mDomainOrLegacyTypeIdentifiers.find(type));
            continue;
          }

          concat.clear();

          bool firstTime = true;

          IdentifierMap &identifiers = (*(mDomainOrLegacyTypeIdentifiers.find(type))).second;
          for (IdentifierMap::iterator innerIter = identifiers.begin(); innerIter != identifiers.end(); ++innerIter)
          {
            const String &identifier = (*innerIter).first;
            if (firstTime) {
              concat = identifier;
              firstTime = false;
              continue;
            }

            concat += safeChar + identifier;
          }

          ZS_LOG_DEBUG(log("adding safe char for type") + ZS_PARAM("type", type) + ZS_PARAM("safe char", (String() + safeChar)))

          mSafeCharDomains[type] = String() + safeChar;
        }

        // we now have a list of domains and a list types/identifiers
        step();
      }

      //-----------------------------------------------------------------------
      IdentityLookup::~IdentityLookup()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      IdentityLookupPtr IdentityLookup::convert(IIdentityLookupPtr contact)
      {
        return dynamic_pointer_cast<IdentityLookup>(contact);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityLookup => IIdentityLookup
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IdentityLookup::toDebug(IIdentityLookupPtr identity)
      {
        if (!identity) return ElementPtr();
        return IdentityLookup::convert(identity)->toDebug();
      }

      //-----------------------------------------------------------------------
      IdentityLookupPtr IdentityLookup::create(
                                               IAccountPtr account,
                                               IIdentityLookupDelegatePtr delegate,
                                               const IdentityLookupInfoList &identities,
                                               const char *identityServiceDomain
                                               )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!account)
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityServiceDomain)

        ZS_THROW_INVALID_ARGUMENT_IF(!UseServicesHelper::isValidDomain(identityServiceDomain))

        IdentityLookupPtr pThis(new IdentityLookup(UseStack::queueCore(), Account::convert(account), delegate, identityServiceDomain));
        pThis->mThisWeak = pThis;
        pThis->init(identities);
        return pThis;
      }

      //-----------------------------------------------------------------------
      bool IdentityLookup::isComplete() const
      {
        AutoRecursiveLock lock(*this);
        return !mDelegate;
      }

      //-----------------------------------------------------------------------
      bool IdentityLookup::wasSuccessful(
                                         WORD *outErrorCode,
                                         String *outErrorReason
                                         ) const
      {
        AutoRecursiveLock lock(*this);
        return 0 == mErrorCode;
      }

      //-----------------------------------------------------------------------
      void IdentityLookup::cancel()
      {
        AutoRecursiveLock lock(*this);

        IdentityLookupPtr pThis = mThisWeak.lock();

        if ((pThis) &&
            (mDelegate)) {
          try {
            ZS_LOG_DEBUG(debug("notifying outer of completion"))
            mDelegate->onIdentityLookupCompleted(pThis);
          } catch(IIdentityLookupDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }

        mDelegate.reset();

        mBootstrappedNetworks.clear();
        mDomainOrLegacyTypeIdentifiers.clear();
        mConcatDomains.clear();
        mTypeToDomainMap.clear();

        // cancel all the monitors
        for (MonitorMap::iterator iter = mMonitors.begin(); iter != mMonitors.end(); ++iter)
        {
          IMessageMonitorPtr monitor = (*iter).second;
          monitor->cancel();
        }

        mMonitors.clear();
      }

      //-----------------------------------------------------------------------
      IdentityContactListPtr IdentityLookup::getUpdatedIdentities() const
      {
        AutoRecursiveLock lock(*this);
        IdentityContactListPtr result(new IdentityContactList);

        for (IdentityContactList::const_iterator iter = mResults.begin(); iter != mResults.end(); ++iter)
        {
          const IdentityContact &info = (*iter);
          ZS_LOG_TRACE(log("found result") + ZS_PARAM("identity", info.mIdentityURI))
          result->push_back(info);
        }

        return result;
      }

      //-----------------------------------------------------------------------
      IdentityLookup::IdentityLookupInfoListPtr IdentityLookup::getUnchangedIdentities() const
      {
        AutoRecursiveLock lock(*this);
        IdentityLookupInfoListPtr result(new IdentityLookupInfoList);

        for (IdentityLookupInfoList::const_iterator iter = mUnchangedResults.begin(); iter != mUnchangedResults.end(); ++iter)
        {
          const IdentityLookupInfo &info = (*iter);
          ZS_LOG_TRACE(log("found unchanged result") + ZS_PARAM("identity", info.mIdentityURI))
          result->push_back(info);
        }

        return result;
      }

      //-----------------------------------------------------------------------
      IdentityLookup::IdentityLookupInfoListPtr IdentityLookup::getInvalidIdentities() const
      {
        AutoRecursiveLock lock(*this);
        IdentityLookupInfoListPtr result(new IdentityLookupInfoList);

        for (IdentityLookupInfoList::const_iterator iter = mInvalidResults.begin(); iter != mInvalidResults.end(); ++iter)
        {
          const IdentityLookupInfo &info = (*iter);
          ZS_LOG_TRACE(log("found invalid result") + ZS_PARAM("identity", info.mIdentityURI))
          result->push_back(info);
        }

        return result;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityLookup => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void IdentityLookup::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr network)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("bootstrapped network prepared notification") + IBootstrappedNetwork::toDebug(network))

        BootstrappedNetworkMap::iterator found = mBootstrappedNetworks.find(network->getDomain());
        if (found == mBootstrappedNetworks.end()) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete bootstrapped network") + IBootstrappedNetwork::toDebug(network))
          return;
        }

        mBootstrappedNetworks.erase(found);

        WORD errorCode = 0;
        String errorResaon;
        bool success = network->wasSuccessful(&errorCode, &errorResaon);
        if (!success) {
          ZS_LOG_ERROR(Detail, log("bootstrapped network failed") + IBootstrappedNetwork::toDebug(network))

          if (mIdentityServiceDomain == network->getDomain()) {
            ZS_LOG_ERROR(Detail, log("cannot access peer contact service's identity lookup service") + ZS_PARAM("provider domain", mIdentityServiceDomain))
            setError(errorCode, errorResaon);
            step();
            return;
          }

          // see if there is already an attempt to use the bootstrapped network for the peer contact service's domain
          BootstrappedNetworkMap::iterator foundProvider = mBootstrappedNetworks.find(mIdentityServiceDomain);
          if (foundProvider == mBootstrappedNetworks.end()) {
            IBootstrappedNetworkPtr providerNetwork = IBootstrappedNetwork::prepare(mIdentityServiceDomain, mThisWeak.lock());
            if (!providerNetwork) {
              ZS_LOG_ERROR(Detail, log("failed to create bootstrapper for domain") + ZS_PARAM("domain", mIdentityServiceDomain))
              setError(errorCode, errorResaon);
              step();
              return;
            }

            // bootstrapped network not prepared yet for this domain, attempt to prepare it now
            mBootstrappedNetworks[mIdentityServiceDomain] = providerNetwork;

            // delay handling this failure until later...
            mFailedBootstrappedNetworks[network->getDomain()] = true;

            ZS_LOG_DETAIL(log("waiting to perform lookup on backup service peer contact's identity lookup service") + ZS_PARAM("failed domain", network->getDomain()) + ZS_PARAM("backup", providerNetwork->getDomain()))

            // wait for the bootstrapped network to complete
            return;
          }

          IBootstrappedNetworkPtr providerNetwork = (*foundProvider).second;
          if (!providerNetwork->isPreparationComplete()) {
            mFailedBootstrappedNetworks[network->getDomain()] = true;

            ZS_LOG_DETAIL(log("waiting to perform lookup on backup service peer contact's identity lookup service") + ZS_PARAM("failed domain", network->getDomain()) + ZS_PARAM("backup", providerNetwork->getDomain()))
            return;
          }

          if (!providerNetwork->wasSuccessful(&errorCode, &errorResaon)) {
            ZS_LOG_ERROR(Detail, log("failed to create bootstrapper for domain") + ZS_PARAM("domain", mIdentityServiceDomain))
            setError(errorCode, errorResaon);
            step();
            return;
          }

          // the domain lookuped was a failure but we can issue a request using the provider's network instead
          mFailedBootstrappedNetworks[network->getDomain()] = true;

          // pretend we are re-issuing a request from the provider domain
          network = providerNetwork;
        }

        typedef IdentityLookupCheckRequest::Provider Provider;
        typedef IdentityLookupCheckRequest::ProviderList ProviderList;

        ProviderList providers;

        // find all identities using this domain
        for (DomainOrLegacyTypeToDomainMap::iterator iter = mTypeToDomainMap.begin(); iter != mTypeToDomainMap.end(); ++iter)
        {
          const String &domainOrType = (*iter).first;
          String &domain = (*iter).second;

          bool lookupThisDomain = (network->getDomain() == domain);

          if (mIdentityServiceDomain == network->getDomain()) {
            if (mAlreadyIssuedForProviderDomain)
              lookupThisDomain = false; // don't issue unless this is for a failed domain

            if (mFailedBootstrappedNetworks.find(domain) != mFailedBootstrappedNetworks.end()) {
              ZS_LOG_DETAIL(log("performing lookup on failed domain") + ZS_PARAM("failed domain", domain) + ZS_PARAM("lookup now done on domain", mIdentityServiceDomain))
              lookupThisDomain = true;
            }
          }

          if (lookupThisDomain) {
            ZS_LOG_DEBUG(log("will perform lookup on type") + ZS_PARAM("type/domain", domainOrType))

            // this type uses this domain
            IdentifierSafeCharDomainLegacyTypeMap::iterator foundConcat = mConcatDomains.find(domainOrType);
            IdentifierSafeCharDomainLegacyTypeMap::iterator foundSafeChar = mSafeCharDomains.find(domainOrType);

            ZS_THROW_BAD_STATE_IF(foundConcat == mConcatDomains.end())
            ZS_THROW_BAD_STATE_IF(foundSafeChar == mSafeCharDomains.end())

            const String &identifiers = (*foundConcat).second;
            const String &splitChar = (*foundSafeChar).second;

            if (identifiers.isEmpty()) {
              ZS_LOG_WARNING(Detail, log("no identifiers found for this domain/type") + ZS_PARAM("domain", network->getDomain()) + ZS_PARAM("type/domain", domainOrType))
              continue;
            }

            Provider provider;
            provider.mBase = IServiceIdentity::joinURI(domainOrType, "");
            provider.mSeparator = splitChar;
            provider.mIdentities = identifiers;

            ZS_LOG_DEBUG(log("adding provider to list") + ZS_PARAM("base", provider.mBase) + ZS_PARAM("seperator", provider.mSeparator) + ZS_PARAM("identities", provider.mIdentities))
            providers.push_back(provider);
          }
        }

        if (network->getDomain() == mIdentityServiceDomain)
          mAlreadyIssuedForProviderDomain = true;

        mFailedBootstrappedNetworks.clear();

        if (providers.size() > 0) {
          // let's issue a request to discover these identities
          IdentityLookupCheckRequestPtr request = IdentityLookupCheckRequest::create();
          request->domain(network->getDomain());
          request->providers(providers);

          IMessageMonitorPtr monitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<IdentityLookupCheckResult>::convert(mThisWeak.lock()), network, "identity-lookup", "identity-lookup-check", request, Seconds(OPENPEER_CORE_IDENTITY_LOOK_REQUEST_TIMEOUT_SECONDS));

          if (!monitor) {
            ZS_LOG_ERROR(Detail, log("failed to create monitor for request"))
            setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to create monitor for identity lookup request");
            step();
            return;
          }

          ZS_LOG_DEBUG(log("monitoring identity lookup request") + IMessageMonitor::toDebug(monitor))
          mMonitors[monitor->getID()] = monitor;
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityLookup => IMessageMonitorResultDelegate<IdentityLookupCheckResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool IdentityLookup::handleMessageMonitorResultReceived(
                                                              IMessageMonitorPtr monitor,
                                                              IdentityLookupCheckResultPtr result
                                                              )
      {
        typedef IdentityLookupCheckRequest::Provider Provider;
        typedef IdentityLookupCheckRequest::ProviderList ProviderList;

        AutoRecursiveLock lock(*this);

        MonitorMap::iterator found = mMonitors.find(monitor->getID());
        if (found == mMonitors.end()) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mMonitors.erase(found);

        IdentityLookupCheckRequestPtr originalRequest = IdentityLookupCheckRequest::convert(monitor->getMonitoredMessage());
        ZS_THROW_BAD_STATE_IF(!originalRequest)

        String originalDomain = originalRequest->domain();
        ZS_THROW_BAD_STATE_IF(!UseServicesHelper::isValidDomain(originalDomain))

        IBootstrappedNetworkPtr network = IBootstrappedNetwork::prepare(originalDomain);
        ZS_THROW_BAD_STATE_IF(!originalRequest)

        const StackIdentityInfoList &resultInfos = result->identities();

        const ProviderList &originalRequestProviders = originalRequest->providers();
        ProviderList providers;

        IdentityMap previousIdentities;
        getPreviousIdentities(originalRequestProviders, previousIdentities);

        for (StackIdentityInfoList::const_iterator iter = resultInfos.begin(); iter != resultInfos.end(); ++iter)
        {
          const StackIdentityInfo &resultInfo = (*iter);

          String domainOrType;
          String identifier;
          if (!IServiceIdentity::splitURI(resultInfo.mURI, domainOrType, identifier)) {
            ZS_LOG_ERROR(Detail, log("failed to split an identity") + ZS_PARAM("identity uri", resultInfo.mURI))
            continue;
          }

          // scope: forget previous identity if now found in result list
          {
            IdentityMap::iterator found = previousIdentities.find(resultInfo.mURI);
            if (found != previousIdentities.end()) {
              ZS_LOG_TRACE(log("previous identifier was found in result") + ZS_PARAM("identity uri", resultInfo.mURI))
              previousIdentities.erase(found);
            }
          }

          IdentifierDomainOrLegacyTypeMap::iterator foundType = mDomainOrLegacyTypeIdentifiers.find(domainOrType);
          if (foundType == mDomainOrLegacyTypeIdentifiers.end()) {
            ZS_LOG_ERROR(Detail, log("failed to find previous known last update for identity") + ZS_PARAM("identity url", resultInfo.mURI))
            continue;
          }

          IdentifierMap &identifiers = (*foundType).second;
          IdentifierMap::iterator foundIdentifier = identifiers.find(identifier);
          if (foundIdentifier == identifiers.end()) {
            ZS_LOG_ERROR(Detail, log("failed to find previous identifier in identitifers map") + ZS_PARAM("identity url", resultInfo.mURI))
            continue;
          }

          Time lastKnownUpdate = (*foundIdentifier).second;

          if (lastKnownUpdate == resultInfo.mUpdated) {
            ZS_LOG_TRACE(log("identity information has not changed since last time") + ZS_PARAM("identity uri", resultInfo.mURI) + ZS_PARAM("last updated", resultInfo.mUpdated))

            // nothing about this identity has changed since last time
            IdentityContact info;

            info.mIdentityURI = resultInfo.mURI;
            info.mIdentityProvider = resultInfo.mProvider;
            info.mLastUpdated = resultInfo.mUpdated;

            mUnchangedResults.push_back(info);
            continue;
          }

          String base = IServiceIdentity::joinURI(domainOrType, "");

          bool found = false;

          // scope: see if a provider for the type/domain has already beena added before, if so append the identifier to the list
          {
            for (ProviderList::iterator provIter = providers.begin(); provIter != providers.end(); ++provIter) {
              Provider &provider = (*provIter);
              if (base == provider.mBase) {
                // correct base to use, add identifier
                provider.mIdentities += provider.mSeparator + identifier;
                found = true;
                break;
              }
            }
          }

          if (!found) {
            // this provider has not been seen before - find a template to base it upon from the last issued request
            for (ProviderList::const_iterator provIter = originalRequestProviders.begin(); provIter != originalRequestProviders.end(); ++provIter) {
              const Provider &provider = (*provIter);
              if (base == provider.mBase) {
                // found correct template for a provider to use
                Provider newProvider(provider);
                newProvider.mIdentities = identifier; // only need to lookup this one identifier (some more may be found later)
                providers.push_back(newProvider);
                found = true;
                break;
              }
            }
          }

          if (!found) {
            ZS_LOG_ERROR(Detail, log("failed to find a previous provider base that can satisfy this identifier's type/domain") + ZS_PARAM("identity url", resultInfo.mURI))
            continue;
          }

          ZS_LOG_TRACE(log("will perform new detailed lookup for identity") + ZS_PARAM("identity", resultInfo.mURI))
        }

        // scope: these identities failed to lookup for whatever reason
        {
          for (IdentityMap::iterator iter = previousIdentities.begin(); iter != previousIdentities.end(); ++iter)
          {
            const String &identityURI = (*iter).first;
            ZS_LOG_WARNING(Trace, log("identity was not found on server when performing lookup") + ZS_PARAM("identity uri", identityURI))
            IdentityLookupInfo info;
            info.mIdentityURI = identityURI;
            mInvalidResults.push_back(info);
          }
        }

        if (providers.size() > 0) {
          // let's issue a request to discover these identities
          IdentityLookupRequestPtr request = IdentityLookupRequest::create();
          request->domain(originalDomain);
          request->providers(providers);

          IMessageMonitorPtr newMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<IdentityLookupResult>::convert(mThisWeak.lock()), network, "identity-lookup", "identity-lookup", request, Seconds(OPENPEER_CORE_IDENTITY_LOOK_REQUEST_TIMEOUT_SECONDS));
          mMonitors[newMonitor->getID()] = newMonitor;
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool IdentityLookup::handleMessageMonitorErrorResultReceived(
                                                                   IMessageMonitorPtr monitor,
                                                                   IdentityLookupCheckResultPtr ignore, // will always be NULL
                                                                   MessageResultPtr result
                                                                   )
      {
        AutoRecursiveLock lock(*this);

        MonitorMap::iterator found = mMonitors.find(monitor->getID());
        if (found == mMonitors.end()) {
          ZS_LOG_WARNING(Detail, log("notified about failure for obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mMonitors.erase(found);

        setError(result->errorCode(), result->errorReason());
        step();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityLookup => IMessageMonitorResultDelegate<IdentityLookupResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool IdentityLookup::handleMessageMonitorResultReceived(
                                                              IMessageMonitorPtr monitor,
                                                              IdentityLookupResultPtr result
                                                              )
      {
        typedef IdentityLookupRequest::ProviderList ProviderList;

        AutoRecursiveLock lock(*this);

        MonitorMap::iterator found = mMonitors.find(monitor->getID());
        if (found == mMonitors.end()) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mMonitors.erase(found);

        IdentityLookupRequestPtr originalRequest = IdentityLookupRequest::convert(monitor->getMonitoredMessage());
        ZS_THROW_BAD_STATE_IF(!originalRequest)

        const ProviderList &originalRequestProviders = originalRequest->providers();

        IdentityMap previousIdentities;
        getPreviousIdentities(originalRequestProviders, previousIdentities);

        const StackIdentityInfoList &resultInfos = result->identities();

        for (StackIdentityInfoList::const_iterator iter = resultInfos.begin(); iter != resultInfos.end(); ++iter)
        {
          const StackIdentityInfo &resultInfo = (*iter);

          if (!resultInfo.mPeerFilePublic) {
            ZS_LOG_WARNING(Detail, log("peer URI found in result not valid") + resultInfo.toDebug())
            continue;
          }

          // scope: forget previous identity if now found in result list
          {
            IdentityMap::iterator found = previousIdentities.find(resultInfo.mURI);
            if (found != previousIdentities.end()) {
              ZS_LOG_TRACE(log("previous identifier was found in result") + ZS_PARAM("identity uri", resultInfo.mURI))
              previousIdentities.erase(found);
            }
          }

          IdentityContact info;
          Helper::convert(resultInfo, info);

          mResults.push_back(info);
        }

        // scope: these identities failed to lookup for whatever reason
        {
          for (IdentityMap::iterator iter = previousIdentities.begin(); iter != previousIdentities.end(); ++iter)
          {
            const String &identityURI = (*iter).first;
            ZS_LOG_WARNING(Trace, log("identity was not found on server when performing lookup") + ZS_PARAM("identity uri", identityURI))
            IdentityLookupInfo info;
            info.mIdentityURI = identityURI;
            mInvalidResults.push_back(info);
          }
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool IdentityLookup::handleMessageMonitorErrorResultReceived(
                                                                   IMessageMonitorPtr monitor,
                                                                   IdentityLookupResultPtr ignore, // will always be NULL
                                                                   MessageResultPtr result
                                                                   )
      {
        AutoRecursiveLock lock(*this);

        MonitorMap::iterator found = mMonitors.find(monitor->getID());
        if (found == mMonitors.end()) {
          ZS_LOG_WARNING(Detail, log("notified about failure for obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mMonitors.erase(found);

        setError(result->errorCode(), result->errorReason());
        step();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityLookup => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params IdentityLookup::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::IdentityLookup");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params IdentityLookup::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr IdentityLookup::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::IdentityLookup");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);
        UseServicesHelper::debugAppend(resultEl, "error code", mErrorCode);
        UseServicesHelper::debugAppend(resultEl, "error reason", mErrorReason);
        UseServicesHelper::debugAppend(resultEl, "identity service domain", mIdentityServiceDomain);
        UseServicesHelper::debugAppend(resultEl, "bootstrapped networks", mBootstrappedNetworks.size());
        UseServicesHelper::debugAppend(resultEl, "monitors", mMonitors.size());
        UseServicesHelper::debugAppend(resultEl, "type identifiers", mDomainOrLegacyTypeIdentifiers.size());
        UseServicesHelper::debugAppend(resultEl, "concat domains", mConcatDomains.size());
        UseServicesHelper::debugAppend(resultEl, "safe char domains", mSafeCharDomains.size());
        UseServicesHelper::debugAppend(resultEl, "type to domains", mTypeToDomainMap.size());
        UseServicesHelper::debugAppend(resultEl, "results", mResults.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void IdentityLookup::prepareIdentity(
                                           const String &domain,
                                           const String &type,
                                           const String &identifier,
                                           const Time &lastUpdated
                                           )
      {
        ZS_LOG_DEBUG(log("preparing domain") + ZS_PARAM("domain", domain) + ZS_PARAM("identifier", identifier))

        // scope: prepare bootstrapper for domain
        {
          BootstrappedNetworkMap::iterator found = mBootstrappedNetworks.find(domain);
          if (found == mBootstrappedNetworks.end()) {
            ZS_LOG_DEBUG(log("domain not found, adding new bootstrapper") + ZS_PARAM("domain", domain))
            IBootstrappedNetworkPtr network = IBootstrappedNetwork::prepare(domain, mThisWeak.lock());
            if (!network) {
              ZS_LOG_WARNING(Detail, log("failed to create bootstrapper for domain") + ZS_PARAM("domain", domain))
              return;
            }

            // bootstrapper was created for this domain
            mBootstrappedNetworks[domain] = network;
          }
        }

        // scope prepare identifiers for domain
        {
          IdentifierDomainOrLegacyTypeMap::iterator found = mDomainOrLegacyTypeIdentifiers.find(type);
          if (found == mDomainOrLegacyTypeIdentifiers.end()) {
            IdentifierMap empty;
            mDomainOrLegacyTypeIdentifiers[type] = empty;
            found = mDomainOrLegacyTypeIdentifiers.find(type);

            ZS_LOG_DEBUG(log("adding contact type") + ZS_PARAM("type", type))

            mConcatDomains[type] = String();
            mTypeToDomainMap[type] = domain;
          }

          IdentifierMap &identifiers = (*found).second;
          identifiers[identifier] = lastUpdated;

          IdentifierSafeCharDomainLegacyTypeMap::iterator foundSafe = mConcatDomains.find(type);
          ZS_THROW_BAD_STATE_IF(foundSafe == mConcatDomains.end())

          // append into one bit string (without any split char yet)
          String &concats = (*foundSafe).second;
          concats += identifier;
        }
      }

      //-----------------------------------------------------------------------
      void IdentityLookup::step()
      {
        ZS_LOG_DEBUG(log("step"))

        if ((mMonitors.size() < 1) &&
            (mBootstrappedNetworks.size() < 1)) {
          ZS_LOG_DEBUG(log("identity lookup is finished"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(log("identity lookup is still pending"))
      }

      //-----------------------------------------------------------------------
      void IdentityLookup::setError(WORD errorCode, const char *inErrorReason)
      {
        if (0 == errorCode) {
          ZS_LOG_DEBUG(log("no error set"))
          return;
        }

        String reason(inErrorReason ? String(inErrorReason) : String());
        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if (0 != mErrorCode) {
          ZS_LOG_ERROR(Detail, debug("error already set (thus ignoring new error)") + ZS_PARAM("error code", errorCode) + ZS_PARAM("reason", reason))
          return;
        }

        mErrorCode = errorCode;
        mErrorReason = reason;

        ZS_LOG_ERROR(Detail, debug("error set"))
      }

      //-----------------------------------------------------------------------
      void IdentityLookup::getPreviousIdentities(
                                                 const IdentityLookupCheckRequest::ProviderList &providers,
                                                 IdentityMap &outIdentities
                                                 )
      {
        typedef IdentityLookupCheckRequest::ProviderList ProviderList;
        typedef IdentityLookupCheckRequest::Provider Provider;
        typedef services::IHelper::SplitMap SplitMap;

        for (ProviderList::const_iterator provIter = providers.begin(); provIter != providers.end(); ++provIter)
        {
          const Provider &provider = (*provIter);

          if (provider.mSeparator.length() != 1) {
            continue;
          }

          String domainOrType;
          String bogusIdentifier;
          IServiceIdentity::splitURI(provider.mBase, domainOrType, bogusIdentifier);

          SplitMap results;
          services::IHelper::split(
                                   provider.mIdentities,
                                   results,
                                   (provider.mSeparator.c_str())[0]
                                   );

          for (SplitMap::iterator iter = results.begin(); iter != results.end(); ++iter)
          {
            String identifier = (*iter).second;
            String identityURI = IServiceIdentity::joinURI(domainOrType, identifier);

            outIdentities[identityURI] = true;
          }
        }
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
    #pragma mark IIdentityLookup
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IIdentityLookup::toDebug(IIdentityLookupPtr identity)
    {
      return internal::IdentityLookup::toDebug(identity);
    }

    //-------------------------------------------------------------------------
    IIdentityLookupPtr IIdentityLookup::create(
                                               IAccountPtr account,
                                               IIdentityLookupDelegatePtr delegate,
                                               const IdentityLookupInfoList &identities,
                                               const char *identityServiceDomain
                                               )
    {
      return internal::IIdentityLookupFactory::singleton().create(account, delegate, identities, identityServiceDomain);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IIdentityLookup::IdentityLookupInfo
    #pragma mark

    //-------------------------------------------------------------------------
    IIdentityLookup::IdentityLookupInfo::IdentityLookupInfo(const RolodexContact &identity) :
      mIdentityURI(identity.mIdentityURI)
    {
    }

    //-------------------------------------------------------------------------
    IIdentityLookup::IdentityLookupInfo::IdentityLookupInfo(const IdentityContact &identity) :
      mIdentityURI(identity.mIdentityURI),
      mLastUpdated(identity.mLastUpdated)
    {
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
