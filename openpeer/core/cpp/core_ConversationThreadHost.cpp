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


#include <openpeer/core/internal/core_ConversationThreadHost.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_Call.h>
#include <openpeer/core/internal/core_Helper.h>
#include <openpeer/core/internal/core_Stack.h>

#include <openpeer/stack/IPublication.h>
#include <openpeer/stack/IHelper.h>

#include <openpeer/services/IHelper.h>

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
      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::ForConversationThread, ForConversationThread)

      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::UseAccount, UseAccount)
      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::UseContact, UseContact)
      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::UseConversationThread, UseConversationThread)

      typedef IStackForInternal UseStack;

      using services::IHelper;

      using namespace core::internal::thread;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "core::ConversationThreadHost");
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadHostForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IConversationThreadHostForConversationThread::toDebug(ForConversationThreadPtr host)
      {
        return ConversationThreadHost::toDebug(ConversationThreadHost::convert(host));
      }

      //-----------------------------------------------------------------------
      ForConversationThreadPtr IConversationThreadHostForConversationThread::create(
                                                                                    ConversationThreadPtr baseThread,
                                                                                    const char *serverName,
                                                                                    Details::ConversationThreadStates state
                                                                                    )
      {
        return IConversationThreadHostFactory::singleton().createConversationThreadHost(baseThread, serverName, state);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost
      #pragma mark

      //-----------------------------------------------------------------------
      const char *ConversationThreadHost::toString(ConversationThreadHostStates state)
      {
        switch (state)
        {
          case ConversationThreadHostState_Pending:       return "Pending";
          case ConversationThreadHostState_Ready:         return "Ready";
          case ConversationThreadHostState_ShuttingDown:  return "Shutting down";
          case ConversationThreadHostState_Shutdown:      return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::ConversationThreadHost(
                                                     IMessageQueuePtr queue,
                                                     AccountPtr account,
                                                     ConversationThreadPtr baseThread,
                                                     const char *threadID,
                                                     const char *serverName
                                                     ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*baseThread),
        mThreadID(threadID ? String(threadID) : services::IHelper::randomString(32)),
        mBaseThread(baseThread),
        mServerName(serverName),
        mCurrentState(ConversationThreadHostState_Pending),
        mAccount(account),
        mSelfContact(UseAccountPtr(account)->getSelfContact())
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::init(Details::ConversationThreadStates state)
      {
        UseAccountPtr account = mAccount.lock();
        ZS_THROW_INVALID_ASSUMPTION_IF(!account)

        AutoRecursiveLock lock(*this);

        UseConversationThreadPtr baseThread = mBaseThread.lock();

        mHostThread = Thread::create(
                                     Account::convert(account),
                                     Thread::ThreadType_Host,
                                     account->getSelfLocation(),
                                     baseThread->getThreadID(),
                                     mThreadID,
                                     "",
                                     "",
                                     mServerName,
                                     state
                                     );

        ZS_THROW_BAD_STATE_IF(!mHostThread)

        mHostThread->updateBegin();
        mHostThread->updateEnd(getPublicationRepostiory());

        step();
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::~ConversationThreadHost()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      ConversationThreadHostPtr ConversationThreadHost::convert(ForConversationThreadPtr object)
      {
        return dynamic_pointer_cast<ConversationThreadHost>(object);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadHost::toDebug(ConversationThreadHostPtr host)
      {
        if (!host) return ElementPtr();
        return host->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost => IConversationThreadHostSlaveBase
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadHostPtr ConversationThreadHost::create(
                                                               ConversationThreadPtr inBaseThread,
                                                               const char *serverName,
                                                               Details::ConversationThreadStates state
                                                               )
      {
        UseConversationThreadPtr baseThread = inBaseThread;

        AccountPtr account = baseThread->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, slog("unable to create a new conversation thread object as account object is null"))
          return ConversationThreadHostPtr();
        }

        ConversationThreadHostPtr pThis(new ConversationThreadHost(UseStack::queueCore(), account, inBaseThread, NULL, serverName));
        pThis->mThisWeak = pThis;
        pThis->init(state);
        return pThis;
      }

      //-----------------------------------------------------------------------
      String ConversationThreadHost::getThreadID() const
      {
        AutoRecursiveLock lock(*this);
        return mThreadID;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::shutdown()
      {
        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::isHostThreadOpen() const
      {
        if (!mHostThread) return false;
        if (!mHostThread->details()) return false;
        return Details::ConversationThreadState_Open == mHostThread->details()->state();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyPublicationUpdated(
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

        PeerContactPtr peerContact = findContact(peerLocation);
        if (!peerContact) {
          ZS_LOG_WARNING(Detail, log("failed to notify of an updated publication because unable to find peer contact") + IPublicationMetaData::toDebug(metaData))
          return;
        }

        peerContact->notifyPublicationUpdated(peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyPublicationGone(
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

        PeerContactPtr peerContact = findContact(peerLocation);
        if (!peerContact) {
          ZS_LOG_WARNING(Detail, log("failed to notify of a publication being gone because unable to find peer contact") + IPublicationMetaData::toDebug(metaData))
          return;
        }

        peerContact->notifyPublicationGone(peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyPeerDisconnected(ILocationPtr peerLocation)
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("notification of a peer shutdown") + ILocation::toDebug(peerLocation))
          return;
        }
        PeerContactPtr peerContact = findContact(peerLocation);

        if (!peerContact) {
          ZS_LOG_WARNING(Detail, log("failed to notify of a peer disconnection because unable to find peer contact"))
          return;
        }

        peerContact->notifyPeerDisconnected(peerLocation);
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::sendMessages(const MessageList &messages)
      {
        AutoRecursiveLock lock(*this);

        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("unable to send messages to a thread that is shutdown"))
          return false;
        }

        ZS_THROW_BAD_STATE_IF(!mHostThread)

        mHostThread->updateBegin();
        mHostThread->addMessages(messages);
        mHostThread->updateEnd(getPublicationRepostiory());

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      Time ConversationThreadHost::getHostCreationTime() const
      {
        AutoRecursiveLock lock(*this);
        if (!mHostThread) return Time();
        return mHostThread->details()->created();
      }

      //-----------------------------------------------------------------------
      String ConversationThreadHost::getHostServerName() const
      {
        AutoRecursiveLock lock(*this);
        if (!mHostThread) return String();
        return mHostThread->details()->serverName();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::safeToChangeContacts() const
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("cannot change contacts during shutdown"))
          return false;
        }

        ZS_THROW_INVALID_ASSUMPTION_IF(!mHostThread)

        if (!isHostThreadOpen()) {
          if (mHostThread->contacts()->contacts().size() > 0) {
            ZS_LOG_DEBUG(log("not safe to add contacts since host thread is not open"))
            return false;
          }
        }

        if (mHostThread->messages().size() > 0) {
          ZS_LOG_DEBUG(log("not safe to add messages with outstanding messages"))
          return false;
        }

        //*********************************************************************
        //*********************************************************************
        //*********************************************************************
        //*********************************************************************
        // HERE - check dialog state?

        return true;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::getContacts(ThreadContactMap &outContacts) const
      {
        AutoRecursiveLock lock(*this);
        if (!mHostThread) {
          ZS_LOG_DEBUG(log("cannot get contacts without a host thread"))
          return;
        }

        outContacts = mHostThread->contacts()->contacts();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::inConversation(UseContactPtr contact) const
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
      void ConversationThreadHost::addContacts(const ContactProfileInfoList &contacts)
      {
        AutoRecursiveLock lock(*this);
        ZS_THROW_INVALID_ASSUMPTION_IF(!safeToChangeContacts())

        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("cannot add contacts as no base thread present"))
          return;
        }

        const ThreadContactMap &oldContacts = mHostThread->contacts()->contacts();

        ThreadContactMap contactMap = oldContacts;

        for (ContactProfileInfoList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
        {
          const ContactProfileInfo &info = (*iter);

          UseContactPtr contact = Contact::convert(info.mContact);

          Time statusTime;
          String statusHash;
          ElementPtr status;

          baseThread->getLastContactStatus(contact, statusTime, statusHash, status);

          ThreadContactPtr threadContact = ThreadContact::create(1, contact, info.mIdentityContacts, statusTime, statusHash, status);

          // remember this contact as needing to be added to the final list...
          contactMap[contact->getPeerURI()] = threadContact;

          if (contact->getPeerURI() != mSelfContact->getPeerURI()) {
            PeerContactMap::iterator found = mPeerContacts.find(contact->getPeerURI());
            if (found == mPeerContacts.end()) {
              PeerContactPtr peerContact = PeerContact::create(getAssociatedMessageQueue(), mThisWeak.lock(), contact, info.mIdentityContacts);
              if (peerContact) {
                mPeerContacts[contact->getPeerURI()] = peerContact;
              }
            }
          }
        }

        // publish changes...
        mHostThread->updateBegin();
        mHostThread->setContacts(contactMap);
        mHostThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::removeContacts(const ContactList &contacts)
      {
        AutoRecursiveLock lock(*this);

        ZS_THROW_INVALID_ASSUMPTION_IF(!safeToChangeContacts())

        const ThreadContactMap &oldContacts = mHostThread->contacts()->contacts();

        ThreadContactMap contactMap = oldContacts;

        // get ride of all those on the list that need to be removed...
        for (ContactList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
        {
          UseContactPtr contact = Contact::convert(*iter);
          ThreadContactMap::iterator found = contactMap.find(contact->getPeerURI());
          if (found == contactMap.end()) continue;

          contactMap.erase(found);
        }

        mHostThread->updateBegin();
        mHostThread->setContacts(contactMap);
        mHostThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      IConversationThread::ContactConnectionStates ConversationThreadHost::getContactConnectionState(UseContactPtr contact) const
      {
        AutoRecursiveLock lock(*this);

        PeerContactMap::const_iterator found = mPeerContacts.find(contact->getPeerURI());
        if (found == mPeerContacts.end()) {
          if (contact->isSelf()) {
            ZS_LOG_DEBUG(log("contact state of self is always based upon account state") + UseContact::toDebug(contact))

            UseAccountPtr account = mAccount.lock();

            if (account) {
              switch (account->getState()) {
                case IAccount::AccountState_Pending:
                case IAccount::AccountState_PendingPeerFilesGeneration:
                case IAccount::AccountState_WaitingForAssociationToIdentity:
                case IAccount::AccountState_WaitingForBrowserWindowToBeLoaded:
                case IAccount::AccountState_WaitingForBrowserWindowToBeMadeVisible:
                case IAccount::AccountState_WaitingForBrowserWindowToClose:
                {
                  return IConversationThread::ContactConnectionState_Finding;
                }
                case IAccount::AccountState_Ready:
                {
                  return IConversationThread::ContactConnectionState_Connected;
                }
                case IAccount::AccountState_ShuttingDown:
                case IAccount::AccountState_Shutdown:
                {
                  return IConversationThread::ContactConnectionState_Disconnected;
                }
              }
            }
            return IConversationThread::ContactConnectionState_NotApplicable;
          }
          ZS_LOG_WARNING(Detail, log("contact was not found as part of the conversation") + UseContact::toDebug(contact))
          return IConversationThread::ContactConnectionState_NotApplicable;
        }

        const PeerContactPtr &peerContact = (*found).second;
        return peerContact->getContactConnectionState();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::placeCalls(const PendingCallMap &pendingCalls)
      {
        if (pendingCalls.size() < 1) {
          ZS_LOG_WARNING(Detail, log("requiest to place calls but no calls to place"))
          return false;
        }

        AutoRecursiveLock lock(*this);
        if (!mHostThread) {
          ZS_LOG_DEBUG(log("no host thread to clean call from..."))
          return false;
        }

        DialogList additions;

        const DialogMap &dialogs = mHostThread->dialogs();
        for (PendingCallMap::const_iterator iter = pendingCalls.begin(); iter != pendingCalls.end(); ++iter)
        {
          const UseCallPtr &call = (*iter).second;
          DialogMap::const_iterator found = dialogs.find(call->getCallID());

          if (found == dialogs.end()) {
            ZS_LOG_DEBUG(log("adding call") +
                         ZS_PARAM("call ID", call->getCallID()) +
                         ZS_PARAM("caller peer URI", UseContactPtr(call->getCaller())->getPeerURI()) + ZS_PARAM("caller self", UseContactPtr(call->getCaller())->isSelf()) +
                         ZS_PARAM("callee peer URI", UseContactPtr(call->getCallee())->getPeerURI()) + ZS_PARAM("callee self", UseContactPtr(call->getCallee())->isSelf()))

            additions.push_back(call->getDialog());
          }
        }

        // publish the changes now...
        mHostThread->updateBegin();
        mHostThread->updateDialogs(additions);
        mHostThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyCallStateChanged(UseCallPtr call)
      {
        AutoRecursiveLock lock(*this);

        if (!mHostThread) {
          ZS_LOG_DEBUG(log("no host thread to clean call from..."))
          return;
        }

        DialogPtr dialog = call->getDialog();
        if (!dialog) {
          ZS_LOG_DEBUG(log("call does not have a dialog yet and is not ready"))
          return;
        }

        DialogList updates;
        updates.push_back(dialog);

        // publish the changes now...
        mHostThread->updateBegin();
        mHostThread->updateDialogs(updates);
        mHostThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyCallCleanup(UseCallPtr call)
      {
        ZS_LOG_DEBUG(log("call cleanup called") + ZS_PARAM("call ID", UseCall::toDebug(call)))

        AutoRecursiveLock lock(*this);

        if (!mHostThread) {
          ZS_LOG_DEBUG(log("no host thread to clean call from..."))
          return;
        }

        const DialogMap &dialogs = mHostThread->dialogs();
        DialogMap::const_iterator found = dialogs.find(call->getCallID());

        if (found == dialogs.end()) {
          ZS_LOG_WARNING(Detail, log("this call is not present on the host conversation thread to remove"))
          return;
        }

        DialogIDList removals;
        removals.push_back(call->getCallID());

        // publish the changes now...
        mHostThread->updateBegin();
        mHostThread->removeDialogs(removals);
        mHostThread->updateEnd(getPublicationRepostiory());

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::gatherDialogReplies(
                                                       const char *callID,
                                                       LocationDialogMap &outDialogs
                                                       ) const
      {
        AutoRecursiveLock lock(*this);
        for (PeerContactMap::const_iterator iter = mPeerContacts.begin(); iter != mPeerContacts.end(); ++iter)
        {
          const PeerContactPtr &peerContact = (*iter).second;
          peerContact->gatherDialogReplies(callID, outDialogs);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::markAllMessagesRead()
      {
        ZS_LOG_DEBUG(log("marking all messages as read for host"))

        AutoRecursiveLock lock(*this);
        get(mMarkAllRead) = true;
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::setStatusInThread(
                                                     UseContactPtr selfContact,
                                                     const IdentityContactList &selfIdentityContacts,
                                                     const Time &contactStatusTime,
                                                     const String &contactStatusInThreadOfSelfHash,
                                                     ElementPtr contactStatusInThreadOfSelf
                                                     )
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("cannot set status during shutdown"))
          return;
        }

        if (!mHostThread) {
          ZS_LOG_DEBUG(log("cannot set status without slave object"))
          return;
        }

        // get the current list of contacts on the thread
        ThreadContactMap contacts = mHostThread->contacts()->contacts();

        if (contacts.end() == contacts.find(selfContact->getPeerURI())) {
          ZS_LOG_WARNING(Detail, log("unable to find self contact in host thread thus cannot update self status"))
          return;
        }

        // replace selt with updated information
        contacts[selfContact->getPeerURI()] = ThreadContact::create(1, selfContact, selfIdentityContacts, contactStatusTime, contactStatusInThreadOfSelfHash, contactStatusInThreadOfSelf);

        // publish the lastest contact list
        mHostThread->updateBegin();
        mHostThread->setContacts(contacts);
        mHostThread->updateEnd(getPublicationRepostiory());

        // kick the conversation thread step routine asynchronously to ensure
        // the thread has a subscription state to its peer
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost => IConversationThreadHostForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadHost::close()
      {
        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("cannot close as already shutting down"))
          return;
        }

        ZS_THROW_BAD_STATE_IF(!mHostThread)

        mHostThread->updateBegin();
        mHostThread->setState(Details::ConversationThreadState_Closed);
        mHostThread->updateEnd(getPublicationRepostiory());
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost => friend PeerContact
      #pragma mark

      //-----------------------------------------------------------------------
      ThreadPtr ConversationThreadHost::getHostThread() const
      {
        AutoRecursiveLock lock(*this);
        return mHostThread;
      }

      //-----------------------------------------------------------------------
      UseAccountPtr ConversationThreadHost::getAccount() const
      {
        AutoRecursiveLock lock(*this);
        return mAccount.lock();
      }

      //-----------------------------------------------------------------------
      IPublicationRepositoryPtr ConversationThreadHost::getRepository() const
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus cannot get stack repository"))
          return IPublicationRepositoryPtr();
        }

        return account->getRepository();
      }

      //-----------------------------------------------------------------------
      UseConversationThreadPtr ConversationThreadHost::getBaseThread() const
      {
        AutoRecursiveLock lock(*this);
        return mBaseThread.lock();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyMessagesReceived(const MessageList &messages)
      {
        AutoRecursiveLock lock(*this);

        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_DEBUG(log("unable to notify of messages received since conversation thread host object is gone"))
          return;
        }

        if (!mHostThread) {
          ZS_LOG_DEBUG(log("unable to notify of messages received since host thread object is NULL"))
          return;
        }

        // tell the base thread about the received messages (but only if we are in the conversation)...
        if (inConversation(mSelfContact)) {
          for (MessageList::const_iterator iter = messages.begin(); iter != messages.end(); ++iter)
          {
            const MessagePtr &message = (*iter);
            ZS_LOG_TRACE(log("notifying of message received") + message->toDebug())
            baseThread->notifyMessageReceived(message);
          }
        }

        // any received messages have to be republished to the host thread...
        mHostThread->updateBegin();
        mHostThread->addMessages(messages);
        mHostThread->updateEnd(getPublicationRepostiory());
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyMessageDeliveryStateChanged(
                                                                     const String &messageID,
                                                                     IConversationThread::MessageDeliveryStates state
                                                                     )
      {
        AutoRecursiveLock lock(*this);

        MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(messageID);
        if (found != mMessageDeliveryStates.end()) {
          IConversationThread::MessageDeliveryStates &deliveryState = (*found).second;
          if (state <= deliveryState) {
            ZS_LOG_DEBUG(log("no need to change delievery state") + ZS_PARAM("current state", IConversationThread::toString(state)) + ZS_PARAM("reported state", IConversationThread::toString(deliveryState)))
            return;
          }
        }

        mMessageDeliveryStates[messageID] = state;

        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("unable to notify of messages received since conversation thread host object is gone"))
          return;
        }
        baseThread->notifyMessageDeliveryStateChanged(messageID, state);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyContactStatus(
                                                       UseContactPtr contact,
                                                       const Time &statusTime,
                                                       const String &statusHash,
                                                       ElementPtr status
                                                       )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!contact)

        AutoRecursiveLock lock(*this);

        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("unable to notify of contact status as base thread gone"))
          return;
        }

        // copy the status of the peer to the slave
        if (mHostThread) {
          const ThreadContactMap &contacts = mHostThread->contacts()->contacts();
          ThreadContactMap::const_iterator found = contacts.find(contact->getPeerURI());
          if (found != contacts.end()) {

            ThreadContactPtr existingContact = (*found).second;

            bool update = true;

            if (Time() != existingContact->statusTime()) {
              if (existingContact->statusTime() > statusTime) {
                ZS_LOG_WARNING(Detail, log("contact status is older than the current status") + ZS_PARAM("current status", existingContact->statusTime()) + ZS_PARAM("status time", statusTime))
                update = false;
              }
            }

            if (update) {
              ThreadContactMap replacementContacts = contacts;

              ThreadContactMap contacts;
              replacementContacts[contact->getPeerURI()] = ThreadContact::create(1, contact, existingContact->identityContacts(), statusTime, statusHash, status);

              mHostThread->updateBegin();
              mHostThread->setContacts(contacts);
              mHostThread->updateEnd(getPublicationRepostiory());
            }
          }
        }

        baseThread->notifyContactStatus(mThisWeak.lock(), contact, statusTime, statusHash, status);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyMessagePush(
                                                     MessagePtr message,
                                                     UseContactPtr toContact
                                                     )
      {
        AutoRecursiveLock lock(*this);
        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("unable to notify about pushed message as base is gone"))
          return;
        }
        baseThread->notifyMessagePush(message, toContact);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyStateChanged(PeerContactPtr peerContact)
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyContactState(
                                                      UseContactPtr contact,
                                                      ContactConnectionStates state
                                                      )
      {
        AutoRecursiveLock lock(*this);
        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("unable to notify about contact state as base is gone"))
          return;
        }
        baseThread->notifyContactConnectionState(mThisWeak.lock(), contact, state);
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::hasCallPlacedTo(UseContactPtr toContact)
      {
        ZS_LOG_TRACE(log("checking for calls placed to") + UseContact::toDebug(toContact))

        AutoRecursiveLock lock(*this);

        ZS_THROW_INVALID_ASSUMPTION_IF(!mHostThread)

        const DialogMap &dialogs = mHostThread->dialogs();
        for (DialogMap::const_iterator iter = dialogs.begin(); iter != dialogs.end(); ++iter)
        {
          const DialogPtr &dialog = (*iter).second;
          ZS_LOG_TRACE(log("thread has call (thus comparing)") + dialog->toDebug())

          if (dialog->calleePeerURI() == toContact->getPeerURI()) {
            ZS_LOG_TRACE(log("found call placed to") + UseContact::toDebug(toContact))
            return true;
          }
        }

        ZS_LOG_TRACE(log("no calls found to this peer URI"))
        return false;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ConversationThreadHost::log(const char *message) const
      {
        String baseThreadID;
        UseConversationThreadPtr baseThread = mBaseThread.lock();

        if (baseThread) baseThreadID = baseThread->getThreadID();

        ElementPtr objectEl = Element::create("core::ConversationThreadHost");
        IHelper::debugAppend(objectEl, "id", mID);
        IHelper::debugAppend(objectEl, "base thread id", baseThreadID);
        IHelper::debugAppend(objectEl, "thread id", mThreadID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadHost::toDebug() const
      {
        AutoRecursiveLock lock(*this);
        UseConversationThreadPtr base = mBaseThread.lock();

        ElementPtr resultEl = Element::create("core::ConversationThreadHost");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "host thread id", mThreadID);
        IHelper::debugAppend(resultEl, "base thread id", base ? base->getThreadID() : String());
        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));
        IHelper::debugAppend(resultEl, "delivery states", mMessageDeliveryStates.size());
        IHelper::debugAppend(resultEl, "peer contacts", mPeerContacts.size());
        IHelper::debugAppend(resultEl, Thread::toDebug(mHostThread));

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::cancel()
      {
        AutoRecursiveLock lock(*this);
        if (isShutdown()) return;

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(ConversationThreadHostState_ShuttingDown);

        for (PeerContactMap::iterator iter_doNotUse = mPeerContacts.begin(); iter_doNotUse != mPeerContacts.end(); )
        {
          PeerContactMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          PeerContactPtr peerContact = (*current).second;
          peerContact->cancel();
        }

        setState(ConversationThreadHostState_Shutdown);

        mGracefulShutdownReference.reset();

        mPeerContacts.clear();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::step()
      {
        ZS_LOG_TRACE(log("step") + toDebug())

        AutoRecursiveLock lock(*this);
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_DEBUG(log("step forwarding to cancel"))
          cancel();
          return;
        }

        if (!mHostThread) {
          ZS_LOG_WARNING(Detail, log("host thread object is NULL thus shutting down"))
          cancel();
          return;
        }

        setState(ConversationThreadHostState_Ready);

        MessageReceiptMap delivered;
        ThreadContactMap contactsToAdd;
        ContactURIList contactsToRemove;

        for (PeerContactMap::iterator iter = mPeerContacts.begin(); iter != mPeerContacts.end(); ++iter)
        {
          PeerContactPtr &peerContact = (*iter).second;
          peerContact->gatherMessagesDelivered(delivered);
          peerContact->gatherContactsToAdd(contactsToAdd);
          peerContact->gatherContactsToRemove(contactsToRemove);
          peerContact->notifyStep();
        }

        mHostThread->updateBegin();
        mHostThread->setDelivered(delivered);
        if (mMarkAllRead) {
          // to mark the same set of messages that were marked as delivered as
          // received since at this point they are now one and the same
          mHostThread->setRead(delivered);

          // do not mark messages again as read until signalled to do so again
          get(mMarkAllRead) = false;
        }
        mHostThread->updateEnd(getPublicationRepostiory());

        if ((contactsToAdd.size() > 0) ||
            (contactsToRemove.size() > 0)) {
          ContactProfileInfoList contactsAsList;
          for (ThreadContactMap::iterator iter = contactsToAdd.begin(); iter != contactsToAdd.end(); ++iter)
          {
            ThreadContactPtr &threadContact = (*iter).second;
            ContactProfileInfo info;
            info.mContact = Contact::convert(threadContact->contact());
            info.mIdentityContacts = threadContact->identityContacts();
            contactsAsList.push_back(info);
          }

          if (!safeToChangeContacts()) {
            // not safe to add these contacts to the current object...
            UseConversationThreadPtr baseThread = mBaseThread.lock();
            if (!baseThread) {
              ZS_LOG_WARNING(Detail, log("cannot add or remove contacts because base thread object is gone"))
              return;
            }
            baseThread->addContacts(contactsAsList);
            baseThread->removeContacts(contactsToRemove);
          } else {
            removeContacts(contactsToRemove);
            addContacts(contactsAsList);
          }
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::setState(ConversationThreadHostStates state)
      {
        AutoRecursiveLock lock(*this);
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ZS_PARAM("old state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;

        UseAccountPtr account = mAccount.lock();
        if (account) {
          ZS_LOG_DEBUG(log("notifying account of conversation thread state change"))
          account->notifyConversationThreadStateChanged();
        }
      }

      //-----------------------------------------------------------------------
      IPublicationRepositoryPtr ConversationThreadHost::getPublicationRepostiory()
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus get publication repository"))
          return IPublicationRepositoryPtr();
        }

        return account->getRepository();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::removeContacts(const ContactURIList &contacts)
      {
        AutoRecursiveLock lock(*this);
        ZS_THROW_INVALID_ASSUMPTION_IF(!safeToChangeContacts())

        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("cannot remove contacts as no base thread present"))
          return;
        }

        ThreadContactMap contactMap;

        for (ContactURIList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
        {
          const String &contactID = (*iter);
          PeerContactMap::iterator found = mPeerContacts.find(contactID);
          if (found == mPeerContacts.end()) continue;

          // remove this peer contact...
          PeerContactPtr peerContact = (*found).second;
          mPeerContacts.erase(found);

          peerContact->cancel();
        }

        // calculate current contacts map...
        for (PeerContactMap::iterator iter = mPeerContacts.begin(); iter != mPeerContacts.end(); ++iter)
        {
          const String contactID = (*iter).first;
          PeerContactPtr &peerContact = (*iter).second;

          Time statusTime;
          String statusHash;
          ElementPtr status;

          baseThread->getLastContactStatus(peerContact->getContact(), statusTime, statusHash, status);

          ThreadContactPtr threadContact(ThreadContact::create(1, peerContact->getContact(), peerContact->getIdentityContacts(), statusTime, statusHash, status));
          contactMap[contactID] = threadContact;
        }

        // publish changes...
        mHostThread->updateBegin();
        mHostThread->setContacts(contactMap);
        mHostThread->updateEnd(getPublicationRepostiory());
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerContactPtr ConversationThreadHost::findContact(ILocationPtr peerLocation) const
      {
        PeerContactMap::const_iterator found = mPeerContacts.find(peerLocation->getPeerURI());
        if (found == mPeerContacts.end()) {
          ZS_LOG_WARNING(Detail, log("failed to find any peer contact for the location given") + ILocation::toDebug(peerLocation))
          return PeerContactPtr();
        }
        const PeerContactPtr &peerContact = (*found).second;
        return peerContact;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }
  }
}
