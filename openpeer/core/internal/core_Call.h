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
#include <openpeer/core/internal/core_thread.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/ICall.h>

#include <openpeer/services/IICESocket.h>
#include <openpeer/services/IICESocketSession.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/Timer.h>

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      interaction IAccountForCall;
      interaction ICallTransportForCall;
      interaction IContactForCall;
      interaction IConversationThreadForCall;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallForConversationThread
      #pragma mark

      interaction ICallForConversationThread
      {
        ZS_DECLARE_TYPEDEF_PTR(ICallForConversationThread, ForConversationThread)

        typedef thread::DialogPtr DialogPtr;

        static ElementPtr toDebug(ForConversationThreadPtr call);

        static ForConversationThreadPtr createForIncomingCall(
                                                              ConversationThreadPtr inConversationThread,
                                                              ContactPtr callerContact,
                                                              const DialogPtr &remoteDialog
                                                              );

        virtual String getCallID() const = 0;

        virtual ContactPtr getCaller(bool ignored = true) const = 0;
        virtual ContactPtr getCallee(bool ignored = true) const = 0;

        virtual DialogPtr getDialog() const = 0;

        virtual void notifyConversationThreadUpdated() = 0;

      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallForCallTransport
      #pragma mark

      interaction ICallForCallTransport
      {
        ZS_DECLARE_TYPEDEF_PTR(ICallForCallTransport, ForCallTransport)

        enum SocketTypes
        {
          SocketType_Audio,
          SocketType_Video,
        };

        static const char *toString(SocketTypes type);

        static ElementPtr toDebug(ForCallTransportPtr call);

        virtual PUID getID() const = 0;

        virtual bool hasAudio() const = 0;
        virtual bool hasVideo() const = 0;

        virtual void notifyLostFocus() = 0;

        virtual bool sendRTPPacket(
                                   PUID toLocationID,
                                   SocketTypes type,
                                   const BYTE *packet,
                                   size_t packetLengthInBytes
                                   ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallAsync
      #pragma mark

      interaction ICallAsync
      {
        virtual void onSetFocus(bool on) = 0;
        virtual void onHangup() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call
      #pragma mark

      class Call  : public Noop,
                    // public zsLib::MessageQueueAssociator,  // we do NOT want to inherit from an queue associator object will use multiple queues
                    public ICall,
                    public ICallForConversationThread,
                    public ICallForCallTransport,
                    public IWakeDelegate,
                    public ICallAsync,
                    public IICESocketDelegate,
                    public ITimerDelegate
      {
      public:
        friend interaction ICallFactory;
        friend interaction ICall;
        friend interaction ICallForConversationThread;
        friend interaction ICallForCallTransport;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForCall, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(ICallTransportForCall, UseCallTransport)
        ZS_DECLARE_TYPEDEF_PTR(IContactForCall, UseContact)
        ZS_DECLARE_TYPEDEF_PTR(IConversationThreadForCall, UseConversationThread)

        struct Exceptions
        {
          ZS_DECLARE_CUSTOM_EXCEPTION(StepFailure)
          ZS_DECLARE_CUSTOM_EXCEPTION(IllegalState)
          ZS_DECLARE_CUSTOM_EXCEPTION(CallClosed)
        };

        typedef thread::DialogPtr DialogPtr;
        typedef IConversationThreadForCall::LocationDialogMap LocationDialogMap;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call::ICallLocation
        #pragma mark

        interaction ICallLocation
        {
          enum CallLocationStates
          {
            CallLocationState_Pending = 0,
            CallLocationState_Ready = 1,
            CallLocationState_Closed = 2,
          };

          static const char *toString(CallLocationStates state);
        };
        typedef ICallLocation::CallLocationStates CallLocationStates;

        ZS_DECLARE_CLASS_PTR(CallLocation)

        typedef std::list<CallLocationPtr> CallLocationList;
        friend class CallLocation;

      protected:
        Call(
             AccountPtr account,
             ConversationThreadPtr conversationThread,
             ICallDelegatePtr delegate,
             bool hasAudio,
             bool hasVideo,
             const char *callID,
             ILocationPtr selfLocation,
             IPeerFilesPtr peerFiles
             );

        Call(Noop) :
          Noop(true),
          mLock(SharedRecursiveLock::create()),
          mStepLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~Call();

        static CallPtr convert(ICallPtr call);
        static CallPtr convert(ForConversationThreadPtr call);
        static CallPtr convert(ForCallTransportPtr call);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => ICall
        #pragma mark

        static ElementPtr toDebug(ICallPtr call);

        static CallPtr placeCall(
                                 ConversationThreadPtr conversationThread,
                                 IContactPtr toContact,
                                 bool includeAudio,
                                 bool includeVideo
                                 );

        virtual String getCallID() const;

        virtual IConversationThreadPtr getConversationThread() const;

        virtual IContactPtr getCaller() const;
        virtual IContactPtr getCallee() const;

        virtual bool hasAudio() const;
        virtual bool hasVideo() const;

        virtual CallStates getState() const;
        virtual CallClosedReasons getClosedReason() const;

        virtual Time getcreationTime() const;
        virtual Time getRingTime() const;
        virtual Time getAnswerTime() const;
        virtual Time getClosedTime() const;

        virtual void ring();
        virtual void answer();
        virtual void hold(bool hold);
        virtual void hangup(CallClosedReasons reason = CallClosedReason_User);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => ICallForCallTransport
        #pragma mark

        virtual PUID getID() const {return mID;}

        // (duplicate) virtual bool hasAudio() const;
        // (duplicate) virtual bool hasVideo() const;

        virtual void notifyLostFocus();

        virtual bool sendRTPPacket(
                                   PUID toLocationID,
                                   SocketTypes type,
                                   const BYTE *packet,
                                   size_t packetLengthInBytes
                                   );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => ICallForConversationThread
        #pragma mark

        static CallPtr createForIncomingCall(
                                             ConversationThreadPtr inConversationThread,
                                             ContactPtr callerContact,
                                             const DialogPtr &remoteDialog
                                             );

        // (duplicate) virtual String getCallID() const;

        virtual DialogPtr getDialog() const;

        virtual ContactPtr getCaller(bool) const;
        virtual ContactPtr getCallee(bool) const;

        virtual void notifyConversationThreadUpdated();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => IWakeDelegate
        #pragma mark

        virtual void onWake() {step();}

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => ICallAsync
        #pragma mark

        virtual void onSetFocus(bool on);
        virtual void onHangup() {cancel();}

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => IICESocketDelegate
        #pragma mark

        virtual void onICESocketStateChanged(
                                             IICESocketPtr socket,
                                             ICESocketStates state
                                             );
        virtual void onICESocketCandidatesChanged(IICESocketPtr socket);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => ITimerDelegate
        #pragma mark
        virtual void onTimer(TimerPtr timer);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => friend Call::CallLocation
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        // (duplicate) const SharedRecursiveLock &getLock() const;

        virtual void notifyReceivedRTPPacket(
                                             PUID locationID,
                                             SocketTypes type,
                                             const BYTE *buffer,
                                             size_t bufferLengthInBytes
                                             );

        void notifyStateChanged(
                                CallLocationPtr location,
                                CallLocationStates state
                                );

        bool isIncoming() const {return mIncomingCall;}

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => (internal)
        #pragma mark

        const SharedRecursiveLock &getLock() const;

        IMessageQueuePtr getQueue() const {return mQueue;}
        IMessageQueuePtr getMediaQueue() const {return mMediaQueue;}

      private:
        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug(bool callData) const;

        bool isShuttingdown() const;
        bool isShutdown() const;

        void cancel();

        void checkState(
                        CallStates state,
                        bool isLocal,
                        bool *callClosedNoThrow = NULL
                        ) const throw (
                                       Exceptions::IllegalState,
                                       Exceptions::CallClosed
                                       );

        void checkLegalWhenNotPicked() const throw (Exceptions::IllegalState);
        void checkLegalWhenPicked(
                                  CallStates state,
                                  bool isLocal
                                  ) const throw (Exceptions::IllegalState);

        bool isLockedToAnotherLocation(const DialogPtr &remoteDialog) const;

        bool stepIsMediaReady() throw (Exceptions::StepFailure);

        bool stepPrepareCallFirstTime() throw (Exceptions::StepFailure);

        bool stepPrepareCallLocations(
                                      const LocationDialogMap locationDialogMap,
                                      CallLocationList &outLocationsToClose
                                      ) throw (Exceptions::CallClosed);

        bool stepVerifyCallState() throw (
                                          Exceptions::IllegalState,
                                          Exceptions::CallClosed
                                          );

        bool stepTryToPickALocation(CallLocationList &outLocationsToClose);

        bool stepHandlePickedLocation() throw (Exceptions::IllegalState);

        bool stepFixCallInProgressStates();

        bool stepFixCandidates();

        bool stepCloseLocations(CallLocationList &locationsToClose);

        void step();
        void setCurrentState(
                             CallStates state,
                             bool forceOverride = false
                             );

        void updateDialog();

        void setClosedReason(CallClosedReasons reason);

        IICESocketSubscriptionPtr findSubscription(
                                                   IICESocketPtr socket,
                                                   bool &outFound,
                                                   SocketTypes *outType = NULL,
                                                   bool *outIsRTP = NULL
                                                   );
        bool placeCallWithConversationThread();

      public:

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call::CallLocation
        #pragma mark

        class CallLocation : public ICallLocation,
                             public services::IICESocketSessionDelegate,
                             public IWakeDelegate
        {
        public:

        protected:
          CallLocation(
                       IMessageQueuePtr queue,
                       IMessageQueuePtr mediaQueue,
                       CallPtr outer,
                       const char *locationID,
                       const DialogPtr &remoteDialog,
                       IICESocketPtr audioSocket,
                       IICESocketPtr videoSocket
                       );

          void init(UseCallTransportPtr tranasport);

        public:
          ~CallLocation();

          static ElementPtr toDebug(
                                    CallLocationPtr location,
                                    bool normal,
                                    bool media
                                    );

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Call::CallLocation => friend Call
          #pragma mark

          static CallLocationPtr create(
                                        IMessageQueuePtr queue,
                                        IMessageQueuePtr mediaQueue,
                                        CallPtr outer,
                                        UseCallTransportPtr transport,
                                        const char *locationID,
                                        const DialogPtr &remoteDialog,
                                        IICESocketPtr audioSocket,
                                        IICESocketPtr videoSocket
                                        );

          PUID getID() const {return mID;}
          const String &getLocationID() const {return mLocationID;}

          void close();

          CallLocationStates getState() const;
          DialogPtr getRemoteDialog() const;

          void updateRemoteDialog(const DialogPtr &remoteDialog);

          bool sendRTPPacket(
                             SocketTypes type,
                             const BYTE *packet,
                             size_t packetLengthInBytes
                             );

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Call::CallLocation => IICESocketSessionDelegate
          #pragma mark

          virtual void onICESocketSessionStateChanged(
                                                      IICESocketSessionPtr session,
                                                      ICESocketSessionStates state
                                                      );

          virtual void onICESocketSessionNominationChanged(IICESocketSessionPtr session);

          virtual void handleICESocketSessionReceivedPacket(
                                                            IICESocketSessionPtr session,
                                                            const BYTE *buffer,
                                                            size_t bufferLengthInBytes
                                                            );

          virtual bool handleICESocketSessionReceivedSTUNPacket(
                                                                IICESocketSessionPtr session,
                                                                STUNPacketPtr stun,
                                                                const String &localUsernameFrag,
                                                                const String &remoteUsernameFrag
                                                                );

          virtual void onICESocketSessionWriteReady(IICESocketSessionPtr session);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Call::CallLocation => IWakeDelegate
          #pragma mark

          virtual void onWake() {step();}

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Call::CallLocation => (internal)
          #pragma mark

          Log::Params log(const char *message) const;
          virtual ElementPtr toDebug(bool normal, bool media) const;

          bool isPending() const  {return CallLocationState_Pending == mCurrentState.get();}
          bool isReady() const    {return CallLocationState_Ready == mCurrentState.get();}
          bool isClosed() const   {return CallLocationState_Closed == mCurrentState.get();}

          bool hasAudio() const   {return mHasAudio;}
          bool hasVideo() const   {return mHasVideo;}

          IMessageQueuePtr getQueue() const {return mQueue;}
          IMessageQueuePtr getMediaQueue() const {return mMediaQueue;}

          void cancel();
          void step();
          void setState(CallLocationStates state);

          IICESocketSessionPtr findSession(
                                           IICESocketSessionPtr session,
                                           bool &outFound,
                                           SocketTypes *outType = NULL,
                                           bool *outIsRTP = NULL
                                           );

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Call::CallLocation => (data)
          #pragma mark

          AutoPUID mID;
          SharedRecursiveLock mLock;

          CallLocationWeakPtr mThisWeakNoQueue;

          IWakeDelegatePtr mThisWakeDelegate;
          IICESocketSessionDelegatePtr mThisICESocketSessionDelegate;

          LockedValue<CallLocationStates> mCurrentState;

          CallWeakPtr mOuter;
          IMessageQueuePtr mQueue;
          IMessageQueuePtr mMediaQueue;

          String mLocationID;
          bool mHasAudio;
          bool mHasVideo;

          IICESocketPtr mAudioSocket;
          IICESocketPtr mVideoSocket;
          
          //-------------------------------------------------------------------
          // variables protected with object lock
          DialogPtr mRemoteDialog;
          AutoBool mChangedRemoteDialog;

          //-------------------------------------------------------------------
          // variables protected with independent locks

          LockedValue<IICESocketSessionPtr, true> mAudioRTPSocketSession;
          LockedValue<IICESocketSessionPtr, true> mVideoRTPSocketSession;
        };

      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Call => (data)
        #pragma mark

        AutoPUID mID;
        SharedRecursiveLock mLock;
        SharedRecursiveLock mStepLock;

        CallWeakPtr mThisWeakNoQueue;
        CallPtr mGracefulShutdownReference;

        IWakeDelegatePtr mThisWakeDelegate;
        ICallAsyncPtr mThisCallAsyncNormalQueue;
        ICallAsyncPtr mThisCallAsyncMediaQueue;
        IICESocketDelegatePtr mThisICESocketDelegate;
        ITimerDelegatePtr mThisTimerDelegate;

        IMessageQueuePtr mQueue;
        IMessageQueuePtr mMediaQueue;

        ICallDelegatePtr mDelegate;

        String mCallID;

        bool mHasAudio;
        bool mHasVideo;

        bool mIncomingCall;
        bool mIncomingNotifiedThreadOfPreparing;

        UseAccountWeakPtr mAccount;
        UseConversationThreadWeakPtr mConversationThread;
        UseCallTransportPtr mTransport;

        UseContactPtr mCaller;
        UseContactPtr mCallee;

        ILocationPtr mSelfLocation;
        IPeerFilesPtr mPeerFiles;

        //---------------------------------------------------------------------
        // variables protected with object lock

        CallStates mCurrentState;
        CallClosedReasons mClosedReason;

        typedef String LocationID;
        typedef std::map<LocationID, CallLocationPtr> CallLocationMap;
        CallLocationMap mCallLocations;

        TimerPtr mPeerAliveTimer;

        TimerPtr mCleanupTimer;
        bool mPlaceCall;

        DialogPtr mDialog;

        bool mRingCalled;
        bool mAnswerCalled;
        bool mLocalOnHold;

        Time mCreationTime;
        Time mRingTime;
        Time mAnswerTime;
        Time mClosedTime;
        Time mFirstClosedRemoteCallTime;

        TimerPtr mFirstClosedRemoteCallTimer;

        String mAudioCandidateVersion;
        String mVideoCandidateVersion;

        //---------------------------------------------------------------------
        // variables protected with independent locks

        LockedValue<TimerPtr> mMediaCheckTimer;

        LockedValue<IICESocketPtr> mAudioSocket;
        LockedValue<IICESocketPtr> mVideoSocket;

        LockedValue<IICESocketSubscriptionPtr, true> mAudioRTPSocketSubscription;
        LockedValue<IICESocketSubscriptionPtr, true> mVideoRTPSocketSubscription;

        LockedValue<bool> mMediaHolding;
        LockedValue<CallLocationPtr> mPickedLocation;
        LockedValue<CallLocationPtr> mEarlyLocation;

        LockedValue<bool> mNotifiedCallTransportDestroyed;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallFactory
      #pragma mark

      interaction ICallFactory
      {
        static ICallFactory &singleton();

        virtual CallPtr placeCall(
                                  ConversationThreadPtr conversationThread,
                                  IContactPtr toContact,
                                  bool includeAudio,
                                  bool includeVideo
                                  );

        virtual CallPtr createForIncomingCall(
                                              ConversationThreadPtr inConversationThread,
                                              ContactPtr callerContact,
                                              const DialogPtr &remoteDialog
                                              );
      };

      class CallFactory : public IFactory<ICallFactory> {};
    }
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::core::internal::ICallAsync)
ZS_DECLARE_PROXY_METHOD_1(onSetFocus, bool)
ZS_DECLARE_PROXY_METHOD_0(onHangup)
ZS_DECLARE_PROXY_END()
