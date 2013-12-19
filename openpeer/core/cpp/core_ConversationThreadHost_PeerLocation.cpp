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


#include <openpeer/core/internal/core_ConversationThreadHost_PeerLocation.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Call.h>
#include <openpeer/core/internal/core_Contact.h>


#include <openpeer/stack/IPublication.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::UseAccount, UseAccount)
//      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::UseContact, UseContact)
//      ZS_DECLARE_TYPEDEF_PTR(ConversationThreadHost::UseConversationThread, UseConversationThread)
//
      using services::IHelper;

      using namespace core::internal::thread;

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
        ZS_LOG_DETAIL(log("created"))
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
        ZS_LOG_DETAIL(log("destroyed"))
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
