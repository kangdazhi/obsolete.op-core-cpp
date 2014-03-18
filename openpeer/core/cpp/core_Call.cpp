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

#include <openpeer/core/internal/core_Call.h>
#include <openpeer/core/internal/core_CallTransport.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_Helper.h>

#include <openpeer/stack/IHelper.h>
#include <openpeer/services/IHelper.h>

#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#define OPENPEER_CALL_CLEANUP_TIMEOUT_IN_SECONDS (60*2)

#define OPENPEER_CALL_FIRST_CLOSED_REMOTE_CALL_TIME_IN_SECONDS (4)
#define OPENPEER_CALL_CALL_CHECK_PEER_ALIVE_TIMER_IN_SECONDS (15)

#define OPENPEER_CALL_RTP_ICE_KEEP_ALIVE_INDICATIONS_SENT_IN_SECONDS (4)
#define OPENPEER_CALL_RTP_ICE_EXPECTING_DATA_WITHIN_IN_SECONDS (10)
#define OPENPEER_CALL_RTP_MAX_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS (15)

#define OPENPEER_CALL_RTCP_ICE_KEEP_ALIVE_INDICATIONS_SENT_IN_SECONDS (20)
#define OPENPEER_CALL_RTCP_ICE_EXPECTING_DATA_WITHIN_IN_SECONDS (45)

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }
namespace openpeer { namespace core { ZS_DECLARE_FORWARD_SUBSYSTEM(openpeer_media) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      ZS_DECLARE_TYPEDEF_PTR(ICallForConversationThread::ForConversationThread, ForConversationThread)

      using services::IHelper;

      using zsLib::ITimerDelegateProxy;

      using stack::CandidateList;

      using namespace core::internal::thread;

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
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "core::Call");
      }

      //-----------------------------------------------------------------------
      static Dialog::DialogStates convert(ICall::CallStates state)
      {
        return (Dialog::DialogStates)state;
      }

      //-----------------------------------------------------------------------
      static ICall::CallStates convert(Dialog::DialogStates state)
      {
        return (ICall::CallStates)state;
      }

      //-----------------------------------------------------------------------
      static Dialog::DialogClosedReasons convert(ICall::CallClosedReasons reason)
      {
        return (Dialog::DialogClosedReasons)reason;
      }

      //-----------------------------------------------------------------------
      static ICall::CallClosedReasons convert(Dialog::DialogClosedReasons reason)
      {
        return (ICall::CallClosedReasons)reason;
      }

      //-----------------------------------------------------------------------
      static ICallTransportForCall::SocketTypes convert(ICallForCallTransport::SocketTypes type)
      {
        switch (type) {
          case ICallForCallTransport::SocketType_Audio: return ICallTransportForCall::SocketType_Audio;
          case ICallForCallTransport::SocketType_Video: return ICallTransportForCall::SocketType_Video;
        }

        ZS_THROW_INVALID_ASSUMPTION("what type of socket is this?")
        return ICallTransportForCall::SocketType_Audio;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ICallForConversationThread::toDebug(ForConversationThreadPtr call)
      {
        return Call::toDebug(Call::convert(call));
      }

      //-----------------------------------------------------------------------
      ForConversationThreadPtr ICallForConversationThread::createForIncomingCall(
                                                                                 ConversationThreadPtr inConversationThread,
                                                                                 ContactPtr callerContact,
                                                                                 const DialogPtr &remoteDialog
                                                                                 )
      {
        return ICallFactory::singleton().createForIncomingCall(inConversationThread, callerContact, remoteDialog);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICallForCallTransport
      #pragma mark

      //-----------------------------------------------------------------------
      const char *ICallForCallTransport::toString(SocketTypes type)
      {
        switch (type)
        {
          case SocketType_Audio:  return "audio";
          case SocketType_Video:  return "video";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      ElementPtr ICallForCallTransport::toDebug(ForCallTransportPtr call)
      {
        return Call::toDebug(Call::convert(call));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call::ICallLocation
      #pragma mark

      //-----------------------------------------------------------------------
      const char *Call::ICallLocation::toString(CallLocationStates state)
      {
        switch (state)
        {
          case CallLocationState_Pending:   return "Pending";
          case CallLocationState_Ready:     return "Ready";
          case CallLocationState_Closed:    return "Closed";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call
      #pragma mark

      //-----------------------------------------------------------------------
      Call::Call(
                 AccountPtr account,
                 ConversationThreadPtr conversationThread,
                 ICallDelegatePtr delegate,
                 bool hasAudio,
                 bool hasVideo,
                 const char *callID,
                 ILocationPtr selfLocation,
                 IPeerFilesPtr peerFiles
                 ) :
        mLock(*conversationThread),
        mStepLock(SharedRecursiveLock::create()),
        mQueue(UseStack::queueCore()),
        mMediaQueue(UseStack::queueMedia()),
        mDelegate(delegate),
        mCallID(callID ? string(callID) : services::IHelper::randomString(32)),
        mHasAudio(hasAudio),
        mHasVideo(hasVideo),
        mIncomingCall(false),
        mIncomingNotifiedThreadOfPreparing(false),
        mAccount(account),
        mConversationThread(conversationThread),
        mTransport(UseAccountPtr(account)->getCallTransport()),
        mSelfLocation(selfLocation),
        mPeerFiles(peerFiles),
        mCurrentState(ICall::CallState_None),
        mClosedReason(ICall::CallClosedReason_None),
        mPlaceCall(false),
        mRingCalled(false),
        mAnswerCalled(false),
        mLocalOnHold(false),
        mCreationTime(zsLib::now())
      {
        ZS_LOG_BASIC(log("created"))

        mMediaHolding.set(false);
        mNotifiedCallTransportDestroyed.set(false);
      }

      //-----------------------------------------------------------------------
      void Call::init()
      {
        mThisWakeDelegate = IWakeDelegateProxy::createWeak(getQueue(), mThisWeakNoQueue.lock());
        mThisCallAsyncNormalQueue = ICallAsyncProxy::createWeak(getQueue(), mThisWeakNoQueue.lock());
        mThisCallAsyncMediaQueue = ICallAsyncProxy::createWeak(getMediaQueue(), mThisWeakNoQueue.lock());
        mThisICESocketDelegate = IICESocketDelegateProxy::createWeak(getMediaQueue(), mThisWeakNoQueue.lock());
        mThisTimerDelegate = ITimerDelegateProxy::createWeak(getQueue(), mThisWeakNoQueue.lock());

        if (mTransport) {
          mTransport->notifyCallCreation(mID);
        }

        setCurrentState(ICall::CallState_Preparing);

        ZS_LOG_DEBUG(log("call init called thus invoking step"))
        IWakeDelegateProxy::create(mThisWakeDelegate)->onWake();
      }

      //-----------------------------------------------------------------------
      Call::~Call()
      {
        if (isNoop()) return;

        mThisWeakNoQueue.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      CallPtr Call::convert(ICallPtr call)
      {
        return dynamic_pointer_cast<Call>(call);
      }

      //-----------------------------------------------------------------------
      CallPtr Call::convert(ForConversationThreadPtr call)
      {
        return dynamic_pointer_cast<Call>(call);
      }

      //-----------------------------------------------------------------------
      CallPtr Call::convert(ForCallTransportPtr call)
      {
        return dynamic_pointer_cast<Call>(call);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call => ICall
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Call::toDebug(ICallPtr call)
      {
        if (!call) return ElementPtr();
        return Call::convert(call)->toDebug(true);
      }

      //-----------------------------------------------------------------------
      CallPtr Call::placeCall(
                              ConversationThreadPtr inConversationThread,
                              IContactPtr toContact,
                              bool includeAudio,
                              bool includeVideo
                              )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inConversationThread)
        ZS_THROW_INVALID_ARGUMENT_IF(!toContact)

        UseConversationThreadPtr conversationThread(inConversationThread);
        UseAccountPtr account = conversationThread->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, slog("account object is gone thus cannot create call"))
          return CallPtr();
        }

        CallPtr pThis(new Call(conversationThread->getAccount(), inConversationThread, account->getCallDelegate(), includeAudio, includeVideo, NULL, account->getSelfLocation(), account->getPeerFiles()));
        pThis->mThisWeakNoQueue = pThis;
        pThis->mCaller = account->getSelfContact();
        pThis->mCallee = Contact::convert(toContact);
        pThis->mIncomingCall = false;
        pThis->mPlaceCall = true;
        ZS_LOG_DEBUG(pThis->log("call being placed") + UseContact::toDebug(pThis->mCallee))
        pThis->init();
        if ((!pThis->mCaller) ||
            (!pThis->mCallee)) {
          ZS_LOG_WARNING(Detail, pThis->log("contact is not valid thus cannot create call"))
          return CallPtr();
        }
        if (!pThis->mTransport) {
          ZS_LOG_WARNING(Detail, pThis->log("transport object is gone from account thus cannot create call (while placing call)"))
          return CallPtr();
        }

        return pThis;
      }

      //-----------------------------------------------------------------------
      String Call::getCallID() const
      {
        return mCallID;
      }

      //-----------------------------------------------------------------------
      IConversationThreadPtr Call::getConversationThread() const
      {
        // look ma - no lock
        return ConversationThread::convert(mConversationThread.lock());
      }

      //-----------------------------------------------------------------------
      IContactPtr Call::getCaller() const
      {
        return Contact::convert(mCaller);
      }

      //-----------------------------------------------------------------------
      IContactPtr Call::getCallee() const
      {
        return Contact::convert(mCallee);
      }

      //-----------------------------------------------------------------------
      bool Call::hasAudio() const
      {
        return mHasAudio;
      }

      //-----------------------------------------------------------------------
      bool Call::hasVideo() const
      {
        return mHasVideo;
      }

      //-----------------------------------------------------------------------
      ICall::CallStates Call::getState() const
      {
        AutoRecursiveLock lock(mLock);
        return mCurrentState;
      }

      ICall::CallClosedReasons Call::getClosedReason() const
      {
        AutoRecursiveLock lock(mLock);
        return mClosedReason;
      }

      //-----------------------------------------------------------------------
      Time Call::getcreationTime() const
      {
        AutoRecursiveLock lock(mLock);
        return mCreationTime;
      }

      //-----------------------------------------------------------------------
      Time Call::getRingTime() const
      {
        AutoRecursiveLock lock(mLock);
        return mCreationTime;
      }

      //-----------------------------------------------------------------------
      Time Call::getAnswerTime() const
      {
        AutoRecursiveLock lock(mLock);
        return mAnswerTime;
      }

      //-----------------------------------------------------------------------
      Time Call::getClosedTime() const
      {
        AutoRecursiveLock lock(mLock);
        return mClosedTime;
      }

      //-----------------------------------------------------------------------
      void Call::ring()
      {
        ZS_THROW_INVALID_USAGE_IF(!mIncomingCall)

        ZS_LOG_DEBUG(log("ring called"))

        AutoRecursiveLock lock(mLock);
        mRingCalled = true;

        ZS_LOG_DEBUG(log("ring called thus invoking step"))
        IWakeDelegateProxy::create(mThisWakeDelegate)->onWake();
      }

      //-----------------------------------------------------------------------
      void Call::answer()
      {
        ZS_THROW_INVALID_USAGE_IF(!mIncomingCall)

        ZS_LOG_DEBUG(log("answer called"))

        AutoRecursiveLock lock(mLock);

        mAnswerCalled = true;

        ZS_LOG_DEBUG(log("answer called thus invoking step"))
        IWakeDelegateProxy::create(mThisWakeDelegate)->onWake();
      }

      //-----------------------------------------------------------------------
      void Call::hold(bool hold)
      {
        ZS_LOG_DEBUG(log("hold called"))

        AutoRecursiveLock lock(mLock);
        mLocalOnHold = hold;

        ZS_LOG_DEBUG(log("hold called thus invoking step"))
        IWakeDelegateProxy::create(mThisWakeDelegate)->onWake();
      }

      //-----------------------------------------------------------------------
      void Call::hangup(CallClosedReasons reason)
      {
        ZS_LOG_DEBUG(log("hangup called") + ZS_PARAM("reason", ICall::toString(reason)))

        AutoRecursiveLock lock(mLock);
        setClosedReason(reason);
        ICallAsyncProxy::create(mThisCallAsyncNormalQueue)->onHangup();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call => ICallForCallTransport
      #pragma mark

      //-----------------------------------------------------------------------
      void Call::notifyLostFocus()
      {
        hold(true);
      }

      //-----------------------------------------------------------------------
      bool Call::sendRTPPacket(
                               PUID toLocationID,
                               SocketTypes type,
                               const BYTE *packet,
                               size_t packetLengthInBytes
                               )
      {
        // NOTE: Intentionally disallow picking of the early location since
        //       local would never send RTP media during early.

        CallLocationPtr callLocation = mPickedLocation.get();

        if (!callLocation) {
          ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("unable to send RTP packet as there is no picked/early location to communicate"))
          return false;
        }
        if (callLocation->getID() != toLocationID) {
          ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("unable to send RTP packet as the picked/early location does not match the to location"))
          return false;
        }
        return callLocation->sendRTPPacket(type, packet, packetLengthInBytes);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call => ICallForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      CallPtr Call::createForIncomingCall(
                                           ConversationThreadPtr inConversationThread,
                                           ContactPtr callerContact,
                                           const DialogPtr &remoteDialog
                                           )
      {
        UseConversationThreadPtr conversationThread(inConversationThread);

        ZS_THROW_INVALID_ARGUMENT_IF(!conversationThread)
        ZS_THROW_INVALID_ARGUMENT_IF(!callerContact)

        UseAccountPtr account = conversationThread->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, slog("account object is gone thus cannot create call"))
          return CallPtr();
        }

        bool hasAudio = false;
        bool hasVideo = false;

        DescriptionList descriptions = remoteDialog->descriptions();
        for (DescriptionList::iterator iter = descriptions.begin(); iter != descriptions.end(); ++iter)
        {
          DescriptionPtr &description = (*iter);

          bool isAudio = false;
          bool isVideo = false;

          if ("audio" == description->mType) {
            isAudio = true;
            hasAudio = true;
          } else if ("video" == description->mType) {
            isVideo = true;
            hasVideo = true;
          }

          if ((!isAudio) && (!isVideo)) {
            ZS_LOG_WARNING(Detail, slog("call does not contain valid media") + ZS_PARAM("type", description->mType))
            continue;
          }
        }

        CallPtr pThis(new Call(conversationThread->getAccount(),  inConversationThread, account->getCallDelegate(), hasAudio, hasVideo, remoteDialog->dialogID(), account->getSelfLocation(), account->getPeerFiles()));
        pThis->mThisWeakNoQueue = pThis;
        pThis->mCaller = callerContact;
        pThis->mCallee = account->getSelfContact();
        pThis->mIncomingCall = true;
        pThis->init();
        if ((!pThis->mCaller) ||
            (!pThis->mCallee)) {
          ZS_LOG_WARNING(Detail, pThis->log("contact is not valid thus cannot create call"))
          return CallPtr();
        }
        if (!pThis->mTransport) {
          ZS_LOG_WARNING(Detail, pThis->log("transport object is gone from account thus cannot create call (for incoming call)"))
          return CallPtr();
        }

        return pThis;
      }

      //-----------------------------------------------------------------------
      DialogPtr Call::getDialog() const
      {
        AutoRecursiveLock lock(mLock);
        return mDialog;
      }

      //-----------------------------------------------------------------------
      ContactPtr Call::getCaller(bool) const
      {
        return Contact::convert(mCaller);
      }

      //-----------------------------------------------------------------------
      ContactPtr Call::getCallee(bool) const
      {
        return Contact::convert(mCallee);
      }

      //-----------------------------------------------------------------------
      void Call::notifyConversationThreadUpdated()
      {
        ZS_LOG_DEBUG(log("notified conversation thread updated thus invoking step"))
        IWakeDelegateProxy::create(mThisWakeDelegate)->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call => ICallAsync
      #pragma mark

      //-----------------------------------------------------------------------
      void Call::onSetFocus(bool on)
      {
        if (!on) {
          ZS_LOG_DEBUG(log("this call should not have focus"))
          mTransport->loseFocus(mID);
          return;
        }

        PUID focusLocationID = 0;

        {
          if (mPickedLocation.get()) {
            focusLocationID = mPickedLocation.get()->getID();
          } else if (mEarlyLocation.get()) {
            focusLocationID = mEarlyLocation.get()->getID();
          }
        }

        // tell the transport to focus on this call/location...
        if (0 != focusLocationID) {
          mTransport->focus(mThisWeakNoQueue.lock(), focusLocationID);
        } else {
          ZS_LOG_WARNING(Detail, log("told to set focus but there is no location to focus"))
          mTransport->loseFocus(mID);
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call => IICESocketDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Call::onICESocketStateChanged(
                                         IICESocketPtr inSocket,
                                         ICESocketStates state
                                         )
      {
        // scope
        {
          bool found = false;
          SocketTypes type = SocketType_Audio;
          bool wasRTP = false;

          IICESocketSubscriptionPtr subscription = findSubscription(inSocket, found, &type, &wasRTP);
          if (!found) {
            ZS_LOG_WARNING(Detail, log("ignoring ICE socket state change on obsolete session"))
            return;
          }

          if (IICESocket::ICESocketState_Shutdown == state) {
            ZS_LOG_DEBUG(log("ICE socket is shutdown") + ZS_PARAM("type", ICallForCallTransport::toString(type)) + ZS_PARAM("is RTP", wasRTP))
            cancel();
          }
        }

        ZS_LOG_DEBUG(log("ICE socket state change thus invoking step"))
        IWakeDelegateProxy::create(mThisWakeDelegate)->onWake();
      }

      //-----------------------------------------------------------------------
      void Call::onICESocketCandidatesChanged(IICESocketPtr inSocket)
      {
        ZS_LOG_DEBUG(log("ICE socket candidates change thus invoking step"))
        IWakeDelegateProxy::create(mThisWakeDelegate)->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Call::onTimer(TimerPtr timer)
      {
        // scope: check out the timer in the context of a lock
        {
          AutoRecursiveLock lock(mLock);

          if (timer == mPeerAliveTimer) {
            ZS_LOG_DEBUG(log("peer alive timer"))
            goto call_step;
          }

          if (timer == mFirstClosedRemoteCallTimer) {
            ZS_LOG_DEBUG(log("first closed remote call timer"))

            mFirstClosedRemoteCallTimer->cancel();
            mFirstClosedRemoteCallTimer.reset();
            goto call_step;
          }

          if (timer == mCleanupTimer) {
            ZS_LOG_DEBUG(log("call cleanup timer fired - tell the conversation thread"))

            mCleanupTimer.reset();

            UseConversationThreadPtr thread = mConversationThread.lock();
            if (!thread) {
              ZS_LOG_WARNING(Debug, log("conversation thread is already shutdown"))
              return;
            }

            thread->notifyCallCleanup(mThisWeakNoQueue.lock());
          }
          return;
        }

      call_step:
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call => friend CallLocation
      #pragma mark

      //-----------------------------------------------------------------------
      void Call::notifyReceivedRTPPacket(
                                         PUID locationID,
                                         SocketTypes type,
                                         const BYTE *buffer,
                                         size_t bufferLengthInBytes
                                         )
      {
        // scope:
        {
          CallLocationPtr picked = mPickedLocation.get();
          CallLocationPtr early = mEarlyLocation.get();

          if (!picked) {
            if (!early) {
              ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("ignoring received RTP packet as no call location was chosen"))
              return;
            }
            if (early->getID() != locationID) {
              ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("ignoring received RTP packet as packet did not come from chosen early location"))
              return;
            }
          } else if (picked->getID() != locationID) {
            ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("ignoring received RTP packet as location specified is not chosen location") + ZS_PARAM("chosen", mPickedLocation.get()->getID()) + ZS_PARAM("specified", locationID))
            return;
          }
        }

        mTransport->notifyReceivedRTPPacket(mID, locationID, internal::convert(type), buffer, bufferLengthInBytes);
      }

      //-----------------------------------------------------------------------
      void Call::notifyStateChanged(
                                    CallLocationPtr location,
                                    CallLocationStates state
                                    )
      {
        if (ICallLocation::CallLocationState_Closed == state) {
          // scope: object
          {
            AutoRecursiveLock lock(mLock);
            CallLocationMap::iterator found = mCallLocations.find(location->getLocationID());
            if (found != mCallLocations.end()) {
              ZS_LOG_DEBUG(log("location shutdown") + ZS_PARAM("object ID", location->getID()) + ZS_PARAM("location ID", location->getLocationID()))
              mCallLocations.erase(found);
            }
          }

          bool pickedRemoved = false;

          // scope: media
          {
            CallLocationPtr picked = mPickedLocation.get();
            if (picked) {
              if (picked->getLocationID() == location->getLocationID()) {
                // the picked location is shutting down...
                ZS_LOG_WARNING(Detail, log("picked location shutdown") + ZS_PARAM("object ID", location->getID()) + ZS_PARAM("location ID", location->getLocationID()))
                mPickedLocation.set(CallLocationPtr());
                pickedRemoved = true;
              }
            }
          }

          if (pickedRemoved)
          {
            // scope: object
            AutoRecursiveLock lock(mLock);
            setClosedReason(CallClosedReason_RequestTimeout);
          }
        }

        CallPtr pThis = mThisWeakNoQueue.lock();
        if (pThis) {
          ZS_LOG_DEBUG(log("call location state changed thus invoking step"))
          IWakeDelegateProxy::create(getQueue(), pThis)->onWake();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      const SharedRecursiveLock &Call::getLock() const
      {
        return mLock;
      }

      //-----------------------------------------------------------------------
      Log::Params Call::slog(const char *message)
      {
        return Log::Params(message, "core::Call");
      }

      //-----------------------------------------------------------------------
      Log::Params Call::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Call");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr Call::toDebug(bool callData) const
      {
        ElementPtr resultEl = Element::create("core::Call");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "call id (s)", mCallID);
        IHelper::debugAppend(resultEl, "has audio", mHasAudio);
        IHelper::debugAppend(resultEl, "has video", mHasVideo);
        IHelper::debugAppend(resultEl, "incoming", mIncomingCall);
        IHelper::debugAppend(resultEl, "caller", UseContact::toDebug(mCaller));
        IHelper::debugAppend(resultEl, "callee", UseContact::toDebug(mCallee));

        if (callData)
        {
          AutoRecursiveLock lock(mLock);

          IHelper::debugAppend(resultEl, "state", ICall::toString(mCurrentState));
          IHelper::debugAppend(resultEl, "closed reason", ICall::toString(mClosedReason));
          IHelper::debugAppend(resultEl, "notified", mIncomingNotifiedThreadOfPreparing);
          IHelper::debugAppend(resultEl, "locations", mCallLocations.size());
          IHelper::debugAppend(resultEl, "place call", mPlaceCall);
          IHelper::debugAppend(resultEl, "ring called", mRingCalled);
          IHelper::debugAppend(resultEl, "answer called", mAnswerCalled);
          IHelper::debugAppend(resultEl, "local on hold", mLocalOnHold);
          IHelper::debugAppend(resultEl, "creation", mCreationTime);
          IHelper::debugAppend(resultEl, "ring", mRingTime);
          IHelper::debugAppend(resultEl, "answer", mAnswerTime);
          IHelper::debugAppend(resultEl, "closed", mClosedTime);
          IHelper::debugAppend(resultEl, "first closed", mFirstClosedRemoteCallTime);
        }

        IHelper::debugAppend(resultEl, "media hold", mMediaHolding.get());
        IHelper::debugAppend(resultEl, "notified destroyed", mNotifiedCallTransportDestroyed.get());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      bool Call::isShuttingdown() const
      {
        AutoRecursiveLock lock(mLock);
        return mCurrentState == ICall::CallState_Closing;
      }

      //-----------------------------------------------------------------------
      bool Call::isShutdown() const
      {
        AutoRecursiveLock lock(mLock);
        return mCurrentState == ICall::CallState_Closed;
      }

      //-----------------------------------------------------------------------
      void Call::cancel()
      {
        typedef zsLib::Timer Timer;
        typedef zsLib::Seconds Seconds;
        typedef std::list<CallLocationPtr> CallLocationList;

        ZS_LOG_DEBUG(log("cancel called"))

        CallLocationList locationsToClose;

        // scope: object
        {
          AutoRecursiveLock lock(mLock);
          if (isShutdown()) {
            ZS_LOG_DEBUG(log("cancel called but call already shutdown"))
            return;
          }

          if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeakNoQueue.lock();

          setCurrentState(ICall::CallState_Closing);

          for (CallLocationMap::iterator iter = mCallLocations.begin(); iter != mCallLocations.end(); ++iter)
          {
            CallLocationPtr &callLocation = (*iter).second;
            locationsToClose.push_back(callLocation);
          }

          mCallLocations.clear();
        }

        ZS_LOG_DEBUG(log("closing call locations") + ZS_PARAM("total", locationsToClose.size()))

        // close the locations now...
        for (CallLocationList::iterator iter = locationsToClose.begin(); iter != locationsToClose.end(); ++iter)
        {
          CallLocationPtr &callLocation = (*iter);
          callLocation->close();
        }

        // scope: media
        {
          mTransport->loseFocus(mID);

          ZS_LOG_DEBUG(log("shutting down audio/video socket subscriptions"))

          if (mAudioRTPSocketSubscription.get()) {
            mAudioRTPSocketSubscription.get()->cancel();
            // do not reset
          }
          if (mVideoRTPSocketSubscription.get()) {
            mVideoRTPSocketSubscription.get()->cancel();
            // do not reset
          }
        }

        // scope: final object shutdown
        {
          AutoRecursiveLock lock(mLock);

          setCurrentState(ICall::CallState_Closed);

          mDelegate.reset();

          // mDialog.reset(); // DO NOT RESET THIS OBJECT - LEAVE IT ALIVE UNTIL THE CALL OBJECT IS DESTROYED

          if (mGracefulShutdownReference) {
            if (!mCleanupTimer) {
              // We will want the cleanup timer to fire to ensure the
              // conversation thread (which is holding a reference to the call)
              // to forget about this call object in time...
              mCleanupTimer = Timer::create(ITimerDelegateProxy::create(getQueue(), mGracefulShutdownReference), Seconds(OPENPEER_CALL_CLEANUP_TIMEOUT_IN_SECONDS), false);
            }
          }

          if (mPeerAliveTimer) {
            mPeerAliveTimer->cancel();
            mPeerAliveTimer.reset();
          }

          if (mFirstClosedRemoteCallTimer) {
            mFirstClosedRemoteCallTimer->cancel();
            mFirstClosedRemoteCallTimer.reset();
          }

          mGracefulShutdownReference.reset();
        }

        bool notifyDestroyed = false;

        // scope: final media shutdown
        {
          mPickedLocation.set(CallLocationPtr());
          mEarlyLocation.set(CallLocationPtr());

          if (!mNotifiedCallTransportDestroyed.get()) {
            mNotifiedCallTransportDestroyed.set(true);
            notifyDestroyed = true;
          }
        }

        if ((mTransport) &&
            (notifyDestroyed)) {
          mTransport->notifyCallDestruction(mID);
        }
        // mTransport.reset();  // WARNING: LEAVE FOR THE DESTRUCTOR TO CLEANUP

        ZS_LOG_DEBUG(log("cancel completed"))
      }

      //-----------------------------------------------------------------------
      void Call::checkState(
                            CallStates state,
                            bool isLocal,
                            bool *callClosedNoThrow
                            ) const throw (
                                           Exceptions::IllegalState,
                                           Exceptions::CallClosed
                                           )
      {
        if (callClosedNoThrow)
          *callClosedNoThrow = false;

        bool didPlaceCall = (isLocal ? !mIncomingCall : mIncomingCall);
        ZS_LOG_DEBUG(log("checking state") + ZS_PARAM("state", ICall::toString(state)) + ZS_PARAM("placed call", didPlaceCall) + ZS_PARAM("side", (isLocal ? "local" : "remote")))
        switch (state) {
          case ICall::CallState_None:       ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is none") + ZS_PARAM("side", (isLocal ? "local" : "remote")))
          case ICall::CallState_Preparing:  break;
          case ICall::CallState_Incoming:   if (didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
          case ICall::CallState_Placed:     if (!didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
          case ICall::CallState_Early:      break;
          case ICall::CallState_Ringing:    if (didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
          case ICall::CallState_Ringback:   if (!didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
          case ICall::CallState_Open:       break;
          case ICall::CallState_Active:     break;
          case ICall::CallState_Inactive:   break;
          case ICall::CallState_Hold:       break;
          case ICall::CallState_Closing:
          case ICall::CallState_Closed:     {
            if (callClosedNoThrow) {
              *callClosedNoThrow = true;
            } else {
              ZS_THROW_CUSTOM(Exceptions::CallClosed, log("call state is closed") + ZS_PARAM("side", (isLocal ? "local" : "remote")))
            }
            break;
          }
        }
      }

      //-----------------------------------------------------------------------
      void Call::checkLegalWhenNotPicked() const throw (Exceptions::IllegalState)
      {
        bool didPlaceCall = !mIncomingCall;
        ZS_LOG_DEBUG(log("checking state legal when not picked") + ZS_PARAM("state", ICall::toString(mCurrentState)) + ZS_PARAM("placed call", didPlaceCall) + ZS_PARAM("side", "local"))
        ZS_THROW_CUSTOM_MSG_IF(Exceptions::IllegalState, !didPlaceCall, log("must have picked a location for the local if the call was incoming"))

        switch (mCurrentState) {
          case ICall::CallState_None:       break;  // already handled
          case ICall::CallState_Preparing:  break;  // legal
          case ICall::CallState_Incoming:   break;  // already handled
          case ICall::CallState_Placed:     break;  // legal
          case ICall::CallState_Early:      break;  // legal
          case ICall::CallState_Ringing:    break;  // already handled
          case ICall::CallState_Ringback:   break;  // legal

          case ICall::CallState_Open:
          case ICall::CallState_Active:
          case ICall::CallState_Inactive:
          case ICall::CallState_Hold:
          {
            ZS_THROW_CUSTOM(Exceptions::IllegalState, log("shutting down call as it's in an illegal state - how can call be in answered state if haven't picked a destination yet"))
          }
          case ICall::CallState_Closing:    break;  // already handled
          case ICall::CallState_Closed:     break;  // already handled
        }
      }

      //-----------------------------------------------------------------------
      void Call::checkLegalWhenPicked(
                                      CallStates state,
                                      bool isLocal
                                      ) const throw (Exceptions::IllegalState)
      {
        bool didPlaceCall = (isLocal ? !mIncomingCall : mIncomingCall);
        ZS_LOG_DEBUG(log("checking state is legal when picked") + ZS_PARAM("state", ICall::toString(state)) + ZS_PARAM("placed call", didPlaceCall) + ZS_PARAM("side", (isLocal ? "local" : "remote")))
        if (isLocal) {
          switch (state) {
            case ICall::CallState_None:       break;  // already handled
            case ICall::CallState_Preparing:  if (didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
            case ICall::CallState_Incoming:   break;
            case ICall::CallState_Placed:     ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal - how could local have picked a location but still be placing the call") + ZS_PARAM("side", (isLocal ? "local" : "remote")))
            case ICall::CallState_Early:      if (didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal - how could local have picked a location but still be in early state") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
            case ICall::CallState_Ringing:    break;
            case ICall::CallState_Ringback:   if (didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal - how could local have picked a location but still be in ringback state") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
            case ICall::CallState_Open:       break;
            case ICall::CallState_Active:     break;
            case ICall::CallState_Inactive:   break;
            case ICall::CallState_Hold:       break;
            case ICall::CallState_Closing:    break;  // already handled
            case ICall::CallState_Closed:     break;  // already handled
          }
        } else {
          switch (state) {
            case ICall::CallState_None:       break;  // already handled
            case ICall::CallState_Preparing:  if (!mIncomingCall) { if (didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal - why pick remote if it's still preparing") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} } break;
            case ICall::CallState_Incoming:   if (didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal - why pick remote if it's still incoming") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
            case ICall::CallState_Placed:     break;
            case ICall::CallState_Early:      if (didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal - why pick remote if it's still early media") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
            case ICall::CallState_Ringing:    if (didPlaceCall) {ZS_THROW_CUSTOM(Exceptions::IllegalState, log("call state is illegal - why pick remote if it's still ringing") + ZS_PARAM("side", (isLocal ? "local" : "remote")))} break;
            case ICall::CallState_Ringback:   break;
            case ICall::CallState_Open:       break;
            case ICall::CallState_Active:     break;
            case ICall::CallState_Inactive:   break;
            case ICall::CallState_Hold:       break;
            case ICall::CallState_Closing:    break;  // already handled
            case ICall::CallState_Closed:     break;  // already handled
          }
        }
      }

      //-----------------------------------------------------------------------
      bool Call::isLockedToAnotherLocation(const DialogPtr &remoteDialog) const
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("unable to determine dialog location locked state as account object is gone"))
          return false;
        }
        String lockedLocationID;
        if (mIncomingCall) {
          // local is the callee and remote is the callee
          lockedLocationID = remoteDialog->calleeLocationID();
        } else {
          // local is the caller and remote is the callee
          lockedLocationID = remoteDialog->callerLocationID();
        }
        if (lockedLocationID.size() < 1) return false;
        return mSelfLocation->getLocationID() != lockedLocationID;
      }

      //-----------------------------------------------------------------------
      bool Call::stepIsMediaReady() throw (Exceptions::StepFailure)
      {
        ZS_LOG_DEBUG(log("checking if media is ready"))

        // setup all the audio ICE socket subscriptions...
        if (hasAudio()) {
          if (!mAudioSocket.get()) {
            mAudioSocket.set(mTransport->getSocket(internal::convert(SocketType_Audio)));
          }

          if (!mAudioRTPSocketSubscription.get()) {
            ZS_LOG_DEBUG(log("subscripting audio RTP socket"))
            mAudioRTPSocketSubscription.set(mAudioSocket.get()->subscribe(mThisICESocketDelegate));
            ZS_THROW_CUSTOM_IF(Exceptions::StepFailure, !mAudioRTPSocketSubscription.get())
          }
        }

        if (hasVideo()) {
          // setup all the video ICE socket subscriptions...
          if (!mVideoSocket.get()) {
            mVideoSocket.set(mTransport->getSocket(internal::convert(SocketType_Video)));
          }
          if (!mVideoRTPSocketSubscription.get()) {
            ZS_LOG_DEBUG(log("subscripting video RTP socket"))
            mVideoRTPSocketSubscription.set(mVideoSocket.get()->subscribe(mThisICESocketDelegate));
            ZS_THROW_CUSTOM_IF(Exceptions::StepFailure, !mVideoRTPSocketSubscription.get())
          }
        }

        if (hasAudio()) {
          mAudioSocket.get()->wakeup();
        }

        if (hasVideo()) {
          mVideoSocket.get()->wakeup();
        }

        ZS_LOG_DEBUG(log("audio and/or video sockets are all told to wake") + ZS_PARAM("has audio", hasAudio()) + ZS_PARAM("has video", hasVideo()))

        return true;
      }

      //-----------------------------------------------------------------------
      bool Call::stepPrepareCallFirstTime() throw (Exceptions::StepFailure)
      {
        typedef Dialog::Description Description;
        typedef Dialog::DescriptionPtr DescriptionPtr;
        typedef Dialog::DescriptionList DescriptionList;

        ZS_LOG_DEBUG(log("preparing first time calls"))

        AutoRecursiveLock lock(mLock);

        if ((isShuttingdown()) ||
            (isShutdown())) {
          ZS_THROW_CUSTOM(Exceptions::StepFailure, log("call unexpectedly shutdown mid step"))
        }

        if (!mDialog) {
          ZS_LOG_DEBUG(log("creating dialog for call"))

          setCurrentState(internal::convert(Dialog::DialogState_Preparing));

          updateDialog();

          ZS_LOG_DEBUG(log("dialog for call created"))
        }

        if (mPlaceCall) {
          ZS_THROW_CUSTOM_IF(Exceptions::StepFailure, !placeCallWithConversationThread())
          mPlaceCall = false;
        }
        return true;
      }

      //-----------------------------------------------------------------------
      bool Call::stepPrepareCallLocations(
                                          const LocationDialogMap locationDialogMap,
                                          CallLocationList &outLocationsToClose
                                          ) throw (Exceptions::CallClosed)
      {
        ZS_LOG_DEBUG(log("preparing call locations"))

        Time tick = zsLib::now();

        CallLocationPtr picked = mPickedLocation.get();

        bool foundPicked = false;
        if (picked) {
          for (LocationDialogMap::const_iterator iter = locationDialogMap.begin(); iter != locationDialogMap.end(); ++iter)
          {
            const String &locationID = (*iter).first;
            if (locationID == picked->getLocationID()) {
              foundPicked = true;
              break;
            }
          }
        }

        if (picked) {
          if (!foundPicked) {
            setClosedReason(CallClosedReason_ServerInternalError);
            ZS_THROW_CUSTOM(Exceptions::CallClosed, log("picked location is now gone"))
          }
          if (mFirstClosedRemoteCallTimer) {
            ZS_LOG_DEBUG(log("cancelling unneeded timer"))
            mFirstClosedRemoteCallTimer->cancel();
            mFirstClosedRemoteCallTimer.reset();
          }
        }

        CallClosedReasons lastClosedReason = CallClosedReason_None;

        for (LocationDialogMap::const_iterator iter = locationDialogMap.begin(); iter != locationDialogMap.end(); ++iter)
        {
          const String &locationID = (*iter).first;
          const DialogPtr &remoteDialog = (*iter).second;

          CallLocationMap::iterator found = mCallLocations.find(locationID);
          if (found != mCallLocations.end()) {
            ZS_LOG_DEBUG(log("updating an existing call location") + ZS_PARAM("remote location ID", locationID))
            CallLocationPtr &callLocation = (*found).second;
            callLocation->updateRemoteDialog(remoteDialog);

            if (picked) {
              if (locationID != picked->getLocationID()) {
                ZS_LOG_DEBUG(log("this call location was not picked therefor must close later") + ZS_PARAM("remote", CallLocation::toDebug(callLocation, true, false)) + ZS_PARAM("picked", CallLocation::toDebug(picked, true, false)))
                // this location must be removed...
                CallLocationPtr &callLocation = (*found).second;
                outLocationsToClose.push_back(callLocation);

                // we don't want this call location anymore since we have picked something else...
                mCallLocations.erase(found);
              }
            }
            continue;
          }

          if (picked) {
            ZS_LOG_DEBUG(log("found a new location for the call but already picked the current location") + ZS_PARAM("location ID", locationID))
            continue;
          }

          CallStates remoteCallState = internal::convert(remoteDialog->dialogState());

          if ((remoteCallState != ICall::CallState_Closing) &&
              (remoteCallState != ICall::CallState_Closed)) {

            ZS_LOG_DEBUG(log("creating a new call location") + ZS_PARAM("remote location ID", locationID))
            CallLocationPtr callLocation = CallLocation::create(getQueue(), getMediaQueue(), mThisWeakNoQueue.lock(), mTransport, locationID, remoteDialog, mAudioSocket.get(), mVideoSocket.get());
            mCallLocations[locationID] = callLocation;

            if (mIncomingCall) {
              ZS_LOG_DEBUG(log("incoming call must pick the remote location") + ZS_PARAM("remote location ID", locationID) + CallLocation::toDebug(callLocation, true, false))
              // we *must* pick this location
              mPickedLocation.set(callLocation);
              picked = callLocation;
            }
            continue;
          }

          CallClosedReasons remoteReason = internal::convert(remoteDialog->closedReason());
          ZS_LOG_DEBUG(log("found a new location for the call but location was already closing/closed") + ZS_PARAM("location ID", locationID) + ZS_PARAM("reason", ICall::toString(remoteReason)))

          switch (internal::convert(remoteDialog->closedReason())) {
            case CallClosedReason_Decline: {
              ZS_LOG_DETAIL(log("call refused by remote party (must end call now)"))
              setClosedReason(internal::convert(remoteDialog->closedReason()));
              return false;
            }
            default:  {
              lastClosedReason = (CallClosedReason_None != remoteReason ? remoteReason : CallClosedReason_User);

              if (Time() == mFirstClosedRemoteCallTime) {
                mFirstClosedRemoteCallTime = tick;
              }
              break;
            }
          }
        }

        if (mIncomingCall) {
          if ((!picked) &&
              (CallClosedReason_None != lastClosedReason)) {
            mFirstClosedRemoteCallTime = Time();
            setClosedReason(lastClosedReason);
            return false;
          }
        }

        //...................................................................
        // check to see if remote dialogs were closed without having any
        // that did not reject the location... if so set up a timer
        // to reject the call in a reasonable time frame

        if (!picked) {
          if (mCallLocations.size() < 1) {
            if (CallClosedReason_None != lastClosedReason) {
              ZS_LOG_DEBUG(log("did not find any remote locations that were open (but one or more were closed)"))

              if (mFirstClosedRemoteCallTime + Seconds(OPENPEER_CALL_FIRST_CLOSED_REMOTE_CALL_TIME_IN_SECONDS) < tick) {
                setClosedReason(lastClosedReason);
                return false;
              }
              if (!mFirstClosedRemoteCallTimer) {
                mFirstClosedRemoteCallTimer = Timer::create(mThisTimerDelegate, Seconds(OPENPEER_CALL_FIRST_CLOSED_REMOTE_CALL_TIME_IN_SECONDS), false);
              }
              ZS_LOG_DEBUG(log("remote party is closed but will try to connect with other locations (if possible)"))
            }
          }
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool Call::stepVerifyCallState() throw (
                                              Exceptions::IllegalState,
                                              Exceptions::CallClosed
                                              )
      {
        ZS_LOG_DEBUG(log("verifying call state"))

        CallLocationPtr picked = mPickedLocation.get();

        if ((mIncomingCall) && (!picked)) {
          setClosedReason(CallClosedReason_ServerInternalError);
          ZS_THROW_CUSTOM(Exceptions::CallClosed, log("shutting down call as incoming call does not have a location picked and it must"))
        }

        bool callClosedException = false;
        checkState(mCurrentState, true, &callClosedException);
        if (callClosedException) {
          ZS_LOG_DEBUG(log("local side closed the call"))
          setClosedReason(CallClosedReason_User);
          return false;
        }

        if (picked) {
          DialogPtr pickedRemoteDialog = picked->getRemoteDialog();
          ZS_THROW_INVALID_ASSUMPTION_IF(!pickedRemoteDialog)
          checkState(internal::convert(pickedRemoteDialog->dialogState()), false, &callClosedException);
          if (callClosedException) {
            ZS_LOG_DEBUG(log("remote side closed the call"))
            setClosedReason(internal::convert(pickedRemoteDialog->closedReason()));
            return false;
          }
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool Call::stepTryToPickALocation(CallLocationList &outLocationsToClose)
      {
        typedef Dialog::Description Description;
        typedef Dialog::DescriptionList DescriptionList;

        CallLocationPtr picked = mPickedLocation.get();
        CallLocationPtr early = mEarlyLocation.get();

        if (picked) {
          DialogPtr remoteDialog = picked->getRemoteDialog();
          if (isLockedToAnotherLocation(remoteDialog)) {
            ZS_LOG_WARNING(Detail, log("remote side locked onto another location... harumph... remove the location") + Dialog::toDebug(remoteDialog))
            return false;
          }

          ZS_LOG_DEBUG(log("location already picked (no need to try and pick one)"))
          return true;
        }

        ZS_LOG_DEBUG(log("do not have a picked location yet for placed call (thus will attempt to pick one)"))

        checkLegalWhenNotPicked();

        CallLocationPtr ready;
        CallLocationPtr ringing;

        // check for a location which has answered or open the call so we can pick it...
        for (CallLocationMap::iterator locIter = mCallLocations.begin(); locIter != mCallLocations.end(); )
        {
          CallLocationMap::iterator current = locIter;
          ++locIter;

          CallLocationPtr &callLocation = (*current).second;
          DialogPtr remoteDialog = callLocation->getRemoteDialog();

          bool mustRemove = false;

          if (isLockedToAnotherLocation(remoteDialog)) {
            ZS_LOG_WARNING(Detail, log("remote side stupidly locked onto another location which did not even place this call - removing the stupid location"))
            mustRemove = true;
          }

          try {
            ZS_LOG_TRACE(log("checking possible location to see if it has a valid state") + CallLocation::toDebug(callLocation, true, false) + Dialog::toDebug(remoteDialog))
            bool callClosedException = false;
            checkState(internal::convert(remoteDialog->dialogState()), false, &callClosedException);
            if (callClosedException) {
              ZS_LOG_WARNING(Detail, log("this non picked location closed the call"))
              mustRemove = true;
            }
          } catch (Exceptions::IllegalState &) {
            ZS_LOG_DEBUG(log("remote call is not in a legal state"))
            mustRemove = true;
          } catch (Exceptions::CallClosed &) {
            ZS_LOG_DEBUG(log("remote call is closed"))
            mustRemove = true;
          }

          if (mustRemove) {
            // this location must close
            outLocationsToClose.push_back(callLocation);
            mCallLocations.erase(current);
            continue;
          }

          if (ICallLocation::CallLocationState_Ready == callLocation->getState()) {
            if (!ready) {
              ready = callLocation;
            }
          }

          switch (internal::convert(remoteDialog->dialogState()))
          {
            case CallState_Early:     {
              if (ICallLocation::CallLocationState_Ready == callLocation->getState()) {
                if (!early) {
                  mEarlyLocation.set(callLocation);
                  early = callLocation;
                }
              }
              break;
            }
            case CallState_Ringing:   {
              if (!ringing) {
                ringing = callLocation;
              }
              break;
            }
            case CallState_Open:
            case CallState_Active:
            case CallState_Inactive:
            case CallState_Hold:      {
              if (ICallLocation::CallLocationState_Ready == callLocation->getState()) {
                ZS_LOG_DEBUG(log("picked location") + CallLocation::toDebug(callLocation, true, false))
                mPickedLocation.set(callLocation);
                picked = callLocation;
              }
              break;
            }
            default: break;
          }

          if (picked) {
            break;
          }
        }

        if (picked) {
          ZS_LOG_DEBUG(log("placed call now has chosen a remote location thus must update dialog"))

          DialogPtr remoteDialog = picked->getRemoteDialog();
          ZS_THROW_CUSTOM_MSG_IF(Exceptions::CallClosed, isLockedToAnotherLocation(remoteDialog), log("remote side has locked onto another location thus call must close"))

          setCurrentState(CallState_Open, true);

          ZS_LOG_DEBUG(log("call state changed to open thus forcing step to force close unchosen locations"))
          IWakeDelegateProxy::create(mThisWakeDelegate)->onWake();
        } else if (early) {
          setCurrentState(CallState_Early);
        } else if (ringing) {
          // force our side into correct state...
          setCurrentState(CallState_Ringback);
        } else if (ready) {
          setCurrentState(CallState_Placed);
        }
        return true;
      }

      //-----------------------------------------------------------------------
      bool Call::stepHandlePickedLocation() throw (Exceptions::IllegalState)
      {
        CallLocationPtr picked = mPickedLocation.get();

        if (!picked) {
          ZS_LOG_DEBUG(log("location is not picked yet"))
          return true;
        }

        DialogPtr pickedRemoteDialog = picked->getRemoteDialog();
        ZS_THROW_BAD_STATE_IF(!pickedRemoteDialog)

        ICallLocation::CallLocationStates pickedLocationState = picked->getState();

        ZS_LOG_DEBUG(log("picked call state logic") + CallLocation::toDebug(picked, true, false) + toDebug(true) + Dialog::toDebug(pickedRemoteDialog))

        // if the picked location is now gone then we must shutdown the object...
        checkLegalWhenPicked(mCurrentState, true);
        checkLegalWhenPicked(internal::convert(pickedRemoteDialog->dialogState()), false);

        switch (pickedLocationState) {
          case ICallLocation::CallLocationState_Pending: {
            mMediaHolding.set(true);
            ZS_THROW_CUSTOM_MSG_IF(Exceptions::IllegalState, !mIncomingCall, log("placed call picked location but picked location is not ready"))
            if (mIncomingCall) {
              if (!mIncomingNotifiedThreadOfPreparing) {
                if (ICall::CallState_Preparing == mCurrentState) {
                  ZS_LOG_DEBUG(log("need to notify the remote party that the call is pending since this is an incoming call..."))
                  mIncomingNotifiedThreadOfPreparing = true;
                  setCurrentState(ICall::CallState_Preparing, true);
                }
              }
            }
            break;
          }
          case ICallLocation::CallLocationState_Ready: {
            if (!mIncomingCall) {
              ZS_LOG_DEBUG(log("now ready to go into open state"))
              mMediaHolding.set(mLocalOnHold || (ICall::CallState_Hold == internal::convert(pickedRemoteDialog->dialogState())));
              setCurrentState(ICall::CallState_Open);
              break;
            }

            // this call is incoming... answer takes priority over ringing...
            if (mAnswerCalled) {
              mMediaHolding.set(mLocalOnHold || (ICall::CallState_Hold == internal::convert(pickedRemoteDialog->dialogState())));
              setCurrentState(ICall::CallState_Open);
            } else if (mRingCalled) {
              mMediaHolding.set(true);
              setCurrentState(ICall::CallState_Ringing);
            } else {
              mMediaHolding.set(true);
              setCurrentState(ICall::CallState_Incoming);
            }
            break;
          }
          case ICallLocation::CallLocationState_Closed: {
            ZS_LOG_DEBUG(log("picked call location is now closed"))
            setClosedReason(internal::convert(pickedRemoteDialog->closedReason()));
            return false;
          }
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool Call::stepFixCallInProgressStates()
      {
        CallLocationPtr picked = mPickedLocation.get();
        CallLocationPtr early = mEarlyLocation.get();

        if ((picked) ||
            (early)) {

          CallLocationPtr usingLocation = (picked ? picked : early);

          UseContactPtr remoteContact = (mCaller->isSelf() ? mCallee : mCaller);
          ZS_THROW_BAD_STATE_IF(remoteContact->isSelf())

          ILocationPtr peerLocation = ILocation::getForPeer(remoteContact->getPeer(), usingLocation->getLocationID());
          if (!peerLocation) {
            ZS_LOG_WARNING(Detail, log("peer location is not known by stack"))
            return false;
          }

          if (!(peerLocation->isConnected())) {
            ZS_LOG_WARNING(Detail, log("peer location is not known by stack"))
            return false;
          }

          if (!mPeerAliveTimer) {
            ZS_LOG_DEBUG(log("peer alive timer is required (thus starting the timer)"))
            mPeerAliveTimer = Timer::create(mThisTimerDelegate, Seconds(OPENPEER_CALL_CALL_CHECK_PEER_ALIVE_TIMER_IN_SECONDS));
          }

          if (ICall::CallState_Hold != mCurrentState) {
            // this call must be in focus...
            ZS_LOG_DEBUG(log("setting focus to this call?") + ZS_PARAM("focus", !mMediaHolding.get()))
            ICallAsyncProxy::create(mThisCallAsyncMediaQueue)->onSetFocus(!mMediaHolding.get());
          } else {
            ZS_LOG_DEBUG(log("this call state is holding thus it should not have focus"))
            ICallAsyncProxy::create(mThisCallAsyncMediaQueue)->onSetFocus(false);
          }

          return true;
        }

        ZS_LOG_DEBUG(log("this call does not have any picked remote locatio thus it should not have focus"))
        ICallAsyncProxy::create(mThisCallAsyncMediaQueue)->onSetFocus(false);

        if (mPeerAliveTimer) {
          ZS_LOG_DEBUG(log("peer alive timer is not required (thus stopping the timer)"))
          mPeerAliveTimer->cancel();
          mPeerAliveTimer.reset();
        }
        return true;
      }

      //-----------------------------------------------------------------------
      bool Call::stepFixCandidates()
      {
        bool changed = false;

        if (hasAudio()) {
          if (mAudioSocket.get()) {
            String version = mAudioSocket.get()->getLocalCandidatesVersion();
            changed = changed || (version != mAudioCandidateVersion);
          }
        }
        if (hasVideo()) {
          if (mVideoSocket.get()) {
            String version = mVideoSocket.get()->getLocalCandidatesVersion();
            changed = changed || (version != mVideoCandidateVersion);
          }
        }

        if (!changed) {
          ZS_LOG_TRACE(log("candidate version has not changed thus no need to update dialog"))
          return true;
        }

        ZS_LOG_DEBUG(log("candidates have changed thus need to update dialog"))
        updateDialog();

        return true;
      }

      //-----------------------------------------------------------------------
      bool Call::stepCloseLocations(CallLocationList &locationsToClose)
      {
        if (locationsToClose.size() > 0) {
          ZS_LOG_DEBUG(log("since locations were closed we must invoke a step to cleanup properly"))
          IWakeDelegateProxy::create(mThisWakeDelegate)->onWake();
        }

        // force a closing of all these call locations...
        for (CallLocationList::iterator iter = locationsToClose.begin(); iter != locationsToClose.end(); ++iter)
        {
          CallLocationPtr &callLocation = (*iter);
          ZS_LOG_DEBUG(log("forcing call location to close") + CallLocation::toDebug(callLocation, true, false))
          callLocation->close();
        }
        return true;
      }

      //-----------------------------------------------------------------------
      void Call::step()
      {
        typedef Dialog::DialogStates DialogStates;

        AutoRecursiveLock lock(mStepLock);

        CallLocationPtr picked;
        CallLocationPtr early;
        bool mediaHolding = false;

        CallLocationList locationsToClose;
        LocationDialogMap locationDialogMap;
        UseConversationThreadPtr thread = mConversationThread.lock();

        // scope: object
        {
          AutoRecursiveLock lock(mLock);
          if ((isShuttingdown()) ||
              (isShutdown())) {
            ZS_LOG_DEBUG(log("step called but call is shutting down instead thus redirecting to cancel()"))
            cancel();
            return;
          }

          if (CallClosedReason_None != mClosedReason) {
            ZS_LOG_WARNING(Detail, log("call close reason set thus call must be shutdown") + ZS_PARAM("reason", ICall::toString(mClosedReason)))
            goto call_closed_exception;
          }
        }

        ZS_LOG_DEBUG(log("step") + toDebug(true))

        // scope: media
        try
        {
          if (!stepIsMediaReady()) {
            ZS_LOG_DEBUG(log("waiting for media to be ready"))
            return;
          }

          picked = mPickedLocation.get();
          early = mEarlyLocation.get();
          mediaHolding = mMediaHolding.get();

        } catch(Exceptions::StepFailure &) {
          ZS_LOG_WARNING(Detail, log("failed to properly setup call object during media setup (thus shutting down)"))
          setClosedReason(CallClosedReason_ServerInternalError);
          goto call_closed_exception;
        }

        // scope: object
        try
        {
          if (!stepPrepareCallFirstTime()) {
            ZS_LOG_DEBUG(log("prepare first call caused call to close"))
            goto call_closed_exception;
          }
        } catch(Exceptions::StepFailure &) {
          ZS_LOG_WARNING(Detail, log("failed to properly setup call object during object setup (thus shutting down)"))
          setClosedReason(CallClosedReason_ServerInternalError);
          goto call_closed_exception;
        }

        ZS_LOG_DEBUG(log("gathering dialog replies"))

        // examine what is going on in the conversation thread...
        thread->gatherDialogReplies(mCallID, locationDialogMap);

        ZS_LOG_DEBUG(log("gathering dialog replies has completed") + ZS_PARAM("total found", locationDialogMap.size()))

        try
        {
          AutoRecursiveLock lock(mLock);
          if ((isShuttingdown()) ||
              (isShutdown())) {
            ZS_THROW_CUSTOM(Exceptions::StepFailure, log("call unexpectedly shutdown mid step"))
          }

          UseAccountPtr account = mAccount.lock();
          if (!account) {
            setClosedReason(CallClosedReason_ServerInternalError);
            ZS_THROW_CUSTOM(Exceptions::CallClosed, log("acount is now gone thus call must close"))
          }

          if (!stepPrepareCallLocations(locationDialogMap, locationsToClose)) {
            ZS_LOG_DEBUG(log("preparing call locations caused call to close"))
            goto call_closed_exception;
          }

          if (!stepVerifyCallState()) {
            ZS_LOG_DEBUG(log("verifying call state caused call to close"))
            goto call_closed_exception;
          }

          if (!stepTryToPickALocation(locationsToClose)) {
            ZS_LOG_DEBUG(log("trying to pick a location caused the call to close"))
            goto call_closed_exception;
          }

          if (!stepHandlePickedLocation()) {
            ZS_LOG_DEBUG(log("handling picked call location caused call to close"))
            goto call_closed_exception;
          }

          if (!stepFixCallInProgressStates()) {
            ZS_LOG_DEBUG(log("fix call in progress state caused call to close"))
            goto call_closed_exception;
          }

          if (!stepFixCandidates()) {
            ZS_LOG_WARNING(Debug, log("fix candidates caused call to close"))
            goto call_closed_exception;
          }

        } catch(Exceptions::StepFailure &) {
          ZS_LOG_WARNING(Debug, log("failed to properly setup call object during media setup thus shutting down"))
          setClosedReason(CallClosedReason_ServerInternalError);
          goto call_closed_exception;
        } catch(Exceptions::IllegalState &) {
          ZS_LOG_WARNING(Debug, log("call was not in legal state thus shutting down"))
          setClosedReason(CallClosedReason_ServerInternalError);
          goto call_closed_exception;
        } catch(Exceptions::CallClosed &) {
          ZS_LOG_WARNING(Debug, log("call was closed thus shutting down"))
          setClosedReason(CallClosedReason_User);
          goto call_closed_exception;
        }


        // scope: media
        {
          bool nowMediaHolding = mMediaHolding.get();
          CallLocationPtr nowPicked = mPickedLocation.get();
          CallLocationPtr nowEarly = mEarlyLocation.get();

          if (mediaHolding != nowMediaHolding) {
            ZS_LOG_DEBUG(log("changing media holding state") + ZS_PARAM("now holding", nowMediaHolding))
          }
          if ((nowEarly) && (early != nowEarly)) {
            ZS_LOG_DEBUG(log("early location now has a picked location") + CallLocation::toDebug(nowEarly, false, true))
          }
          if ((nowPicked) && (picked != nowPicked)) {
            ZS_LOG_DEBUG(log("this call now has a picked location") + CallLocation::toDebug(nowPicked, false, true))
            mEarlyLocation.set(CallLocationPtr());
          }
        }

        if (!stepCloseLocations(locationsToClose)) {
          ZS_LOG_DEBUG(log("closing locations caused the call to close"))
          goto call_closed_exception;
        }

        ZS_LOG_DEBUG(log("step completed") + toDebug(true))
        return;

      call_closed_exception:
        {
          ZS_LOG_WARNING(Debug, log("call was closed thus shutting down (likely be normal hangup/refuse call)"))
          setClosedReason(CallClosedReason_User);
          cancel();
        }
      }

      //-----------------------------------------------------------------------
      void Call::setCurrentState(
                                 CallStates state,
                                 bool forceOverride
                                 )
      {
        typedef Dialog::DescriptionList DescriptionList;
        typedef Dialog::DialogStates DialogStates;

        AutoRecursiveLock lock(mLock);

        if (mLocalOnHold) {
          switch (state) {
            case CallState_Open:
            case CallState_Active:
            case CallState_Inactive:
            case CallState_Hold:
            {
              ZS_LOG_DEBUG(log("call state is being forced to be treated as 'hold' state") + ZS_PARAM("was state", ICall::toString(state)))
              state = CallState_Hold;
              break;
            }
            default:  break;
          }
        }

        if (!forceOverride) {
          if (state == mCurrentState) return;
        }

        bool changedState = false;
        if (state != mCurrentState) {
          ZS_LOG_BASIC(log("state changed") + ZS_PARAM("new state", ICall::toString(state)) + toDebug(true))
          mCurrentState = state;
          changedState = true;
        } else {
          ZS_LOG_DEBUG(log("forcing call to state change") + ZS_PARAM("state", ICall::toString(state)) + toDebug(true))
        }

        // record when certain states changed...
        switch (mCurrentState)
        {
          case CallState_None:
          case CallState_Preparing:
          case CallState_Incoming:
          case CallState_Placed:
          case CallState_Early:       break;
          case CallState_Ringing:
          case CallState_Ringback:
          {
            if (Time() == mRingTime) {
              mRingTime = zsLib::now();
            }
            break;
          }
          case CallState_Open:
          case CallState_Active:
          case CallState_Inactive:
          case CallState_Hold:
          {
            if (Time() == mRingTime) {
              mRingTime = zsLib::now();
            }
            if (Time() == mAnswerTime) {
              mAnswerTime = zsLib::now();
            }
            break;
          }
          case CallState_Closing:
          case CallState_Closed:
          {
            if (Time() == mClosedTime) {
              mClosedTime = zsLib::now();
            }
            break;
          }
        }

        if (mDialog) {
          ZS_LOG_DEBUG(log("creating dialog state"))

          updateDialog();
        }

        CallPtr pThis = mThisWeakNoQueue.lock();
        if (pThis) {
          if (changedState) {
            ZS_LOG_DEBUG(log("notifying delegate of state change") + ZS_PARAM("state", ICall::toString(state)) + toDebug(true))
            try {
              mDelegate->onCallStateChanged(pThis, state);
            } catch (ICallDelegateProxy::Exceptions::DelegateGone &) {
              ZS_LOG_WARNING(Detail, log("delegate gone"))
              mDelegate.reset();
            }
          }
        }
      }

      //-----------------------------------------------------------------------
      void Call::updateDialog()
      {
        ZS_LOG_DEBUG(log("creating replacement dialog state"))

        DescriptionList descriptions;

        if (!mDialog) {
          ZS_LOG_DEBUG(log("candidates are being fetched"))

          //*******************************************************************
          //*******************************************************************
          //*******************************************************************
          //*******************************************************************
          // HERE - USE ORTC

          // audio description...
          if (hasAudio()) {
            if (!mAudioSocket.get()) {
              ZS_LOG_DEBUG(log("audio socket is not setup"))
              return;
            }

            DescriptionPtr desc = Description::create();
            desc->mVersion = 1;
            desc->mDescriptionID = services::IHelper::randomString(20);
            desc->mType = "audio";
            desc->mSSRC = 0;
            desc->mICEUsernameFrag = mAudioSocket.get()->getUsernameFrag();
            desc->mICEPassword = mAudioSocket.get()->getPassword();
            descriptions.push_back(desc);
          }

          // video description...
          if (hasVideo()) {
            if (!mVideoSocket.get()) {
              ZS_LOG_DEBUG(log("video socket is not setup"))
              return;
            }

            DescriptionPtr desc = Description::create();
            desc->mVersion = 1;
            desc->mDescriptionID = services::IHelper::randomString(20);
            desc->mType = "video";
            desc->mSSRC = 0;
            desc->mICEUsernameFrag = mVideoSocket.get()->getUsernameFrag();
            desc->mICEPassword = mVideoSocket.get()->getPassword();
            descriptions.push_back(desc);
          }
        } else {
          descriptions = mDialog->descriptions();
        }

        for (DescriptionList::iterator iter = descriptions.begin(); iter != descriptions.end(); ++iter)
        {
          DescriptionPtr desc = (*iter);

          String *useVersion = NULL;
          IICESocketPtr socket;

          if (desc->mType == "audio") {
            useVersion = &mAudioCandidateVersion;
            socket = mAudioSocket.get();
          } else if (desc->mType == "video") {
            useVersion = &mVideoCandidateVersion;
            socket = mVideoSocket.get();
          }

          if (NULL == useVersion) continue;
          if (!socket) continue;

          if (socket->getLocalCandidatesVersion() == (*useVersion)) continue; // no change

          String actualVersion;

          IICESocket::CandidateList tempCandidates;
          socket->getLocalCandidates(tempCandidates, &actualVersion);

          CandidateList candidates;
          stack::IHelper::convert(tempCandidates, candidates);

          desc->mCandidates = candidates;
        }

        String remoteLocationID;

        CallLocationPtr picked = mPickedLocation.get();
        if (picked) {
          remoteLocationID = picked->getLocationID();
        }

        mDialog = Dialog::create(
                                 mDialog ? mDialog->version() + 1 : 1,
                                 mCallID,
                                 internal::convert(mCurrentState),
                                 CallState_Closed == mCurrentState ? internal::convert(mClosedReason) : internal::convert(CallClosedReason_None),
                                 mCaller->getPeerURI(),
                                 (mIncomingCall ? remoteLocationID : mSelfLocation->getLocationID()),
                                 mCallee->getPeerURI(),
                                 (mIncomingCall ? mSelfLocation->getLocationID() : remoteLocationID),
                                 NULL,
                                 descriptions,
                                 mPeerFiles
                                 );

        CallPtr pThis = mThisWeakNoQueue.lock();
        if (pThis) {
          UseConversationThreadPtr thread = mConversationThread.lock();
          if (thread) {
            ZS_LOG_DEBUG(log("notifying conversation thread of dialog state change") + ZS_PARAM("state", ICall::toString(mCurrentState)) + toDebug(true))
            thread->notifyCallStateChanged(pThis);
          }
        }
      }

      //-----------------------------------------------------------------------
      void Call::setClosedReason(CallClosedReasons reason)
      {
        AutoRecursiveLock lock(mLock);
        if (CallClosedReason_None == reason) {
          ZS_LOG_DEBUG(log("cannot set call closed reason to none"))
          return;
        }
        if (CallClosedReason_None != mClosedReason) {
          ZS_LOG_WARNING(Debug, log("once the call closed reason is set it cannot be changed (probably okay)") + ZS_PARAM("current reason", ICall::toString(mClosedReason)) + ZS_PARAM("attempted to change to", ICall::toString(reason)))
          return;
        }
        ZS_LOG_DEBUG(log("call closed reason is now set") + ZS_PARAM("reason", ICall::toString(reason)))
        mClosedReason = reason;
      }

      //-----------------------------------------------------------------------
      IICESocketSubscriptionPtr Call::findSubscription(
                                                       IICESocketPtr socket,
                                                       bool &outFound,
                                                       SocketTypes *outType,
                                                       bool *outIsRTP
                                                       )
      {
        static IICESocketSubscriptionPtr bogus;

        outFound = true;
        if (outType) *outType = SocketType_Audio;
        if (outIsRTP) *outIsRTP = true;

        if (!socket) {
          ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Detail, log("received request to find a NULL socket"))
          return bogus;
        }

        if (hasAudio()) {
          if (socket == mAudioSocket.get()) {
            return mAudioRTPSocketSubscription.get();
          }
        }

        if (outType) *outType = SocketType_Video;
        if (hasVideo()) {
          if (socket == mVideoSocket.get()) {
            return mVideoRTPSocketSubscription.get();
          }
        }

        ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Detail, log("did not find socket subscription for socket") + ZS_PARAM("socket ID", socket->getID()))

        outFound = false;
        if (outType) *outType = SocketType_Audio;
        if (outIsRTP) *outIsRTP = false;
        return bogus;
      }

      //-----------------------------------------------------------------------
      bool Call::placeCallWithConversationThread()
      {
        UseConversationThreadPtr thread = mConversationThread.lock();
        if (!thread) {
          ZS_LOG_WARNING(Detail, log("unable to place call as conversation thread object is gone"))
          return false;
        }
        return thread->placeCall(mThisWeakNoQueue.lock());
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call::CallLocation
      #pragma mark

      //-----------------------------------------------------------------------
      Call::CallLocation::CallLocation(
                                       IMessageQueuePtr queue,
                                       IMessageQueuePtr mediaQueue,
                                       CallPtr outer,
                                       const char *locationID,
                                       const DialogPtr &remoteDialog,
                                       IICESocketPtr audioSocket,
                                       IICESocketPtr videoSocket
                                       ) :
        mQueue(queue),
        mMediaQueue(mediaQueue),
        mLock(outer->getLock()),
        mOuter(outer),
        mLocationID(locationID ? String(locationID) : String()),
        mRemoteDialog(remoteDialog),
        mHasAudio((bool)audioSocket),
        mHasVideo((bool)videoSocket),
        mAudioSocket(audioSocket),
        mVideoSocket(videoSocket)
      {
        mCurrentState.set(CallLocationState_Pending);
        ZS_THROW_INVALID_ARGUMENT_IF(!remoteDialog)
      }

      //-----------------------------------------------------------------------
      void Call::CallLocation::init(UseCallTransportPtr transport)
      {
        mThisWakeDelegate = IWakeDelegateProxy::create(mQueue, mThisWeakNoQueue.lock());
        mThisICESocketSessionDelegate = IICESocketSessionDelegateProxy::create(mMediaQueue, mThisWeakNoQueue.lock());

        ZS_THROW_INVALID_ARGUMENT_IF(!transport)

        CallPtr outer = mOuter.lock();
        ZS_THROW_INVALID_ASSUMPTION_IF(!outer)

        bool audioFinal = false;
        bool videoFinal = false;

        DescriptionList descriptions = mRemoteDialog->descriptions();
        for (DescriptionList::iterator iter = descriptions.begin(); iter != descriptions.end(); ++iter)
        {
          DescriptionPtr &description = (*iter);

          bool isAudio = false;
          bool isVideo = false;

          if ("audio" == description->mType) {
            isAudio = hasAudio();
            audioFinal = description->mFinal;
          } else if ("video" == description->mType) {
            isVideo = hasVideo();
            videoFinal = description->mFinal;
          }

          if ((!isAudio) && (!isVideo)) {
            ZS_LOG_WARNING(Detail, log("call location media type is not supported") + ZS_PARAM("type", description->mType))
            continue;
          }

          IICESocketPtr rtpSocket = isAudio ? mAudioSocket : mVideoSocket;
          if (!rtpSocket) {
            ZS_LOG_WARNING(Detail, log("failed to object transport's sockets for media type") + ZS_PARAM("type", description->mType))
            continue;
          }

          IICESocket::CandidateList tempCandidates;
          stack::IHelper::convert(description->mCandidates, tempCandidates);

          if (isAudio) {
            if (!mAudioRTPSocketSession.get()) {
              mAudioRTPSocketSession.set(IICESocketSession::create(mThisICESocketSessionDelegate, rtpSocket, description->mICEUsernameFrag, description->mICEPassword, tempCandidates, outer->isIncoming() ? IICESocket::ICEControl_Controlled : IICESocket::ICEControl_Controlling));
            }
          } else if (isVideo) {
            if (!mVideoRTPSocketSession.get()) {
              mVideoRTPSocketSession.set(IICESocketSession::create(mThisICESocketSessionDelegate, rtpSocket, description->mICEUsernameFrag, description->mICEPassword, tempCandidates, outer->isIncoming() ? IICESocket::ICEControl_Controlled : IICESocket::ICEControl_Controlling));
            }
          }
        }

        if (mAudioRTPSocketSession.get()) {
          mAudioRTPSocketSession.get()->setKeepAliveProperties(Seconds(OPENPEER_CALL_RTP_ICE_KEEP_ALIVE_INDICATIONS_SENT_IN_SECONDS), Seconds(OPENPEER_CALL_RTP_ICE_EXPECTING_DATA_WITHIN_IN_SECONDS), Seconds(OPENPEER_CALL_RTP_MAX_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS));
          if (audioFinal) {
            mAudioRTPSocketSession.get()->endOfRemoteCandidates();
          }
        }
        if (mVideoRTPSocketSession.get()) {
          mVideoRTPSocketSession.get()->setKeepAliveProperties(Seconds(OPENPEER_CALL_RTP_ICE_KEEP_ALIVE_INDICATIONS_SENT_IN_SECONDS), Seconds(OPENPEER_CALL_RTP_ICE_EXPECTING_DATA_WITHIN_IN_SECONDS), Seconds(OPENPEER_CALL_RTP_MAX_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS));
          if (videoFinal) {
            mVideoRTPSocketSession.get()->endOfRemoteCandidates();
          }
        }

        ZS_LOG_DEBUG(log("init completed") +
                     ZS_PARAM("audio RTP session ID", mAudioRTPSocketSession.get() ? mAudioRTPSocketSession.get()->getID() : 0) +
                     ZS_PARAM("video RTP session ID", mVideoRTPSocketSession.get() ? mVideoRTPSocketSession.get()->getID() : 0) +
                     ZS_PARAM("audio final", audioFinal) +
                     ZS_PARAM("video final", videoFinal))
      }

      //-----------------------------------------------------------------------
      Call::CallLocation::~CallLocation()
      {
        mThisWeakNoQueue.reset();
        close();
      }

      //-----------------------------------------------------------------------
      ElementPtr Call::CallLocation::toDebug(
                                             CallLocationPtr location,
                                             bool normal,
                                             bool media
                                             )
      {
        if (!location) return ElementPtr();
        return location->toDebug(normal, media);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call::CallLocation => friend Call
      #pragma mark

      //-----------------------------------------------------------------------
      Call::CallLocationPtr Call::CallLocation::create(
                                                       IMessageQueuePtr queue,
                                                       IMessageQueuePtr mediaQueue,
                                                       CallPtr outer,
                                                       UseCallTransportPtr transport,
                                                       const char *locationID,
                                                       const DialogPtr &remoteDialog,
                                                       IICESocketPtr audioSocket,
                                                       IICESocketPtr videoSocket
                                                       )
      {
        CallLocationPtr pThis(new CallLocation(queue, mediaQueue, outer, locationID, remoteDialog, audioSocket, videoSocket));
        pThis->mThisWeakNoQueue = pThis;
        pThis->init(transport);
        return pThis;
      }

      //-----------------------------------------------------------------------
      void Call::CallLocation::close()
      {
        ZS_LOG_DEBUG(log("close called on call location"))
        cancel();
      }

      //-----------------------------------------------------------------------
      Call::ICallLocation::CallLocationStates Call::CallLocation::getState() const
      {
        AutoRecursiveLock lock(mLock);
        return mCurrentState.get();
      }

      //-----------------------------------------------------------------------
      DialogPtr Call::CallLocation::getRemoteDialog() const
      {
        AutoRecursiveLock lock(mLock);
        return mRemoteDialog;
      }

      //-----------------------------------------------------------------------
      void Call::CallLocation::updateRemoteDialog(const DialogPtr &remoteDialog)
      {
        AutoRecursiveLock lock(mLock);
        if (remoteDialog != mRemoteDialog) {
          get(mChangedRemoteDialog) = true;
        }
        mRemoteDialog = remoteDialog;

        mThisWakeDelegate->onWake();
      }

      //-----------------------------------------------------------------------
      bool Call::CallLocation::sendRTPPacket(
                                             SocketTypes type,
                                             const BYTE *packet,
                                             size_t packetLengthInBytes
                                             )
      {
        IICESocketSessionPtr session;

        // scope: media
        {
          switch (type) {
            case SocketType_Audio: session = mAudioRTPSocketSession.get(); break;
            case SocketType_Video: session = mVideoRTPSocketSession.get(); break;
          }
        }

        if (!session) {
          ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("unable to send RTP packet as there is no ICE session object"))
          return false;
        }
        return session->sendPacket(packet, packetLengthInBytes);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call::CallLocation => IICESocketSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Call::CallLocation::onICESocketSessionStateChanged(
                                                              IICESocketSessionPtr inSession,
                                                              ICESocketSessionStates state
                                                              )
      {
        // scope: media
        {
          if (isClosed()) {
            ZS_LOG_WARNING(Detail, log("received notification of ICE socket change state but already closed (probably okay)"))
            return;
          }

          bool found = false;
          SocketTypes type = SocketType_Audio;
          bool wasRTP = false;

          IICESocketSessionPtr session = findSession(inSession, found, &type, &wasRTP);
          if (!found) {
            ZS_LOG_WARNING(Detail, log("ignoring ICE socket session state change on obsolete session") + ZS_PARAM("session ID", inSession->getID()))
            return;
          }

          if (IICESocketSession::ICESocketSessionState_Shutdown == state) {
            ZS_LOG_DEBUG(log("ICE socket session is shutdown") + ZS_PARAM("type", ICallForCallTransport::toString(type)) + ZS_PARAM("is RTP", wasRTP))
            session.reset();
          }
        }

        ZS_LOG_DEBUG(log("ICE socket session state changed thus invoking step"))
        mThisWakeDelegate->onWake();
      }

      //-----------------------------------------------------------------------
      void Call::CallLocation::onICESocketSessionNominationChanged(IICESocketSessionPtr inSession)
      {
        // ignored
      }

      //-----------------------------------------------------------------------
      void Call::CallLocation::handleICESocketSessionReceivedPacket(
                                                                    IICESocketSessionPtr inSession,
                                                                    const zsLib::BYTE *buffer,
                                                                    size_t bufferLengthInBytes
                                                                    )
      {
        CallPtr outer = mOuter.lock();

        if (!outer) {
          ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("ignoring ICE socket packet as call object is gone"))
          return;
        }

        bool found = false;
        SocketTypes type = SocketType_Audio;
        bool wasRTP = false;

        // scope: media
        {
          if (isClosed()) {
            ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Detail, log("received packet but already closed (probably okay)"))
            return;
          }

          findSession(inSession, found, &type, &wasRTP);
          if (!found) {
            ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("ignoring ICE socket packet from obsolete session"))
            return;
          }
        }

        outer->notifyReceivedRTPPacket(mID, type, buffer, bufferLengthInBytes);
      }

      //-----------------------------------------------------------------------
      bool Call::CallLocation::handleICESocketSessionReceivedSTUNPacket(
                                                                        IICESocketSessionPtr session,
                                                                        STUNPacketPtr stun,
                                                                        const String &localUsernameFrag,
                                                                        const String &remoteUsernameFrag
                                                                        )
      {
        // IGNORED
        return false;
      }

      //-----------------------------------------------------------------------
      void Call::CallLocation::onICESocketSessionWriteReady(IICESocketSessionPtr session)
      {
        // IGNORED
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Call::CallLocation => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Call::CallLocation::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Call::CallLocation");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr Call::CallLocation::toDebug(
                                             bool normal,
                                             bool media
                                             ) const
      {
        ElementPtr resultEl = Element::create("core::Call::CallLocation");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "location id", mLocationID);
        IHelper::debugAppend(resultEl, "audio", mHasAudio);
        IHelper::debugAppend(resultEl, "video", mHasAudio);

        if (normal)
        {
          AutoRecursiveLock lock(mLock);
          IHelper::debugAppend(resultEl, Dialog::toDebug(mRemoteDialog));
        }
        if (media)
        {
          IHelper::debugAppend(resultEl, "audio rtp socket session", mAudioRTPSocketSession.get() ? mAudioRTPSocketSession.get()->getID() : 0);
          IHelper::debugAppend(resultEl, "video rtp socket session", mVideoRTPSocketSession.get() ? mVideoRTPSocketSession.get()->getID() : 0);
        }

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void Call::CallLocation::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        // scope: object
        {
          AutoRecursiveLock lock(mLock);
          if (isClosed()) {
            ZS_LOG_DEBUG(log("cancel called but call already shutdown"))
            return;
          }
        }

        setState(CallLocationState_Closed);

        // scope: media
        {
          ZS_LOG_DEBUG(log("shutting down audio/video socket sessions"))

          if (mAudioRTPSocketSession.get()) {
            mAudioRTPSocketSession.get()->close();
            // do not reset
          }
          if (mVideoRTPSocketSession.get()) {
            mVideoRTPSocketSession.get()->close();
            // do not reset
          }
        }

        // scope: final object shutdown
        {
          //AutoRecursiveLock lock(mLock);  // uncomment if needed
        }

        // scope: final media shutdown
        {
        }
      }

      //-----------------------------------------------------------------------
      void Call::CallLocation::step()
      {
        bool changed = false;
        CandidateList audioCandidates;
        bool audioFinal = false;
        CandidateList videoCandidates;
        bool videoFinal = false;

        // scope: object
        {
          AutoRecursiveLock lock(mLock);
          if (isClosed()) {
            ZS_LOG_DEBUG(log("step called by call location is closed thus redirecting to cancel()"))
            goto cancel;
          }

          changed = mChangedRemoteDialog;
          get(mChangedRemoteDialog) = false;

          if (changed) {
            DescriptionList descriptions = mRemoteDialog->descriptions();
            for (DescriptionList::iterator iter = descriptions.begin(); iter != descriptions.end(); ++iter)
            {
              DescriptionPtr &description = (*iter);

              bool isAudio = false;
              bool isVideo = false;

              if ("audio" == description->mType) {
                isAudio = hasAudio();
              } else if ("video" == description->mType) {
                isVideo = hasVideo();
              }

              if ((!isAudio) && (!isVideo)) {
                ZS_LOG_WARNING(Detail, log("call location media type is not supported") + ZS_PARAM("type", description->mType))
                continue;
              }

              if (isAudio) {
                audioCandidates = description->mCandidates;
                audioFinal = description->mFinal;
              }
              if (isVideo) {
                videoCandidates = description->mCandidates;
                videoFinal = description->mFinal;
              }
            }
          }
        }

        if (hasAudio()) {
          if (!mAudioRTPSocketSession.get()) {
            ZS_LOG_WARNING(Detail, log("audio RTP socket session is unexpectedly gone"))
            goto cancel;
          }
        }

        if (hasVideo()) {
          if (!mVideoRTPSocketSession.get()) {
            ZS_LOG_WARNING(Detail, log("video RTP socket session is unexpectedly gone"))
            goto cancel;
          }
        }

        if (changed) {
          if (hasAudio()) {
            IICESocket::CandidateList tempCandidates;
            stack::IHelper::convert(audioCandidates, tempCandidates);

            ZS_LOG_DEBUG(log("updating remote audio candidates") + ZS_PARAM("size", tempCandidates.size()) + ZS_PARAM("final", audioFinal))

            mAudioRTPSocketSession.get()->updateRemoteCandidates(tempCandidates);
            if (audioFinal) {
              mAudioRTPSocketSession.get()->endOfRemoteCandidates();
            }
          }

          if (hasVideo()) {
            IICESocket::CandidateList tempCandidates;
            stack::IHelper::convert(videoCandidates, tempCandidates);

            ZS_LOG_DEBUG(log("updating remote video candidates") + ZS_PARAM("size", tempCandidates.size()) + ZS_PARAM("final", videoFinal))

            mVideoRTPSocketSession.get()->updateRemoteCandidates(tempCandidates);
            if (videoFinal) {
              mVideoRTPSocketSession.get()->endOfRemoteCandidates();
            }
          }
        }

        // scope: media
        {
          ZS_LOG_DEBUG(log("checking to see if media is setup"))

          if (hasAudio()) {
            IICESocketSession::ICESocketSessionStates state = mAudioRTPSocketSession.get()->getState();

            if ((IICESocketSession::ICESocketSessionState_Nominated != state) &&
                (IICESocketSession::ICESocketSessionState_Completed != state)) {
              ZS_LOG_DEBUG(log("waiting on audio RTP socket to be nominated...") + ZS_PARAM("socket session ID", mAudioRTPSocketSession.get()->getID()))
              return;
            }
          }

          if (hasVideo()) {
            IICESocketSession::ICESocketSessionStates state = mVideoRTPSocketSession.get()->getState();

            if (mVideoRTPSocketSession.get()) {
              if ((IICESocketSession::ICESocketSessionState_Nominated != state) &&
                  (IICESocketSession::ICESocketSessionState_Completed != state)) {
                ZS_LOG_DEBUG(log("waiting on video RTP socket to be nominated...") + ZS_PARAM("socket session ID", mVideoRTPSocketSession.get()->getID()))
                return;
              }
            }
          }
        }

        setState(CallLocationState_Ready);

        return;

      cancel:
        cancel();
      }

      //-----------------------------------------------------------------------
      void Call::CallLocation::setState(CallLocationStates state)
      {
        // scope: object
        {
          AutoRecursiveLock lock(mLock);
          if (state <= mCurrentState.get()) return;

          ZS_LOG_DETAIL(log("state changed") + ZS_PARAM("old state", toString(mCurrentState.get())) + ZS_PARAM("new state", toString(state)))

          mCurrentState.set(state);
        }

        CallPtr outer = mOuter.lock();
        if (outer) {
          CallLocationPtr pThis = mThisWeakNoQueue.lock();
          if (pThis) {
            outer->notifyStateChanged(pThis, state);
          }
        }
      }

      //-----------------------------------------------------------------------
      IICESocketSessionPtr Call::CallLocation::findSession(
                                                           IICESocketSessionPtr session,
                                                           bool &outFound,
                                                           SocketTypes *outType,
                                                           bool *outIsRTP
                                                           )
      {
        static IICESocketSessionPtr bogus;

        outFound = true;
        if (outType) *outType = SocketType_Audio;
        if (outIsRTP) *outIsRTP = true;

        if (!session) return bogus;

        if (hasAudio()) {
          if (session == mAudioRTPSocketSession.get()) {
            return mAudioRTPSocketSession.get();
          }
        }

        if (outType) *outType = SocketType_Video;
        if (hasVideo()) {
          if (session == mVideoRTPSocketSession.get()) {
            return mVideoRTPSocketSession.get();
          }
        }

        ZS_LOG_SUBSYSTEM_WARNING(mediaSubsystem(), Trace, log("did not find socket session thus returning bogus session") + ZS_PARAM("socket session ID", session->getID()))

        outFound = false;
        if (outType) *outType = SocketType_Audio;
        if (outIsRTP) *outIsRTP = false;
        return bogus;
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ICall
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr ICall::toDebug(ICallPtr call)
    {
      return internal::Call::toDebug(call);
    }

    //-------------------------------------------------------------------------
    const char *ICall::toString(CallStates state)
    {
      return internal::thread::Dialog::toString(internal::convert(state));
    }

    //-------------------------------------------------------------------------
    const char *ICall::toString(CallClosedReasons reason)
    {
      return internal::thread::Dialog::toString(internal::convert(reason));
    }

    //-------------------------------------------------------------------------
    ICallPtr ICall::placeCall(
                              IConversationThreadPtr conversationThread,
                              IContactPtr toContact,
                              bool includeAudio,
                              bool includeVideo
                              )
    {
      return internal::ICallFactory::singleton().placeCall(internal::ConversationThread::convert(conversationThread), toContact, includeAudio, includeVideo);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
  }
}
