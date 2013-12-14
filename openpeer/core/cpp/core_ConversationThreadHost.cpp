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
                                                                                    Details::ConversationThreadStates state
                                                                                    )
      {
        return IConversationThreadHostFactory::singleton().createConversationThreadHost(baseThread, state);
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
                                                     UseAccountPtr account,
                                                     UseConversationThreadPtr baseThread,
                                                     const char *threadID
                                                     ) :
        MessageQueueAssociator(queue),
        mID(zsLib::createPUID()),
        mThreadID(threadID ? String(threadID) : services::IHelper::randomString(32)),
        mBaseThread(baseThread),
        mCurrentState(ConversationThreadHostState_Pending),
        mAccount(account),
        mSelfContact(account->getSelfContact())
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::init(Details::ConversationThreadStates state)
      {
        UseAccountPtr account = mAccount.lock();
        ZS_THROW_INVALID_ASSUMPTION_IF(!account)

        AutoRecursiveLock lock(account->getLock());

        UseConversationThreadPtr baseThread = mBaseThread.lock();

        mHostThread = Thread::create(Account::convert(account), Thread::ThreadType_Host, account->getSelfLocation(), baseThread->getThreadID(), mThreadID, "", "", state);
        ZS_THROW_BAD_STATE_IF(!mHostThread)

        publish(true, true);

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
                                                               Details::ConversationThreadStates state
                                                               )
      {
        UseConversationThreadPtr baseThread = inBaseThread;

        AccountPtr account = baseThread->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, slog("unable to create a new conversation thread object as account object is null"))
          return ConversationThreadHostPtr();
        }

        ConversationThreadHostPtr pThis(new ConversationThreadHost(UseStack::queueCore(), account, baseThread, NULL));
        pThis->mThisWeak = pThis;
        pThis->init(state);
        return pThis;
      }

      //-----------------------------------------------------------------------
      String ConversationThreadHost::getThreadID() const
      {
        AutoRecursiveLock lock(getLock());
        return mThreadID;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::shutdown()
      {
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());

        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("unable to send messages to a thread that is shutdown"))
          return false;
        }

        ZS_THROW_BAD_STATE_IF(!mHostThread)

        mHostThread->updateBegin();
        mHostThread->addMessages(messages);
        publish(mHostThread->updateEnd(), false);

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      Time ConversationThreadHost::getHostCreationTime() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mHostThread) return Time();
        return mHostThread->details()->created();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::safeToChangeContacts() const
      {
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
        if (!mHostThread) {
          ZS_LOG_DEBUG(log("cannot get contacts without a host thread"))
          return;
        }

        outContacts = mHostThread->contacts()->contacts();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::inConversation(UseContactPtr contact) const
      {
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
        ZS_THROW_INVALID_ASSUMPTION_IF(!safeToChangeContacts())

        bool foundSelf = false;

        const ThreadContactMap &oldContacts = mHostThread->contacts()->contacts();

        ThreadContactMap contactMap = oldContacts;

        for (ContactProfileInfoList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
        {
          const ContactProfileInfo &info = (*iter);

          UseContactPtr contact = Contact::convert(info.mContact);
          if (contact->getPeerURI() == mSelfContact->getPeerURI()) {
            ThreadContactPtr threadContact = ThreadContact::create(mSelfContact, info.mProfileBundleEl);

            // do not allow self to become a 'PeerContact'
            foundSelf = true;

            // make sure we are on the final list though...
            contactMap[mSelfContact->getPeerURI()] = threadContact;
            continue;
          }

          ThreadContactPtr threadContact = ThreadContact::create(contact, info.mProfileBundleEl);

          // remember this contact as needing to be added to the final list...
          contactMap[contact->getPeerURI()] = threadContact;

          PeerContactMap::iterator found = mPeerContacts.find(contact->getPeerURI());
          if (found == mPeerContacts.end()) {
            PeerContactPtr peerContact = PeerContact::create(getAssociatedMessageQueue(), mThisWeak.lock(), contact, info.mProfileBundleEl);
            if (peerContact) {
              mPeerContacts[contact->getPeerURI()] = peerContact;
            }
          }
        }

        // publish changes...
        mHostThread->updateBegin();
        mHostThread->setContacts(contactMap);
        publish(mHostThread->updateEnd(), true);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::removeContacts(const ContactList &contacts)
      {
        AutoRecursiveLock lock(getLock());

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
        publish(mHostThread->updateEnd(), true);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      IConversationThread::ContactStates ConversationThreadHost::getContactState(UseContactPtr contact) const
      {
        AutoRecursiveLock lock(getLock());

        PeerContactMap::const_iterator found = mPeerContacts.find(contact->getPeerURI());
        if (found == mPeerContacts.end()) {
          ZS_LOG_WARNING(Detail, log("contact was not found as part of the conversation") + UseContact::toDebug(contact))
          return IConversationThread::ContactState_NotApplicable;
        }

        const PeerContactPtr &peerContact = (*found).second;
        return peerContact->getContactState();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::placeCalls(const PendingCallMap &pendingCalls)
      {
        if (pendingCalls.size() < 1) {
          ZS_LOG_WARNING(Detail, log("requiest to place calls but no calls to place"))
          return false;
        }

        AutoRecursiveLock lock(getLock());
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
        publish(mHostThread->updateEnd(), true);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyCallStateChanged(UseCallPtr call)
      {
        AutoRecursiveLock lock(getLock());

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
        publish(mHostThread->updateEnd(), true);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyCallCleanup(UseCallPtr call)
      {
        ZS_LOG_DEBUG(log("call cleanup called") + ZS_PARAM("call ID", UseCall::toDebug(call)))

        AutoRecursiveLock lock(getLock());

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
        publish(mHostThread->updateEnd(), true);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::gatherDialogReplies(
                                                       const char *callID,
                                                       LocationDialogMap &outDialogs
                                                       ) const
      {
        AutoRecursiveLock lock(getLock());
        for (PeerContactMap::const_iterator iter = mPeerContacts.begin(); iter != mPeerContacts.end(); ++iter)
        {
          const PeerContactPtr &peerContact = (*iter).second;
          peerContact->gatherDialogReplies(callID, outDialogs);
        }
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
        AutoRecursiveLock lock(getLock());
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("cannot close as already shutting down"))
          return;
        }

        ZS_THROW_BAD_STATE_IF(!mHostThread)

        mHostThread->updateBegin();
        mHostThread->setState(Details::ConversationThreadState_Closed);
        publish(mHostThread->updateEnd(), false);
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
        AutoRecursiveLock lock(getLock());
        return mHostThread;
      }

      //-----------------------------------------------------------------------
      UseAccountPtr ConversationThreadHost::getAccount() const
      {
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
        return mBaseThread.lock();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyMessagesReceived(const MessageList &messages)
      {
        AutoRecursiveLock lock(getLock());

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
        publish(mHostThread->updateEnd(), false);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyMessageDeliveryStateChanged(
                                                                     const String &messageID,
                                                                     IConversationThread::MessageDeliveryStates state
                                                                     )
      {
        AutoRecursiveLock lock(getLock());

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
      void ConversationThreadHost::notifyMessagePush(
                                                     MessagePtr message,
                                                     UseContactPtr toContact
                                                     )
      {
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::notifyContactState(
                                                      UseContactPtr contact,
                                                      ContactStates state
                                                      )
      {
        AutoRecursiveLock lock(getLock());
        UseConversationThreadPtr baseThread = mBaseThread.lock();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("unable to notify about contact state as base is gone"))
          return;
        }
        baseThread->notifyContactState(mThisWeak.lock(), contact, state);
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::hasCallPlacedTo(UseContactPtr toContact)
      {
        ZS_LOG_TRACE(log("checking for calls placed to") + UseContact::toDebug(toContact))

        AutoRecursiveLock lock(getLock());

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
        AutoRecursiveLock lock(getLock());
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
      RecursiveLock &ConversationThreadHost::getLock() const
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) return mBogusLock;
        return account->getLock();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::cancel()
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) return;

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(ConversationThreadHostState_ShuttingDown);
        setState(ConversationThreadHostState_Shutdown);

        mGracefulShutdownReference.reset();

        mPeerContacts.clear();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::step()
      {
        ZS_LOG_TRACE(log("step") + toDebug())

        AutoRecursiveLock lock(getLock());
        if ((isShuttingDown()) || (isShutdown())) {cancel();}

        if (!mHostThread) {
          ZS_LOG_WARNING(Detail, log("host thread object is NULL thus shutting down"))
          cancel();
          return;
        }

        setState(ConversationThreadHostState_Ready);

        MessageReceiptMap receipts;
        ThreadContactMap contactsToAdd;
        ContactURIList contactsToRemove;

        for (PeerContactMap::iterator iter = mPeerContacts.begin(); iter != mPeerContacts.end(); ++iter)
        {
          PeerContactPtr &peerContact = (*iter).second;
          peerContact->gatherMessageReceipts(receipts);
          peerContact->gatherContactsToAdd(contactsToAdd);
          peerContact->gatherContactsToRemove(contactsToRemove);
          peerContact->notifyStep();
        }

        mHostThread->updateBegin();
        mHostThread->setReceived(receipts);
        publish(mHostThread->updateEnd(), false);

        if ((contactsToAdd.size() > 0) ||
            (contactsToRemove.size() > 0)) {
          ContactProfileInfoList contactsAsList;
          for (ThreadContactMap::iterator iter = contactsToAdd.begin(); iter != contactsToAdd.end(); ++iter)
          {
            ThreadContactPtr &threadContact = (*iter).second;
            ContactProfileInfo info;
            info.mContact = Contact::convert(threadContact->contact());
            info.mProfileBundleEl = threadContact->profileBundleElement();
            contactsAsList.push_back(info);
          }

          if (!safeToChangeContacts()) {
            // not safe to add these contacts to the current object...
            UseConversationThreadPtr baseThread = mBaseThread.lock();
            if (!baseThread) {
              ZS_LOG_WARNING(Detail, log("cannot add or remove contacts becaues base thread object is gone"))
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
        AutoRecursiveLock lock(getLock());
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ZS_PARAM("old state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::publish(
                                            bool publishHostPublication,
                                            bool publishHostPermissionPublication
                                            ) const
      {
        AutoRecursiveLock lock(getLock());

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus cannot publish any publications"))
          return;
        }

        IPublicationRepositoryPtr repo = account->getRepository();
        if (!repo) {
          ZS_LOG_WARNING(Detail, log("publication repository was NULL thus cannot publish any publications"))
          return;
        }

        if (!mHostThread) {
          ZS_LOG_WARNING(Detail, log("host thread is not available thus cannot publish any publications"))
          return;
        }

        if (publishHostPermissionPublication) {
          ZS_LOG_DEBUG(log("publishing host thread permission document"))
          repo->publish(IPublicationPublisherDelegateProxy::createNoop(getAssociatedMessageQueue()), mHostThread->permissionPublication());
        }

        if (publishHostPublication) {
          ZS_LOG_DEBUG(log("publishing host thread document"))
          repo->publish(IPublicationPublisherDelegateProxy::createNoop(getAssociatedMessageQueue()), mHostThread->publication());
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::removeContacts(const ContactURIList &contacts)
      {
        AutoRecursiveLock lock(getLock());
        ZS_THROW_INVALID_ASSUMPTION_IF(!safeToChangeContacts())

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
          ThreadContactPtr threadContact(ThreadContact::create(peerContact->getContact(), peerContact->getProfileBundle()));
          contactMap[contactID] = threadContact;
        }

        // publish changes...
        mHostThread->updateBegin();
        mHostThread->setContacts(contactMap);
        publish(mHostThread->updateEnd(), true);
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
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact
      #pragma mark

      //-----------------------------------------------------------------------
      const char *ConversationThreadHost::PeerContact::toString(PeerContactStates state)
      {
        switch (state)
        {
          case PeerContactState_Pending:      return "Pending";
          case PeerContactState_Ready:        return "Ready";
          case PeerContactState_ShuttingDown: return "Shutting down";
          case PeerContactState_Shutdown:     return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerContact::PeerContact(
                                                       IMessageQueuePtr queue,
                                                       ConversationThreadHostPtr host,
                                                       UseContactPtr contact,
                                                       ElementPtr profileBundleEl
                                                       ) :
        MessageQueueAssociator(queue),
        mID(zsLib::createPUID()),
        mCurrentState(PeerContactState_Pending),
        mOuter(host),
        mContact(contact),
        mProfileBundleEl(profileBundleEl)
      {
        ZS_LOG_DEBUG(log("created") + UseContact::toDebug(contact))
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::init()
      {
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerContact::~PeerContact()
      {
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadHost::PeerContact::toDebug(PeerContactPtr contact)
      {
        if (!contact) return ElementPtr();
        return contact->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => friend ConversationThreadHost
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerContactPtr ConversationThreadHost::PeerContact::create(
                                                                                         IMessageQueuePtr queue,
                                                                                         ConversationThreadHostPtr host,
                                                                                         UseContactPtr contact,
                                                                                         ElementPtr profileBundleEl
                                                                                         )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!host)
        ZS_THROW_INVALID_ARGUMENT_IF(!contact)
        PeerContactPtr pThis(new PeerContact(queue, host, contact, profileBundleEl));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyPublicationUpdated(
                                                                         ILocationPtr peerLocation,
                                                                         IPublicationMetaDataPtr metaData,
                                                                         const SplitMap &split
                                                                         )
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("received publication update notification after shutdown"))
          return;
        }

        PeerLocationPtr location = findPeerLocation(peerLocation);
        if (!location) {
          location = PeerLocation::create(getAssociatedMessageQueue(), mThisWeak.lock(), peerLocation);
          if (!location) {
            ZS_LOG_WARNING(Detail, log("failed to create peer location") + ILocation::toDebug(peerLocation))
            return;
          }
          ZS_LOG_DEBUG(log("created new oeer location") + UseContact::toDebug(mContact))
          mPeerLocations[location->getLocationID()] = location;
        }

        location->notifyPublicationUpdated(peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyPublicationGone(
                                                                      ILocationPtr peerLocation,
                                                                      IPublicationMetaDataPtr metaData,
                                                                      const SplitMap &split
                                                                      )
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("received publication gone notification after shutdown"))
          return;
        }
        PeerLocationPtr location = findPeerLocation(peerLocation);
        if (!location) {
          ZS_LOG_WARNING(Detail, log("location was not found to pass on publication gone notification") + ILocation::toDebug(peerLocation))
          return;
        }
        location->notifyPublicationGone(peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyPeerDisconnected(ILocationPtr peerLocation)
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("received notification of peer disconnection after shutdown"))
          return;
        }

        PeerLocationPtr location = findPeerLocation(peerLocation);
        if (!location) {
          ZS_LOG_WARNING(Detail, log("unable to pass on peer disconnection notification as location is not available") + ILocation::toDebug(peerLocation))
          return;
        }
        location->notifyPeerDisconnected(peerLocation);
      }

      //-----------------------------------------------------------------------
      UseContactPtr ConversationThreadHost::PeerContact::getContact() const
      {
        AutoRecursiveLock lock(getLock());
        return mContact;
      }

      //-----------------------------------------------------------------------
      const ElementPtr &ConversationThreadHost::PeerContact::getProfileBundle() const
      {
        AutoRecursiveLock lock(getLock());
        return mProfileBundleEl;
      }

      //-----------------------------------------------------------------------
      IConversationThread::ContactStates ConversationThreadHost::PeerContact::getContactState() const
      {
        AutoRecursiveLock lock(getLock());

        // first check to see if any locations are connected...
        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          if (peerLocation->isConnected()) {
            return IConversationThread::ContactState_Connected;
          }
        }

        if (mContact) {
          switch (mContact->getPeer()->getFindState()) {
            case IPeer::PeerFindState_Idle:       return (mPeerLocations.size() > 0 ? IConversationThread::ContactState_Disconnected : IConversationThread::ContactState_NotApplicable);
            case IPeer::PeerFindState_Finding:    return IConversationThread::ContactState_Finding;
            case IPeer::PeerFindState_Completed:  return (mPeerLocations.size() > 0 ? IConversationThread::ContactState_Disconnected : IConversationThread::ContactState_NotApplicable);
          }
        }

        return IConversationThread::ContactState_NotApplicable;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::gatherMessageReceipts(MessageReceiptMap &receipts) const
      {
        AutoRecursiveLock lock(getLock());
        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          peerLocation->gatherMessageReceipts(receipts);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::gatherContactsToAdd(ThreadContactMap &contacts) const
      {
        AutoRecursiveLock lock(getLock());
        if (!isStillPartOfCurrentConversation(mContact)) {
          ZS_LOG_WARNING(Debug, log("ignoring request to add contacts as peer contact is not part of current conversation"))
          return;
        }

        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          peerLocation->gatherContactsToAdd(contacts);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::gatherContactsToRemove(ContactURIList &contacts) const
      {
        AutoRecursiveLock lock(getLock());
        if (!isStillPartOfCurrentConversation(mContact)) {
          ZS_LOG_WARNING(Debug, log("ignoring request to remove contacts as peer contact is not part of current conversation"))
          return;
        }

        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          peerLocation->gatherContactsToRemove(contacts);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::gatherDialogReplies(
                                                                    const char *callID,
                                                                    LocationDialogMap &outDialogs
                                                                    ) const
      {
        AutoRecursiveLock lock(getLock());
        for (PeerLocationMap::const_iterator iter = mPeerLocations.begin(); iter != mPeerLocations.end(); ++iter)
        {
          const PeerLocationPtr &peerLocation = (*iter).second;
          peerLocation->gatherDialogReplies(callID, outDialogs);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyStep(bool performStepAsync)
      {
        if (performStepAsync) {
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return;
        }

        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => IPeerSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(getLock());
        if (subscription != mSlaveSubscription) {
          ZS_LOG_WARNING(Detail, log("ignoring shutdown notification on obsolete slave peer subscription (probably okay)"))
          return;
        }

        mSlaveSubscription.reset();
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onPeerSubscriptionFindStateChanged(
                                                                                   IPeerSubscriptionPtr subscription,
                                                                                   IPeerPtr peer,
                                                                                   PeerFindStates state
                                                                                   )
      {
        AutoRecursiveLock lock(getLock());
        if (subscription != mSlaveSubscription) {
          ZS_LOG_WARNING(Detail, log("notified of subscription state from obsolete subscription (probably okay)"))
          return;
        }
        ZS_LOG_DEBUG(log("notified peer subscription state changed") + ZS_PARAM("state", IPeer::toString(state)))
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onPeerSubscriptionLocationConnectionStateChanged(
                                                                                                 IPeerSubscriptionPtr subscription,
                                                                                                 ILocationPtr location,
                                                                                                 LocationConnectionStates state
                                                                                                 )
      {
        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onPeerSubscriptionMessageIncoming(
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
      #pragma mark ConversationThreadHost::PeerContact => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::onTimer(TimerPtr timer)
      {
        ZS_LOG_DEBUG(log("timer"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => friend ConversationThreadHost::PeerLocation
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadHostPtr ConversationThreadHost::PeerContact::getOuter() const
      {
        AutoRecursiveLock lock(getLock());
        return mOuter.lock();
      }

      //-----------------------------------------------------------------------
      UseConversationThreadPtr ConversationThreadHost::PeerContact::getBaseThread() const
      {
        AutoRecursiveLock lock(getLock());
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("failed to obtain conversation thread because conversation thread host object is gone"))
          return ConversationThreadPtr();
        }
        return outer->getBaseThread();
      }

      //-----------------------------------------------------------------------
      ThreadPtr ConversationThreadHost::PeerContact::getHostThread() const
      {
        AutoRecursiveLock lock(getLock());
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("failed to obtain repository because conversation thread host object is gone"))
          return ThreadPtr();
        }
        return outer->getHostThread();
      }

      //-----------------------------------------------------------------------
      UseAccountPtr ConversationThreadHost::PeerContact::getAccount() const
      {
        AutoRecursiveLock lock(getLock());
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) return AccountPtr();
        return outer->getAccount();
      }

      //-----------------------------------------------------------------------
      IPublicationRepositoryPtr ConversationThreadHost::PeerContact::getRepository() const
      {
        AutoRecursiveLock lock(getLock());
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("failed to obtain repository because conversation thread host object is gone"))
          return IPublicationRepositoryPtr();
        }
        return outer->getRepository();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyMessagesReceived(const MessageList &messages)
      {
        AutoRecursiveLock lock(getLock());
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_DEBUG(log("unable to notify of messages received since conversation thread host object is gone"))
          return;
        }
        return outer->notifyMessagesReceived(messages);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyMessageDeliveryStateChanged(
                                                                                  const String &messageID,
                                                                                  IConversationThread::MessageDeliveryStates state
                                                                                  )
      {
        AutoRecursiveLock lock(getLock());

        MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(messageID);
        if (found != mMessageDeliveryStates.end()) {
          DeliveryStatePair &deliveryStatePair = (*found).second;
          IConversationThread::MessageDeliveryStates &deliveryState = deliveryStatePair.second;
          if (state <= deliveryState) {
            ZS_LOG_DEBUG(log("no need to change delievery state") + ZS_PARAM("current state", IConversationThread::toString(state)) + ZS_PARAM("reported state", IConversationThread::toString(deliveryState)))
            return;
          }
        }

        mMessageDeliveryStates[messageID] = DeliveryStatePair(zsLib::now(), state);

        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_DEBUG(log("unable to notify of messages received since conversation thread host object is gone"))
          return;
        }
        return outer->notifyMessageDeliveryStateChanged(messageID, state);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyStateChanged(PeerLocationPtr peerLocation)
      {
        AutoRecursiveLock lock(getLock());
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_DEBUG(log("unable to notify conversation thread host of state change"))
          return;
        }
        return outer->notifyStateChanged(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::notifyPeerLocationShutdown(PeerLocationPtr location)
      {
        AutoRecursiveLock lock(getLock());

        ZS_LOG_DEBUG(log("notified peer location shutdown") + UseContact::toDebug(mContact) + PeerLocation::toDebug(location))

        PeerLocationMap::iterator found = mPeerLocations.find(location->getLocationID());
        if (found != mPeerLocations.end()) {
          ZS_LOG_TRACE(log("peer location removed from map") + UseContact::toDebug(mContact) + PeerLocation::toDebug(location))
          mPeerLocations.erase(found);
        }

        PeerContactPtr pThis = mThisWeak.lock();
        if (pThis) {
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerContact => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &ConversationThreadHost::PeerContact::getLock() const
      {
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) return mBogusLock;
        return outer->getLock();
      }

      //-----------------------------------------------------------------------
      Log::Params ConversationThreadHost::PeerContact::log(const char *message) const
      {
        String peerURI;
        if (mContact) {
          peerURI = mContact->getPeerURI();
        }

        ElementPtr objectEl = Element::create("core::ConversationThreadHost::PeerContact");
        IHelper::debugAppend(objectEl, "id", mID);
        IHelper::debugAppend(objectEl, "peer uri", peerURI);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadHost::PeerContact::toDebug() const
      {
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("core::ConversationThreadHost::PeerContact");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));
        IHelper::debugAppend(resultEl, UseContact::toDebug(mContact));
        IHelper::debugAppend(resultEl, "profile bundle", (bool)mProfileBundleEl);
        IHelper::debugAppend(resultEl, IPeerSubscription::toDebug(mSlaveSubscription));
        IHelper::debugAppend(resultEl, "slave delivery timer", (bool)mSlaveMessageDeliveryTimer);
        IHelper::debugAppend(resultEl, "locations", mPeerLocations.size());
        IHelper::debugAppend(resultEl, "delivery states", mMessageDeliveryStates.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::cancel()
      {
        AutoRecursiveLock lock(getLock());

        if (isShutdown()) return;

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(PeerContactState_ShuttingDown);

        if (mSlaveSubscription) {
          mSlaveSubscription->cancel();
          mSlaveSubscription.reset();
        }

        if (mSlaveMessageDeliveryTimer) {
          mSlaveMessageDeliveryTimer->cancel();
          mSlaveMessageDeliveryTimer.reset();
        }

        // cancel all the locations
        for (PeerLocationMap::iterator peerIter = mPeerLocations.begin(); peerIter != mPeerLocations.end(); )
        {
          PeerLocationMap::iterator current = peerIter;
          ++peerIter;

          PeerLocationPtr &location = (*current).second;
          location->cancel();
        }

        setState(PeerContactState_Shutdown);

        mGracefulShutdownReference.reset();

        // mContact.reset();  // DO NOT RESET -- LEAVE THIS TO THE DESTRUCTOR

        mPeerLocations.clear();
        mMessageDeliveryStates.clear();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::step()
      {
        ZS_LOG_TRACE(log("step"))

        AutoRecursiveLock lock(getLock());
        if ((isShuttingDown()) ||
            (isShutdown())) {cancel();}

        // check to see if there are any undelivered messages
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("shutting down peer contact since conversation thread host is shutdown"))
          cancel();
          return;
        }

        ThreadPtr hostThread = outer->getHostThread();
        if (!hostThread) {
          ZS_LOG_WARNING(Detail, log("host thread not available"))
          return;
        }

        UseAccountPtr account = getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is not available thus shutting down"))
          cancel();
          return;
        }

        stack::IAccountPtr stackAccount = account->getStackAccount();
        if (!stackAccount) {
          ZS_LOG_WARNING(Detail, log("stack account is not available thus shutting down"))
          cancel();
          return;
        }

        setState(PeerContactState_Ready);

        for (PeerLocationMap::iterator peerIter = mPeerLocations.begin(); peerIter != mPeerLocations.end(); )
        {
          PeerLocationMap::iterator current = peerIter;
          ++peerIter;

          PeerLocationPtr &location = (*current).second;
          location->step();
        }

        // check to see if there are outstanding messages needing to be delivered to this contact
        bool requiresSubscription = false;
        bool requiresTimer = false;

        // check to see if there are undelivered messages, if so we will need a subscription...
        const MessageList &messages = hostThread->messages();
        MessageList::const_reverse_iterator last = messages.rbegin();
        if (last != messages.rend()) {
          const MessagePtr &message = (*last);

          // check to see if this message has been acknowledged before...
          MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(message->messageID());
          if (found != mMessageDeliveryStates.end()) {
            DeliveryStatePair &deliveryStatePair = (*found).second;
            IConversationThread::MessageDeliveryStates &deliveryState = deliveryStatePair.second;
            if (IConversationThread::MessageDeliveryState_Delivered != deliveryState) {
              ZS_LOG_DEBUG(log("still requires subscription because of undelivered message") + message->toDebug() + ZS_PARAM("was in delivery state", IConversationThread::toString(deliveryState)))
              requiresTimer = (IConversationThread::MessageDeliveryState_UserNotAvailable != deliveryState);
              requiresSubscription = true;
            }
          } else {
            ZS_LOG_DEBUG(log("still requires subscription because of undelivered message") + message->toDebug())
            requiresTimer = requiresSubscription = true;
          }
        }

        if (outer->hasCallPlacedTo(mContact)) {
          ZS_LOG_DEBUG(log("calls are being placed to this slave contact thus subscription is required"))
          // as there is a call being placed to this contact thus a subscription is required
          requiresSubscription = true;
        }

        if (requiresSubscription) {
          ZS_LOG_DEBUG(log("subscription to slave is required") + toDebug())
          if (!mSlaveSubscription) {
            mSlaveSubscription = IPeerSubscription::subscribe(mContact->getPeer(), mThisWeak.lock());
          }
        } else {
          ZS_LOG_DEBUG(log("subscription to slave is NOT required") + toDebug())
          if (mSlaveSubscription) {
            mSlaveSubscription->cancel();
            mSlaveSubscription.reset();
          }
        }

        if (requiresTimer) {
          ZS_LOG_DEBUG(log("timer for peer slave is required") + toDebug())
          if (!mSlaveMessageDeliveryTimer) {
            mSlaveMessageDeliveryTimer = Timer::create(mThisWeak.lock(), Seconds(1));
          }
        } else {
          ZS_LOG_DEBUG(log("timer for peer slave is NOT required") + toDebug())
          if (mSlaveMessageDeliveryTimer) {
            mSlaveMessageDeliveryTimer->cancel();
            mSlaveMessageDeliveryTimer.reset();
          }
        }

        Time tick = zsLib::now();

        // scope: fix the state of pending messages...
        if (mSlaveSubscription) {
          IPeerPtr peer = mContact->getPeer();
          IPeer::PeerFindStates state = peer->getFindState();
          ZS_LOG_DEBUG(log("slave subscription state") + ZS_PARAM("state", IPeer::toString(state)))

          LocationListPtr peerLocations = peer->getLocationsForPeer(false);

          const MessageList &messages = hostThread->messages();

          // Search from the back of the list to the front for messages that
          // aren't delivered as they need to be marked as undeliverable
          // since there are no peer locations available for this user...
          for (MessageList::const_reverse_iterator iter = messages.rbegin(); iter != messages.rend(); ++iter) {
            const MessagePtr &message = (*iter);
            MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(message->messageID());

            Time lastStateChangeTime = tick;

            if (found != mMessageDeliveryStates.end()) {
              bool stopProcessing = false;
              DeliveryStatePair &deliveryStatePair = (*found).second;
              IConversationThread::MessageDeliveryStates &deliveryState = deliveryStatePair.second;
              lastStateChangeTime = deliveryStatePair.first;

              switch (deliveryState) {
                case IConversationThread::MessageDeliveryState_Discovering:   {
                  break;
                }
                case IConversationThread::MessageDeliveryState_Delivered:
                case IConversationThread::MessageDeliveryState_UserNotAvailable: {
                  stopProcessing = true;
                  break;
                }
              }

              if (stopProcessing) {
                ZS_LOG_DEBUG(log("processing undeliverable messages stopped since message already has a delivery state") + message->toDebug())
                break;
              }
            } else {
              mMessageDeliveryStates[message->messageID()] = DeliveryStatePair(tick, IConversationThread::MessageDeliveryState_Discovering);
            }

            if (((IPeer::PeerFindState_Finding != state) &&
                 (peerLocations->size() < 1)) ||
                (lastStateChangeTime + Seconds(OPENPEER_CONVERSATION_THREAD_MAX_WAIT_DELIVERY_TIME_BEFORE_PUSH_IN_SECONDS) < tick)) {
              ZS_LOG_DEBUG(log("state must now be set to undeliverable") + ZS_PARAM("message", message->toDebug()) + ZS_PARAM("peer find state", IPeer::toString(state)) + ZS_PARAM("last state changed time", lastStateChangeTime) + ZS_PARAM("current time", tick))
              mMessageDeliveryStates[message->messageID()] = DeliveryStatePair(zsLib::now(), IConversationThread::MessageDeliveryState_UserNotAvailable);
              outer->notifyMessageDeliveryStateChanged(message->messageID(), IConversationThread::MessageDeliveryState_UserNotAvailable);

              // tell the application to push this message out as a push notification
              outer->notifyMessagePush(message, mContact);
            }
          }
        }

        outer->notifyContactState(mContact, getContactState());
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerContact::setState(PeerContactStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ZS_PARAM("previous state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerLocationPtr ConversationThreadHost::PeerContact::findPeerLocation(ILocationPtr peerLocation) const
      {
        AutoRecursiveLock lock(getLock());
        PeerLocationMap::const_iterator found = mPeerLocations.find(peerLocation->getLocationID());
        if (found == mPeerLocations.end()) return PeerLocationPtr();
        const PeerLocationPtr &location = (*found).second;
        return location;
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::PeerContact::isStillPartOfCurrentConversation(UseContactPtr contact) const
      {
        ConversationThreadHostPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("cannot tell if still part of current conversation because conversation thread host is gone"))
          return false;
        }

        // only respond to contacts to add if this contact is part of the current "live" thread...
        UseConversationThreadPtr baseThread = outer->getBaseThread();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("cannot tell if still part of current conversation because conversation thread is gone"))
          return false;
        }

        // check the base thread to see if this contact is still part of the conversation
        return baseThread->inConversation(contact);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerLocation
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerLocation::PeerLocation(
                                                         IMessageQueuePtr queue,
                                                         PeerContactPtr peerContact,
                                                         ILocationPtr peerLocation
                                                         ) :
        MessageQueueAssociator(queue),
        mID(zsLib::createPUID()),
        mOuter(peerContact),
        mShutdown(false),
        mPeerLocation(peerLocation)
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::init()
      {
        PeerContactPtr outer = mOuter.lock();
        ZS_THROW_INVALID_ASSUMPTION_IF(!outer)

        IPublicationRepositoryPtr repo = outer->getRepository();
        if (repo) {
          mFetcher = IConversationThreadDocumentFetcher::create(mThisWeak.lock(), repo);
        }
      }

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerLocation::~PeerLocation()
      {
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadHost::PeerLocation::toDebug(PeerLocationPtr peerLocation)
      {
        if (!peerLocation) return ElementPtr();
        return peerLocation->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerLocation => friend ConversationThreadHost::PeerContact
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadHost::PeerLocationPtr ConversationThreadHost::PeerLocation::create(
                                                                                           IMessageQueuePtr queue,
                                                                                           PeerContactPtr peerContact,
                                                                                           ILocationPtr peerLocation
                                                                                           )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!peerContact)
        ZS_THROW_INVALID_ARGUMENT_IF(!peerLocation)

        PeerLocationPtr pThis(new PeerLocation(queue, peerContact, peerLocation));
        pThis->mThisWeak = pThis;
        pThis->init();
        if (!pThis->mFetcher) return PeerLocationPtr();
        return pThis;
      }

      //-----------------------------------------------------------------------
      String ConversationThreadHost::PeerLocation::getLocationID() const
      {
        AutoRecursiveLock lock(getLock());
        return mPeerLocation->getLocationID();
      }

      //-----------------------------------------------------------------------
      bool ConversationThreadHost::PeerLocation::isConnected() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mPeerLocation) return false;

        return mPeerLocation->isConnected();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::notifyPublicationUpdated(
                                                                          ILocationPtr peerLocation,
                                                                          IPublicationMetaDataPtr metaData,
                                                                          const SplitMap &split
                                                                          )
      {
        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("notification of an updated document after shutdown") + IPublicationMetaData::toDebug(metaData))
          return;
        }
        mFetcher->notifyPublicationUpdated(peerLocation, metaData);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::notifyPublicationGone(
                                                                       ILocationPtr peerLocation,
                                                                       IPublicationMetaDataPtr metaData,
                                                                       const SplitMap &split
                                                                       )
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("notification that publication is gone after shutdown") + IPublicationMetaData::toDebug(metaData))
          return;
        }
        mFetcher->notifyPublicationGone(peerLocation, metaData);
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::notifyPeerDisconnected(ILocationPtr peerLocation)
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("notification of a peer shutdown") + ILocation::toDebug(peerLocation))
          return;
        }
        mFetcher->notifyPeerDisconnected(peerLocation);

        if (mPeerLocation->getLocationID() == peerLocation->getLocationID()) {
          ZS_LOG_DEBUG(log("this peer location is gone thus this location must shutdown"))
          cancel();
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::gatherMessageReceipts(MessageReceiptMap &receipts) const
      {
        typedef zsLib::Time Time;

        AutoRecursiveLock lock(getLock());
        if (!mSlaveThread) {
          ZS_LOG_WARNING(Detail, log("unable to gather message receipts as there is no slave thread object"))
          return;
        }

        const MessageList &messages = mSlaveThread->messages();

        if (messages.size() < 1) {
          ZS_LOG_DEBUG(log("no messages receipts to acknowledge from this slave"))
          return;
        }

        const MessagePtr &lastMessage = messages.back();
        Time when = mSlaveThread->messagedChangedTime();

        // remember out when this message was received
        receipts[lastMessage->messageID()] = when;
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::gatherContactsToAdd(ThreadContactMap &contacts) const
      {
        AutoRecursiveLock lock(getLock());
        if (!mSlaveThread) {
          ZS_LOG_WARNING(Detail, log("unable to gather contacts to add as there is no slave thread object"))
          return;
        }

        const ThreadContactMap &contactsToAdd = mSlaveThread->contacts()->addContacts();
        for (ThreadContactMap::const_iterator iter = contactsToAdd.begin(); iter != contactsToAdd.end(); ++iter)
        {
          const String &contactID = (*iter).first;
          const ThreadContactPtr &contact = (*iter).second;
          contacts[contactID] = contact;
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::gatherContactsToRemove(ContactURIList &contacts) const
      {
        AutoRecursiveLock lock(getLock());
        if (!mSlaveThread) {
          ZS_LOG_WARNING(Detail, log("unable to gather contacts to remove as there is no slave thread object"))
          return;
        }

        const ContactURIList &contactsToRemove = mSlaveThread->contacts()->removeContacts();
        for (ContactURIList::const_iterator iter = contactsToRemove.begin(); iter != contactsToRemove.end(); ++iter)
        {
          const String &contactID = (*iter);
          contacts.push_back(contactID);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::gatherDialogReplies(
                                                                     const char *callID,
                                                                     LocationDialogMap &outDialogs
                                                                     ) const
      {
        AutoRecursiveLock lock(getLock());
        ZS_THROW_INVALID_ASSUMPTION_IF(!mPeerLocation)

        if (!mSlaveThread) {
          ZS_LOG_WARNING(Detail, log("unable to gather dialog replies as slave thread object is invalid"))
          return;
        }

        const DialogMap &dialogs = mSlaveThread->dialogs();
        DialogMap::const_iterator found = dialogs.find(callID);
        if (found == dialogs.end()) return;

        const DialogPtr dialog = (*found).second;
        outDialogs[mPeerLocation->getLocationID()] = dialog;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerLocation => IConversationThreadDocumentFetcherDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::onConversationThreadDocumentFetcherPublicationUpdated(
                                                                                                       IConversationThreadDocumentFetcherPtr fetcher,
                                                                                                       ILocationPtr peerLocation,
                                                                                                       IPublicationPtr publication
                                                                                                       )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!publication)

        AutoRecursiveLock lock(getLock());

        ZS_LOG_DEBUG(log("publication was updated notification received") + ZS_PARAM("name", publication->getName()))

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("received notification of updated slave document when shutdown") + IPublication::toDebug(publication))
          return;
        }

        PeerContactPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("unable to update publication as peer contact object is gone") + IPublication::toDebug(publication))
          return;
        }

        ConversationThreadHostPtr conversationThreadHost = outer->getOuter();
        if (!conversationThreadHost) {
          ZS_LOG_WARNING(Detail, log("unable to update publication as conversation thread host object is gone"))
          return;
        }

        UseAccountPtr account = outer->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("unable to update publication as account object is gone") + IPublication::toDebug(publication))
          return;
        }

        UseConversationThreadPtr baseThread = outer->getBaseThread();
        if (!baseThread) {
          ZS_LOG_WARNING(Detail, log("unable to update publication as base conversation thread is gone"))
          return;
        }

        if (!mSlaveThread) {
          mSlaveThread = Thread::create(Account::convert(account), publication);
          if (!mSlaveThread) {
            ZS_LOG_WARNING(Detail, log("unable to create slave thread for publication") + IPublication::toDebug(publication))
            return;
          }
        } else {
          mSlaveThread->updateFrom(Account::convert(account), publication);
        }

        //.......................................................................
        // NOTE: We don't need to check contact changes because the outer will
        //       automatically gather up all the contacts to add/remove.

        //.......................................................................
        // check for incoming calls that are gone...

        const DialogIDList &removedDialogs = mSlaveThread->dialogsRemoved();
        for (DialogIDList::const_iterator iter = removedDialogs.begin(); iter != removedDialogs.end(); ++iter) {
          const String &dialogID = (*iter);
          CallHandlers::iterator found = mIncomingCallHandlers.find(dialogID);
          if (found == mIncomingCallHandlers.end()) {
            baseThread->notifyPossibleCallReplyStateChange(dialogID);
            continue;
          }

          // this incoming call is now gone, clean it out...
          UseCallPtr &call = (*found).second;

          ZS_LOG_DEBUG(log("call object is now gone") + UseCall::toDebug(call))

          call->notifyConversationThreadUpdated();

          mIncomingCallHandlers.erase(found);
          ZS_LOG_DEBUG(log("removed incoming call handler") + toDebug())
        }

        //.......................................................................
        // check for dialogs that are now incoming/updated

        String selfPeerURI = UseContactPtr(account->getSelfContact())->getPeerURI();

        const DialogMap &changedDialogs = mSlaveThread->dialogsChanged();
        for (DialogMap::const_iterator iter = changedDialogs.begin(); iter != changedDialogs.end(); ++iter)
        {
          const String &dialogID = (*iter).first;
          const DialogPtr &dialog = (*iter).second;

          ZS_LOG_TRACE(log("call detected") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI))

          CallHandlers::iterator found = mIncomingCallHandlers.find(dialogID);
          if (found != mIncomingCallHandlers.end()) {
            UseCallPtr &call = (*found).second;
            ZS_LOG_DEBUG(log("call object is updated") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI))
            call->notifyConversationThreadUpdated();
            continue;
          }

          if (dialog->calleePeerURI() != selfPeerURI) {
            if (dialog->callerPeerURI() == selfPeerURI) {
              ZS_LOG_DEBUG(log("call detected this must be a reply to a call previously placed") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI))
              baseThread->notifyPossibleCallReplyStateChange(dialogID);
            } else {
              ZS_LOG_WARNING(Detail, log("call detected but the call is not going to this contact") + dialog->toDebug() + ZS_PARAM("our peer URI", selfPeerURI))
            }
            continue;
          }

          const String callerContactID = dialog->callerPeerURI();
          ContactPtr contact = account->findContact(callerContactID);
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
          baseThread->requestAddIncomingCallHandler(dialogID, conversationThreadHost, call);
        }

        //.......................................................................
        // examine all the newly received messages

        ThreadPtr hostThread = outer->getHostThread();
        if (hostThread) {
          const MessageList &messagesChanged = mSlaveThread->messagedChanged();
          outer->notifyMessagesReceived(messagesChanged);

          // examine all the acknowledged messages
          const MessageList &messages = hostThread->messages();
          const MessageMap &messagesMap = hostThread->messagesAsMap();

          // can only examine message receipts that are part of the slave thread...
          const MessageReceiptMap &messageReceiptsChanged = mSlaveThread->messageReceiptsChanged();
          for (MessageReceiptMap::const_iterator iter = messageReceiptsChanged.begin(); iter != messageReceiptsChanged.end(); ++iter)
          {
            const MessageID &id = (*iter).first;

            ZS_LOG_TRACE(log("examining message receipt") + ZS_PARAM("receipt ID", id))

            // check to see if this receipt has already been marked as delivered...
            MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(id);
            if (found != mMessageDeliveryStates.end()) {
              IConversationThread::MessageDeliveryStates &deliveryState = (*found).second;
              if (IConversationThread::MessageDeliveryState_Delivered == deliveryState) {
                ZS_LOG_DEBUG(log("message receipt was already notified as delivered thus no need to notify any further") + ZS_PARAM("message ID", id))
                continue;
              }
            }

            MessageMap::const_iterator foundInMap = messagesMap.find(id);
            if (foundInMap == messagesMap.end()) {
              ZS_LOG_WARNING(Detail, log("host never send this message to the slave (what is slave acking?)") + ZS_PARAM("receipt ID", id))
              continue;
            }

            bool foundMessageID = false;

            // Find this receipt on the host's message list... (might not exist if
            // receipt is for a message from a different contact)
            MessageList::const_reverse_iterator messageIter = messages.rbegin();
            for (; messageIter != messages.rend(); ++messageIter)
            {
              const MessagePtr &message = (*messageIter);
              if (message->messageID() == id) {
                ZS_LOG_TRACE(log("found message matching receipt") + ZS_PARAM("receipt ID", id))
                foundMessageID = true;
              }

              ZS_LOG_TRACE(log("processing host message") + ZS_PARAM("found", foundMessageID) + message->toDebug())

              if (foundMessageID) {
                // first check if this delivery was already sent...
                found = mMessageDeliveryStates.find(message->messageID());
                if (found != mMessageDeliveryStates.end()) {
                  // check to see if this message was already marked as delivered
                  IConversationThread::MessageDeliveryStates &deliveryState = (*found).second;
                  if (IConversationThread::MessageDeliveryState_Delivered == deliveryState) {
                    // stop notifying of delivered since it's alerady been marked as delivered
                    ZS_LOG_DEBUG(log("message was already notified as delivered thus no need to notify any further") + message->toDebug())
                    break;
                  }

                  ZS_LOG_DEBUG(log("message is now notified as delivered") + message->toDebug() + ZS_PARAM("was in state", IConversationThread::toString(deliveryState)))

                  // change the state to delivered since it wasn't delivered
                  deliveryState = IConversationThread::MessageDeliveryState_Delivered;
                } else {
                  ZS_LOG_DEBUG(log("message is now delivered") + message->toDebug())
                  mMessageDeliveryStates[message->messageID()] = IConversationThread::MessageDeliveryState_Delivered;
                }

                // this message is now considered acknowledged so tell the master thread of the new state...
                outer->notifyMessageDeliveryStateChanged(id, IConversationThread::MessageDeliveryState_Delivered);
              }
            }
          }
        }

        outer->notifyStateChanged(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::onConversationThreadDocumentFetcherPublicationGone(
                                                                                                    IConversationThreadDocumentFetcherPtr fetcher,
                                                                                                    ILocationPtr peerLocation,
                                                                                                    IPublicationMetaDataPtr metaData
                                                                                                    )
      {
        AutoRecursiveLock lock(getLock());
        //*********************************************************************
        //*********************************************************************
        //*********************************************************************
        //*********************************************************************
        // HERE - self destruct?
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThreadHost::PeerLocation => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ConversationThreadHost::PeerLocation::log(const char *message) const
      {
        ZS_THROW_INVALID_ASSUMPTION_IF(!mPeerLocation)

        String peerURI = mPeerLocation->getPeerURI();
        String locationID = mPeerLocation->getLocationID();

        ElementPtr objectEl = Element::create("core::ConversationThreadHost::PeerLocation");
        IHelper::debugAppend(objectEl, "id", mID);
        IHelper::debugAppend(objectEl, "peer uri", peerURI);
        IHelper::debugAppend(objectEl, "peer location id", locationID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThreadHost::PeerLocation::toDebug() const
      {
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("core::ConversationThreadHost::PeerLocation");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, ILocation::toDebug(mPeerLocation));
        IHelper::debugAppend(resultEl, Thread::toDebug(mSlaveThread));
        IHelper::debugAppend(resultEl, IConversationThreadDocumentFetcher::toDebug(mFetcher));
        IHelper::debugAppend(resultEl, "message delivery states", mMessageDeliveryStates.size());
        IHelper::debugAppend(resultEl, "incoming call handlers", mIncomingCallHandlers.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      RecursiveLock &ConversationThreadHost::PeerLocation::getLock() const
      {
        PeerContactPtr outer = mOuter.lock();
        if (!outer) return mBogusLock;
        return outer->getLock();
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        mShutdown = true;

        PeerContactPtr outer = mOuter.lock();
        if (outer) {
          UseConversationThreadPtr baseThread = outer->getBaseThread();
          if (!baseThread) {
            for (CallHandlers::iterator iter = mIncomingCallHandlers.begin(); iter != mIncomingCallHandlers.end(); ++iter)
            {
              const CallID &callID = (*iter).first;
              baseThread->requestRemoveIncomingCallHandler(callID);
            }
          }

          PeerLocationPtr pThis = mThisWeak.lock();
          if (pThis) {
            outer->notifyPeerLocationShutdown(pThis);
          }
        }

        if (mFetcher) {
          mFetcher->cancel();
          mFetcher.reset();
        }

        mSlaveThread.reset();

        mMessageDeliveryStates.clear();

        mIncomingCallHandlers.clear();

        // mPeerLocation.reset(); // DO NOT RESET THIS - LEAVE ALIVE UNTIL OBJECT IS DESTROYED!
      }

      //-----------------------------------------------------------------------
      void ConversationThreadHost::PeerLocation::step()
      {
        ZS_LOG_TRACE(log("step"))

        AutoRecursiveLock lock(getLock());
        if (isShutdown()) {cancel();}

      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }
  }
}
