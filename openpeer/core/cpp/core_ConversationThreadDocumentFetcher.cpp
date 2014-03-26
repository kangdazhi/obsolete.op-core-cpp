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


#include <openpeer/core/internal/core_ConversationThreadDocumentFetcher.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/stack/ILocation.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/Stringize.h>
#include <zsLib/helpers.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;

      using stack::IPublicationFetcher;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static bool isSamePublication(IPublicationMetaDataPtr obj1, IPublicationMetaDataPtr obj2)
      {
        if (obj1->getCreatorLocation() != obj2->getCreatorLocation()) return false;
        if (obj1->getPublishedLocation() != obj2->getPublishedLocation()) return false;

        return true;
      }

      //-----------------------------------------------------------------------
      static bool isFromPeer(
                             IPublicationMetaDataPtr metaData,
                             ILocationPtr location
                             )
      {
        if (metaData->getPublishedLocation() != location) return false;
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadDocumentFetcher
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IConversationThreadDocumentFetcher::toDebug(IConversationThreadDocumentFetcherPtr fetcher)
      {
        if (!fetcher) return ElementPtr();
        return ConversationThreadDocumentFetcher::convert(fetcher)->toDebug();
      }

      //-----------------------------------------------------------------------
      IConversationThreadDocumentFetcherPtr IConversationThreadDocumentFetcher::create(
                                                                                       IConversationThreadDocumentFetcherDelegatePtr delegate,
                                                                                       IPublicationRepositoryPtr repository
                                                                                       )
      {
        return IConversationThreadDocumentFetcherFactory::singleton().create(delegate, repository);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadDocumentFetcher
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadDocumentFetcher::ConversationThreadDocumentFetcher(
                                                                           IMessageQueuePtr queue,
                                                                           IConversationThreadDocumentFetcherDelegatePtr delegate,
                                                                           IPublicationRepositoryPtr repository
                                                                           ) :
        MessageQueueAssociator(queue),
        mID(zsLib::createPUID()),
        mRepository(repository),
        mDelegate(IConversationThreadDocumentFetcherDelegateProxy::createWeak(queue, delegate))
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!repository)
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      void ConversationThreadDocumentFetcher::init()
      {
      }

      //-----------------------------------------------------------------------
      ConversationThreadDocumentFetcher::~ConversationThreadDocumentFetcher()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      ConversationThreadDocumentFetcherPtr ConversationThreadDocumentFetcher::convert(IConversationThreadDocumentFetcherPtr fetcher)
      {
        return dynamic_pointer_cast<ConversationThreadDocumentFetcher>(fetcher);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadDocumentFetcher::toDebug(ConversationThreadDocumentFetcherPtr fetcher)
      {
        if (!fetcher) return ElementPtr();
        return fetcher->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadDocumentFetcher => IConversationThreadDocumentFetcher
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadDocumentFetcherPtr ConversationThreadDocumentFetcher::create(
                                                                                     IConversationThreadDocumentFetcherDelegatePtr delegate,
                                                                                     IPublicationRepositoryPtr repository
                                                                                     )
      {
        ConversationThreadDocumentFetcherPtr pThis(new ConversationThreadDocumentFetcher(UseStack::queueCore(), delegate, repository));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadDocumentFetcher::notifyPublicationUpdated(
                                                                       ILocationPtr peerLocation,
                                                                       IPublicationMetaDataPtr metaData
                                                                       )
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) return;

        // find all with the same publication name/peer and remove them and replace with latest
        for (PendingPublicationList::iterator pendingIter = mPendingPublications.begin(); pendingIter != mPendingPublications.end(); )
        {
          PendingPublicationList::iterator current = pendingIter;
          ++pendingIter;

          IPublicationMetaDataPtr &pending = (*current).second;
          if (isSamePublication(pending, metaData)) {
            ZS_LOG_DEBUG(log("publication removed because publication is updated again") + IPublicationMetaData::toDebug(pending))
            mPendingPublications.erase(current); // no need to fetch this publication since a new one is replacing it now
          }
        }

        mPendingPublications.push_back(PeerLocationPublicationPair(peerLocation, metaData));
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadDocumentFetcher::notifyPublicationGone(
                                                                    ILocationPtr peerLocation,
                                                                    IPublicationMetaDataPtr metaData
                                                                    )
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) return;

        if (mFetcher) {
          if (isSamePublication(mFetcher->getPublicationMetaData(), metaData)) {
            ZS_LOG_DEBUG(log("publication removed because publication is gone") + IPublicationFetcher::toDebug(mFetcher))

            mFetcherPeerLocation.reset();

            mFetcher->cancel();
            mFetcher.reset();
          }
        }

        // find all with the same publication name/peer and remove them
        for (PendingPublicationList::iterator pendingIter = mPendingPublications.begin(); pendingIter != mPendingPublications.end(); )
        {
          PendingPublicationList::iterator current = pendingIter;
          ++pendingIter;

          IPublicationMetaDataPtr &pending = (*current).second;
          if (isSamePublication(pending, metaData)) {
            ZS_LOG_DEBUG(log("publication removed because publication is gone") + IPublicationMetaData::toDebug(pending))
            mPendingPublications.erase(current); // no need to fetch this publication since a new one is replacing it now
          }
        }

        // notify the delegate that it's all done
        try {
          mDelegate->onConversationThreadDocumentFetcherPublicationGone(mThisWeak.lock(), peerLocation, metaData);
        } catch(IConversationThreadDocumentFetcherDelegateProxy::Exceptions::DelegateGone &) {
        }

        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadDocumentFetcher::notifyPeerDisconnected(ILocationPtr peerLocation)
      {
        AutoRecursiveLock lock(getLock());

        bool removeFetcher = false;
        bool alreadyNotifiedAboutDocument = false;

        if (mFetcher) {
          if (isFromPeer(mFetcher->getPublicationMetaData(), peerLocation)) {
            ZS_LOG_DEBUG(log("publication removed because peer is gone") + IPublicationFetcher::toDebug(mFetcher))
            removeFetcher = true;
          }
        }

        for (PendingPublicationList::iterator pendingIter = mPendingPublications.begin(); pendingIter != mPendingPublications.end(); )
        {
          PendingPublicationList::iterator current = pendingIter;
          ++pendingIter;

          ILocationPtr &pendingPeerLocation = (*current).first;
          IPublicationMetaDataPtr &pending = (*current).second;
          if (isFromPeer(pending, peerLocation)) {
            ZS_LOG_DEBUG(log("publication removed because peer is gone") + IPublicationMetaData::toDebug(pending))
            if ((mFetcher) &&
                (!alreadyNotifiedAboutDocument)) {
              if (isSamePublication(mFetcher->getPublicationMetaData(), pending)) {
                alreadyNotifiedAboutDocument = true;
              }
            }
            try {
              mDelegate->onConversationThreadDocumentFetcherPublicationGone(mThisWeak.lock(), pendingPeerLocation, pending);
            } catch(IConversationThreadDocumentFetcherDelegateProxy::Exceptions::DelegateGone &) {
            }
            mPendingPublications.erase(current);
          }
        }

        if (removeFetcher) {
          if (!alreadyNotifiedAboutDocument) {
            ZS_LOG_DEBUG(log("notifying publication removed because peer is gone") + IPublicationFetcher::toDebug(mFetcher))
            try {
              mDelegate->onConversationThreadDocumentFetcherPublicationGone(mThisWeak.lock(), mFetcherPeerLocation, mFetcher->getPublicationMetaData());
            } catch(IConversationThreadDocumentFetcherDelegateProxy::Exceptions::DelegateGone &) {
            }
          }

          mFetcherPeerLocation.reset();

          mFetcher->cancel();
          mFetcher.reset();
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadDocumentFetcher => IPublicationFetcherDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadDocumentFetcher::onPublicationFetcherCompleted(IPublicationFetcherPtr fetcher)
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) return;

        if (fetcher != mFetcher) {
          ZS_LOG_WARNING(Detail, log("publication fetched on obsolete fetcher") + IPublicationFetcher::toDebug(fetcher))
          return;
        }

        // fetcher is now complete
        if (fetcher->wasSuccessful()) {
          try {
            mDelegate->onConversationThreadDocumentFetcherPublicationUpdated(mThisWeak.lock(), mFetcherPeerLocation, mFetcher->getFetchedPublication());
          } catch(IConversationThreadDocumentFetcherDelegateProxy::Exceptions::DelegateGone &) {
          }
        } else {
          ZS_LOG_DEBUG(log("publication removed because peer is gone") + IPublicationFetcher::toDebug(mFetcher))
        }

        mFetcherPeerLocation.reset();
        mFetcher.reset();

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadDocumentFetcher => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      bool ConversationThreadDocumentFetcher::isShutdown() const
      {
        AutoRecursiveLock lock(getLock());
        return !mDelegate;
      }

      //-----------------------------------------------------------------------
      Log::Params ConversationThreadDocumentFetcher::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::ConversationThreadDocumentFetcher");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadDocumentFetcher::toDebug() const
      {
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("core::ConversationThreadDocumentFetcher");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, ILocation::toDebug(mFetcherPeerLocation));
        IHelper::debugAppend(resultEl, IPublicationFetcher::toDebug(mFetcher));
        IHelper::debugAppend(resultEl, "pending publications", mPendingPublications.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadDocumentFetcher::cancel()
      {
        if (isShutdown()) return;

        ZS_LOG_DEBUG(log("cancel called"))

        ConversationThreadDocumentFetcherPtr pThis = mThisWeak.lock();

        if (pThis) {
          try {
            if (mFetcher) {
              mDelegate->onConversationThreadDocumentFetcherPublicationGone(pThis, mFetcherPeerLocation, mFetcher->getPublicationMetaData());
            }

            for (PendingPublicationList::iterator iter = mPendingPublications.begin(); iter != mPendingPublications.end(); ++iter)
            {
              ILocationPtr &pendingPeerLocation = (*iter).first;
              IPublicationMetaDataPtr &pending = (*iter).second;
              mDelegate->onConversationThreadDocumentFetcherPublicationGone(pThis, pendingPeerLocation, pending);
            }
          } catch(IConversationThreadDocumentFetcherDelegateProxy::Exceptions::DelegateGone &) {
          }
        }

        mFetcherPeerLocation.reset();
        if (mFetcher) {
          mFetcher->cancel();
          mFetcher.reset();
        }

        mPendingPublications.clear();

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadDocumentFetcher::step()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step forwarding to cancel"))
          cancel();
          return;
        }

        if (mFetcher) {
          ZS_LOG_DEBUG(log("fetcher already active"))
          return;
        }

        if (mPendingPublications.size() < 1) {
          ZS_LOG_DEBUG(log("no publications to fetch"))
          return;
        }

        ILocationPtr peerLocation = mPendingPublications.front().first;
        IPublicationMetaDataPtr metaData = mPendingPublications.front().second;
        mPendingPublications.pop_front();

        mFetcherPeerLocation = peerLocation;
        mFetcher = mRepository->fetch(mThisWeak.lock(), metaData);
        if (mFetcher) {
          ZS_LOG_DEBUG(log("fetching next publication") + IPublicationFetcher::toDebug(mFetcher))
        } else{
          ZS_LOG_ERROR(Detail, log("fetching next publication failed to return fetcher") + toDebug())
          notifyPublicationGone(peerLocation, metaData);
        }
      }
    }
  }
}
