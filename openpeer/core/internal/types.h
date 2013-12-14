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

#pragma once

#include <openpeer/core/internal/types.h>
#include <openpeer/core/types.h>
#include <openpeer/stack/types.h>
#include <openpeer/stack/message/types.h>
#include <openpeer/services/types.h>

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      using boost::shared_ptr;
      using boost::weak_ptr;
      using boost::dynamic_pointer_cast;

      using zsLib::string;
      using zsLib::Noop;
      using zsLib::BYTE;
      using zsLib::CSTR;
      using zsLib::INT;
      using zsLib::UINT;
      using zsLib::DWORD;
      using zsLib::AutoPUID;
      using zsLib::AutoLock;
      using zsLib::AutoRecursiveLock;
      using zsLib::Lock;
      using zsLib::RecursiveLock;
      using zsLib::Log;
      using zsLib::MessageQueue;
      using zsLib::IMessageQueuePtr;
      using zsLib::MessageQueuePtr;
      using zsLib::MessageQueueAssociator;
      using zsLib::IMessageQueueNotify;
      using zsLib::IMessageQueueMessagePtr;
      using zsLib::IMessageQueueThread;
      using zsLib::MessageQueueThread;
      using zsLib::IMessageQueueThreadPtr;
      using zsLib::MessageQueueThreadPtr;
      using zsLib::Timer;
      using zsLib::TimerPtr;
      using zsLib::ITimerDelegate;
      using zsLib::ITimerDelegatePtr;
      using zsLib::Seconds;
      using zsLib::Socket;

      using zsLib::XML::AttributePtr;
      using zsLib::XML::Document;
      using zsLib::XML::DocumentPtr;
      using zsLib::XML::Generator;
      using zsLib::XML::GeneratorPtr;

      using stack::Candidate;
      using stack::CandidateList;
      using stack::AutoRecursiveLockPtr;
      using stack::IBootstrappedNetwork;
      using stack::IBootstrappedNetworkPtr;
      using stack::IBootstrappedNetworkDelegate;
      using stack::ILocation;
      using stack::ILocationPtr;
      using stack::LocationList;
      using stack::LocationListPtr;
      using stack::IMessageIncomingPtr;
      using stack::IMessageMonitor;
      using stack::IMessageMonitorPtr;
      using stack::IPeer;
      using stack::IPeerPtr;
      using stack::IPeerFiles;
      using stack::IPeerFilesPtr;
      using stack::IPeerFilePrivatePtr;
      using stack::IPeerFilePublic;
      using stack::IPeerFilePublicPtr;
      using stack::IPeerSubscription;
      using stack::IPeerSubscriptionPtr;
      using stack::IPeerSubscriptionDelegate;
      using stack::ILocation;
      using stack::IPublication;
      using stack::IPublicationPtr;
      using stack::IPublicationMetaData;
      using stack::IPublicationMetaDataPtr;
      using stack::IPublicationFetcherPtr;
      using stack::IPublicationPublisherPtr;
      using stack::IPublicationPublisherDelegateProxy;
      using stack::IPublicationRepository;
      using stack::IPublicationRepositoryPtr;
      using stack::IPublicationSubscription;
      using stack::IPublicationSubscriptionPtr;
      using stack::IPublicationSubscriptionDelegate;
      using stack::IServiceIdentity;
      using stack::IServiceIdentityPtr;
      using stack::IServiceIdentitySession;
      using stack::IServiceIdentitySessionPtr;
      using stack::IServiceIdentitySessionDelegate;
      using stack::ServiceIdentitySessionList;
      using stack::ServiceIdentitySessionListPtr;
      using stack::IServiceLockbox;
      using stack::IServiceLockboxPtr;
      using stack::IServiceLockboxSession;
      using stack::IServiceLockboxSessionPtr;
      using stack::IServiceLockboxSessionDelegate;
      using stack::IServiceNamespaceGrantSession;
      using stack::IServiceNamespaceGrantSessionPtr;
      using stack::IServiceNamespaceGrantSessionDelegate;

      using stack::message::IdentityInfo;

      using services::IICESocket;
      using services::IICESocketPtr;
      using services::IICESocketDelegate;
      using services::IICESocketDelegatePtr;
      using services::IICESocketDelegateProxy;
      using services::IICESocketSubscriptionPtr;
      using services::IICESocketSession;
      using services::IICESocketSessionPtr;
      using services::IICESocketSessionDelegatePtr;
      using services::IICESocketSessionDelegateProxy;
      using services::IHTTP;
      using services::IWakeDelegate;
      using services::IWakeDelegatePtr;
      using services::IWakeDelegateWeakPtr;
      using services::IWakeDelegateProxy;

      ZS_DECLARE_INTERACTION_PTR(ICallTransport)
      ZS_DECLARE_INTERACTION_PTR(IConversationThreadHostSlaveBase)
      ZS_DECLARE_INTERACTION_PTR(IConversationThreadDocumentFetcher)
      ZS_DECLARE_INTERACTION_PTR(IStackShutdownCheckAgain)

      ZS_DECLARE_INTERACTION_PROXY(ICallAsync)
      ZS_DECLARE_INTERACTION_PROXY(ICallTransportDelegate)
      ZS_DECLARE_INTERACTION_PROXY(ICallTransportAsync)
      ZS_DECLARE_INTERACTION_PROXY(IConversationThreadDocumentFetcherDelegate)
      ZS_DECLARE_INTERACTION_PROXY(IShutdownCheckAgainDelegate)

      ZS_DECLARE_CLASS_PTR(Account)
      ZS_DECLARE_CLASS_PTR(Cache)
      ZS_DECLARE_CLASS_PTR(Call)
      ZS_DECLARE_CLASS_PTR(CallTransport)
      ZS_DECLARE_CLASS_PTR(Contact)
      ZS_DECLARE_CLASS_PTR(ContactPeerFilePublicLookup)
      ZS_DECLARE_CLASS_PTR(ConversationThread)
      ZS_DECLARE_CLASS_PTR(ConversationThreadHost)
      ZS_DECLARE_CLASS_PTR(ConversationThreadSlave)
      ZS_DECLARE_CLASS_PTR(ConversationThreadDocumentFetcher)
      ZS_DECLARE_CLASS_PTR(Factory)
      ZS_DECLARE_CLASS_PTR(Identity)
      ZS_DECLARE_CLASS_PTR(IdentityLookup)
      ZS_DECLARE_CLASS_PTR(MediaEngine)
      ZS_DECLARE_CLASS_PTR(Stack)
      ZS_DECLARE_CLASS_PTR(VideoViewPort)
    }
  }
}
