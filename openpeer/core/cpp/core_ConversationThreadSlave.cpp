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


#include <openpeer/core/internal/core_ConversationThreadSlave.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_Call.h>
#include <openpeer/core/internal/core_Helper.h>
#include <openpeer/core/internal/core_Stack.h>

#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/IPublication.h>
#include <openpeer/stack/IHelper.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>
#include <zsLib/XML.h>

#include <regex>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadSlave::ForConversationThread, ForConversationThread)

      typedef IStackForInternal UseStack;
      typedef ConversationThreadSlave::UseContactPtr UseContactPtr;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(core::internal::Helper, UseCoreHelper)

      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)

      using namespace core::internal::thread;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadSlaveForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IConversationThreadSlaveForConversationThread::toDebug(ForConversationThreadPtr thread)
      {
        return ConversationThreadSlave::toDebug(ConversationThreadSlave::convert(thread));
      }

      //-----------------------------------------------------------------------
      ForConversationThreadPtr IConversationThreadSlaveForConversationThread::create(
                                                                                     ConversationThreadPtr baseThread,
                                                                                     ILocationPtr peerLocation,
                                                                                     IPublicationMetaDataPtr metaData,
                                                                                     const SplitMap &split,
                                                                                     const char *serverName
                                                                                     )
      {
        return IConversationThreadSlaveFactory::singleton().createConversationThreadSlave(baseThread, peerLocation, metaData, split, serverName);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave
      #pragma mark

      //-----------------------------------------------------------------------
      const char *ConversationThreadSlave::toString(ConversationThreadSlaveStates state)
      {
        switch (state)
        {
          case ConversationThreadSlaveState_Pending:       return "Pending";
          case ConversationThreadSlaveState_Ready:         return "Ready";
          case ConversationThreadSlaveState_ShuttingDown:  return "Shutting down";
          case ConversationThreadSlaveState_Shutdown:      return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      ConversationThreadSlave::ConversationThreadSlave(
                                                       IMessageQueuePtr queue,
                                                       AccountPtr account,
                                                       ILocationPtr peerLocation,
                                                       ConversationThreadPtr baseThread,
                                                       const char *threadID,
                                                       const char *serverName
                                                       ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*baseThread),
        mBaseThread(baseThread),
        mAccount(account),
        mThreadID(threadID ? String(threadID) : UseServicesHelper::randomString(32)),
        mServerName(serverName),
        mPeerLocation(peerLocation),
        mCurrentState(ConversationThreadSlaveState_Pending),
        mConvertedToHostBecauseOriginalHostLikelyGoneForever(false)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::init()
      {
        AutoRecursiveLock lock(*this);
        mFetcher = IConversationThreadDocumentFetcher::create(mThisWeak.lock(), mAccount.lock()->getRepository());

        mBackgroundingSubscription = IBackgrounding::subscribe(mThisWeak.lock(), UseSettings::getUInt(OPENPEER_CORE_SETTING_CONVERSATION_THREAD_HOST_BACKGROUNDING_PHASE));
      }

      //-----------------------------------------------------------------------
      ConversationThreadSlave::~ConversationThreadSlave()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      ConversationThreadSlavePtr ConversationThreadSlave::convert(ForConversationThreadPtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(ConversationThreadSlave, object);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadSlave::toDebug(ConversationThreadSlavePtr thread)
      {
        if (!thread) return ElementPtr();
        return thread->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave => IConversationThreadHostSlaveBase
      #pragma mark

      //-----------------------------------------------------------------------
      String ConversationThreadSlave::getThreadID() const
      {
        AutoRecursiveLock lock(*this);
        return mThreadID;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::shutdown()
      {
        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadSlave::isHostThreadOpen() const
      {
        if (mConvertedToHostBecauseOriginalHostLikelyGoneForever) return false;
        if (!mHostThread) return false;
        if (!mHostThread->details()) return false;
        return Details::ConversationThreadState_Open == mHostThread->details()->state();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::notifyPublicationUpdated(
                                                             ILocationPtr peerLocation,
                                                             IPublicationMetaDataPtr metaData,
                                                             const SplitMap &split
                                                             )
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_WARNING(Detail, log("notification of an updated document after shutdown") + IPublicationMetaData::toDebug(metaData))
          return;
        }

        if (!mHostThread) {
          ZS_LOG_TRACE(log("holding an extra reference to ourselves until fetcher completes it's job"))
          mSelfHoldingStartupReferenceUntilPublicationFetchCompletes = mThisWeak.lock();
        }
        mFetcher->notifyPublicationUpdated(peerLocation, metaData);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::notifyPublicationGone(
                                                          ILocationPtr peerLocation,
                                                          IPublicationMetaDataPtr metaData,
                                                          const SplitMap &split
                                                          )
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("notification of a document gone after shutdown") + IPublicationMetaData::toDebug(metaData))
          return;
        }
        mFetcher->notifyPublicationGone(peerLocation, metaData);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::notifyPeerDisconnected(ILocationPtr peerLocation)
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("notification of a peer shutdown") + ILocation::toDebug(peerLocation))
          return;
        }
        mFetcher->notifyPeerDisconnected(peerLocation);
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadSlave::sendMessages(const MessageList &messages)
      {
        if (messages.size() < 1) return false;

        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("cannot send messages during shutdown"))
          return false;
        }

        if (!mSlaveThread) {
          ZS_LOG_DEBUG(log("cannot send messages without a slave thread object"))
          return false;
        }

        mSlaveThread->updateBegin();
        mSlaveThread->addMessages(messages);
        mSlaveThread->updateEnd(getPublicationRepostiory());

        // kick the conversation thread step routine asynchronously to ensure
        // the thread has a subscription state to its peer
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      Time ConversationThreadSlave::getHostCreationTime() const
      {
        AutoRecursiveLock lock(*this);
        if (!mHostThread) return Time();
        return mHostThread->details()->created();
      }

      //-----------------------------------------------------------------------
      String ConversationThreadSlave::getHostServerName() const
      {
        AutoRecursiveLock lock(*this);
        if (!mHostThread) return String();
        return mHostThread->details()->serverName();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadSlave::safeToChangeContacts() const
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("cannot change contacts is shutting down"))
          return false;
        }
        if (!isHostThreadOpen()) {
          ZS_LOG_DEBUG(log("cannot change contacts since host thread is not open"))
          return false;
        }
        if (!mSlaveThread) {
          ZS_LOG_DEBUG(log("cannot change contacts since slave thread is not ready"))
          return false;
        }

        // we can ask the host to change contacts on our behalf...
        return true;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::getContacts(ThreadContactMap &outContacts) const
      {
        if (!mHostThread) {
          ZS_LOG_DEBUG(log("cannot get contacts without a host thread"))
          return;
        }

        outContacts = mHostThread->contacts()->contacts();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadSlave::inConversation(UseContactPtr contact) const
      {
        AutoRecursiveLock lock(*this);
        if (!mHostThread) {
          ZS_LOG_DEBUG(log("cannot check if contact is in conversation without a host thread"))
          return false;
        }

        const ThreadContactMap &contacts = mHostThread->contacts()->contacts();
        ThreadContactMap::const_iterator found = contacts.find(contact->getPeerURI());
        return found != contacts.end();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::addContacts(const ContactProfileInfoList &contacts)
      {
        AutoRecursiveLock lock(*this);
        ZS_THROW_INVALID_ASSUMPTION_IF(!safeToChangeContacts())

        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("base thread is gone thus thread must shutdown (when attempting to add contacts)"))
          cancel();
          return;
        }

        const ThreadContactMap &existingContacts = mHostThread->contacts()->contacts();

        ThreadContactMap contactMap;
        for (ContactProfileInfoList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
        {
          const ContactProfileInfo &info = (*iter);

          UseContactPtr contact = Contact::convert(info.mContact);

          if (existingContacts.end() != existingContacts.find(info.mContact->getPeerURI())) {
            ZS_LOG_WARNING(Trace, log("will not add contact as it already exists in host thread") + UseContact::toDebug(contact))
            continue;
          }

          ContactStatusInfo status;

          baseThread->getLastContactStatus(contact, status);

          ThreadContactPtr threadContact = ThreadContact::create(1, contact, info.mIdentityContacts, status);
          contactMap[contact->getPeerURI()] = threadContact;
        }

        mSlaveThread->updateBegin();
        mSlaveThread->setContactsToAdd(contactMap);
        mSlaveThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::removeContacts(const ContactList &contacts)
      {
        AutoRecursiveLock lock(*this);
        ZS_THROW_INVALID_ASSUMPTION_IF(!safeToChangeContacts())

        ContactURIList contactIDList;
        for (ContactList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
        {
          UseContactPtr contact = Contact::convert(*iter);
          contactIDList.push_back(contact->getPeerURI());
        }

        mSlaveThread->updateBegin();
        mSlaveThread->setContactsToRemove(contactIDList);
        mSlaveThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      IConversationThread::ContactConnectionStates ConversationThreadSlave::getContactConnectionState(UseContactPtr contact) const
      {
        AutoRecursiveLock lock(*this);

        UseContactPtr hostContact = getHostContact();
        if (!hostContact) {
          ZS_LOG_ERROR(Basic, log("host contact not found on slave thread"))
          return IConversationThread::ContactConnectionState_NotApplicable;
        }
        if (hostContact->getPeerURI() != contact->getPeerURI()) {
          ZS_LOG_DEBUG(log("contact state requested is not the host contact (thus do not know state of peer connnection)"))
          return IConversationThread::ContactConnectionState_NotApplicable;
        }

        if (mPeerLocation) {
          if (mPeerLocation->isConnected()) {
            return IConversationThread::ContactConnectionState_Connected;
          }
        }

        switch (hostContact->getPeer()->getFindState()) {
          case IPeer::PeerFindState_Pending:    return IConversationThread::ContactConnectionState_NotApplicable;
          case IPeer::PeerFindState_Idle:       return (mPeerLocation ? IConversationThread::ContactConnectionState_Disconnected : IConversationThread::ContactConnectionState_NotApplicable);
          case IPeer::PeerFindState_Finding:    return IConversationThread::ContactConnectionState_Finding;
          case IPeer::PeerFindState_Completed:  return (mPeerLocation ? IConversationThread::ContactConnectionState_Disconnected : IConversationThread::ContactConnectionState_NotApplicable);
        }

        return IConversationThread::ContactConnectionState_NotApplicable;
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadSlave::placeCalls(const PendingCallMap &pendingCalls)
      {
        AutoRecursiveLock lock(*this);
        if (!mSlaveThread) {
          ZS_LOG_DEBUG(log("no host thread to clean call from..."))
          return false;
        }

        DialogList additions;

        const DialogMap &dialogs = mSlaveThread->dialogs();
        for (PendingCallMap::const_iterator iter = pendingCalls.begin(); iter != pendingCalls.end(); ++iter)
        {
          const UseCallPtr &call = (*iter).second;
          DialogMap::const_iterator found = dialogs.find(call->getCallID());

          if (found == dialogs.end()) {
            ZS_LOG_DEBUG(log("added call") + call->getDialog()->toDebug())

            additions.push_back(call->getDialog());
          }
        }

        // publish the changes now...
        mSlaveThread->updateBegin();
        mSlaveThread->updateDialogs(additions);
        mSlaveThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::notifyCallStateChanged(UseCallPtr call)
      {
        AutoRecursiveLock lock(*this);

        if (!mSlaveThread) {
          ZS_LOG_WARNING(Detail, log("no slave thread to change the call state call from..."))
          return;
        }
        if (!mHostThread) {
          ZS_LOG_WARNING(Detail, log("no host thread to check if call exists from..."))
        }

        DialogPtr dialog = call->getDialog();
        if (!dialog) {
          ZS_LOG_DEBUG(log("call does not have a dialog yet and is not ready"))
          return;
        }

        DialogList updates;
        updates.push_back(dialog);

        // publish the changes now...
        mSlaveThread->updateBegin();
        mSlaveThread->updateDialogs(updates);
        mSlaveThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::notifyCallCleanup(UseCallPtr call)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!call)

        ZS_LOG_DEBUG(log("call cleanup called") + UseCall::toDebug(call))

        AutoRecursiveLock lock(*this);

        if (!mSlaveThread) {
          ZS_LOG_DEBUG(log("no slave thread to clean call from..."))
          return;
        }

        const DialogMap &dialogs = mSlaveThread->dialogs();
        DialogMap::const_iterator found = dialogs.find(call->getCallID());

        if (found == dialogs.end()) {
          ZS_LOG_WARNING(Detail, log("this call is not present on the host conversation thread thus cannot be cleaned"))
          return;
        }

        DialogIDList removeCallIDs;
        removeCallIDs.push_back(call->getCallID());

        // publish the changes now...
        mSlaveThread->updateBegin();
        mSlaveThread->removeDialogs(removeCallIDs);
        mSlaveThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::gatherDialogReplies(
                                                        const char *callID,
                                                        LocationDialogMap &outDialogs
                                                        ) const
      {
        AutoRecursiveLock lock(*this);
        if (!mHostThread) {
          ZS_LOG_WARNING(Detail, log("unable to gather dialogs from slave's host as host thread object is not valid"))
          return;
        }
        if (!mPeerLocation) {
          ZS_LOG_WARNING(Detail, log("unable to gather dialogs from slave's host as peer location is not valid"))
          return;
        }

        const DialogMap &dialogs = mHostThread->dialogs();
        DialogMap::const_iterator found = dialogs.find(callID);
        if (found == dialogs.end()) {
          ZS_LOG_TRACE(log("did not find any dialog replies") + ZS_PARAM("call ID", callID))
          return;
        }

        const DialogPtr &dialog = (*found).second;
        outDialogs[mPeerLocation->getLocationID()] = dialog;

        ZS_LOG_TRACE(log("found dialog reply") + ZS_PARAM("call ID", callID))
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::markAllMessagesRead()
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("cannot mark read during shutdown"))
          return;
        }

        if (!mSlaveThread) {
          ZS_LOG_DEBUG(log("cannot mark read without a slave thread object"))
          return;
        }

        if (!mHostThread) {
          ZS_LOG_DEBUG(log("no host thread thus nothing to mark as read"))
          return;
        }

        const MessageList &messages = mHostThread->messages();

        if (messages.size() < 1) {
          ZS_LOG_DEBUG(log("no messages to mark as read"))
          return;
        }

        mSlaveThread->updateBegin();

        const MessagePtr &lastMessage = messages.back();
        mSlaveThread->setRead(lastMessage);

        mSlaveThread->updateEnd(getPublicationRepostiory());

        // kick the conversation thread step routine asynchronously to ensure
        // the thread has a subscription state to its peer
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::setStatusInThread(
                                                      UseContactPtr selfContact,
                                                      const IdentityContactList &selfIdentityContacts,
                                                      const ContactStatusInfo &statusOfSelf
                                                      )
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("cannot set status during shutdown"))
          return;
        }

        if (!mSlaveThread) {
          ZS_LOG_DEBUG(log("cannot set status without slave object"))
          return;
        }

        ThreadContactMap contacts;
        contacts[selfContact->getPeerURI()] = ThreadContact::create(1, selfContact, selfIdentityContacts, statusOfSelf);

        mSlaveThread->updateBegin();
        mSlaveThread->setContacts(contacts);
        mSlaveThread->updateEnd(getPublicationRepostiory());

        // kick the conversation thread step routine asynchronously to ensure
        // the thread has a subscription state to its peer
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave => IConversationThreadSlaveForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadSlavePtr ConversationThreadSlave::create(
                                                                 ConversationThreadPtr inBaseThread,
                                                                 ILocationPtr peerLocation,
                                                                 IPublicationMetaDataPtr metaData,
                                                                 const SplitMap &split,
                                                                 const char *serverName
                                                                 )
      {
        UseConversationThreadPtr baseThread = inBaseThread;

        AccountPtr account = baseThread->getAccount();
        if (!account) return ConversationThreadSlavePtr();

        String hostThreadID = UseServicesHelper::get(split, OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX);
        ZS_THROW_INVALID_ARGUMENT_IF(hostThreadID.size() < 1)

        ConversationThreadSlavePtr pThis(new ConversationThreadSlave(UseStack::queueCore(), account, peerLocation, inBaseThread, hostThreadID, serverName));
        pThis->mThisWeak = pThis;

        AutoRecursiveLock lock(*pThis);
        pThis->init();
        pThis->notifyPublicationUpdated(peerLocation, metaData, split);
        return pThis;
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadSlave::getHostMetaData() const
      {
        if (!mHostThread) return ElementPtr();
        
        return UseCoreHelper::clone(mHostThread->details()->metaData());
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave => IConversationThreadDocumentFetcherDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onConversationThreadDocumentFetcherPublicationUpdated(
                                                                                          IConversationThreadDocumentFetcherPtr fetcher,
                                                                                          ILocationPtr peerLocation,
                                                                                          IPublicationPtr publication
                                                                                          )
      {
        ZS_LOG_DEBUG(log("publication was updated notification received") + IPublication::toDebug(publication))

        AutoRecursiveLock lock(*this);

        if (mSelfHoldingStartupReferenceUntilPublicationFetchCompletes) {
          ZS_LOG_DEBUG(log("extra reference to ourselves is removed as publication fetcher is complete"))
          mSelfHoldingStartupReferenceUntilPublicationFetchCompletes.reset();
        }

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("notified about updated publication after shutdown") + IPublication::toDebug(publication))
          return;
        }

        ZS_THROW_BAD_STATE_IF(fetcher != mFetcher)
        ZS_THROW_INVALID_ARGUMENT_IF(!publication)

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus thread must shutdown - happened when receiving updated document") + IPublication::toDebug(publication))
          cancel();
          return;
        }

        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("base thread is gone thus thread must shutdown - happened when receiving updated document") + IPublication::toDebug(publication))
          cancel();
          return;
        }

        // check to see the type of publication
        std::regex e("^\\/contacts\\/1\\.0\\/.*$");
        if (std::regex_match(publication->getName(), e)) {
          // this it a public peer file document to process
          IPublicationLockerPtr locker;
          DocumentPtr doc = publication->getJSON(locker);
          if (!doc) {
            ZS_LOG_WARNING(Detail, log("failed to get peer file contact") + IPublication::toDebug(publication))
            return;
          }
          ElementPtr peerEl = doc->getFirstChildElement();
          if (!peerEl) {
            ZS_LOG_WARNING(Detail, log("failed to get peer element from contact file") + IPublication::toDebug(publication))
            return;
          }

          IPeerFilePublicPtr peerFilePublic = IPeerFilePublic::loadFromElement(peerEl);
          if (!peerFilePublic) {
            ZS_LOG_WARNING(Detail, log("failed to create peer file public") +IPublication::toDebug(publication))
            return;
          }

          IPeerPtr tempPeer = IPeer::create(account->getStackAccount(), peerFilePublic);
          if (!tempPeer) {
            ZS_LOG_WARNING(Detail, log("failed to peer temporary peer element") + IPublication::toDebug(publication))
            return;
          }

          ZS_LOG_DEBUG(log("successfully loaded peer contact") + IPublication::toDebug(publication))
          return;
        }
        
        if (!mHostThread) {
          mHostThread = Thread::create(Account::convert(account), publication);
          if (!mHostThread) {
            ZS_LOG_WARNING(Detail, log("failed to parse conversation thread from host") + IPublication::toDebug(publication))
            cancel();
            return;
          }
          baseThread->notifyAboutNewThreadNowIfNotNotified(mThisWeak.lock());
        } else {
          mHostThread->updateFrom(Account::convert(account), publication);
        }

        //.......................................................................
        // scope: ensure all peer files are fetched for each contact
        {
          const ThreadContactMap &contacts = mHostThread->contacts()->contacts();
          for (ThreadContactMap::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
          {
            const ThreadContactPtr &threadContact = (*iter).second;
            UseContactPtr contact = threadContact->contact();

            // notify of contact status updates (the host is the authoritative source of all statuses (except for the self contact but the base filters those updates)
            baseThread->notifyContactStatus(mThisWeak.lock(), contact, threadContact->status());

            bool hasPeerFilePulic = (bool)contact->getPeerFilePublic();
            if (hasPeerFilePulic) {
              ZS_LOG_TRACE(log("peer file public is found for contact") + UseContact::toDebug(contact))
              continue;
            }

            if (mPreviouslyFetchedContacts.end() != mPreviouslyFetchedContacts.find(contact->getPeerURI())) {
              ZS_LOG_TRACE(log("already attempted to fetch this contact") + UseContact::toDebug(contact))
              continue;
            }

            ZS_LOG_WARNING(Detail, log("peer file public is missing for contact") + UseContact::toDebug(contact))

            IPublicationMetaData::PublishToRelationshipsMap empty;
            IPublicationMetaDataPtr contactMetaData = IPublicationMetaData::create(
                                                                                   0, 0, 0,
                                                                                   publication->getCreatorLocation(),
                                                                                   mHostThread->getContactDocumentName(contact),
                                                                                   publication->getMimeType(),
                                                                                   publication->getEncoding(),
                                                                                   empty,
                                                                                   publication->getPublishedLocation()
                                                                                   );

            // singal to the fetcher it's been updated so the fetcher will download the document immediately
            mFetcher->notifyPublicationUpdated(mPeerLocation, contactMetaData);

            mPreviouslyFetchedContacts[contact->getPeerURI()] = true;
          }
        }

        //.......................................................................
        // examine all the newly received messages...

        const MessageList &messagesChanged = mHostThread->messagedChanged();
        for (MessageList::const_iterator iter = messagesChanged.begin(); iter != messagesChanged.end(); ++iter)
        {
          const MessagePtr &message = (*iter);

          if (mSlaveThread) {
            const MessageMap &sentMessages = mSlaveThread->messagesAsMap();
            MessageMap::const_iterator found = sentMessages.find(message->messageID());
            if (found != sentMessages.end()) {
              ZS_LOG_TRACE(log("no need to notify about messages sent by ourself") + message->toDebug())
              continue;
            }
          }

          ZS_LOG_TRACE(log("notifying of message received") + message->toDebug())
          baseThread->notifyMessageReceived(message);
        }

        //.......................................................................
        // examine all the acknowledged messages

        processReceiptsFromHostDocument(IConversationThread::MessageDeliveryState_Delivered, mHostThread->messagesDeliveredChanged());
        processReceiptsFromHostDocument(IConversationThread::MessageDeliveryState_Read, mHostThread->messagesReadChanged());

        //.......................................................................
        // figure out what dialogs were incoming but need to be removed...

        const DialogIDList &removedDialogs = mHostThread->dialogsRemoved();
        for (DialogIDList::const_iterator iter = removedDialogs.begin(); iter != removedDialogs.end(); ++iter) {
          const String &dialogID = (*iter);
          CallHandlers::iterator found = mIncomingCallHandlers.find(dialogID);
          if (found == mIncomingCallHandlers.end()) {
            ZS_LOG_DEBUG(log("call object is not present (ignored)") + ZS_PARAM("call ID", dialogID))
            continue;
          }

          // this incoming call is now gone, clean it out...
          UseCallPtr &call = (*found).second;

          ZS_LOG_DEBUG(log("call object is now gone") + UseCall::toDebug(call))

          call->notifyConversationThreadUpdated();

          mIncomingCallHandlers.erase(found);

          ZS_LOG_DEBUG(log("removed incoming calll handler") + ZS_PARAM("total handlers", mIncomingCallHandlers.size()))
        }

        //.......................................................................
        // check for dialogs that are now incoming/updated

        const DialogMap &changedDialogs = mHostThread->dialogsChanged();
        for (DialogMap::const_iterator iter = changedDialogs.begin(); iter != changedDialogs.end(); ++iter)
        {
          const String &dialogID = (*iter).first;
          const DialogPtr &dialog = (*iter).second;

          String selfPeerURI = UseContactPtr(account->getSelfContact())->getPeerURI();

          ZS_LOG_TRACE(log("call detected") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI) + ILocation::toDebug(mPeerLocation))

          CallHandlers::iterator found = mIncomingCallHandlers.find(dialogID);
          if (found != mIncomingCallHandlers.end()) {
            UseCallPtr &call = (*found).second;

            ZS_LOG_DEBUG(log("call object is updated") + dialog->toDebug() + UseCall::toDebug(call))
            call->notifyConversationThreadUpdated();
            continue;
          }

          if (dialog->calleePeerURI() != selfPeerURI) {
            if (dialog->callerPeerURI() == selfPeerURI) {
              ZS_LOG_DEBUG(log("call detected this must be a reply to a call previously placed") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI))
              baseThread->notifyPossibleCallReplyStateChange(dialogID);
            } else {
              ZS_LOG_WARNING(Detail, log("incoming call detected but the call is not going to this contact") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI))
            }
            continue;
          }

          String callerPeerURI = dialog->callerPeerURI();
          ContactPtr contact = account->findContact(callerPeerURI);
          if (!contact) {
            ZS_LOG_WARNING(Detail, log("while an incoming call was found the contact is not known to the account thus cannot accept the call") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI))
            continue;
          }

          UseCallPtr call = UseCall::createForIncomingCall(ConversationThread::convert(baseThread), contact, dialog);

          if (!call) {
            ZS_LOG_WARNING(Detail, log("unable to create incoming call from contact") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI))
            continue;
          }

          ZS_LOG_DEBUG(log("found new call") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI))

          // new call handler found...
          mIncomingCallHandlers[dialogID] = call;
          baseThread->requestAddIncomingCallHandler(dialogID, mThisWeak.lock(), call);
        }


        //.......................................................................
        // create the slave thread if it's not created

        if (!mSlaveThread) {
          mSlaveThread = Thread::create(
                                        Account::convert(account),
                                        Thread::ThreadType_Slave,
                                        account->getSelfLocation(),
                                        baseThread->getThreadID(),
                                        mThreadID,
                                        NULL,
                                        NULL,
                                        mServerName,
                                        mHostThread->details()->metaData(),
                                        Details::ConversationThreadState_None,
                                        publication->getPublishedLocation()
                                        );

          if (!mSlaveThread) {
            ZS_LOG_WARNING(Detail, log("failed to create slave thread object - happened when receiving updated document") + IPublication::toDebug(publication))
            cancel();
            return;
          }
        }


        //.......................................................................
        // start updating the slave thread information...

        mSlaveThread->updateBegin();

        if (messagesChanged.size() > 0) {
          const MessagePtr &lastMessage = messagesChanged.back();
          mSlaveThread->setDelivered(lastMessage);
        }


        //.......................................................................
        // figure out all the contact changes

        ThreadContactMap hostContacts = mHostThread->contacts()->contacts();

        ThreadContactMap replacementAddContacts;

        // check to see which contacts do not need to be added anymore and remove them...
        const ThreadContactMap &addContacts = mSlaveThread->contacts()->addContacts();
        for (ThreadContactMap::const_iterator iter = addContacts.begin(); iter != addContacts.end(); ++iter)
        {
          const String &contactID = (*iter).first;
          const ThreadContactPtr &threadContact = (*iter).second;

          ThreadContactMap::const_iterator found = hostContacts.find(contactID);
          if (found == hostContacts.end()) {
            // this contact still needs to be added...
            replacementAddContacts[contactID] = threadContact;
          }
        }


        //.......................................................................
        // these contacts still need to be added (if any)...

        mSlaveThread->setContactsToAdd(replacementAddContacts);


        //.......................................................................
        // figure out which contacts no longer need removal

        ContactURIList replacementRemoveContacts;
        const ContactURIList &removeContacts = mSlaveThread->contacts()->removeContacts();
        for (ContactURIList::const_iterator iter = removeContacts.begin(); iter != removeContacts.end(); ++iter)
        {
          const String &contactID = (*iter);

          ThreadContactMap::const_iterator found = hostContacts.find(contactID);
          if (found != hostContacts.end()) {
            // this contact still needs removing...
            replacementRemoveContacts.push_back(contactID);
          }
        }

        //.......................................................................
        // these contacts still need to be removed (if any)....

        mSlaveThread->setContactsToRemove(replacementRemoveContacts);


        //.......................................................................
        // publish the changes

        mSlaveThread->updateEnd(getPublicationRepostiory());

        step();

        // notify the outer thread that this thread's state might have changed...
        baseThread->notifyStateChanged(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onConversationThreadDocumentFetcherPublicationGone(
                                                                                       IConversationThreadDocumentFetcherPtr fetcher,
                                                                                       ILocationPtr peerLocation,
                                                                                       IPublicationMetaDataPtr metaData
                                                                                       )
      {
        ZS_LOG_WARNING(Detail, log("document fetcher notified publication is gone") + IConversationThreadDocumentFetcher::toDebug(fetcher) + ILocation::toDebug(peerLocation) + IPublicationMetaData::toDebug(metaData))

        AutoRecursiveLock lock(*this);

        if (mSelfHoldingStartupReferenceUntilPublicationFetchCompletes) {
          ZS_LOG_WARNING(Detail, log("extra reference to ourselves is removed as publication fetcher is complete (albeit a failure case)"))
          mSelfHoldingStartupReferenceUntilPublicationFetchCompletes.reset();
        }

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_DEBUG(log("notified publication gone after shutdown") + IPublicationMetaData::toDebug(metaData))
          return;
        }
        ZS_THROW_BAD_STATE_IF(fetcher != mFetcher)

        //*********************************************************************
        //*********************************************************************
        //*********************************************************************
        //*********************************************************************
        // HERE!!!! - self descruct?
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave => IBackgroundingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onBackgroundingGoingToBackground(
                                                                     IBackgroundingSubscriptionPtr subscription,
                                                                     IBackgroundingNotifierPtr notifier
                                                                     )
      {
        ZS_LOG_DEBUG(log("notified going to background") + ZS_PARAM("subscription id", subscription->getID()) + ZS_PARAM("notifier", notifier->getID()))

        AutoRecursiveLock lock(*this);

        mBackgroundingNotifier = notifier;
        mBackgroundingNow = false;
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onBackgroundingGoingToBackgroundNow(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("notified going to background now") + ZS_PARAM("subscription id", subscription->getID()))

        AutoRecursiveLock lock(*this);

        mBackgroundingNow = true;
        step();

        mBackgroundingNotifier.reset();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onBackgroundingReturningFromBackground(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("notified returning from background now") + ZS_PARAM("subscription id", subscription->getID()))

        AutoRecursiveLock lock(*this);

        mBackgroundingNotifier.reset();

        mBackgroundingNow = false;
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onBackgroundingApplicationWillQuit(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("notified application will quit") + ZS_PARAM("subscription id", subscription->getID()))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onWake()
      {
        ZS_LOG_DEBUG(log("on wake"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onTimer(TimerPtr timer)
      {
        ZS_LOG_DEBUG(log("on timer"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave => IPeerSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(*this);
        if (subscription != mHostSubscription) {
          ZS_LOG_DEBUG(log("received peer subscription shutdown on an obsolete subscription"))
          return;
        }

        // be sure it's truly gone...
        mHostSubscription->cancel();
        mHostSubscription.reset();

        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onPeerSubscriptionFindStateChanged(
                                                                       IPeerSubscriptionPtr subscription,
                                                                       IPeerPtr peer,
                                                                       PeerFindStates state
                                                                       )
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onPeerSubscriptionLocationConnectionStateChanged(
                                                                                     IPeerSubscriptionPtr subscription,
                                                                                     ILocationPtr location,
                                                                                     LocationConnectionStates state
                                                                                     )
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("peer subscriptions location changed called"))
        if (subscription != mHostSubscription) {
          ZS_LOG_WARNING(Detail, log("peer subscription notification came from obsolete subscription") + IPeerSubscription::toDebug(subscription))
          return;
        }

        UseContactPtr hostContact = getHostContact();
        if (!hostContact) {
          cancel();
          return;
        }

        IPeerPtr peer = hostContact->getPeer();

        LocationListPtr peerLocations = peer->getLocationsForPeer(true);

        bool mustConvertToHost = false;

        if ((peerLocations->size() > 0) &&
            (mSlaveThread) &&
            (!mConvertedToHostBecauseOriginalHostLikelyGoneForever)) {
          ZS_LOG_DEBUG(log("peer locations are detected - checking if original host location is still active") + ILocation::toDebug(mPeerLocation))

          bool foundHostLocation = false;
          for (LocationList::iterator iter = peerLocations->begin(); iter != peerLocations->end(); ++iter)
          {
            ILocationPtr &peerLocation = (*iter);
            ZS_LOG_TRACE(log("detected peer location") + ZS_PARAM("peer's peer uri", peerLocation->getPeerURI()) + ILocation::toDebug(peerLocation))
            if (peerLocation->getLocationID() == mPeerLocation->getLocationID()) {
              ZS_LOG_DEBUG(log("found host's peer location expected") + ILocation::toDebug(peerLocation))
              foundHostLocation = true;
              break;
            }
          }

          if (!foundHostLocation) {
            ZS_LOG_WARNING(Detail, log("found the host contact's peer location but it's not the location expected") + ZS_PARAM("expecting", ILocation::toDebug(mPeerLocation)))
            // found the host but it's not the host we expected... It's likely
            // the peer restarted their application thus we need to make sure
            // any undelivered messages get pushed into a new conversation thread

            const MessageList &messages = mSlaveThread->messages();
            if (messages.size() > 0) {
              const MessagePtr &message = messages.back();
              ZS_LOG_TRACE(log("examining message delivery state for message") + message->toDebug())

              MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(message->messageID());
              if (found == mMessageDeliveryStates.end()) {
                ZS_LOG_DEBUG(log("found message that has not delivered yet") + message->toDebug())
                mustConvertToHost = true;
                goto done_checking_for_undelivered_messages;
              }

              MessageDeliveryStatePtr &deliveryState = (*found).second;
              if (IConversationThread::MessageDeliveryState_Delivered > deliveryState->mState) {
                ZS_LOG_DEBUG(log("found message that has not delivered or read yet") + message->toDebug() + ZS_PARAM("state", IConversationThread::toString(deliveryState->mState)))
                mustConvertToHost = true;
                goto done_checking_for_undelivered_messages;
              }
            }
          }
        }

      done_checking_for_undelivered_messages:

        if (!mHostThread) {
          ZS_LOG_WARNING(Detail, log("no host thread associated with this slave"))
          mustConvertToHost = false;
        }

        if (mustConvertToHost) {
          UseConversationThreadPtr baseThread = mBaseThread.lock();
          if (!baseThread) {
            ZS_LOG_WARNING(Detail, log("base conversation thread is gone (thus slave must self destruct)"))
            cancel();
            return;
          }

          ZS_LOG_DETAIL(log("converting slave to host as likely original host is gone forever and messages need delivering") + ILocation::toDebug(mPeerLocation))
          mConvertedToHostBecauseOriginalHostLikelyGoneForever = true;
          baseThread->convertSlaveToClosedHost(mThisWeak.lock(), mHostThread, mSlaveThread);
        }
        
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::onPeerSubscriptionMessageIncoming(
                                                                      IPeerSubscriptionPtr subscription,
                                                                      IMessageIncomingPtr message
                                                                      )
      {
        // IGNORED
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ConversationThreadSlave::log(const char *message) const
      {
        String baseThreadID;
        UseConversationThreadPtr baseThread = mBaseThread.lock();

        if (baseThread) baseThreadID = baseThread->getThreadID();

        ElementPtr objectEl = Element::create("core::ConversationThreadSlave");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        UseServicesHelper::debugAppend(objectEl, "base thread id", baseThreadID);
        UseServicesHelper::debugAppend(objectEl, "slave thread id", mThreadID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadSlave::toDebug() const
      {
        AutoRecursiveLock lock(*this);
        UseConversationThreadPtr base = mBaseThread.lock();
        UseAccountPtr account = mAccount.lock();

        ElementPtr resultEl = Element::create("core::ConversationThreadSlave");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "base thread id", base ? base->getThreadID() : String());
        UseServicesHelper::debugAppend(resultEl, "account id", (bool)account);

        UseServicesHelper::debugAppend(resultEl, "slave thread id", mThreadID);
        UseServicesHelper::debugAppend(resultEl, ILocation::toDebug(mPeerLocation));

        UseServicesHelper::debugAppend(resultEl, "state", toString(mCurrentState));

        UseServicesHelper::debugAppend(resultEl, IConversationThreadDocumentFetcher::toDebug(mFetcher));

        UseServicesHelper::debugAppend(resultEl, Thread::toDebug(mHostThread));
        UseServicesHelper::debugAppend(resultEl, Thread::toDebug(mSlaveThread));

        UseServicesHelper::debugAppend(resultEl, "backgrounding subscription id", mBackgroundingSubscription ? mBackgroundingSubscription->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "backgrounding notifier id", mBackgroundingNotifier ? mBackgroundingNotifier->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "backgrounding now", mBackgroundingNow);

        UseServicesHelper::debugAppend(resultEl, IPeerSubscription::toDebug(mHostSubscription));

        UseServicesHelper::debugAppend(resultEl, "convert to host", mConvertedToHostBecauseOriginalHostLikelyGoneForever);

        UseServicesHelper::debugAppend(resultEl, "delivery states", mMessageDeliveryStates.size());

        UseServicesHelper::debugAppend(resultEl, "incoming call handlers", mIncomingCallHandlers.size());
        UseServicesHelper::debugAppend(resultEl, "previously fetched contacts", mPreviouslyFetchedContacts.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        mSelfHoldingStartupReferenceUntilPublicationFetchCompletes.reset();

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(ConversationThreadSlaveState_ShuttingDown);

        UseConversationThreadPtr baseThread = mBaseThread.lock();

        if (mBackgroundingSubscription) {
          mBackgroundingSubscription->cancel();
          mBackgroundingSubscription.reset();
        }

        mBackgroundingNotifier.reset();

        if (baseThread) {
          for (CallHandlers::iterator iter = mIncomingCallHandlers.begin(); iter != mIncomingCallHandlers.end(); ++iter)
          {
            const CallID &callID = (*iter).first;
            baseThread->requestRemoveIncomingCallHandler(callID);
          }
        }

        if (mFetcher) {
          mFetcher->cancel();
          mFetcher.reset();
        }

        if (mHostSubscription) {
          mHostSubscription->cancel();
          mHostSubscription.reset();
        }

        setState(ConversationThreadSlaveState_Shutdown);

        mGracefulShutdownReference.reset();

        mMessageDeliveryStates.clear();

        mHostThread.reset();
        mSlaveThread.reset();

        mPeerLocation.reset();

        ZS_LOG_DEBUG(log("cancel completed"))
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::step()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("step") + toDebug())

        if ((isShuttingDown()) ||
            (isShutdown())) {
          cancel();
          return;
        }

        UseAccountPtr account = mAccount.lock();
        stack::IAccountPtr stackAccount = account->getStackAccount();
        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if ((!account) ||
            (!stackAccount) ||
            (!baseThread)) {
          ZS_LOG_WARNING(Detail, log("account, stack account or base thread is gone thus thread must shutdown"))
          cancel();
          return;
        }

        if (!mHostThread) {
          ZS_LOG_TRACE(log("waiting for host thread to be ready..."))
          return;
        }

        if (!mSlaveThread) {
          ZS_LOG_TRACE(log("waiting for slave thread to be ready..."))
          return;
        }

        setState(ConversationThreadSlaveState_Ready);

        bool requiresSubscription = false;

        // check to see if there are undelivered messages, if so we will need a subscription...
        const MessageList &messages = mSlaveThread->messages();
        MessageList::const_reverse_iterator last = messages.rbegin();
        if (last != messages.rend()) {
          const MessagePtr &message = (*last);

          // check to see if this message has been acknowledged before...
          MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(message->messageID());
          if (found != mMessageDeliveryStates.end()) {
            MessageDeliveryStatePtr &deliveryState = (*found).second;
            if (IConversationThread::MessageDeliveryState_Delivered > deliveryState->mState) {
              ZS_LOG_TRACE(log("requires subscription because of undelivered message") + message->toDebug() + ZS_PARAM("current delivery state", IConversationThread::toString(deliveryState->mState)))
              requiresSubscription = true;
            }
          } else {
            ZS_LOG_TRACE(log("requires subscription because of undelivered message") + message->toDebug())
            requiresSubscription = true;
          }
        }

        if (!requiresSubscription) {
          ZS_LOG_DEBUG(log("no outstanding undelivered messages thus no need to prevent backgrounding") + ZS_PARAM("notifier", mBackgroundingNotifier ? mBackgroundingNotifier->getID() : 0))
          mBackgroundingNotifier.reset();
        }

        if (mSlaveThread->dialogs().size() > 0) {
          ZS_LOG_TRACE(log("slave thread has dialogs (i.e. calls) so a subscription is required"))
          requiresSubscription = true;
        }

        UseContactPtr hostContact = getHostContact();
        if (!hostContact) {
          cancel();
          return;
        }

        if (requiresSubscription) {
          ZS_LOG_TRACE(log("subscription to host is required"))
          if (!mHostSubscription) {
            mHostSubscription = IPeerSubscription::subscribe(hostContact->getPeer(), mThisWeak.lock());
          }
        } else {
          ZS_LOG_TRACE(log("subscription to host is NOT required"))
          if (mHostSubscription) {
            mHostSubscription->cancel();
            mHostSubscription.reset();
          }
        }

        // scope: fix the state of pending messages...
        if (mHostSubscription) {
          IPeerPtr peer = hostContact->getPeer();
          IPeer::PeerFindStates state = peer->getFindState();
          ZS_LOG_TRACE(log("host subscription state") + ZS_PARAM("state", IPeer::toString(state)))

          LocationListPtr peerLocations = peer->getLocationsForPeer(false);

          const MessageList &messages = mSlaveThread->messages();

          // Search from the back of the list to the front for messages that
          // aren't delivered as they need to be marked as undeliverable
          // since there are no peer locations available for this user...
          for (MessageList::const_reverse_iterator iter = messages.rbegin(); iter != messages.rend(); ++iter) {
            const MessagePtr &message = (*iter);
            MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(message->messageID());

            MessageDeliveryStatePtr deliveryState;

            if (found != mMessageDeliveryStates.end()) {
              bool stopProcessing = false;
              deliveryState = (*found).second;
              switch (deliveryState->mState) {
                case IConversationThread::MessageDeliveryState_Discovering:   {
                  break;
                }
                case IConversationThread::MessageDeliveryState_UserNotAvailable:
                case IConversationThread::MessageDeliveryState_Sent:
                case IConversationThread::MessageDeliveryState_Delivered:
                case IConversationThread::MessageDeliveryState_Read:          {
                  stopProcessing = true;
                  break;
                }
              }

              if (stopProcessing) {
                ZS_LOG_TRACE(log("processing undeliverable messages stopped since message already has a delivery state") + message->toDebug())
                break;
              }
            } else {
              deliveryState = MessageDeliveryState::create(mThisWeak.lock(), IConversationThread::MessageDeliveryState_Discovering);
              mMessageDeliveryStates[message->messageID()] = deliveryState;
            }

            if ( (((IPeer::PeerFindState_Completed == state) ||
                   (IPeer::PeerFindState_Idle == state)) &&
                  (peerLocations->size() < 1)) ||
                (deliveryState->shouldPush(mBackgroundingNow))) {
              ZS_LOG_TRACE(log("message develivery state must now be set to undeliverable") + message->toDebug() + ZS_PARAM("peer find state", IPeer::toString(state)) + ZS_PARAM("last state changed time", deliveryState->mLastStateChanged) + ZS_PARAM("current time", zsLib::now()))

              deliveryState->setState(IConversationThread::MessageDeliveryState_UserNotAvailable);
              baseThread->notifyMessageDeliveryStateChanged(message->messageID(), IConversationThread::MessageDeliveryState_UserNotAvailable);

              // tell the application to push this message out as a push notification
              baseThread->notifyMessagePush(message, hostContact);
            }
          }
        }

        baseThread->notifyContactConnectionState(mThisWeak.lock(), hostContact, getContactConnectionState(hostContact));

        ZS_LOG_TRACE(log("step complete") + toDebug())
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::setState(ConversationThreadSlaveStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ZS_PARAM("old state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;
      }


      //-----------------------------------------------------------------------
      UseContactPtr ConversationThreadSlave::getHostContact() const
      {
        if (!mHostThread) {
          ZS_LOG_WARNING(Detail, log("cannot obtain host contact because host thread is NULL"))
          return UseContactPtr();
        }
        IPublicationPtr publication = mHostThread->publication();
        if (!publication) {
          ZS_LOG_ERROR(Detail, log("cannot obtain host contact because host thread publication is NULL"))
          return ContactPtr();
        }

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("cannot obtain host contact because account is gone"))
          return ContactPtr();
        }

        ContactPtr contact = account->findContact(publication->getCreatorLocation()->getPeer()->getPeerURI());
        if (!contact) {
          ZS_LOG_WARNING(Detail, log("cannot obtain host contact because contact was not found") + IPublication::toDebug(publication))
        }
        return contact;
      }

      //-----------------------------------------------------------------------
      IPublicationRepositoryPtr ConversationThreadSlave::getPublicationRepostiory()
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus cannot publish any publications"))
          return IPublicationRepositoryPtr();
        }

        return account->getRepository();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::processReceiptsFromHostDocument(
                                                                    MessageDeliveryStates applyDeliveryState,
                                                                    const MessageReceiptMap &messagesChanged
                                                                    )
      {
        UseConversationThreadPtr baseThread = mBaseThread.lock();

        if (!mSlaveThread) {
          ZS_LOG_DEBUG(log("no slave thread object to process messages"))
          return;
        }

        const MessageList &messages = mSlaveThread->messages();
        const MessageMap &messagesMap = mSlaveThread->messagesAsMap();

        // can only examine message receipts that are part of the slave thread...
        for (MessageReceiptMap::const_iterator iter = messagesChanged.begin(); iter != messagesChanged.end(); ++iter)
        {
          const MessageID &id = (*iter).first;

          ZS_LOG_TRACE(log("examining message delivery") + ZS_PARAM("message ID", id))

          // check to see if this receipt has already been marked as delivered...
          MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(id);
          if (found != mMessageDeliveryStates.end()) {
            MessageDeliveryStatePtr &deliveryState = (*found).second;

            if (deliveryState->mState >= applyDeliveryState) {
              ZS_LOG_DEBUG(log("message delivery state was already notified as a greater state (thus no need to notify any further)") + ZS_PARAM("message ID", id) + ZS_PARAM("current state", IConversationThread::toString(deliveryState->mState)) + ZS_PARAM("apply state", IConversationThread::toString(applyDeliveryState)))
              continue;
            }
          }

          MessageMap::const_iterator foundInMap = messagesMap.find(id);
          if (foundInMap == messagesMap.end()) {
            ZS_LOG_WARNING(Detail, log("slave never sent this message to the host (message delivery acking a different slave?)") + ZS_PARAM("message ID", id))
            continue;
          }

          bool foundMessageID = false;

          // Need to acknowledge of the delivery state of every message sent
          // before the ACKed message since an acknowledgement on a later
          // message is an acknowledgement of an earlier message.
          //
          // Any message sent after the found message cannot be acked.
          MessageList::const_reverse_iterator messageIter = messages.rbegin();
          for (; messageIter != messages.rend(); ++messageIter)
          {
            const MessagePtr &message = (*messageIter);
            if (message->messageID() == id) {
              ZS_LOG_TRACE(log("found message matching delivery") + ZS_PARAM("message ID", id))
              foundMessageID = true;
            }

            ZS_LOG_TRACE(log("processing slave message") + ZS_PARAM("found", foundMessageID) + message->toDebug())

            if (!foundMessageID) continue;

            // first check if this delivery was already sent...
            found = mMessageDeliveryStates.find(message->messageID());
            if (found != mMessageDeliveryStates.end()) {
              // check to see if this message was already marked as delivered
              MessageDeliveryStatePtr &deliveryState = (*found).second;

              if (deliveryState->mState >= applyDeliveryState) {
                ZS_LOG_DEBUG(log("stopping backward list receipt acking because message delivery state was already notified as a greater state (thus no need to notify any further)") + ZS_PARAM("message ID", id) + ZS_PARAM("current state", IConversationThread::toString(deliveryState->mState)) + ZS_PARAM("apply state", IConversationThread::toString(applyDeliveryState)) + message->toDebug())
                break;
              }

              ZS_LOG_DEBUG(log("message receipt is now processed") + ZS_PARAM("old state", IConversationThread::toString(deliveryState->mState)) + ZS_PARAM("apply state", IConversationThread::toString(applyDeliveryState)) + message->toDebug() )

              // change the state to delivered since it wasn't delivered
              deliveryState->setState(applyDeliveryState);
            } else {
              ZS_LOG_DEBUG(log("message is delivery state is now set") + ZS_PARAM("apply state", IConversationThread::toString(applyDeliveryState)) + message->toDebug())
              mMessageDeliveryStates[message->messageID()] = MessageDeliveryState::create(mThisWeak.lock(), applyDeliveryState);
            }

            if (baseThread) {
              // this message is now considered acknowledged so tell the master thread of the new state...
              baseThread->notifyMessageDeliveryStateChanged(id, applyDeliveryState);
            }
          }
        }
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadSlave::MessageDeliveryState
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadSlave::MessageDeliveryState::~MessageDeliveryState()
      {
        mOuter.reset();
        if (mPushTimer) {
          mPushTimer->cancel();
          mPushTimer.reset();
        }
      }

      //-----------------------------------------------------------------------
      ConversationThreadSlave::MessageDeliveryStatePtr ConversationThreadSlave::MessageDeliveryState::create(
                                                                                                             ConversationThreadSlavePtr owner,
                                                                                                             MessageDeliveryStates state
                                                                                                             )
      {
        MessageDeliveryStatePtr pThis(new MessageDeliveryState);
        pThis->mOuter = owner;
        pThis->mState = state;
        pThis->setState(state);
        return pThis;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadSlave::MessageDeliveryState::setState(MessageDeliveryStates state)
      {
        mLastStateChanged = zsLib::now();
        mState = state;

        if (IConversationThread::MessageDeliveryState_Discovering == state) {
          if (mPushTimer) return;


          ConversationThreadSlavePtr outer = mOuter.lock();
          if (!outer) return; // no point adding a timer if the outer object is gone

          mPushTimer = Timer::create(outer, Seconds(OPENPEER_CONVERSATION_THREAD_MAX_WAIT_DELIVERY_TIME_BEFORE_PUSH_IN_SECONDS), false);
          mPushTime = mLastStateChanged + Seconds(OPENPEER_CONVERSATION_THREAD_MAX_WAIT_DELIVERY_TIME_BEFORE_PUSH_IN_SECONDS);
          return;
        }

        if (!mPushTimer) return;

        mPushTimer->cancel();
        mPushTimer.reset();

        mPushTime = Time();

        switch (mState) {
          case IConversationThread::MessageDeliveryState_Discovering:       break;  // not possible anyway
          case IConversationThread::MessageDeliveryState_UserNotAvailable:
          case IConversationThread::MessageDeliveryState_Sent:
          case IConversationThread::MessageDeliveryState_Delivered:
          case IConversationThread::MessageDeliveryState_Read:              mOuter.reset(); break;  // no longer require link to outer
        }
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadSlave::MessageDeliveryState::shouldPush(bool backgroundingNow) const
      {
        if (IConversationThread::MessageDeliveryState_Discovering != mState) return false;
        if (Time() == mPushTime) return false;
        if (backgroundingNow) return true;

        return (zsLib::now() >= mPushTime);
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
        return ConversationThreadSlaveFactory::singleton();
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

    }
  }
}
