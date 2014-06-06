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
      using boost::dynamic_pointer_cast;

      using zsLib::string;
      using zsLib::Noop;
      using zsLib::BYTE;
      using zsLib::CSTR;
      using zsLib::INT;
      using zsLib::UINT;
      using zsLib::DWORD;
      using zsLib::AutoPUID;
      using zsLib::AutoBool;
      using zsLib::AutoLock;
      using zsLib::Lock;
      using zsLib::Log;
      using zsLib::MessageQueueAssociator;
      using zsLib::Seconds;
      using zsLib::Socket;
      using zsLib::PrivateGlobalLock;
      using zsLib::Singleton;
      using zsLib::SingletonLazySharedPtr;

      ZS_DECLARE_TYPEDEF_PTR(zsLib::RecursiveLock, RecursiveLock)
      ZS_DECLARE_TYPEDEF_PTR(zsLib::AutoRecursiveLock, AutoRecursiveLock)

      ZS_DECLARE_USING_PTR(zsLib, MessageQueue)
      ZS_DECLARE_USING_PTR(zsLib, IMessageQueue)
      ZS_DECLARE_USING_PTR(zsLib, IMessageQueueNotify)
      ZS_DECLARE_USING_PTR(zsLib, IMessageQueueMessage)
      ZS_DECLARE_USING_PTR(zsLib, IMessageQueueThread)
      ZS_DECLARE_USING_PTR(zsLib, MessageQueueThread)
      ZS_DECLARE_USING_PTR(zsLib, Timer)
      ZS_DECLARE_USING_PTR(zsLib, ITimerDelegate)

      ZS_DECLARE_USING_PTR(zsLib::XML, Attribute)
      ZS_DECLARE_USING_PTR(zsLib::XML, Document)
      ZS_DECLARE_USING_PTR(zsLib::XML, Generator)

      using stack::Candidate;
      using stack::CandidateList;

      ZS_DECLARE_USING_PTR(stack, IBootstrappedNetwork)
      ZS_DECLARE_USING_PTR(stack, IBootstrappedNetworkDelegate)
      ZS_DECLARE_USING_PTR(stack, ILocation)
      ZS_DECLARE_USING_PTR(stack, LocationList)
      ZS_DECLARE_USING_PTR(stack, IMessageIncoming)
      ZS_DECLARE_USING_PTR(stack, IMessageMonitor)
      ZS_DECLARE_USING_PTR(stack, IPeer)
      ZS_DECLARE_USING_PTR(stack, IPeerFiles)
      ZS_DECLARE_USING_PTR(stack, IPeerFilePrivate)
      ZS_DECLARE_USING_PTR(stack, IPeerFilePublic)
      ZS_DECLARE_USING_PTR(stack, IPeerSubscription)
      ZS_DECLARE_USING_PTR(stack, IPeerSubscriptionDelegate)
      ZS_DECLARE_USING_PTR(stack, ILocation)
      ZS_DECLARE_USING_PTR(stack, IPublication)
      ZS_DECLARE_USING_PTR(stack, IPublicationMetaData)
      ZS_DECLARE_USING_PTR(stack, IPublicationFetcher)
      ZS_DECLARE_USING_PTR(stack, IPublicationPublisher)
      ZS_DECLARE_USING_PTR(stack, IPublicationRepository)
      ZS_DECLARE_USING_PTR(stack, IPublicationSubscription)
      ZS_DECLARE_USING_PTR(stack, IPublicationSubscriptionDelegate)
      ZS_DECLARE_USING_PTR(stack, IServiceIdentity)
      ZS_DECLARE_USING_PTR(stack, IServiceIdentitySession)
      ZS_DECLARE_USING_PTR(stack, IServiceIdentitySessionDelegate)
      ZS_DECLARE_USING_PTR(stack, ServiceIdentitySessionList)
      ZS_DECLARE_USING_PTR(stack, IServiceLockbox)
      ZS_DECLARE_USING_PTR(stack, IServiceLockboxSession)
      ZS_DECLARE_USING_PTR(stack, IServiceLockboxSessionDelegate)
      ZS_DECLARE_USING_PTR(stack, IServiceNamespaceGrantSession)
      ZS_DECLARE_USING_PTR(stack, IServiceNamespaceGrantSessionDelegate)

      ZS_DECLARE_USING_PROXY(stack, IPublicationPublisherDelegate)

      ZS_DECLARE_USING_PTR(stack::message, IdentityInfo)

      ZS_DECLARE_USING_PTR(services, IBackgrounding)
      ZS_DECLARE_USING_PTR(services, IBackgroundingSubscription)
      ZS_DECLARE_USING_PTR(services, IBackgroundingNotifier)

      ZS_DECLARE_USING_PTR(services, IICESocket)
      ZS_DECLARE_USING_PTR(services, IICESocketSession)
      ZS_DECLARE_USING_PTR(services, IICESocketSubscription)
      ZS_DECLARE_USING_PTR(services, IHTTP)

      ZS_DECLARE_USING_PROXY(services, IBackgroundingDelegate)
      ZS_DECLARE_USING_PROXY(services, IICESocketDelegate)
      ZS_DECLARE_USING_PROXY(services, IICESocketSessionDelegate)
      ZS_DECLARE_USING_PROXY(services, IWakeDelegate)

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
      ZS_DECLARE_CLASS_PTR(Backgrounding)
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
      ZS_DECLARE_CLASS_PTR(PushMessaging)
      ZS_DECLARE_CLASS_PTR(Settings)
      ZS_DECLARE_CLASS_PTR(Stack)
      ZS_DECLARE_CLASS_PTR(VideoViewPort)
    }
  }
}
