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


#include <openpeer/core/internal/core_CallTransport.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Call.h>
#include <openpeer/core/internal/core_MediaEngine.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Stringize.h>
#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#define OPENPEER_CALLTRANSPORT_CLOSE_UNUSED_SOCKETS_AFTER_IN_SECONDS (90)

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }
namespace openpeer { namespace core { ZS_DECLARE_FORWARD_SUBSYSTEM(openpeer_media) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(IMediaEngineForCallTransport, UseMediaEngine)

      typedef IStackForInternal UseStack;
      typedef ICallTransportForAccount::ForAccountPtr ForAccountPtr;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static zsLib::Subsystem &mediaSubsystem()
      {
        return ZS_GET_OTHER_SUBSYSTEM(openpeer::core, openpeer_media);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransport
      #pragma mark

      //-----------------------------------------------------------------------
      const char *ICallTransport::toString(CallTransportStates state)
      {
        switch (state) {
          case CallTransportState_Pending:      return "Pending";
          case CallTransportState_Ready:        return "Ready";
          case CallTransportState_ShuttingDown: return "Shutting down";
          case CallTransportState_Shutdown:     return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      ElementPtr ICallTransport::toDebug(ICallTransportPtr transport)
      {
        if (!transport) return ElementPtr();
        return CallTransport::convert(transport)->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportForCall
      #pragma mark

      const char *ICallTransportForCall::toString(SocketTypes type)
      {
        switch (type) {
          case SocketType_Audio: return "audio";
          case SocketType_Video: return "video";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ForAccountPtr ICallTransportForAccount::create(
                                                     ICallTransportDelegatePtr delegate,
                                                     const IICESocket::TURNServerInfoList &turnServers,
                                                     const IICESocket::STUNServerInfoList &stunServers
                                                     )
      {
        return ICallTransportFactory::singleton().create(delegate, turnServers, stunServers);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport
      #pragma mark

      //-----------------------------------------------------------------------
      CallTransport::CallTransport(
                                   IMessageQueuePtr queue,
                                   ICallTransportDelegatePtr delegate,
                                   const IICESocket::TURNServerInfoList &turnServers,
                                   const IICESocket::STUNServerInfoList &stunServers
                                   ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDelegate(ICallTransportDelegateProxy::createWeak(queue, delegate)),
        mTURNServers(turnServers),
        mSTUNServers(stunServers),
        mCurrentState(CallTransportState_Pending),
        mTotalCalls(0),
        mFocusCallID(0),
        mFocusLocationID(0),
        mStarted(false),
        mHasAudio(false),
        mHasVideo(false),
        mBlockUntilStartStopCompleted(0)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void CallTransport::init()
      {
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      CallTransport::~CallTransport()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      CallTransportPtr CallTransport::convert(ICallTransportPtr transport)
      {
        return ZS_DYNAMIC_PTR_CAST(CallTransport, transport);
      }

      //-----------------------------------------------------------------------
      CallTransportPtr CallTransport::convert(ForAccountPtr transport)
      {
        return ZS_DYNAMIC_PTR_CAST(CallTransport, transport);
      }

      //-----------------------------------------------------------------------
      CallTransportPtr CallTransport::convert(ForCallPtr transport)
      {
        return ZS_DYNAMIC_PTR_CAST(CallTransport, transport);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      CallTransportPtr CallTransport::create(
                                             ICallTransportDelegatePtr delegate,
                                             const IICESocket::TURNServerInfoList &turnServers,
                                             const IICESocket::STUNServerInfoList &stunServers
                                             )
      {
        CallTransportPtr pThis(new CallTransport(UseStack::queueMedia(), delegate, turnServers, stunServers));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void CallTransport::shutdown()
      {
        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      ICallTransport::CallTransportStates CallTransport::getState() const
      {
        AutoRecursiveLock lock(*this);
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport => ICallTransportForCall
      #pragma mark

      //-----------------------------------------------------------------------
      void CallTransport::notifyCallCreation(PUID idCall)
      {
        AutoRecursiveLock lock(*this);
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("told about a new call during call transport shutdown"))
          return;
        }

        ++mTotalCalls;
        fixSockets();
      }

      //-----------------------------------------------------------------------
      void CallTransport::notifyCallDestruction(PUID idCall)
      {
        AutoRecursiveLock lock(*this);
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_DEBUG(log("ignoring call destructionn during shutdown"))
          return;
        }

        ZS_THROW_BAD_STATE_IF(mTotalCalls < 1)

        --mTotalCalls;

        if (mSocketCleanupTimer) {
          mSocketCleanupTimer->cancel();
          mSocketCleanupTimer.reset();
        }

        mSocketCleanupTimer = Timer::create(mThisWeak.lock(), Seconds(OPENPEER_CALLTRANSPORT_CLOSE_UNUSED_SOCKETS_AFTER_IN_SECONDS), false);
      }

      //-----------------------------------------------------------------------
      void CallTransport::focus(
                                CallPtr inCall,
                                PUID locationID
                                )
      {
        UseCallPtr call = inCall;
        UseCallPtr oldFocus;

        // scope: do not want to call notifyLostFocus from inside a lock (possible deadlock)
        {
          AutoRecursiveLock lock(*this);

          if (call) {
            ZS_THROW_BAD_STATE_IF(mTotalCalls < 1)

            if (mFocusCallID != call->getID()) {
              // focus has changed...

              oldFocus = mFocus.lock();

              ZS_LOG_DEBUG(log("focused call ID changed") + ZS_PARAM("was", mFocusCallID) + ZS_PARAM("now", call->getID()) + ZS_PARAM("block count", mBlockUntilStartStopCompleted))

              mFocus = call;
              mFocusCallID = call->getID();
              mFocusLocationID = locationID;

              // must restart the media
              ++mBlockUntilStartStopCompleted;
              ICallTransportAsyncProxy::create(mThisWeak.lock())->onStart();
            } else {
              if (locationID != mFocusLocationID) {
                ZS_LOG_DEBUG(log("focused location ID changed") + ZS_PARAM("was", mFocusLocationID) + ZS_PARAM("now", locationID) + ZS_PARAM("block count", mBlockUntilStartStopCompleted))

                // must restart the media
                ++mBlockUntilStartStopCompleted;
                ICallTransportAsyncProxy::create(mThisWeak.lock())->onStart();
                mFocusLocationID = locationID;
              }
            }
          } else {
            ZS_LOG_DEBUG(log("no focus at all") + ZS_PARAM("was", mFocusLocationID) + ZS_PARAM("now", locationID) + ZS_PARAM("block count", mBlockUntilStartStopCompleted))
            mFocusCallID = 0;
            mFocusLocationID = 0;
            ++mBlockUntilStartStopCompleted;
            ICallTransportAsyncProxy::create(mThisWeak.lock())->onStop();
          }
        }

        if (oldFocus) {
          ZS_LOG_DEBUG(log("telling old focus to go on hold..."))
          oldFocus->notifyLostFocus();
        }
      }

      //-----------------------------------------------------------------------
      void CallTransport::loseFocus(PUID callID)
      {
        AutoRecursiveLock lock(*this);

        if (0 == mFocusCallID) {
          ZS_LOG_DEBUG(log("no call has focus (ignoring request to lose focus)") + ZS_PARAM("lose focus ID", callID))
          return;
        }

        if (callID != mFocusCallID) {
          ZS_LOG_WARNING(Detail, log("did not have focus on current call to lose") + ZS_PARAM("focusing on ID", mFocusCallID) + ZS_PARAM("lose focus ID", callID))
          return;
        }

        focus(CallPtr(), 0);
      }

      //-----------------------------------------------------------------------
      IICESocketPtr CallTransport::getSocket(SocketTypes type) const
      {
        AutoRecursiveLock lock(*this);

        switch (type) {
          case SocketType_Audio:  return mAudioSocket ? mAudioSocket->getRTPSocket() : IICESocketPtr();
          case SocketType_Video:  return mVideoSocket ? mVideoSocket->getRTPSocket() : IICESocketPtr();
        }

        ZS_THROW_INVALID_ASSUMPTION(log("what type of socket is this?"))
        return IICESocketPtr();
      }

      //-----------------------------------------------------------------------
      void CallTransport::notifyReceivedRTPPacket(
                                                  PUID callID,
                                                  PUID locationID,
                                                  SocketTypes type,
                                                  const BYTE *buffer,
                                                  size_t bufferLengthInBytes
                                                  )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!buffer)

        if (bufferLengthInBytes < (sizeof(BYTE)*2)) return;

        BYTE payloadType = buffer[1];
        BYTE filterType = (payloadType & 0x7F);
        bool isRTP = ((filterType < 64) || (filterType > 96));

        ZS_LOG_SUBSYSTEM_TRACE(mediaSubsystem(), log("notified of packet") + ZS_PARAM("type", (isRTP ? "RTP" : "RTCP")) + ZS_PARAM("from call ID", callID) + ZS_PARAM("from location ID", locationID) + ZS_PARAM("socket type", ICallTransportForCall::toString(type)) + ZS_PARAM("payload type", payloadType) + ZS_PARAM("length", bufferLengthInBytes))

        // scope - get locked variable
        {
          AutoRecursiveLock lock(*this);

          if (0 != mBlockUntilStartStopCompleted) {
            ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Debug, log("ignoring RTP/RTCP packet as media is blocked until the start/stop routine complete") + ZS_PARAM("blocked count", mBlockUntilStartStopCompleted))
            return;
          }

          if (!mStarted) {
            ZS_LOG_TRACE(log("ignoring RTP/RTCP packet as media is not started"))
            return;
          }

          if ((callID != mFocusCallID) ||
              (locationID != mFocusLocationID)) {
            ZS_LOG_SUBSYSTEM_TRACE(mediaSubsystem(), log("ignoring RTP/RTCP packet as not from call/location ID in focus") + ZS_PARAM("focus call ID", mFocusCallID) + ZS_PARAM("focus location ID", mFocusLocationID))
            return;
          }

          if ((SocketType_Audio == type) &&
              (!mHasAudio)) {
            ZS_LOG_SUBSYSTEM_TRACE(mediaSubsystem(), log("ignoring RTP/RTCP packet as audio was not started for this call"))
            return;
          }

          if ((SocketType_Video == type) &&
              (!mHasVideo)) {
            ZS_LOG_SUBSYSTEM_TRACE(mediaSubsystem(), log("ignoring RTP/RTCP packet as video was not started for this call"))
            return;
          }
        }

        UseMediaEnginePtr engine = UseMediaEngine::singleton();
        if (!engine) return;

        if (SocketType_Audio == type) {
          if (isRTP) {
            engine->receivedVoiceRTPPacket(buffer, bufferLengthInBytes);
          } else {
            engine->receivedVoiceRTCPPacket(buffer, bufferLengthInBytes);
          }
        } else {
          if (isRTP) {
            engine->receivedVideoRTPPacket(buffer, bufferLengthInBytes);
          } else {
            engine->receivedVideoRTCPPacket(buffer, bufferLengthInBytes);
          }
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------

      //-----------------------------------------------------------------------
      void CallTransport::onTimer(TimerPtr timer)
      {
        ZS_LOG_DEBUG(log("on timer") + ZS_PARAM("timer id", timer->getID()))

        AutoRecursiveLock lock(*this);
        if (timer != mSocketCleanupTimer) {
          ZS_LOG_WARNING(Detail, log("notification from obsolete timer") + ZS_PARAM("timer id", timer->getID()))
          return;
        }

        mSocketCleanupTimer->cancel();
        mSocketCleanupTimer.reset();

        fixSockets();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void CallTransport::onWake()
      {
        ZS_LOG_DEBUG(log("on wake"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport => ICallTransportAsync
      #pragma mark

      //-----------------------------------------------------------------------
      void CallTransport::onStart()
      {
        ZS_LOG_DEBUG(log("on start"))
        start();
      }

      //-----------------------------------------------------------------------
      void CallTransport::onStop()
      {
        ZS_LOG_DEBUG(log("on stop"))
        stop();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport => friend TransportSocket
      #pragma mark

      //-----------------------------------------------------------------------
      int CallTransport::sendRTPPacket(PUID socketID, const void *data, int len)
      {
        UseCallPtr call;
        PUID locationID = 0;

        ICallForCallTransport::SocketTypes type = ICallForCallTransport::SocketType_Audio;

        // scope - find the call
        {
          AutoRecursiveLock lock(*this);

          if (0 != mBlockUntilStartStopCompleted) {
            ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Debug, log("ignoring request to send RTP packet as media is blocked until the start/stop routine complete") + ZS_PARAM("blocked count", mBlockUntilStartStopCompleted))
            return 0;
          }

          if ((0 == mFocusCallID) ||
              (!mStarted)) {
            ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("unable to send RTP packet media isn't start or there is no focus object") + ZS_PARAM("started", mStarted) + ZS_PARAM("focus ID", mFocusCallID))
            return 0;
          }

          call = mFocus.lock();
          locationID = mFocusLocationID;
          if (!call) {
            ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("unable to send RTP packet as focused call object is gone"))
            return 0;
          }

          if (socketID == mAudioSocketID) {
            type = ICallForCallTransport::SocketType_Audio;
          } else {
            type = ICallForCallTransport::SocketType_Video;
          }
        }

        return (call->sendRTPPacket(locationID, type, (const BYTE *)data, (size_t)len) ? len : 0);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params CallTransport::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::CallTransport");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr CallTransport::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::CallTransport");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "state", ICallTransport::toString(mCurrentState));
        UseServicesHelper::debugAppend(resultEl, "turn servers", mTURNServers.size());
        UseServicesHelper::debugAppend(resultEl, "stun", mSTUNServers.size());
        UseServicesHelper::debugAppend(resultEl, "total calls", mTotalCalls);
        UseServicesHelper::debugAppend(resultEl, "socket cleanup timer", (bool)mSocketCleanupTimer);
        UseServicesHelper::debugAppend(resultEl, "started", mStarted);
        UseServicesHelper::debugAppend(resultEl, UseCall::toDebug(mFocus.lock()));
        UseServicesHelper::debugAppend(resultEl, "focus call id", mFocusCallID);
        UseServicesHelper::debugAppend(resultEl, "focus location id", mFocusLocationID);
        UseServicesHelper::debugAppend(resultEl, "has audio", mHasAudio);
        UseServicesHelper::debugAppend(resultEl, "has video", mHasAudio);
        UseServicesHelper::debugAppend(resultEl, "blocked until", mBlockUntilStartStopCompleted);
        UseServicesHelper::debugAppend(resultEl, "audio", TransportSocket::toDebug(mAudioSocket));
        UseServicesHelper::debugAppend(resultEl, "video", TransportSocket::toDebug(mVideoSocket));
        UseServicesHelper::debugAppend(resultEl, "audio socket id", mAudioSocketID);
        UseServicesHelper::debugAppend(resultEl, "video socket id", mVideoSocketID);
        UseServicesHelper::debugAppend(resultEl, "obsolete sockets", mObsoleteSockets.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      class CallTransport_EnsureDecrementOnReturn : public SharedRecursiveLock
      {
      public:
        typedef zsLib::ULONG size_type;
        typedef zsLib::RecursiveLock RecursiveLock;

        CallTransport_EnsureDecrementOnReturn(
                                              const SharedRecursiveLock &lock,
                                              size_type &refCount
                                              ) :
          SharedRecursiveLock(lock),
          mRefCount(refCount)
        {
        }

        ~CallTransport_EnsureDecrementOnReturn()
        {
          AutoRecursiveLock lock(*this);
          --mRefCount;
        }

      private:
        size_type &mRefCount;
      };

      //-----------------------------------------------------------------------
      void CallTransport::start()
      {
        CallTransport_EnsureDecrementOnReturn(*this, mBlockUntilStartStopCompleted);

        bool hasAudio = false;
        bool hasVideo = false;
        TransportSocketPtr audioSocket;
        TransportSocketPtr videoSocket;

        // scope: media engine can't be called from within our lock or it might deadlock
        {
          AutoRecursiveLock lock(*this);

          if (mStarted) {
            ZS_LOG_DEBUG(log("must stop existing media before starting a new focus") + ZS_PARAM("block count", mBlockUntilStartStopCompleted))
            ++mBlockUntilStartStopCompleted;  // extra count required because we are calling manually
            stop();
          }

          if (0 == mFocusCallID) {
            ZS_LOG_DEBUG(log("no need to start audio as there is no call in focus"))
            return;
          }

          UseCallPtr call = mFocus.lock();
          if (!call) {
            ZS_LOG_WARNING(Detail, log("call in focus is now gone thus cannot start media engine"))
            return;
          }

          hasAudio = mHasAudio = call->hasAudio();
          hasVideo = mHasVideo = call->hasVideo();
          mStarted = true;

          audioSocket = mAudioSocket;
          videoSocket = mVideoSocket;

          ZS_LOG_DETAIL(log("starting media engine") + ZS_PARAM("audio", mHasAudio) + ZS_PARAM("video", mHasVideo))
        }

        UseMediaEnginePtr engine = UseMediaEngine::singleton();
        if (!engine) return;

        if (hasAudio) {
          ZS_LOG_DETAIL(log("registering audio media engine transports"))

          engine->registerVoiceExternalTransport(*(audioSocket.get()));
        }
        if (hasVideo) {
          ZS_LOG_DETAIL(log("registering video media engine transports"))
          engine->registerVideoExternalTransport(*(videoSocket.get()));
        }

        {
          AutoRecursiveLock lock(*this);
          if (hasAudio) {
            engine->startVoice();
          }
          if (hasVideo) {
            engine->startVideoChannel();
          }
        }
      }

      //-----------------------------------------------------------------------
      void CallTransport::stop()
      {
        CallTransport_EnsureDecrementOnReturn(*this, mBlockUntilStartStopCompleted);

        bool hasAudio = false;
        bool hasVideo = false;

        // scope: media engine can't be called from within our lock or it might deadlock
        {
          AutoRecursiveLock lock(*this);
          if (!mStarted) return;

          hasAudio = mHasAudio;
          hasVideo = mHasVideo;

          ZS_LOG_DETAIL(log("stopping media engine") + ZS_PARAM("audio", mHasAudio) + ZS_PARAM("video", mHasVideo))

          mStarted = false;
          mHasAudio = false;
          mHasVideo = false;
        }

        UseMediaEnginePtr engine = UseMediaEngine::singleton();
        if (!engine) return;

        if (hasVideo) {
          ZS_LOG_DETAIL(log("stopping media engine video"))
          engine->stopVideoChannel();
        }
        if (hasAudio) {
          ZS_LOG_DETAIL(log("stopping media engine audio"))
          engine->stopVoice();
        }

        if (hasVideo) {
          ZS_LOG_DETAIL(log("deregistering video media engine transport"))
          engine->deregisterVideoExternalTransport();
        }

        if (hasAudio) {
          ZS_LOG_DETAIL(log("deregistering audio media engine transport"))
          engine->deregisterVoiceExternalTransport();
        }
      }

      //-----------------------------------------------------------------------
      void CallTransport::cancel()
      {
        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("cancel called but already shutdown"))
          return;
        }

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(CallTransportState_ShuttingDown);

        mTotalCalls = 0;

        if (mAudioSocket) {
          mObsoleteSockets.push_back(mAudioSocket);
          mAudioSocket.reset();
          mAudioSocketID = 0;
        }
        if (mVideoSocket) {
          mObsoleteSockets.push_back(mVideoSocket);
          mVideoSocket.reset();
          mVideoSocketID = 0;
        }

        cleanObsoleteSockets();

        if (mGracefulShutdownReference) {
          if (!cleanObsoleteSockets()) {
            ZS_LOG_DEBUG(log("waiting for transport sockets to shutdown"))
            return;
          }
        }

        setState(CallTransportState_Shutdown);

        mGracefulShutdownReference.reset();

        mDelegate.reset();

        mObsoleteSockets.clear();
      }

      //-----------------------------------------------------------------------
      void CallTransport::step()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("step called"))

        cleanObsoleteSockets();

        if ((isShuttingDown()) ||
            (isShutdown())) {
          cancel();
          return;
        }

        setState(CallTransportState_Ready);
      }

      //-----------------------------------------------------------------------
      void CallTransport::setState(CallTransportStates state)
      {
        AutoRecursiveLock lock(*this);

        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ZS_PARAM("old state", ICallTransport::toString(mCurrentState)) + ZS_PARAM("new state", ICallTransport::toString(state)))

        mCurrentState = state;

        CallTransportPtr pThis = mThisWeak.lock();
        if (!pThis) {
          ZS_LOG_WARNING(Detail, log("nobody holding reference to this object"))
          return;
        }

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("delegate is not set"))
          return;
        }

        try {
          mDelegate->onCallTransportStateChanged(pThis, state);
        } catch (ICallTransportDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("call transport delegate is gone"))
          mDelegate.reset();
        }
      }

      //-----------------------------------------------------------------------
      void CallTransport::fixSockets()
      {
        if (mTotalCalls > 0) {

          if (mSocketCleanupTimer) {
            mSocketCleanupTimer->cancel();
            mSocketCleanupTimer.reset();
          }

          if (!mAudioSocket) {
            ZS_LOG_DEBUG(log("creating audio sockets"))
            mAudioSocket = TransportSocket::create(getAssociatedMessageQueue(), mThisWeak.lock(), mTURNServers, mSTUNServers);
            mAudioSocketID = mAudioSocket->getID();
          }
          if (!mVideoSocket) {
            ZS_LOG_DEBUG(log("creating video sockets"))
            mVideoSocket = TransportSocket::create(getAssociatedMessageQueue(), mThisWeak.lock(), mTURNServers, mSTUNServers);
            mVideoSocketID = mVideoSocket->getID();
          }
          return;
        }

        if ((!mAudioSocket) && (!mVideoSocket)) {
          ZS_LOG_DEBUG(log("audio/video sockets are already closed (thus nothing to do)"))
          return;
        }

        if (mSocketCleanupTimer) {
          ZS_LOG_DEBUG(log("still have socket cleanup timer thus no need to shutdown sockets (yet)"))
          return;
        }

        if (mAudioSocket) {
          ZS_LOG_DEBUG(log("adding audio socket to obsolete socket list (for cleanup)"))
          mObsoleteSockets.push_back(mAudioSocket);
          mAudioSocketID = 0;
          mAudioSocket.reset();
        }

        if (mVideoSocket) {
          ZS_LOG_DEBUG(log("adding video socket to obsolete socket list (for cleanup)"))
          mObsoleteSockets.push_back(mVideoSocket);
          mVideoSocketID = 0;
          mVideoSocket.reset();
        }

        cleanObsoleteSockets();
      }

      //-----------------------------------------------------------------------
      bool CallTransport::cleanObsoleteSockets()
      {
        for (TransportSocketList::iterator obsoleteIter = mObsoleteSockets.begin(); obsoleteIter != mObsoleteSockets.end();)
        {
          TransportSocketList::iterator current = obsoleteIter;
          ++obsoleteIter;

          TransportSocketPtr socket = (*current);

          socket->shutdown();

          if (socket->isShutdown()) {
            ZS_LOG_DEBUG(log("transport socket is now shutdown") + ZS_PARAM("transport socket ID", socket->getID()))
            mObsoleteSockets.erase(current);
            continue;
          }

          ZS_LOG_DEBUG(log("transport socket is still shutting down") + ZS_PARAM("transport socket ID", socket->getID()))
        }

        return mObsoleteSockets.size() < 1;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport::TransportSocket
      #pragma mark

      //-----------------------------------------------------------------------
      CallTransport::TransportSocket::TransportSocket(
                                                      IMessageQueuePtr queue,
                                                      CallTransportPtr outer
                                                      ) :
        MessageQueueAssociator(queue),
        mID(zsLib::createPUID()),
        mOuter(outer)
      {
      }

      //-----------------------------------------------------------------------
      void CallTransport::TransportSocket::init(
                                                const IICESocket::TURNServerInfoList &turnServers,
                                                const IICESocket::STUNServerInfoList &stunServers
                                                )
      {
        mRTPSocket = IICESocket::create(getAssociatedMessageQueue(), mThisWeak.lock(), turnServers, stunServers, 0, true);
      }

      //-----------------------------------------------------------------------
      CallTransport::TransportSocket::~TransportSocket()
      {
        mThisWeak.reset();
        if (mRTPSocket) {
          mRTPSocket->shutdown();
          mRTPSocket.reset();
        }
      }

      //-----------------------------------------------------------------------
      ElementPtr CallTransport::TransportSocket::toDebug(TransportSocketPtr socket)
      {
        if (!socket) return ElementPtr();
        return socket->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport::TransportSocket => friend CallTransport
      #pragma mark

      //-----------------------------------------------------------------------
      CallTransport::TransportSocketPtr CallTransport::TransportSocket::create(
                                                                               IMessageQueuePtr queue,
                                                                               CallTransportPtr outer,
                                                                               const IICESocket::TURNServerInfoList &turnServers,
                                                                               const IICESocket::STUNServerInfoList &stunServers
                                                                               )
      {
        TransportSocketPtr pThis(new TransportSocket(queue, outer));
        pThis->mThisWeak = pThis;
        pThis->init(turnServers, stunServers);
        return pThis;
      }

      //-----------------------------------------------------------------------
      void CallTransport::TransportSocket::shutdown()
      {
        if (mRTPSocket) {
          mRTPSocket->shutdown();
          if (IICESocket::ICESocketState_Shutdown == mRTPSocket->getState()) {
            mRTPSocket.reset();
          }
        }
      }

      //-----------------------------------------------------------------------
      bool CallTransport::TransportSocket::isReady() const
      {
        if (!mRTPSocket) return false;

        if (IICESocket::ICESocketState_Ready != mRTPSocket->getState()) return false;
        return true;
      }

      //-----------------------------------------------------------------------
      bool CallTransport::TransportSocket::isShutdown() const
      {
        if (mRTPSocket) {
          if (IICESocket::ICESocketState_Shutdown != mRTPSocket->getState()) return false;
        }
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport::TransportSocket => IICESocketDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void CallTransport::TransportSocket::onICESocketStateChanged(
                                                                   IICESocketPtr socket,
                                                                   ICESocketStates state
                                                                   )
      {
        CallTransportPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("ICE state change ignored as call transport object is gone"))
          return;
        }

        ZS_LOG_DEBUG(log("on ice socket state changed"))
        IWakeDelegateProxy::create(outer)->onWake();
      }

      //-----------------------------------------------------------------------
      void CallTransport::TransportSocket::onICESocketCandidatesChanged(IICESocketPtr socket)
      {
        CallTransportPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("ICE candidates change ignored as call transport object is gone"))
          return;
        }

        ZS_LOG_DEBUG(log("on ice socket candidates changed"))
        IWakeDelegateProxy::create(outer)->onWake();
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport::TransportSocket->webrtc::Transport
      #pragma mark

      //-----------------------------------------------------------------------
      int CallTransport::TransportSocket::SendPacket(int channel, const void *data, size_t len)
      {
        if (len < (sizeof(BYTE)*2)) return 0;

        BYTE payloadType = ((const BYTE *)data)[1];
        ZS_LOG_SUBSYSTEM_TRACE(mediaSubsystem(), log("request to send RTP packet") + ZS_PARAM("payload type", payloadType) + ZS_PARAM("length", len))

        CallTransportPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_SUBSYSTEM_TRACE(mediaSubsystem(), log("cannot send RTP packet because call transport object is gone"))
          return 0;
        }

        return outer->sendRTPPacket(mID, data, len);
      }

      //-----------------------------------------------------------------------
      int CallTransport::TransportSocket::SendRTCPPacket(int channel, const void *data, size_t len)
      {
        if (len < (sizeof(BYTE)*2)) return 0;

        BYTE payloadType = ((const BYTE *)data)[1];
        ZS_LOG_SUBSYSTEM_TRACE(mediaSubsystem(), log("request to send RTCP packet") + ZS_PARAM("payload type", payloadType) + ZS_PARAM("length", len))

        CallTransportPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("cannot send RTCP packet because call transport object is gone"))
          return 0;
        }

        return outer->sendRTPPacket(mID, data, len);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark CallTransport::TransportSocket => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params CallTransport::TransportSocket::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::CallTransport::TransportSocket");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr CallTransport::TransportSocket::toDebug() const
      {
        ElementPtr resultEl = Element::create("core::CallTransport::TransportSocket");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "rtp socket id", mRTPSocket ? mRTPSocket->getID() : 0);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallTransportFactory
      #pragma mark

      //-----------------------------------------------------------------------
      ICallTransportFactory &ICallTransportFactory::singleton()
      {
        return CallTransportFactory::singleton();
      }

      //-----------------------------------------------------------------------
      CallTransportPtr ICallTransportFactory::create(
                                                     ICallTransportDelegatePtr delegate,
                                                     const IICESocket::TURNServerInfoList &turnServers,
                                                     const IICESocket::STUNServerInfoList &stunServers
                                                     )
      {
        if (this) {}
        return CallTransport::create(delegate, turnServers, stunServers);
      }

    }
  }
}
