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

#ifndef OPENPEER_CORE_CONVERSATION_THREAD_HOST_INCLUDE_PEER_LOCATION
#include <openpeer/core/internal/core_ConversationThreadHost.h>
#else

#if 0
namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      class ConversationThreadHost
      {
        // ...
#endif //0


        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ConversationThreadHost::PeerLocation
        #pragma mark

        class PeerLocation : public MessageQueueAssociator,
                             public SharedRecursiveLock,
                             public IConversationThreadDocumentFetcherDelegate
        {
        public:
          typedef thread::MessageReceiptMap MessageReceiptMap;

          typedef IConversationThread::MessageDeliveryStates MessageDeliveryStates;

          typedef String MessageID;
          typedef std::map<MessageID, MessageDeliveryStates> MessageDeliveryStatesMap;

          typedef String CallID;
          typedef std::map<CallID, UseCallPtr> CallHandlers;

          friend class PeerContact;

          typedef String ContactURI;
          typedef std::map<ContactURI, bool> ContactFetchedMap;


        protected:
          PeerLocation(
                       IMessageQueuePtr queue,
                       PeerContactPtr peerContact,
                       ILocationPtr peerLocation
                       );

          void init();

        public:
          ~PeerLocation();

          static ElementPtr toDebug(PeerLocationPtr contact);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerLocation => friend ConversationThreadHost::PeerContact
          #pragma mark

          static PeerLocationPtr create(
                                        IMessageQueuePtr queue,
                                        PeerContactPtr peerContact,
                                        ILocationPtr peerLocation
                                        );

          PUID getID() const {return mID;}
          String getLocationID() const;

          bool isConnected() const;

          void notifyPublicationUpdated(
                                        ILocationPtr peerLocation,
                                        IPublicationMetaDataPtr metaData,
                                        const SplitMap &split
                                        );
          void notifyPublicationGone(
                                     ILocationPtr peerLocation,
                                     IPublicationMetaDataPtr metaData,
                                     const SplitMap &split
                                     );
          void notifyPeerDisconnected(ILocationPtr peerLocation);

          void gatherMessagesDelivered(MessageReceiptMap &delivered) const;

          void gatherContactsToAdd(ThreadContactMap &contacts) const;
          void gatherContactsToRemove(ContactURIList &contacts) const;

          void gatherDialogReplies(
                                   const char *callID,
                                   LocationDialogMap &outDialogs
                                   ) const;

          // (duplicate) void cancel();
          // (duplicate) void step();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerLocation => IConversationThreadDocumentFetcherDelegate
          #pragma mark

          virtual void onConversationThreadDocumentFetcherPublicationUpdated(
                                                                             IConversationThreadDocumentFetcherPtr fetcher,
                                                                             ILocationPtr peerLocation,
                                                                             IPublicationPtr publication
                                                                             );

          virtual void onConversationThreadDocumentFetcherPublicationGone(
                                                                          IConversationThreadDocumentFetcherPtr fetcher,
                                                                          ILocationPtr peerLocation,
                                                                          IPublicationMetaDataPtr metaData
                                                                          );

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerLocation => (internal)
          #pragma mark

        private:
          Log::Params log(const char *message) const;

          bool isShutdown() {return mShutdown;}

        protected:
          virtual ElementPtr toDebug() const;

          void cancel();
          void step();

          void processReceiptsFromSlaveDocument(
                                                ThreadPtr hostThread,
                                                MessageDeliveryStates applyDeliveryState,
                                                const MessageReceiptMap &messagesChanged
                                                );
        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ConversationThreadHost::PeerLocation => (data)
          #pragma mark

          PeerLocationWeakPtr mThisWeak;

          AutoPUID mID;
          PeerContactWeakPtr mOuter;
          bool mShutdown;

          ILocationPtr mPeerLocation;

          ThreadPtr mSlaveThread;

          IConversationThreadDocumentFetcherPtr mFetcher;

          MessageDeliveryStatesMap mMessageDeliveryStates;

          CallHandlers mIncomingCallHandlers;

          ContactFetchedMap mPreviouslyFetchedContacts;
        };

#if 0
      }
    }
  }
}

#endif //0

#endif //OPENPEER_CORE_CONVERSATION_THREAD_HOST_INCLUDE_PEER_LOCATION
