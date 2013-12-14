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
#include <openpeer/core/internal/core_MediaEngine.h>

#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>
#include <zsLib/Timer.h>

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      interaction ICallForCallTransport;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransport
      #pragma mark

      interaction ICallTransport
      {
        enum CallTransportStates
        {
          CallTransportState_Pending,
          CallTransportState_Ready,
          CallTransportState_ShuttingDown,
          CallTransportState_Shutdown,
        };

        static const char *toString(CallTransportStates state);

        static ElementPtr toDebug(ICallTransportPtr transport);

        virtual PUID getID() const = 0;
        virtual CallTransportStates getState() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportDelegate
      #pragma mark

      interaction ICallTransportDelegate
      {
        typedef ICallTransport::CallTransportStates CallTransportStates;

        virtual void onCallTransportStateChanged(
                                                 ICallTransportPtr transport,
                                                 CallTransportStates state
                                                 ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportForAccount
      #pragma mark

      interaction ICallTransportForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(ICallTransportForAccount, ForAccount)

        typedef ICallTransport::CallTransportStates CallTransportStates;

        static ForAccountPtr create(
                                    ICallTransportDelegatePtr delegate,
                                    const IICESocket::TURNServerInfoList &turnServers,
                                    const IICESocket::STUNServerInfoList &stunServers
                                    );

        virtual void shutdown() = 0;
        virtual CallTransportStates getState() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportForCall
      #pragma mark

      interaction ICallTransportForCall
      {
        ZS_DECLARE_TYPEDEF_PTR(ICallTransportForCall, ForCall)

        enum SocketTypes
        {
          SocketType_Audio,
          SocketType_Video,
        };

        static const char *toString(SocketTypes type);

        virtual RecursiveLock &getLock() const = 0;

        virtual void notifyCallCreation(PUID idCall) = 0;
        virtual void notifyCallDestruction(PUID idCall) = 0;

        virtual void focus(
                           CallPtr call,
                           PUID locationID
                           ) = 0;
        virtual void loseFocus(PUID callID) = 0;

        virtual IICESocketPtr getSocket(SocketTypes type) const = 0;

        virtual void notifyReceivedRTPPacket(
                                             PUID callID,
                                             PUID locationID,
                                             SocketTypes type,
                                             const BYTE *buffer,
                                             size_t bufferLengthInBytes
                                             ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportAsync
      #pragma mark

      interaction ICallTransportAsync
      {
        virtual void onStart() = 0;
        virtual void onStop() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport
      #pragma mark

      class CallTransport  : public Noop,
                             public MessageQueueAssociator,
                             public ICallTransport,
                             public ICallTransportForAccount,
                             public ICallTransportForCall,
                             public IWakeDelegate,
                             public ICallTransportAsync,
                             public ITimerDelegate
      {
      public:
        friend interaction ICallTransportFactory;
        friend interaction ICallTransport;

        ZS_DECLARE_TYPEDEF_PTR(ICallForCallTransport, UseCall)

        ZS_DECLARE_CLASS_PTR(TransportSocket)

        friend class TransportSocket;

        typedef ICallTransport::CallTransportStates CallTransportStates;

        typedef std::list<TransportSocketPtr> TransportSocketList;

      protected:
        CallTransport(
                      IMessageQueuePtr queue,
                      ICallTransportDelegatePtr delegate,
                      const IICESocket::TURNServerInfoList &turnServers,
                      const IICESocket::STUNServerInfoList &stunServers
                      );

        CallTransport(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init();

      public:
        ~CallTransport();

        static CallTransportPtr convert(ICallTransportPtr transport);
        static CallTransportPtr convert(ForAccountPtr transport);
        static CallTransportPtr convert(ForCallPtr transport);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark CallTransport => ICallTransport
        #pragma mark

        virtual PUID getID() const {return mID;}
        // (duplicate) virtual CallTransportStates getState() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark CallTransport => ICallTransportForAccount
        #pragma mark

        static CallTransportPtr create(
                                       ICallTransportDelegatePtr delegate,
                                       const IICESocket::TURNServerInfoList &turnServers,
                                       const IICESocket::STUNServerInfoList &stunServers
                                       );

        virtual void shutdown();
        virtual CallTransportStates getState() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark CallTransport => ICallTransportForCall
        #pragma mark

        virtual RecursiveLock &getLock() const;

        virtual void notifyCallCreation(PUID idCall);
        virtual void notifyCallDestruction(PUID idCall);

        virtual void focus(
                           CallPtr call,
                           PUID locationID
                           );
        virtual void loseFocus(PUID callID);

        virtual IICESocketPtr getSocket(SocketTypes type) const;

        virtual void notifyReceivedRTPPacket(
                                             PUID callID,
                                             PUID locationID,
                                             SocketTypes type,
                                             const BYTE *buffer,
                                             size_t bufferLengthInBytes
                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark CallTransport => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark CallTransport => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark CallTransport => ICallTransportAsync
        #pragma mark

        virtual void onStart();
        virtual void onStop();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark CallTransport => friend TransportSocket
        #pragma mark

        int sendRTPPacket(PUID socketID, const void *data, int len);

      protected:
        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug() const;

        bool isPending() const {return CallTransportState_Pending == mCurrentState;}
        bool isReady() const {return CallTransportState_Ready == mCurrentState;}
        bool isShuttingDown() const {return CallTransportState_ShuttingDown == mCurrentState;}
        bool isShutdown() const {return CallTransportState_ShuttingDown == mCurrentState;}

        void start();
        void stop();

        void cancel();
        void step();
        void setState(CallTransportStates state);

        void fixSockets();
        bool cleanObsoleteSockets();

      public:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark CallTransport::TransportSocket

        class TransportSocket : public MessageQueueAssociator,
                                public IICESocketDelegate,
                                public webrtc::Transport
        {
        public:
          friend class CallTransport;

        protected:
          TransportSocket(
                          IMessageQueuePtr queue,
                          CallTransportPtr outer
                          );

          void init(
                    const IICESocket::TURNServerInfoList &turnServers,
                    const IICESocket::STUNServerInfoList &stunServers
                    );

        public:
          ~TransportSocket();

          static ElementPtr toDebug(TransportSocketPtr socket);

          //-------------------------------------------------------------------
          #pragma mark CallTransport::TransportSocket => friend CallTransport

          static TransportSocketPtr create(
                                           IMessageQueuePtr queue,
                                           CallTransportPtr outer,
                                           const IICESocket::TURNServerInfoList &turnServers,
                                           const IICESocket::STUNServerInfoList &stunServers
                                           );

          PUID getID() const {return mID;}

          IICESocketPtr getRTPSocket() const {return mRTPSocket;}

          void shutdown();
          bool isReady() const;
          bool isShutdown() const;

          //-------------------------------------------------------------------
          #pragma mark CallTransport::TransportSocket => IICESocketDelegate

          virtual void onICESocketStateChanged(
                                               IICESocketPtr socket,
                                               ICESocketStates state
                                               );
          virtual void onICESocketCandidatesChanged(IICESocketPtr socket);

          //-------------------------------------------------------------------
          #pragma mark CallTransport::TransportSocket => webrtc::Transport

          virtual int SendPacket(int channel, const void *data, int len);
          virtual int SendRTCPPacket(int channel, const void *data, int len);

        protected:
          //-------------------------------------------------------------------
          #pragma mark CallTransport::TransportSocket => (internal)

          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          void cancel();

        protected:
          //-------------------------------------------------------------------
          #pragma mark CallTransport::TransportSocket => (data)
          PUID mID;
          TransportSocketWeakPtr mThisWeak;
          CallTransportWeakPtr mOuter;

          IICESocketPtr mRTPSocket;
        };

      protected:
        //-------------------------------------------------------------------
        #pragma mark CallTransport => (data)
        PUID mID;
        mutable RecursiveLock mLock;
        CallTransportWeakPtr mThisWeak;
        CallTransportPtr mGracefulShutdownReference;

        ICallTransportDelegatePtr mDelegate;

        IICESocket::TURNServerInfoList mTURNServers;
        IICESocket::STUNServerInfoList mSTUNServers;

        CallTransportStates mCurrentState;

        ULONG mTotalCalls;
        TimerPtr mSocketCleanupTimer;

        bool mStarted;
        UseCallWeakPtr mFocus;
        PUID mFocusCallID;
        PUID mFocusLocationID;
        bool mHasAudio;
        bool mHasVideo;
        ULONG mBlockUntilStartStopCompleted;

        TransportSocketPtr mAudioSocket;
        TransportSocketPtr mVideoSocket;

        PUID mAudioSocketID;
        PUID mVideoSocketID;

        TransportSocketList mObsoleteSockets;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportFactory
      #pragma mark

      interaction ICallTransportFactory
      {
        static ICallTransportFactory &singleton();

        virtual CallTransportPtr create(
                                        ICallTransportDelegatePtr delegate,
                                        const IICESocket::TURNServerInfoList &turnServers,
                                        const IICESocket::STUNServerInfoList &stunServers
                                        );
      };
      
    }
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::core::internal::ICallTransportDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::internal::ICallTransportPtr, ICallTransportPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::internal::ICallTransport::CallTransportStates, CallTransportStates)
ZS_DECLARE_PROXY_METHOD_2(onCallTransportStateChanged, ICallTransportPtr, CallTransportStates)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::core::internal::ICallTransportAsync)
ZS_DECLARE_PROXY_METHOD_0(onStart)
ZS_DECLARE_PROXY_METHOD_0(onStop)
ZS_DECLARE_PROXY_END()
