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


#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_ConversationThreadHost.h>
#include <openpeer/core/internal/core_ConversationThreadSlave.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_Call.h>
#include <openpeer/core/internal/core_Helper.h>
#include <openpeer/core/internal/core_Stack.h>

#include <openpeer/core/ComposingStatus.h>

#include <openpeer/stack/IHelper.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <cryptopp/crc.h>

#include <zsLib/Stringize.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>


namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForAccount, ForAccount)
      ZS_DECLARE_TYPEDEF_PTR(IConversationThreadHostForConversationThread, UseConversationThreadHost)
      ZS_DECLARE_TYPEDEF_PTR(IConversationThreadSlaveForConversationThread, UseConversationThreadSlave)

      typedef IStackForInternal UseStack;

      using namespace core::internal::thread;

      typedef CryptoPP::CRC32 CRC32;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(IHelperForInternal, UseHelper)
      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static void convert(const ThreadContactMap &input, ContactList &output)
      {
        for (ThreadContactMap::const_iterator iter = input.begin(); iter != input.end(); ++iter)
        {
          const ThreadContactPtr &contact = (*iter).second;
          output.push_back(Contact::convert(contact->contact()));
        }
      }

      //-----------------------------------------------------------------------
      static void convert(const ThreadContactMap &input, ContactProfileInfoList &output)
      {
        for (ThreadContactMap::const_iterator iter = input.begin(); iter != input.end(); ++iter)
        {
          const ThreadContactPtr &contact = (*iter).second;
          ContactProfileInfo info;
          info.mContact = Contact::convert(contact->contact());
          info.mIdentityContacts = contact->identityContacts();
          output.push_back(info);
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadHostSlaveBase
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IConversationThreadHostSlaveBase::toDebug(IConversationThreadHostSlaveBasePtr hostOrSlave)
      {
        if (!hostOrSlave) return ElementPtr();
        if (hostOrSlave->isHost()) return UseConversationThreadHost::toDebug(UseConversationThreadHostPtr(hostOrSlave->toHost()));
        return UseConversationThreadSlave::toDebug(hostOrSlave->toSlave());
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadForHostOrSlave
      #pragma mark

      //-----------------------------------------------------------------------
      bool IConversationThreadForHostOrSlave::shouldUpdateContactStatus(
                                                                        const ContactStatusInfo &existingStatus,
                                                                        const ContactStatusInfo &newStatus,
                                                                        bool forceUpdate
                                                                        )
      {
        return ConversationThread::shouldUpdateContactStatus(existingStatus, newStatus, forceUpdate);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ForAccountPtr IConversationThreadForAccount::create(
                                                          AccountPtr account,
                                                          ILocationPtr peerLocation,
                                                          IPublicationMetaDataPtr metaData,
                                                          const SplitMap &split
                                                          )
      {
        return IConversationThreadFactory::singleton().createConversationThread(account, peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IConversationThreadForCall
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      const char *ConversationThread::toString(ConversationThreadStates state)
      {
        switch (state)
        {
          case ConversationThreadState_Pending:       return "Pending";
          case ConversationThreadState_Ready:         return "Ready";
          case ConversationThreadState_ShuttingDown:  return "Shutting down";
          case ConversationThreadState_Shutdown:      return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      ConversationThread::ConversationThread(
                                             IMessageQueuePtr queue,
                                             AccountPtr account,
                                             const char *threadID,
                                             const char *serverName
                                             ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*account),
        mAccount(account),
        mDelegate(UseAccountPtr(account)->getConversationThreadDelegate()),
        mThreadID(threadID ? String(threadID) : services::IHelper::randomString(32)),
        mServerName(serverName),
        mCurrentState(ConversationThreadState_Pending),
        mMustNotifyAboutNewThread(false),
        mOpenThreadInactivityTimeout(Seconds(UseSettings::getUInt(OPENPEER_CORE_SETTING_CONVERSATION_THREAD_HOST_INACTIVE_CLOSE_TIME_IN_SECONDS))),
        mHandleContactsChangedCRC(0)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void ConversationThread::init()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("initialized"))
        mTimer = Timer::create(mThisWeak.lock(), mOpenThreadInactivityTimeout);
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      ConversationThread::~ConversationThread()
      {
        if(isNoop()) return;
        
        ZS_LOG_BASIC(log("destroyed"))
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr ConversationThread::convert(IConversationThreadPtr thread)
      {
        return dynamic_pointer_cast<ConversationThread>(thread);
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr ConversationThread::convert(ForAccountPtr thread)
      {
        return dynamic_pointer_cast<ConversationThread>(thread);
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr ConversationThread::convert(ForCallPtr thread)
      {
        return dynamic_pointer_cast<ConversationThread>(thread);
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr ConversationThread::convert(ForHostOrSlavePtr thread)
      {
        return dynamic_pointer_cast<ConversationThread>(thread);
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr ConversationThread::convert(ForHostPtr thread)
      {
        return dynamic_pointer_cast<ConversationThread>(thread);
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr ConversationThread::convert(ForSlavePtr thread)
      {
        return dynamic_pointer_cast<ConversationThread>(thread);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThread => IConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ConversationThread::toDebug(IConversationThreadPtr thread)
      {
        if (!thread) return ElementPtr();
        return ConversationThread::convert(thread)->toDebug();
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr ConversationThread::create(
                                                       AccountPtr inAccount,
                                                       const IdentityContactList &identityContacts
                                                       )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)

        UseAccountPtr account(inAccount);

        ConversationThreadPtr pThis(new ConversationThread(UseStack::queueCore(), inAccount, NULL, NULL));
        pThis->mThisWeak = pThis;
        pThis->mSelfIdentityContacts = identityContacts;

        AutoRecursiveLock lock(*pThis);
        pThis->init();

        ZS_LOG_DEBUG(pThis->log("created for API caller"))

        // add "ourself" to the contact list...
        ContactProfileInfoList contacts;
        ContactProfileInfo info;
        info.mContact = account->getSelfContact();
        info.mIdentityContacts = identityContacts;
        contacts.push_back(info);

        pThis->addContacts(contacts);

        account->notifyConversationThreadCreated(pThis, false);
        pThis->handleContactsChanged();
        return pThis;
      }

      //-----------------------------------------------------------------------
      ConversationThreadListPtr ConversationThread::getConversationThreads(IAccountPtr inAccount)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)

        UseAccountPtr account = Account::convert(inAccount);

        ConversationThreadListPtr result(new ConversationThreadList);
        account->getConversationThreads(*result);

        return result;
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr ConversationThread::getConversationThreadByID(
                                                                          IAccountPtr inAccount,
                                                                          const char *threadID
                                                                          )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)
        ZS_THROW_INVALID_ARGUMENT_IF(!threadID)

        UseAccountPtr account = Account::convert(inAccount);

        return account->getConversationThreadByID(threadID);
      }

      //-----------------------------------------------------------------------
      String ConversationThread::getThreadID() const
      {
        AutoRecursiveLock lock(*this);
        return mThreadID;
      }

      //-----------------------------------------------------------------------
      bool ConversationThread::amIHost() const
      {
        AutoRecursiveLock lock(*this);
        if (!mLastOpenThread) return true;

        return mLastOpenThread->isHost();
      }

      //-----------------------------------------------------------------------
      IAccountPtr ConversationThread::getAssociatedAccount() const
      {
        // look ma - no lock
        return Account::convert(mAccount.lock());
      }

      //-----------------------------------------------------------------------
      ContactListPtr ConversationThread::getContacts() const
      {
        AutoRecursiveLock lock(*this);

        ContactListPtr result(new ContactList);

        if (!mLastOpenThread) {
          ZS_LOG_WARNING(Detail, log("no contacts have been added to this conversation thread"))
          return result;
        }

        ThreadContactMap contacts;
        mLastOpenThread->getContacts(contacts);

        internal::convert(contacts, *result);

        return result;
      }

      //-----------------------------------------------------------------------
      void ConversationThread::addContacts(const ContactProfileInfoList &inputContacts)
      {
        if (inputContacts.size() < 1) {
          ZS_LOG_WARNING(Debug, log("called add contacts method but did not specify any contacts to add"))
          return;
        }

        ContactProfileInfoList contacts = inputContacts;

        AutoRecursiveLock lock(*this);

        if (!mLastOpenThread) {
          mOpenThread = IConversationThreadHostForConversationThread::create(mThisWeak.lock(), mServerName);
          ZS_THROW_BAD_STATE_IF(!mOpenThread)
          mThreads[mOpenThread->getThreadID()] = mOpenThread;
          mLastOpenThread = mOpenThread;
        }

        if (mLastOpenThread->safeToChangeContacts()) {
          ZS_LOG_DEBUG(log("able to add contacts to the current thread"))
          mLastOpenThread->addContacts(contacts);

          // just in case...
          handleContactsChanged();
          handleLastOpenThreadChanged();
          return;
        }

        ContactProfileInfoList newContacts = contacts;

        ThreadContactMap oldContacts;
        mLastOpenThread->getContacts(oldContacts);

        internal::convert(oldContacts, newContacts);

        if (mLastOpenThread->isHost()) {
          UseConversationThreadHostPtr(mLastOpenThread->toHost())->close();
        }

        // not safe to add contacts, we need to create a newly open thread...
        mOpenThread = IConversationThreadHostForConversationThread::create(mThisWeak.lock(), mServerName);
        ZS_THROW_BAD_STATE_IF(!mOpenThread)
        mThreads[mOpenThread->getThreadID()] = mOpenThread;

        mLastOpenThread = mOpenThread;

        mOpenThread->addContacts(newContacts);

        handleContactsChanged();
        handleLastOpenThreadChanged();
      }

      //-----------------------------------------------------------------------
      void ConversationThread::removeContacts(const ContactList &contacts)
      {
        if (contacts.size() < 1) return;

        AutoRecursiveLock lock(*this);

        if (!mLastOpenThread) {
          ZS_LOG_DEBUG(log("no need to remove any contacts as there was no last open thread"))
          return;
        }

        if (mLastOpenThread->safeToChangeContacts()) {
          ZS_LOG_DEBUG(log("able to add contacts to the current thread"))
          mLastOpenThread->removeContacts(contacts);
          handleContactsChanged();
          return;
        }

        ThreadContactMap oldContacts;
        mLastOpenThread->getContacts(oldContacts);

        for (ContactList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
        {
          const IContactPtr &contact = (*iter);
          ThreadContactMap::iterator found = oldContacts.find(contact->getPeerURI());
          if (found != oldContacts.end()) {
            oldContacts.erase(found);
          }
        }

        if (mLastOpenThread->isHost()) {
          UseConversationThreadHostPtr(mLastOpenThread->toHost())->close();
        }

        // not safe to add contacts, we need to create a newly open thread...
        mOpenThread = IConversationThreadHostForConversationThread::create(mThisWeak.lock(), mServerName);
        ZS_THROW_BAD_STATE_IF(!mOpenThread)
        mThreads[mOpenThread->getThreadID()] = mOpenThread;
        mLastOpenThread = mOpenThread;

        // convert the old contacts into a new contact list
        ContactProfileInfoList newContacts;
        internal::convert(oldContacts, newContacts);

        if (newContacts.size() > 0) {
          mOpenThread->addContacts(newContacts);
        }

        handleContactsChanged();
        handleLastOpenThreadChanged();

        //***********************************************************************
        //***********************************************************************
        //***********************************************************************
        //***********************************************************************
        // HERE - check if trying to remove "self"
      }
      
      //-----------------------------------------------------------------------
      IdentityContactListPtr ConversationThread::getIdentityContactList(IContactPtr inContact) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inContact)

        IdentityContactListPtr result(new IdentityContactList);

        AutoRecursiveLock lock(*this);

        UseContactPtr contact = UseContactPtr(Contact::convert(inContact));
        if (contact->isSelf()) {
          if (mSelfIdentityContacts.size() > 0) {
            *result = mSelfIdentityContacts;
            return result;
          }
        }

        if (!mLastOpenThread) {
          ZS_LOG_WARNING(Detail, log("cannot get identity contacts as no contacts have been added to this conversation thread"))
          return result;
        }

        ThreadContactMap contacts;
        mLastOpenThread->getContacts(contacts);

        ThreadContactMap::iterator found = contacts.find(contact->getPeerURI());
        if (found == contacts.end()) {
          ZS_LOG_WARNING(Detail, log("cannot get identity contacts as contact was not found") + UseContact::toDebug(contact))
          return result;
        }
        const ThreadContactPtr &threadContact = (*found).second;

        (*result) = threadContact->identityContacts();
        return result;
      }

      //-----------------------------------------------------------------------
      IConversationThread::ContactConnectionStates ConversationThread::getContactConnectionState(IContactPtr inContact) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inContact)

        ContactPtr contact = Contact::convert(inContact);

        AutoRecursiveLock lock(*this);

        if (!mLastOpenThread) {
          ZS_LOG_WARNING(Detail, log("no conversation thread was ever openned"))
          return IConversationThread::ContactConnectionState_NotApplicable;
        }

        return mLastOpenThread->getContactConnectionState(contact);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThread::createEmptyStatus()
      {
        return Element::create("status");
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThread::getContactStatus(IContactPtr contact) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!contact)

        AutoRecursiveLock lock(*this);
        ContactStatusMap::const_iterator found = mLastReportedContactStatuses.find(contact->getPeerURI());

        if (found == mLastReportedContactStatuses.end()) return ElementPtr();

        const ContactStatus &status = (*found).second;

        return status.mStatus.mStatusEl ? status.mStatus.mStatusEl->clone()->toElement() : ElementPtr();
      }

      //-----------------------------------------------------------------------
      void ConversationThread::setStatusInThread(ElementPtr contactStatusInThreadOfSelf)
      {
        AutoRecursiveLock lock(*this);

        contactStatusInThreadOfSelf = contactStatusInThreadOfSelf ? contactStatusInThreadOfSelf->clone()->toElement() : ElementPtr();

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus unable to set status"))
          return;
        }

        UseContactPtr self = UseContact::getForSelf(Account::convert(account));

        ContactStatusMap::iterator found = mLastReportedContactStatuses.find(self->getPeerURI());
        if (found == mLastReportedContactStatuses.end()) {
          ContactStatus contactStatus;
          contactStatus.mContact = self;

          mLastReportedContactStatuses[self->getPeerURI()] = contactStatus;
          found = mLastReportedContactStatuses.find(self->getPeerURI());
        }

        ContactStatus &contactStatus = (*found).second;

        contactStatus.mStatus = ContactStatusInfo(contactStatusInThreadOfSelf);

        ZS_LOG_DEBUG(log("changing contact status for self") + contactStatus.mStatus.toDebug())

        if (!mLastOpenThread) {
          ZS_LOG_WARNING(Detail, log("no conversation thread was ever openned"))
          return;
        }

        mLastOpenThread->setStatusInThread(self, mSelfIdentityContacts, contactStatus.mStatus);
      }

      //-----------------------------------------------------------------------
      void ConversationThread::sendMessage(
                                           const char *messageID,
                                           const char *replacesMessageID,
                                           const char *messageType,
                                           const char *body,
                                           bool signMessage
                                           )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!messageID)
        ZS_THROW_INVALID_ARGUMENT_IF('\0' == *messageID)
        ZS_THROW_INVALID_ARGUMENT_IF(!messageType)
        ZS_THROW_INVALID_ARGUMENT_IF('\0' == *messageType)

        AutoRecursiveLock lock(*this);

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus unable to send the message") + ZS_PARAM("message ID", messageID))
          return;
        }

        IPeerFilesPtr peerFiles = account->getPeerFiles();
        if (!peerFiles) {
          ZS_LOG_WARNING(Detail, log("peer files are not generated thus unable to send the message") + ZS_PARAM("message ID", messageID))
          return;
        }

        MessagePtr message = Message::create(messageID, replacesMessageID, UseContactPtr(account->getSelfContact())->getPeerURI(), messageType, body, zsLib::now(), signMessage ? peerFiles : IPeerFilesPtr());
        if (!message) {
          ZS_LOG_ERROR(Detail, log("failed to create message object") + ZS_PARAM("message ID", messageID))
          return;
        }

        mPendingDeliveryMessages.push_back(message);
        mMessageDeliveryStates[messageID] = IConversationThread::MessageDeliveryState_Discovering;

        step();
      }

      //-----------------------------------------------------------------------
      bool ConversationThread::getMessage(
                                          const char *messageID,
                                          String &outReplacesMessageID,
                                          IContactPtr &outFrom,
                                          String &outMessageType,
                                          String &outMessage,
                                          Time &outTime,
                                          bool &outValidated
                                          ) const
      {
        AutoRecursiveLock lock(*this);
        ZS_THROW_INVALID_ARGUMENT_IF(!messageID)

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus unable to fetch message") + ZS_PARAM("message ID", messageID))
          return false;
        }

        MessageReceivedMap::const_iterator found = mReceivedOrPushedMessages.find(messageID);
        if (found == mReceivedOrPushedMessages.end()) {
          ZS_LOG_WARNING(Detail, log("unable to locate any message with the message ID provided") + ZS_PARAM("message ID", messageID))
          return false;
        }

        const MessagePtr &message = (*found).second;

        const String &peerURI = message->fromPeerURI();
        ContactPtr contact = account->findContact(peerURI);
        if (!contact) {
          ZS_LOG_ERROR(Detail, log("unable to find the contact for the message") + ZS_PARAM("message ID", messageID) + ZS_PARAM("peer URI", peerURI))
          return false;
        }

        outReplacesMessageID = message->replacesMessageID();
        outFrom = contact;
        outMessageType = message->mimeType();
        outMessage = message->body();
        outTime = message->sent();
        outValidated = message->validated();

        ZS_LOG_DEBUG(log("obtained message information") + ZS_PARAM("message ID", messageID) + ZS_PARAM("peer URI", peerURI) + IContact::toDebug(contact) + ZS_PARAM("type", outMessageType) + ZS_PARAM("message", outMessage) + ZS_PARAM("time", outTime))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ConversationThread::getMessageDeliveryState(
                                                       const char *messageID,
                                                       MessageDeliveryStates &outDeliveryState
                                                       ) const
      {
        AutoRecursiveLock lock(*this);
        outDeliveryState = IConversationThread::MessageDeliveryState_Discovering;
        MessageDeliveryStatesMap::const_iterator found = mMessageDeliveryStates.find(messageID);
        if (found == mMessageDeliveryStates.end()) {
          ZS_LOG_WARNING(Detail, log("unable to find message delivery state for message ID"))
          return false;
        }
        outDeliveryState = (*found).second;
        return true;
      }

      //-----------------------------------------------------------------------
      void ConversationThread::markAllMessagesRead()
      {
        AutoRecursiveLock lock(*this);

        for (ThreadMap::iterator iter = mThreads.begin(); iter != mThreads.end(); ++iter)
        {
          IConversationThreadHostSlaveBasePtr &thread = (*iter).second;
          thread->markAllMessagesRead();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThread => IConversationThreadForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadPtr ConversationThread::create(
                                                       AccountPtr inAccount,
                                                       ILocationPtr peerLocation,
                                                       IPublicationMetaDataPtr metaData,
                                                       const SplitMap &split
                                                       )
      {
        ConversationThreadPtr pThis(new ConversationThread(UseStack::queueCore(), inAccount, services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_BASE_THREAD_ID_INDEX), NULL));
        pThis->mThisWeak = pThis;
        pThis->mMustNotifyAboutNewThread = true;

        AutoRecursiveLock lock(*pThis);
        pThis->init();
        pThis->notifyPublicationUpdated(peerLocation, metaData, split);
        if (pThis->mThreads.size() < 1) {
          ZS_LOG_WARNING(Detail, pThis->log("publication did not result in a slave thread being created thus aborting"))
          return ConversationThreadPtr();
        }
        return pThis;
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyPublicationUpdated(
                                                        ILocationPtr peerLocation,
                                                        IPublicationMetaDataPtr metaData,
                                                        const SplitMap &split
                                                        )
      {
        AutoRecursiveLock lock(*this);

        String hostThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX);
        ZS_THROW_INVALID_ARGUMENT_IF(hostThreadID.size() < 1)

        ThreadMap::iterator found = mThreads.find(hostThreadID);
        if (found == mThreads.end()) {
          // could not find the publication... must be a host document or something is wrong...
          String type = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_TYPE_INDEX);
          if (type != "host") {
            // whatever this is it cannot be understood...
            ZS_LOG_WARNING(Detail, log("expecting a host document type but received something else") + ZS_PARAM("type", type) + IPublicationMetaData::toDebug(metaData))
            return;
          }

          ZS_LOG_DEBUG(log("creating a new conversation thread slave for updated publication") + ZS_PARAM("host thread ID", hostThreadID) + IPublicationMetaData::toDebug(metaData))

          UseConversationThreadSlavePtr slave = UseConversationThreadSlave::create(mThisWeak.lock(), peerLocation, metaData, split, mServerName);
          if (!slave) {
            ZS_LOG_WARNING(Detail, log("slave was not created for host document") + IPublicationMetaData::toDebug(metaData))
            return;
          }

          mThreads[hostThreadID] = slave;
        } else {
          ZS_LOG_DEBUG(log("reusing existing conversation thread slave for updated publication") + ZS_PARAM("host thread ID", hostThreadID) + IPublicationMetaData::toDebug(metaData))
          IConversationThreadHostSlaveBasePtr &thread = (*found).second;
          thread->notifyPublicationUpdated(peerLocation, metaData, split);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyPublicationGone(
                                                     ILocationPtr peerLocation,
                                                     IPublicationMetaDataPtr metaData,
                                                     const SplitMap &split
                                                     )
      {
        AutoRecursiveLock lock(*this);
        String hostThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX);
        ZS_THROW_INVALID_ARGUMENT_IF(hostThreadID.size() < 1)

        ThreadMap::iterator found = mThreads.find(hostThreadID);
        if (found == mThreads.end()) {
          ZS_LOG_WARNING(Detail, log("notification about a publication that is gone for a conversation that does not exist") + IPublicationMetaData::toDebug(metaData))
          return;
        }

        IConversationThreadHostSlaveBasePtr &thread = (*found).second;
        thread->notifyPublicationGone(peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyPeerDisconnected(ILocationPtr peerLocation)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("peer disconnected notification received") + ILocation::toDebug(peerLocation))

        for (ThreadMap::iterator iter = mThreads.begin(); iter != mThreads.end(); ++iter)
        {
          IConversationThreadHostSlaveBasePtr &thread = (*iter).second;
          thread->notifyPeerDisconnected(peerLocation);
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThread::ConversationThread::shutdown()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("shutdown requested"))
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThread => IConversationThreadForHostOrSlave
      #pragma mark

      //-----------------------------------------------------------------------
      AccountPtr ConversationThread::getAccount() const
      {
        // look ma - no lock
        return Account::convert(mAccount.lock());
      }

      //-----------------------------------------------------------------------
      IPublicationRepositoryPtr ConversationThread::getRepository() const
      {
        // look ma - no lock
        UseAccountPtr account = mAccount.lock();
        if (!account) return IPublicationRepositoryPtr();
        return account->getRepository();
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyStateChanged(IConversationThreadHostSlaveBasePtr thread)
      {
        AutoRecursiveLock lock(*this);
        step();
        handleContactsChanged();
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyContactConnectionState(
                                                            IConversationThreadHostSlaveBasePtr thread,
                                                            UseContactPtr contact,
                                                            ContactConnectionStates state
                                                            )
      {
        AutoRecursiveLock lock(*this);

        if (mLastOpenThread != thread) {
          ZS_LOG_TRACE(log("will not notify about contact connection states if not the last opened thread") + ZS_PARAM("from host/slave thead ID", thread->getThreadID()) + UseContact::toDebug(contact) + ZS_PARAM("reported state", IConversationThread::toString(state)))
          return;
        }

        bool changed = false;
        ContactConnectionStates lastState = IConversationThread::ContactConnectionState_NotApplicable;

        ContactConnectionStateMap::iterator found = mLastReportedContactConnectionStates.find(contact->getPeerURI());
        if (found != mLastReportedContactConnectionStates.end()) {
          ContactConnectionStatePair &statePair = (*found).second;
          lastState = statePair.second;
          changed = (lastState != state);
        } else {
          changed = true;
        }

        if (!changed) return;

        ZS_LOG_DEBUG(log("contact connection state changed") + ZS_PARAM("old state", IConversationThread::toString(lastState)) + ZS_PARAM("new state", IConversationThread::toString(state)) + UseContact::toDebug(contact))

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("conversation thread delegate not found"))
          return;
        }

        // remember the last reported state so it isn't repeated
        mLastReportedContactConnectionStates[contact->getPeerURI()] = ContactConnectionStatePair(contact, state);

        try {
          mDelegate->onConversationThreadContactConnectionStateChanged(mThisWeak.lock(), Contact::convert(contact), state);
        } catch (IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("conversation thread delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      bool ConversationThread::shouldUpdateContactStatus(
                                                         const ContactStatusInfo &existingStatus,
                                                         const ContactStatusInfo &newStatus,
                                                         bool forceUpdate
                                                         )
      {
        if (forceUpdate) {
          ZS_LOG_TRACE(slog("should update contact status as being forced to update"))
          return true;
        }

        ComposingStatusPtr existingComposingStatus = ComposingStatus::extract(existingStatus.mStatusEl);
        ComposingStatusPtr newComposingStatus = ComposingStatus::extract(existingStatus.mStatusEl);


        ComposingStatus::ComposingStates existingState = (existingComposingStatus ? existingComposingStatus->mComposingStatus : ComposingStatus::ComposingState_None);
        ComposingStatus::ComposingStates newState = (newComposingStatus ? newComposingStatus->mComposingStatus : ComposingStatus::ComposingState_None);

        if (ComposingStatus::ComposingState_Gone == existingState) {
          if (ComposingStatus::ComposingState_Gone != newState) {
            ZS_LOG_TRACE(slog("should update contact status as old status was gone but new status is not gone") + ZS_PARAM("new status", newStatus.toDebug()) + ZS_PARAM("existing status", newStatus.toDebug()))
            return true;
          }
        }

        if (Time() == existingStatus.mCreated) {
          if (Time() != newStatus.mCreated) {
            ZS_LOG_TRACE(slog("should update contact status as new status has a time set (but old did not)") + ZS_PARAM("new status", newStatus.toDebug()) + ZS_PARAM("existing status", newStatus.toDebug()))
            return true;
          }
        } else {
          if (Time() == newStatus.mCreated) {
            ZS_LOG_WARNING(Trace, slog("should NOT update contact status as new status does not contain a status time but existing status has a time") + ZS_PARAM("new status", newStatus.toDebug()) + ZS_PARAM("existing status", newStatus.toDebug()))
            return false;
          }
        }

        if (existingStatus == newStatus) {
          ZS_LOG_WARNING(Trace, slog("should NOT update contact status as status has not actually changed") + ZS_PARAM("new status", newStatus.toDebug()) + ZS_PARAM("existing status", newStatus.toDebug()))
          return false;
        }

        if (ComposingStatus::ComposingState_None == existingState) {
          if (ComposingStatus::ComposingState_None != newState) {
            ZS_LOG_TRACE(slog("should update contact status as new status has status (but old did not)") + ZS_PARAM("new status", newStatus.toDebug()) + ZS_PARAM("existing status", newStatus.toDebug()))
            return true;
          }
        }

        return existingStatus.mCreated < newStatus.mCreated;
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyContactStatus(
                                                   IConversationThreadHostSlaveBasePtr thread,
                                                   UseContactPtr contact,
                                                   const ContactStatusInfo &status,
                                                   bool forceUpdate
                                                   )
      {
        AutoRecursiveLock lock(*this);

        if (mLastOpenThread != thread) {
          ZS_LOG_TRACE(log("will not notify about contact status if not the last opened thread") + ZS_PARAM("from host/slave thead ID", thread->getThreadID()) + UseContact::toDebug(contact))
          return;
        }

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus unable to update status"))
          return;
        }

        UseContactPtr self = UseContact::getForSelf(Account::convert(account));

        if (self->getPeerURI() == contact->getPeerURI()) {
          ZS_LOG_WARNING(Trace, log("cannot adopt remote status of self contact") + UseContact::toDebug(self))
          return;
        }

        ContactStatusMap::iterator found = mLastReportedContactStatuses.find(contact->getPeerURI());
        if (found == mLastReportedContactStatuses.end()) {
          ContactStatus contactStatus;
          contactStatus.mContact = contact;

          mLastReportedContactStatuses[contact->getPeerURI()] = contactStatus;
          found = mLastReportedContactStatuses.find(contact->getPeerURI());
        }

        ContactStatus &contactStatus = (*found).second;

        if (!shouldUpdateContactStatus(contactStatus.mStatus, status, forceUpdate)) {
          ZS_LOG_TRACE(log("contact status should not update") + ZS_PARAM("existing status", contactStatus.mStatus.toDebug()) + ZS_PARAM("new status", status.toDebug()) + UseContact::toDebug(contact))
          return;
        }

        contactStatus.mStatus = status;

        ZS_LOG_DEBUG(log("contact status changed") + UseContact::toDebug(contact) + contactStatus.mStatus.toDebug())

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("conversation thread delegate not found"))
          return;
        }

        try {
          mDelegate->onConversationThreadContactStatusChanged(mThisWeak.lock(), Contact::convert(contact));
        } catch (IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("conversation thread delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      bool ConversationThread::getLastContactStatus(
                                                    UseContactPtr contact,
                                                    ContactStatusInfo &outStatus
                                                    )
      {
        AutoRecursiveLock lock(*this);

        outStatus = ContactStatusInfo();

        ContactStatusMap::iterator found = mLastReportedContactStatuses.find(contact->getPeerURI());
        if (found == mLastReportedContactStatuses.end()) return false;

        ContactStatus &contactStatus = (*found).second;

        ZS_LOG_TRACE(log("getting contact status") + UseContact::toDebug(contact) + contactStatus.mStatus.toDebug())

        outStatus = contactStatus.mStatus;
        return outStatus.hasData();
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyMessageReceived(MessagePtr message)
      {
        AutoRecursiveLock lock(*this);

        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_WARNING(Detail, log("message received after already shutdown"))
          return;
        }

        MessageReceivedMap::iterator found = mReceivedOrPushedMessages.find(message->messageID());
        if (found != mReceivedOrPushedMessages.end()) {
          ZS_LOG_DEBUG(log("message received already delivered to delegate (thus ignoring)") + message->toDebug())
          return;
        }

        // remember that this message is received
        mReceivedOrPushedMessages[message->messageID()] = message;
        ZS_LOG_DEBUG(log("message received and is being delivered to delegate") + message->toDebug())

        try {
          mDelegate->onConversationThreadMessage(mThisWeak.lock(), message->messageID());
        } catch (IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate is gone"))
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyMessageDeliveryStateChanged(
                                                                 const char *messageID,
                                                                 IConversationThread::MessageDeliveryStates state
                                                                 )
      {
        AutoRecursiveLock lock(*this);

        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_WARNING(Detail, log("message delivery state change received after already shutdown"))
          return;
        }

        bool stateChanged = false;

        MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(messageID);
        if (found == mMessageDeliveryStates.end()) {
          ZS_LOG_DEBUG(log("message delivery state has changed for message not sent from self contact (likely sent by a slave and thus ignoring)") + ZS_PARAM("message ID", messageID) + ZS_PARAM("delivery state", IConversationThread::toString(state)))
          return;
        }

        MessageDeliveryStates &deliveryState = (*found).second;
        if (state > deliveryState) {
          ZS_LOG_DEBUG(log("message delivery state has changed") + ZS_PARAM("message ID", messageID) + ZS_PARAM("old delivery state", IConversationThread::toString(deliveryState)) + ZS_PARAM("new delivery state", IConversationThread::toString(state)))
          // this state has a higher priority than the old state
          deliveryState = state;
          stateChanged = true;
        } else {
          ZS_LOG_DEBUG(log("message delivery state is being ignored since it has less significance") + ZS_PARAM("message ID", messageID) + ZS_PARAM("old delivery state", IConversationThread::toString(deliveryState)) + ZS_PARAM("new delivery state", IConversationThread::toString(state)))
        }

        if (stateChanged) {
          try {
            mDelegate->onConversationThreadMessageDeliveryStateChanged(mThisWeak.lock(), messageID, state);
          } catch (IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate is gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyMessagePush(
                                                 MessagePtr message,
                                                 UseContactPtr toContact
                                                 )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)
        ZS_THROW_INVALID_ARGUMENT_IF(!toContact)

        AutoRecursiveLock lock(*this);

        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_WARNING(Detail, log("ignoring message push notification received while shutdown") + message->toDebug())
          return;
        }

        // scope: filter out messages not sent by the self contact
        {
          MessageDeliveryStatesMap::iterator found = mMessageDeliveryStates.find(message->messageID());
          if (found == mMessageDeliveryStates.end()) {
            ZS_LOG_DEBUG(log("notified to push for message not sent from self contact (likely sent by a slave and thus ignoring)") + message->toDebug())
            return;
          }
        }

        // scope: remember this was one of the messages received or pushed (so getMessage will work)
        {
          MessageReceivedMap::iterator found = mReceivedOrPushedMessages.find(message->messageID());
          if (found == mReceivedOrPushedMessages.end()) {
            mReceivedOrPushedMessages[message->messageID()] = message;
          }
        }

        try {
          ZS_LOG_DEBUG(log("requesting push notification for conversation thread message") + message->toDebug() + UseContact::toDebug(toContact))
          mDelegate->onConversationThreadPushMessage(mThisWeak.lock(), message->messageID(), Contact::convert(toContact));
        } catch(IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("unable to push message as delegate was gone"))
        }
      }

      //-----------------------------------------------------------------------
      void ConversationThread::requestAddIncomingCallHandler(
                                                             const char *dialogID,
                                                             IConversationThreadHostSlaveBasePtr hostOrSlaveThread,
                                                             UseCallPtr newCall
                                                             )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!dialogID)
        ZS_THROW_INVALID_ARGUMENT_IF(!hostOrSlaveThread)
        ZS_THROW_INVALID_ARGUMENT_IF(!newCall)

        AutoRecursiveLock lock(*this);
        CallHandlerMap::iterator found = mCallHandlers.find(dialogID);
        if (found != mCallHandlers.end()) {
          ZS_LOG_WARNING(Detail, log("already have a call handler for this call thus ignoring request to add one") + ZS_PARAM("call ID", dialogID))
          return;
        }

        ZS_LOG_DEBUG(log("call handler added for incoming call") + ZS_PARAM("call ID", dialogID))
        mCallHandlers[dialogID] = CallHandlerPair(hostOrSlaveThread, newCall);
      }

      //-----------------------------------------------------------------------
      void ConversationThread::requestRemoveIncomingCallHandler(const char *dialogID)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!dialogID)

        AutoRecursiveLock lock(*this);
        CallHandlerMap::iterator found = mCallHandlers.find(dialogID);
        if (found == mCallHandlers.end()) {
          ZS_LOG_WARNING(Detail, log("unable to find incoming call handler to remove") + ZS_PARAM("call ID", dialogID))
          return;
        }
        ZS_LOG_DEBUG(log("removing incoming call handler") + ZS_PARAM("call ID", dialogID))
        mCallHandlers.erase(found);
        ZS_LOG_DEBUG(log("incoming call handler removed") + toDebug())
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyPossibleCallReplyStateChange(const char *dialogID)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!dialogID)

        AutoRecursiveLock lock(*this);
        CallHandlerMap::iterator found = mCallHandlers.find(dialogID);
        if (found == mCallHandlers.end()) {
          ZS_LOG_WARNING(Detail, log("unable to find call handler for call") + ZS_PARAM("call ID", dialogID))
          return;
        }

        ZS_LOG_DEBUG(log("nudging the call to notify about a potential call reply state change") + ZS_PARAM("call ID", dialogID))

        CallHandlerPair &handlerPair = (*found).second;
        UseCallPtr &call = handlerPair.second;

        call->notifyConversationThreadUpdated();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThread => IConversationThreadForHost
      #pragma mark

      //-----------------------------------------------------------------------
      bool ConversationThread::inConversation(UseContactPtr contact) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!contact)
        AutoRecursiveLock lock(*this);
        if (!mLastOpenThread) {
          return false;
        }
        return mLastOpenThread->inConversation(contact);
      }

      //-----------------------------------------------------------------------
      void ConversationThread::removeContacts(const ContactURIList &contacts)
      {
        if (contacts.size() < 1) return;
        AutoRecursiveLock lock(*this);

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone"))
          return;
        }

        ContactList contactList;
        for (ContactURIList::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter)
        {
          const String &peerURI = (*iter);
          ContactPtr contact = account->findContact(peerURI);
          if (!contact) {
            ZS_LOG_WARNING(Detail, log("could not find peer URI in contact list") + ZS_PARAM("peer URI", peerURI))
            continue;
          }

          ZS_LOG_DEBUG(log("need to remove contact") + IContact::toDebug(contact))
          contactList.push_back(contact);
        }

        if (contactList.size() < 1) {
          ZS_LOG_DEBUG(log("no contacts found needing to be removed"))
          return;
        }

        removeContacts(contactList);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThread => IConversationThreadForSlave
      #pragma mark

      //-----------------------------------------------------------------------
      void ConversationThread::notifyAboutNewThreadNowIfNotNotified(ConversationThreadSlavePtr slave)
      {
        AutoRecursiveLock lock(*this);
        if (!mMustNotifyAboutNewThread) {
          ZS_LOG_DEBUG(log("no need to notify about thread creation as it is already created"))
          return;
        }

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("unable to notify about new thread as account object is gone"))
          return;
        }

        ZS_LOG_DEBUG(log("notifying that new conversation thread is now created"))

        if (!mLastOpenThread) {
          ZS_LOG_DEBUG(log("slave has now become last open thread"))
          mLastOpenThread = slave;
        }

        account->notifyConversationThreadCreated(mThisWeak.lock(), true);
        mMustNotifyAboutNewThread = false;

        handleLastOpenThreadChanged();
      }

      //-----------------------------------------------------------------------
      void ConversationThread::convertSlaveToClosedHost(
                                                        ConversationThreadSlavePtr inSlave,
                                                        ThreadPtr originalHost,
                                                        ThreadPtr originalSlave
                                                        )
      {
        UseConversationThreadSlavePtr slave = inSlave;

        AutoRecursiveLock lock(*this);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("unable to convert slave to closed host as basee thread is shutting down/shutdown") + UseConversationThreadSlave::toDebug(slave))
          return;
        }

        ZS_LOG_DEBUG(log("converting slave to closed host") + ZS_PARAM("slave thread ID", UseConversationThreadSlave::toDebug(slave)))

        if (mOpenThread == slave) {
          ZS_LOG_DETAIL(log("slave thread is no longer considered 'open'") + UseConversationThreadSlave::toDebug(slave))
          mOpenThread.reset();
        }

        ThreadContactsPtr contacts = originalHost->contacts();

        const ThreadContactMap &originalContacts = contacts->contacts();

        UseConversationThreadHostPtr newClosedHost = UseConversationThreadHost::create(mThisWeak.lock(), mServerName, thread::Details::ConversationThreadState_Closed);
        ZS_THROW_BAD_STATE_IF(!newClosedHost)
        mThreads[newClosedHost->getThreadID()] = newClosedHost;

        ZS_LOG_DEBUG(log("new closed host created for slave") + ZS_PARAM("slave", UseConversationThreadSlave::toDebug(slave)) + ZS_PARAM("new closed thread ID", UseConversationThreadHost::toDebug(newClosedHost)))

        ContactProfileInfoList newContacts;
        internal::convert(originalContacts, newContacts);

        newClosedHost->addContacts(newContacts);

        // gather all the messages
        MessageList messages;
        MessageMap messagesAsMap;

        const MessageList &hostMessages = originalHost->messages();
        const MessageList &slaveMessages = originalSlave->messages();

        for (MessageList::const_iterator iter = hostMessages.begin(); iter != hostMessages.end(); ++iter)
        {
          const MessagePtr &message = (*iter);
          MessageMap::iterator found = messagesAsMap.find(message->messageID());
          if (found != messagesAsMap.end()) {
            ZS_LOG_TRACE(log("igoring host message as message as already added") + message->toDebug())
            continue;
          }
          ZS_LOG_TRACE(log("add host message to new closed host thread") + message->toDebug())
          messages.push_back(message);
          messagesAsMap[message->messageID()] = message;
        }

        // NOTE:
        // Order does matter for the messages... ensure the most likely
        // undelivered messages are added last to the conversation thread.
        for (MessageList::const_iterator iter = slaveMessages.begin(); iter != slaveMessages.end(); ++iter)
        {
          const MessagePtr &message = (*iter);
          MessageMap::iterator found = messagesAsMap.find(message->messageID());
          if (found != messagesAsMap.end()) {
            ZS_LOG_TRACE(log("igoring slave message as message as already added") + message->toDebug())
            continue;
          }
          ZS_LOG_TRACE(log("add slave message to new closed host thread") + message->toDebug())
          messages.push_back(message);
          messagesAsMap[message->messageID()] = message;
        }

        ZS_THROW_BAD_STATE_IF(messages.size() < 1)

        ZS_LOG_DEBUG(log("requesting to deliver messages to closed thread"))
        newClosedHost->sendMessages(messages);

        // force a step on the conversation thread to cleanup anything state wise...
        ZS_LOG_DEBUG(log("forcing step"))
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThread => IConversationThreadForCall
      #pragma mark

      //-----------------------------------------------------------------------
      bool ConversationThread::placeCall(CallPtr inCall)
      {
        UseCallPtr call = inCall;

        ZS_THROW_INVALID_ARGUMENT_IF(!call)

        AutoRecursiveLock lock(*this);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          return false;
        }

        ZS_LOG_DEBUG(log("adding all to pending list") + UseCall::toDebug(call))

        mPendingCalls[call->getCallID()] = call;

        ZS_LOG_DEBUG(log("forcing step"))
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyCallStateChanged(CallPtr inCall)
      {
        UseCallPtr call = inCall;

        ZS_THROW_INVALID_ARGUMENT_IF(!call)

        AutoRecursiveLock lock(*this);

        CallHandlerMap::iterator found = mCallHandlers.find(call->getCallID());
        if (found == mCallHandlers.end()) {
          ZS_LOG_DEBUG(log("call is not known yet to the conversation thread") + UseCall::toDebug(call))
          return;
        }
        IConversationThreadHostSlaveBasePtr &thread = (*found).second.first;
        thread->notifyCallStateChanged(call);
      }

      //-----------------------------------------------------------------------
      void ConversationThread::notifyCallCleanup(CallPtr inCall)
      {
        UseCallPtr call = inCall;

        ZS_THROW_INVALID_ARGUMENT_IF(!call)

        AutoRecursiveLock lock(*this);

        PendingCallMap::iterator foundPending = mPendingCalls.find(call->getCallID());
        if (foundPending != mPendingCalls.end()) {
          ZS_LOG_DEBUG(log("call found on pending list thus removing") + UseCall::toDebug(call))
          mPendingCalls.erase(foundPending);
          ZS_LOG_DEBUG(log("call found on pending list removed") + toDebug())
        }

        CallHandlerMap::iterator found = mCallHandlers.find(call->getCallID());
        if (found == mCallHandlers.end()) {
          ZS_LOG_WARNING(Detail, log("did not find any call handlers for this call") + UseCall::toDebug(call))
          return;
        }

        ZS_LOG_DEBUG(log("call found and is being removed") + UseCall::toDebug(call))
        IConversationThreadHostSlaveBasePtr &thread = (*found).second.first;
        thread->notifyCallCleanup(call);
        mCallHandlers.erase(found);

        ZS_LOG_DEBUG(log("call handler removed") + toDebug())
      }

      //-----------------------------------------------------------------------
      void ConversationThread::gatherDialogReplies(
                                                   const char *callID,
                                                   LocationDialogMap &outDialogs
                                                   ) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!callID)

        AutoRecursiveLock lock(*this);

        CallHandlerMap::const_iterator found = mCallHandlers.find(callID);
        if (found == mCallHandlers.end()) {
          ZS_LOG_DEBUG(log("no replies found for this call") + ZS_PARAM("call ID", callID))
          return;
        }

        const IConversationThreadHostSlaveBasePtr &thread = (*found).second.first;

        ZS_LOG_DEBUG(log("requesting replies from thread") + ZS_PARAM("call ID", callID) + IConversationThreadHostSlaveBase::toDebug(thread))
        thread->gatherDialogReplies(callID, outDialogs);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ConversationThread => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ConversationThread::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::ConversationThread");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ConversationThread::slog(const char *message)
      {
        ElementPtr objectEl = Element::create("core::ConversationThread");
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr ConversationThread::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::ConversationThread");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "graceful shutdown reference", (bool)mGracefulShutdownReference);

        UseServicesHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        UseServicesHelper::debugAppend(resultEl, "account", (bool)mAccount.lock());

        UseServicesHelper::debugAppend(resultEl, "thread id", mThreadID);
        UseServicesHelper::debugAppend(resultEl, "server name", mServerName);

        UseServicesHelper::debugAppend(resultEl, "current state", toString(mCurrentState));
        UseServicesHelper::debugAppend(resultEl, "must notify", mMustNotifyAboutNewThread);

        UseServicesHelper::debugAppend(resultEl, IConversationThreadHostSlaveBase::toDebug(mOpenThread));
        UseServicesHelper::debugAppend(resultEl, IConversationThreadHostSlaveBase::toDebug(mLastOpenThread));

        UseServicesHelper::debugAppend(resultEl, "timer", mTimer ? mTimer->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "inactivity timeout (s)", mOpenThreadInactivityTimeout);

        UseServicesHelper::debugAppend(resultEl, IConversationThreadHostSlaveBase::toDebug(mHandleThreadChanged));
        UseServicesHelper::debugAppend(resultEl, "crc", mHandleContactsChangedCRC);

        UseServicesHelper::debugAppend(resultEl, "threads", mThreads.size());

        UseServicesHelper::debugAppend(resultEl, "received or pushed", mReceivedOrPushedMessages.size());

        UseServicesHelper::debugAppend(resultEl, "delivery states", mMessageDeliveryStates.size());
        UseServicesHelper::debugAppend(resultEl, "pending delivery", mPendingDeliveryMessages.size());

        UseServicesHelper::debugAppend(resultEl, "pending calls", mPendingCalls.size());

        UseServicesHelper::debugAppend(resultEl, "call handlers", mCallHandlers.size());

        UseServicesHelper::debugAppend(resultEl, "self identity contacts", mSelfIdentityContacts.size());


        UseServicesHelper::debugAppend(resultEl, "last reported connection states", mLastReportedContactConnectionStates.size());
        UseServicesHelper::debugAppend(resultEl, "last reported contact status", mLastReportedContactStatuses.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ConversationThread::cancel()
      {
        ZS_LOG_DEBUG(log("cancel"))

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("cancel called but already shutdown"))
          return;
        }

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(ConversationThreadState_ShuttingDown);

        for (ThreadMap::iterator iter_doNotUse = mThreads.begin(); iter_doNotUse != mThreads.end();)
        {
          ThreadMap::iterator current = iter_doNotUse; ++iter_doNotUse;

          IConversationThreadHostSlaveBasePtr &thread = (*current).second;
          thread->shutdown();
        }

        if (mGracefulShutdownReference) {
          for (ThreadMap::iterator iter = mThreads.begin(); iter != mThreads.end(); ++iter)
          {
            IConversationThreadHostSlaveBasePtr &thread = (*iter).second;
            if (!thread->isShutdown()) {
              ZS_LOG_DEBUG(log("waiting for thread to shutdown") + ZS_PARAM("thread ID", (*iter).first))
              return;
            }
          }
        }

        setState(ConversationThreadState_Shutdown);

        mGracefulShutdownReference.reset();

        mDelegate.reset();

        mOpenThread.reset();
        mLastOpenThread.reset();

        if (mTimer) {
          mTimer->cancel();
          mTimer.reset();
        }

        mThreads.clear();

        mReceivedOrPushedMessages.clear();
        mMessageDeliveryStates.clear();
        mPendingDeliveryMessages.clear();
      }

      //-----------------------------------------------------------------------
      void ConversationThread::step()
      {
        ZS_LOG_DEBUG(log("step"))

        AutoRecursiveLock lock(*this);

        if ((isShuttingDown())
            || (isShutdown())) {
          ZS_LOG_DEBUG(log("step forwarding to cancel"))
          cancel();
          return;
        }

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account is gone thus unable step"))
          cancel();
          return;
        }

        ZS_LOG_TRACE(log("step continued"))

        setState(ConversationThreadState_Ready);

        // figure out how many threads are open and which was open last
        UINT totalOpen = 0;
        IConversationThreadHostSlaveBasePtr mostRecentOpen;
        Time mostRecentOpenTime;
        String mostRecentServerName;
        for (ThreadMap::iterator iter = mThreads.begin(); iter != mThreads.end(); ++iter)
        {
          IConversationThreadHostSlaveBasePtr &thread = (*iter).second;
          String hostServerName = thread->getHostServerName();

          // scope: check if new thread should be used
          {
            if (!thread->isHostThreadOpen()) continue;

            ++totalOpen;

            if (1 == totalOpen) {
              ZS_LOG_TRACE(log("first thread found open (thus choosing)") + IConversationThreadHostSlaveBase::toDebug(thread))
              goto use_loop_thread;
            }

            if (hostServerName.hasData()) {
              if (mostRecentServerName.hasData()) {
                ZS_LOG_TRACE(log("found thread is a server and most recent open is a server (do confict resolution)") + IConversationThreadHostSlaveBase::toDebug(thread))

                if (mostRecentServerName < hostServerName) {
                  ZS_LOG_TRACE(log("found thread lost conflict resolution (thus NOT choosing)") + ZS_PARAM("most recent open server name is", mostRecentServerName) + IConversationThreadHostSlaveBase::toDebug(thread))
                  continue;
                }
                ZS_LOG_TRACE(log("found thread is a server won conflict resolution (thus choosing)") + ZS_PARAM("most recent open server name was", mostRecentServerName) + IConversationThreadHostSlaveBase::toDebug(thread))
                goto use_loop_thread;
              }

              ZS_LOG_TRACE(log("found thread is a server (thus choosing)") + IConversationThreadHostSlaveBase::toDebug(thread))
              goto use_loop_thread;
            }

            if (mostRecentServerName.hasData()) {
              ZS_LOG_TRACE(log("thread found does not have a server name but most recent open does (thus NOT choosing this thread)") + IConversationThreadHostSlaveBase::toDebug(thread))
              continue;
            }

            Time created = thread->getHostCreationTime();
            if (created > mostRecentOpenTime) {
              ZS_LOG_TRACE(log("thread found is the most recent (thus choosing)") + ZS_PARAM("most recent time was", mostRecentOpenTime) + IConversationThreadHostSlaveBase::toDebug(thread))
              goto use_loop_thread;
            }

            if (created < mostRecentOpenTime) {
              ZS_LOG_TRACE(log("thread found is older than the most recent (this NOT choosing)") + ZS_PARAM("most recent time is", mostRecentOpenTime) + IConversationThreadHostSlaveBase::toDebug(thread))
              continue;
            }

            ZS_LOG_TRACE(log("found thread is created at exactly the same time (use conflict resolution)") + IConversationThreadHostSlaveBase::toDebug(thread))
            String checkingThreadID = thread->getThreadID();
            String mostRecentOpenThreadID = thread->getThreadID();

            if (checkingThreadID > mostRecentOpenThreadID) {
              ZS_LOG_TRACE(log("conflict resolution says use found thread instead of last most recent open thread (thus choosing)") + ZS_PARAM("most recent open thread ID was", mostRecentOpenThreadID) + IConversationThreadHostSlaveBase::toDebug(thread))
              goto use_loop_thread;
            }

            ZS_LOG_TRACE(log("conflict resolution says use most recent open instead of found thread (thus NOT choosing)") + ZS_PARAM("most recent open thread ID is", mostRecentOpenThreadID) + IConversationThreadHostSlaveBase::toDebug(thread))
            continue;
          }

        use_loop_thread:
          {
            mostRecentOpen = thread;
            mostRecentOpenTime = thread->getHostCreationTime();
            mostRecentServerName = hostServerName;
          }
        }

        ZS_LOG_TRACE(log("finished counting open threads") + ZS_PARAM("total open", totalOpen))

        if (totalOpen > 1) {
          ZS_LOG_DEBUG(log("found more than one thread open (thus will close any hosts that are not the most recent)"))

          // can only have one thread open maxmimum, all other host threads must be closed...
          for (ThreadMap::iterator iter = mThreads.begin(); iter != mThreads.end(); ++iter)
          {
            IConversationThreadHostSlaveBasePtr &thread = (*iter).second;
            if (thread != mostRecentOpen) {
              if (thread->isHost()) {
                ZS_LOG_DEBUG(log("due to more than one thread being open host is being closed (as it's not the most recent)") + IConversationThreadHostSlaveBase::toDebug(thread))
                // close the thread...
                UseConversationThreadHostPtr(thread->toHost())->close();
              }
            }
          }
        }

        if (mostRecentOpen) {
          // remember which thread is open now...
          ZS_LOG_TRACE(log("determined which thread is the most recent open") + IConversationThreadHostSlaveBase::toDebug(mostRecentOpen))
          mOpenThread = mostRecentOpen;
          mLastOpenThread = mostRecentOpen;
        }

        // attempt to obtain the self identity contacts based upon the last open thread if no identity contacts are set for self
        if (mLastOpenThread) {
          if (mSelfIdentityContacts.size() < 1) {
            ThreadContactMap contacts;
            mLastOpenThread->getContacts(contacts);

            UseContactPtr selfContact = UseContact::getForSelf(Account::convert(account));

            ThreadContactMap::iterator found = contacts.find(selfContact->getPeerURI());
            if (found != contacts.end()) {
              const ThreadContactPtr &threadContact = (*found).second;
              mSelfIdentityContacts = threadContact->identityContacts();
              if (mSelfIdentityContacts.size() > 0) {
                ZS_LOG_DEBUG(log("found identities to use for self") + ZS_PARAM("totoal", mSelfIdentityContacts.size()))
              } else {
                ZS_LOG_WARNING(Debug, log("cannot get identity contacts as no identities were found found") + UseContact::toDebug(selfContact))
              }
            } else {
              ZS_LOG_WARNING(Trace, log("cannot get identity contacts as self contact was not found") + UseContact::toDebug(selfContact))
            }
          }
        }

        if (0 == totalOpen) {
          ZS_LOG_TRACE(log("no open conversation threads found"))
          mOpenThread.reset();
        }

        bool mustHaveOpenThread = false;

        if (mPendingDeliveryMessages.size() > 0) {
          ZS_LOG_TRACE(log("messages are pending delivery") + ZS_PARAM("total pending", mPendingDeliveryMessages.size()))
          mustHaveOpenThread = true;
        }

        if (mPendingCalls.size() > 0) {
          ZS_LOG_TRACE(log("calls are pending being placed") + ZS_PARAM("total pending", mPendingCalls.size()))
          mustHaveOpenThread = true;
        }

        ZS_LOG_TRACE(log("finished checking if must have open thread") + ZS_PARAM("must have", mustHaveOpenThread))
        if (mustHaveOpenThread) {
          if (!mOpenThread) {
            // create a host thread since there is no open thread...
            mOpenThread = IConversationThreadHostForConversationThread::create(mThisWeak.lock(), mServerName);
            ZS_THROW_BAD_STATE_IF(!mOpenThread)

            ZS_LOG_DEBUG(log("no thread found to be open thus creating a new host") + IConversationThreadHostSlaveBase::toDebug(mOpenThread))
            mThreads[mOpenThread->getThreadID()] = mOpenThread;

            if (mLastOpenThread) {
              // make sure the contacts from the last open thread are carried over...
              ThreadContactMap contacts;
              mLastOpenThread->getContacts(contacts);

              ZS_LOG_DEBUG(log("contacts from last open thread are being brought into new thread") + ZS_PARAM("total contacts", contacts.size()))

              ContactProfileInfoList addContacts;
              internal::convert(contacts, addContacts);

              mOpenThread->addContacts(addContacts);
            }

            mLastOpenThread = mOpenThread;
          }
        } else {
          // do not require an open thread but will keep it open if it's active
          if (mOpenThread) {
            if (mOpenThread->isHost()) {
              UseConversationThreadHostPtr host = mOpenThread->toHost();

              Time lastActivity = host->getLastActivity();
              Time now = zsLib::now();
              if (lastActivity + mOpenThreadInactivityTimeout < now) {
                ZS_LOG_DEBUG(log("thread is inactive so closing open host thread now") + ZS_PARAM("last activity", lastActivity) + ZS_PARAM("now", now) + ZS_PARAM("inactivity timeout (s)", mOpenThreadInactivityTimeout))
                host->close();
                mOpenThread.reset();
              }
            }
          }
        }

        if (mOpenThread) {
          ZS_LOG_TRACE(log("thread has open thread") + IConversationThreadHostSlaveBase::toDebug(mOpenThread))

          if (mPendingDeliveryMessages.size() > 0) {
            bool sent = mOpenThread->sendMessages(mPendingDeliveryMessages);
            if (sent) {
              ZS_LOG_DEBUG(log("messages were accepted by open thread"))
              mPendingDeliveryMessages.clear();
            }
          }

          if (mPendingCalls.size() > 0) {
            bool sent = mOpenThread->placeCalls(mPendingCalls);
            if (sent) {
              // remember that this thread is handling all these placed calls...
              for (PendingCallMap::iterator iter = mPendingCalls.begin(); iter != mPendingCalls.end(); ++iter) {
                const CallID &callID = (*iter).first;
                UseCallPtr &call = (*iter).second;
                ZS_LOG_DEBUG(log("call placed and now handled via open thread") + UseCall::toDebug(call))
                mCallHandlers[callID] = CallHandlerPair(mOpenThread, call);

                // nudge the call to tell it state has changed...
                call->notifyConversationThreadUpdated();
              }

              ZS_LOG_DEBUG(log("pending calls are now removed"))
              mPendingCalls.clear();
            } else {
              ZS_LOG_WARNING(Detail, log("unable to place calls via open thread"))
            }
          }
        }

        handleLastOpenThreadChanged();

        ZS_LOG_TRACE(log("step completed"))
      }

      //-----------------------------------------------------------------------
      void ConversationThread::setState(ConversationThreadStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ZS_PARAM("old state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;
      }

      //-----------------------------------------------------------------------
      void ConversationThread::handleLastOpenThreadChanged()
      {
        if (mHandleThreadChanged == mLastOpenThread) {
          ZS_LOG_TRACE(log("last open thread did not change"))
          return;
        }

        ZS_LOG_DEBUG(log("last open thread changed"))
        mHandleThreadChanged = mLastOpenThread;

        if (!mLastOpenThread) {
          ZS_LOG_WARNING(Detail, log("last open thread has become NULL"))
          return;
        }

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("delegate is NULL"))
          return;
        }

        ThreadContactMap contacts;
        mLastOpenThread->getContacts(contacts);

        for (ThreadContactMap::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter) {
          const ThreadContactPtr &threadContact = (*iter).second;
          UseContactPtr contact = threadContact->contact();
          ContactConnectionStates state = mLastOpenThread->getContactConnectionState(contact);

          bool changed = false;

          ContactConnectionStateMap::iterator found = mLastReportedContactConnectionStates.find(contact->getPeerURI());
          if (found != mLastReportedContactConnectionStates.end()) {
            ContactConnectionStatePair &statePair = (*found).second;
            if (statePair.second != state) {
              statePair.second = state;
              changed = true;
            }
          } else {
            mLastReportedContactConnectionStates[contact->getPeerURI()] = ContactConnectionStatePair(contact, state);
            changed = true;
          }

          try {
            ZS_LOG_DEBUG(log("notifying of contact state changed") + ZS_PARAM("state", IConversationThread::toString(state)) + UseContact::toDebug(contact))
            mDelegate->onConversationThreadContactConnectionStateChanged(mThisWeak.lock(), Contact::convert(contact), state);
          } catch (IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("conversation thread delegate gone"))
          }
        }

        handleContactsChanged();
      }

      //-----------------------------------------------------------------------
      void ConversationThread::handleContactsChanged()
      {
        if (!mLastOpenThread) {
          ZS_LOG_WARNING(Detail, log("last open thread has become NULL"))
          return;
        }

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("delegate is NULL"))
          return;
        }

        DWORD crcValue = 0;

        ThreadContactMap contacts;
        mLastOpenThread->getContacts(contacts);

        CRC32 crc;
        for (ThreadContactMap::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter) {
          const String &contactID = (*iter).first;

          crc.Update((const BYTE *)(contactID.c_str()), contactID.length());
          crc.Update((const BYTE *)(":"), strlen(":"));
        }

        crc.Final((BYTE *)(&crcValue));

        if (mHandleContactsChangedCRC == crcValue) {
          ZS_LOG_DEBUG(log("contact change not detected") + ZS_PARAM("CRC value", crcValue))
          return;
        }

        mHandleContactsChangedCRC = crcValue;
        ZS_LOG_DEBUG(log("contact change detected") + ZS_PARAM("CRC value", crcValue))

        try {
          mDelegate->onConversationThreadContactsChanged(mThisWeak.lock());
        } catch (IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("conversation thread delegate gone"))
        }
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
        return ConversationThreadFactory::singleton();
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr IConversationThreadFactory::createConversationThread(
                                                                                 AccountPtr account,
                                                                                 const IdentityContactList &identityContacts
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

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IConversationThread
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IConversationThread::toString(MessageDeliveryStates state)
    {
      switch (state) {
        case MessageDeliveryState_Discovering:      return "Discovering";
        case MessageDeliveryState_UserNotAvailable: return "User not available";
        case MessageDeliveryState_Delivered:        return "Delivered";
        case MessageDeliveryState_Read:             return "Read";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    const char *IConversationThread::toString(ContactConnectionStates state)
    {
      switch (state) {
        case ContactConnectionState_NotApplicable:  return "Not applicable";
        case ContactConnectionState_Finding:        return "Finding";
        case ContactConnectionState_Connected:      return "Connected";
        case ContactConnectionState_Disconnected:   return "Disconnected";
      }

      return "UNDEFINED";
    }

    //-----------------------------------------------------------------------
    ElementPtr IConversationThread::toDebug(IConversationThreadPtr thread)
    {
      return internal::ConversationThread::toDebug(thread);
    }

    //-----------------------------------------------------------------------
    IConversationThreadPtr IConversationThread::create(
                                                       IAccountPtr account,
                                                       const IdentityContactList &identityContacts
                                                       )
    {
      return internal::IConversationThreadFactory::singleton().createConversationThread(internal::Account::convert(account), identityContacts);
    }

    //-----------------------------------------------------------------------
    ConversationThreadListPtr IConversationThread::getConversationThreads(IAccountPtr account)
    {
      return internal::ConversationThread::getConversationThreads(account);
    }

    //-----------------------------------------------------------------------
    IConversationThreadPtr IConversationThread::getConversationThreadByID(
                                                                          IAccountPtr account,
                                                                          const char *threadID
                                                                          )
    {
      return internal::ConversationThread::getConversationThreadByID(account, threadID);
    }

    //-----------------------------------------------------------------------
    ElementPtr IConversationThread::createEmptyStatus()
    {
      return internal::ConversationThread::createEmptyStatus();
    }
  }
}
