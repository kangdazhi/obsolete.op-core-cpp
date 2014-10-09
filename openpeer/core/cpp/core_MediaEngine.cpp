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

#include <openpeer/core/internal/core_MediaEngine.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_thread.h>
#include <openpeer/core/ILogger.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#include <boost/thread.hpp>

#include <video_capture_factory.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef _ANDROID
#include <pthread.h>
#include <android/log.h>
#endif

#ifdef TARGET_OS_IPHONE
#include <sys/sysctl.h>
#endif

#define OPENPEER_MEDIA_ENGINE_VOICE_CODEC_ISAC
//#define OPENPEER_MEDIA_ENGINE_VOICE_CODEC_OPUS
#define OPENPEER_MEDIA_ENGINE_INVALID_CAPTURE (-1)
#define OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL (-1)
#define OPENPEER_MEDIA_ENGINE_MTU (576)

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_webrtc) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(MediaEngine::ForCallTransport, ForCallTransport)

      typedef IStackForInternal UseStack;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

      typedef zsLib::ThreadPtr ThreadPtr;
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMediaEngineForStack
      #pragma mark

      //-----------------------------------------------------------------------
      void IMediaEngineForStack::setup(IMediaEngineDelegatePtr delegate)
      {
        MediaEngine::setup(delegate);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMediaEngineForCallTransport
      #pragma mark

      //-----------------------------------------------------------------------
      ForCallTransportPtr IMediaEngineForCallTransport::singleton()
      {
        return MediaEngine::singleton();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine
      #pragma mark

      //-----------------------------------------------------------------------
      MediaEngine::MediaEngine(
                               IMessageQueuePtr queue,
                               IMediaEngineDelegatePtr delegate
                               ) :
        MessageQueueAssociator(queue),
        mError(0),
        mMtu(OPENPEER_MEDIA_ENGINE_MTU),
        mID(zsLib::createPUID()),
        mDelegate(IMediaEngineDelegateProxy::createWeak(delegate)),
        mEcEnabled(false),
        mAgcEnabled(false),
        mNsEnabled(false),
        mVoiceRecordFile(""),
        mMuteEnabled(false),
        mLoudspeakerEnabled(false),
        mDefaultVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mRecordVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mFaceDetection(false),
        mCameraType(CameraType_Front),
        mCaptureRenderView(NULL),
        mChannelRenderView(NULL),
        mCaptureRenderViewCropLeft(0.0F),
        mCaptureRenderViewCropTop(0.0F),
        mCaptureRenderViewCropRight(1.0F),
        mCaptureRenderViewCropBottom(1.0F),
        mChannelRenderViewCropLeft(0.0F),
        mChannelRenderViewCropTop(0.0F),
        mChannelRenderViewCropRight(1.0F),
        mChannelRenderViewCropBottom(1.0F),
        mContinuousVideoCapture(true),
        mVoiceChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
        mVoiceTransport(&mRedirectVoiceTransport),
        mVoiceExternalTransport(NULL),
        mVideoChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
        mVideoTransport(&mRedirectVideoTransport),
        mVideoExternalTransport(NULL),
        mCaptureId(OPENPEER_MEDIA_ENGINE_INVALID_CAPTURE),
        mVoiceEngine(NULL),
        mVoiceBase(NULL),
        mVoiceCodec(NULL),
        mVoiceNetwork(NULL),
        mVoiceRtpRtcp(NULL),
        mVoiceAudioProcessing(NULL),
        mVoiceVolumeControl(NULL),
        mVoiceHardware(NULL),
        mVoiceFile(NULL),
        mVoiceEngineReady(false),
        mVcpm(NULL),
        mVideoEngine(NULL),
        mVideoBase(NULL),
        mVideoNetwork(NULL),
        mVideoRender(NULL),
        mVideoCapture(NULL),
        mVideoRtpRtcp(NULL),
        mVideoCodec(NULL),
        mVideoEngineReady(false),
        mRedirectVoiceTransport("voice"),
        mRedirectVideoTransport("video"),
        mLifetimeWantAudio(false),
        mLifetimeWantVideoCapture(false),
        mLifetimeWantVideoChannel(false),
        mLifetimeWantRecordVideoCapture(false),
        mLifetimeHasAudio(false),
        mLifetimeHasVideoCapture(false),
        mLifetimeHasVideoChannel(false),
        mLifetimeHasRecordVideoCapture(false),
        mLifetimeInProgress(false),
        mLifetimeWantEcEnabled(false),
        mLifetimeWantAgcEnabled(false),
        mLifetimeWantNsEnabled(false),
        mLifetimeWantVoiceRecordFile(""),
        mLifetimeWantMuteEnabled(false),
        mLifetimeWantLoudspeakerEnabled(false),
        mLifetimeOutputAudioRoute(OutputAudioRoute_BuiltInSpeaker),
        mLifetimeWantDefaultVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mLifetimeWantRecordVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mLifetimeWantSetVideoOrientation(false),
        mLifetimeWantFaceDetection(false),
        mLifetimeWantCameraType(CameraType_Front),
        mLifetimeWantCaptureRenderView(NULL),
        mLifetimeWantChannelRenderView(NULL),
        mLifetimeWantCaptureRenderViewCropLeft(0.0F),
        mLifetimeWantCaptureRenderViewCropTop(0.0F),
        mLifetimeWantCaptureRenderViewCropRight(1.0F),
        mLifetimeWantCaptureRenderViewCropBottom(1.0F),
        mLifetimeWantChannelRenderViewCropLeft(0.0F),
        mLifetimeWantChannelRenderViewCropTop(0.0F),
        mLifetimeWantChannelRenderViewCropRight(1.0F),
        mLifetimeWantChannelRenderViewCropBottom(1.0F),
        mLifetimeWantContinuousVideoCapture(true),
        mLifetimeWantVideoRecordFile(""),
        mLifetimeWantSaveVideoToLibrary(false),
        mLifetimeWantVoiceExternalTransport(NULL),
        mLifetimeWantVideoExternalTransport(NULL)
      {
#ifdef TARGET_OS_IPHONE
        int name[] = {CTL_HW, HW_MACHINE};
        size_t size;
        sysctl(name, 2, NULL, &size, NULL, 0);
        char *machine = (char *)malloc(size);
        sysctl(name, 2, machine, &size, NULL, 0);
        mMachineName = machine;
        free(machine);
#endif
      }
      
      MediaEngine::MediaEngine(Noop) :
        Noop(true),
        MessageQueueAssociator(IMessageQueuePtr()),
        mError(0),
        mMtu(OPENPEER_MEDIA_ENGINE_MTU),
        mID(zsLib::createPUID()),
        mEcEnabled(false),
        mAgcEnabled(false),
        mNsEnabled(false),
        mVoiceRecordFile(""),
        mMuteEnabled(false),
        mLoudspeakerEnabled(false),
        mDefaultVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mRecordVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mFaceDetection(false),
        mCameraType(CameraType_Front),
        mCaptureRenderView(NULL),
        mChannelRenderView(NULL),
        mCaptureRenderViewCropLeft(0.0F),
        mCaptureRenderViewCropTop(0.0F),
        mCaptureRenderViewCropRight(1.0F),
        mCaptureRenderViewCropBottom(1.0F),
        mChannelRenderViewCropLeft(0.0F),
        mChannelRenderViewCropTop(0.0F),
        mChannelRenderViewCropRight(1.0F),
        mChannelRenderViewCropBottom(1.0F),
        mContinuousVideoCapture(true),
        mVoiceChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
        mVoiceTransport(NULL),
        mVoiceExternalTransport(NULL),
        mVideoChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
        mVideoTransport(NULL),
        mVideoExternalTransport(NULL),
        mCaptureId(OPENPEER_MEDIA_ENGINE_INVALID_CAPTURE),
        mVoiceEngine(NULL),
        mVoiceBase(NULL),
        mVoiceCodec(NULL),
        mVoiceNetwork(NULL),
        mVoiceRtpRtcp(NULL),
        mVoiceAudioProcessing(NULL),
        mVoiceVolumeControl(NULL),
        mVoiceHardware(NULL),
        mVoiceFile(NULL),
        mVoiceEngineReady(false),
        mVcpm(NULL),
        mVideoEngine(NULL),
        mVideoBase(NULL),
        mVideoNetwork(NULL),
        mVideoRender(NULL),
        mVideoCapture(NULL),
        mVideoRtpRtcp(NULL),
        mVideoCodec(NULL),
        mVideoEngineReady(false),
        mRedirectVoiceTransport("voice"),
        mRedirectVideoTransport("video"),
        mLifetimeWantAudio(false),
        mLifetimeWantVideoCapture(false),
        mLifetimeWantVideoChannel(false),
        mLifetimeWantRecordVideoCapture(false),
        mLifetimeHasAudio(false),
        mLifetimeHasVideoCapture(false),
        mLifetimeHasVideoChannel(false),
        mLifetimeHasRecordVideoCapture(false),
        mLifetimeInProgress(false),
        mLifetimeWantEcEnabled(false),
        mLifetimeWantAgcEnabled(false),
        mLifetimeWantNsEnabled(false),
        mLifetimeWantVoiceRecordFile(""),
        mLifetimeWantMuteEnabled(false),
        mLifetimeWantLoudspeakerEnabled(false),
        mLifetimeOutputAudioRoute(OutputAudioRoute_BuiltInSpeaker),
        mLifetimeWantDefaultVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mLifetimeWantRecordVideoOrientation(IMediaEngine::VideoOrientation_LandscapeLeft),
        mLifetimeWantSetVideoOrientation(false),
        mLifetimeWantFaceDetection(false),
        mLifetimeWantCameraType(CameraType_Front),
        mLifetimeWantCaptureRenderView(NULL),
        mLifetimeWantChannelRenderView(NULL),
        mLifetimeWantCaptureRenderViewCropLeft(0.0F),
        mLifetimeWantCaptureRenderViewCropTop(0.0F),
        mLifetimeWantCaptureRenderViewCropRight(1.0F),
        mLifetimeWantCaptureRenderViewCropBottom(1.0F),
        mLifetimeWantChannelRenderViewCropLeft(0.0F),
        mLifetimeWantChannelRenderViewCropTop(0.0F),
        mLifetimeWantChannelRenderViewCropRight(1.0F),
        mLifetimeWantChannelRenderViewCropBottom(1.0F),
        mLifetimeWantContinuousVideoCapture(true),
        mLifetimeWantVideoRecordFile(""),
        mLifetimeWantSaveVideoToLibrary(false),
        mLifetimeWantVoiceExternalTransport(NULL),
        mLifetimeWantVideoExternalTransport(NULL)
      {
#ifdef TARGET_OS_IPHONE
        int name[] = {CTL_HW, HW_MACHINE};
        size_t size;
        sysctl(name, 2, NULL, &size, NULL, 0);
        char *machine = (char *)malloc(size);
        sysctl(name, 2, machine, &size, NULL, 0);
        mMachineName = machine;
        free(machine);
#endif
      }

      //-----------------------------------------------------------------------
      MediaEngine::~MediaEngine()
      {
        if(isNoop()) return;
        
        destroyMediaEngine();
      }

      //-----------------------------------------------------------------------
      void MediaEngine::init()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("init media engine"))

        mVoiceEngine = webrtc::VoiceEngine::Create();
        if (mVoiceEngine == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to create voice engine"))
          return;
        }
        mVoiceBase = webrtc::VoEBase::GetInterface(mVoiceEngine);
        if (mVoiceBase == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice base"))
          return;
        }
        mVoiceCodec = webrtc::VoECodec::GetInterface(mVoiceEngine);
        if (mVoiceCodec == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice codec"))
          return;
        }
        mVoiceNetwork = webrtc::VoENetwork::GetInterface(mVoiceEngine);
        if (mVoiceNetwork == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice network"))
          return;
        }
        mVoiceRtpRtcp = webrtc::VoERTP_RTCP::GetInterface(mVoiceEngine);
        if (mVoiceRtpRtcp == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice RTP/RTCP"))
          return;
        }
        mVoiceAudioProcessing = webrtc::VoEAudioProcessing::GetInterface(mVoiceEngine);
        if (mVoiceAudioProcessing == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for audio processing"))
          return;
        }
        mVoiceVolumeControl = webrtc::VoEVolumeControl::GetInterface(mVoiceEngine);
        if (mVoiceVolumeControl == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for volume control"))
          return;
        }
        mVoiceHardware = webrtc::VoEHardware::GetInterface(mVoiceEngine);
        if (mVoiceHardware == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for audio hardware"))
          return;
        }
        mVoiceFile = webrtc::VoEFile::GetInterface(mVoiceEngine);
        if (mVoiceFile == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice file"))
          return;
        }

        mError = mVoiceBase->Init();
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to initialize voice base") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        } else if (mVoiceBase->LastError() > 0) {
          ZS_LOG_WARNING(Detail, log("an error has occured during voice base init") + ZS_PARAM("error", mVoiceBase->LastError()))
        }
        mError = mVoiceBase->RegisterVoiceEngineObserver(*this);
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to register voice engine observer") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }

        mVideoEngine = webrtc::VideoEngine::Create();
        if (mVideoEngine == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to create video engine"))
          return;
        }

        mVideoBase = webrtc::ViEBase::GetInterface(mVideoEngine);
        if (mVideoBase == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video base"))
          return;
        }
        mVideoCapture = webrtc::ViECapture::GetInterface(mVideoEngine);
        if (mVideoCapture == NULL) {
          ZS_LOG_ERROR(Detail, log("failed get interface for video capture"))
          return;
        }
        mVideoRtpRtcp = webrtc::ViERTP_RTCP::GetInterface(mVideoEngine);
        if (mVideoRtpRtcp == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video RTP/RTCP"))
          return;
        }
        mVideoNetwork = webrtc::ViENetwork::GetInterface(mVideoEngine);
        if (mVideoNetwork == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video network"))
          return;
        }
        mVideoRender = webrtc::ViERender::GetInterface(mVideoEngine);
        if (mVideoRender == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video render"))
          return;
        }
        mVideoCodec = webrtc::ViECodec::GetInterface(mVideoEngine);
        if (mVideoCodec == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video codec"))
          return;
        }
#if 0
        mVideoFile = webrtc::ViEFile::GetInterface(mVideoEngine);
        if (mVideoFile == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video file"))
          return;
        }
#endif
        
        mError = mVideoBase->Init();
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to initialize video base") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        } else if (mVideoBase->LastError() > 0) {
          ZS_LOG_WARNING(Detail, log("an error has occured during video base init") + ZS_PARAM("error", mVideoBase->LastError()))
        }

        mError = mVideoBase->SetVoiceEngine(mVoiceEngine);
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to set voice engine for video base") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
        setLogLevel();

        Log::Level logLevel = ZS_GET_LOG_LEVEL();

        unsigned int traceFilter;
        switch (logLevel) {
          case Log::None:
            traceFilter = webrtc::kTraceNone;
            break;
          case Log::Basic:
            traceFilter = webrtc::kTraceWarning | webrtc::kTraceError | webrtc::kTraceCritical;
            break;
          case Log::Detail:
            traceFilter = webrtc::kTraceStateInfo | webrtc::kTraceWarning | webrtc::kTraceError | webrtc::kTraceCritical | webrtc::kTraceApiCall;
            break;
          case Log::Debug:
            traceFilter = webrtc::kTraceDefault | webrtc::kTraceDebug | webrtc::kTraceInfo;
            break;
          case Log::Trace:
            traceFilter = webrtc::kTraceAll;
            break;
          default:
            traceFilter = webrtc::kTraceNone;
            break;
        }

        if (logLevel != Log::None) {
          mError = mVoiceEngine->SetTraceFilter(traceFilter);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace filter for voice") + ZS_PARAM("error", mVoiceBase->LastError()))
            return;
          }
          mError = mVoiceEngine->SetTraceCallback(this);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace callback for voice") + ZS_PARAM("error", mVoiceBase->LastError()))
            return;
          }
          mError = mVideoEngine->SetTraceFilter(traceFilter);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace filter for video") + ZS_PARAM("error", mVideoBase->LastError()))
            return;
          }
          mError = mVideoEngine->SetTraceCallback(this);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace callback for video") + ZS_PARAM("error", mVideoBase->LastError()))
            return;
          }
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::destroyMediaEngine()
      {
        // scope: delete voice engine
        {
          if (mVoiceBase) {
            mError = mVoiceBase->DeRegisterVoiceEngineObserver();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to deregister voice engine observer") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
            mError = mVoiceBase->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice base") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
          }
          
          if (mVoiceCodec) {
            mError = mVoiceCodec->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice codec") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
          }
          
          if (mVoiceNetwork) {
            mError = mVoiceNetwork->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice network") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
          }
          
          if (mVoiceRtpRtcp) {
            mError = mVoiceRtpRtcp->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice RTP/RTCP") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
          }
          
          if (mVoiceAudioProcessing) {
            mError = mVoiceAudioProcessing->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release audio processing") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
          }
          
          if (mVoiceVolumeControl) {
            mError = mVoiceVolumeControl->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release volume control") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
          }
          
          if (mVoiceHardware) {
            mError = mVoiceHardware->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release audio hardware") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
          }
          
          if (mVoiceFile) {
            mError = mVoiceFile->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice file") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
          }
          
          if (!VoiceEngine::Delete(mVoiceEngine)) {
            ZS_LOG_ERROR(Detail, log("failed to delete voice engine"))
            return;
          }
        }
        
        // scope; delete video engine
        {
          if (mVideoBase) {
            mError = mVideoBase->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video base") + ZS_PARAM("error", mVideoBase->LastError()))
              return;
            }
          }
          
          if (mVideoNetwork) {
            mError = mVideoNetwork->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video network") + ZS_PARAM("error", mVideoBase->LastError()))
              return;
            }
          }
          
          if (mVideoRender) {
            mError = mVideoRender->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video render") + ZS_PARAM("error", mVideoBase->LastError()))
              return;
            }
          }
          
          if (mVideoCapture) {
            mError = mVideoCapture->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video capture") + ZS_PARAM("error", mVideoBase->LastError()))
              return;
            }
          }
          
          if (mVideoRtpRtcp) {
            mError = mVideoRtpRtcp->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video RTP/RTCP") + ZS_PARAM("error", mVideoBase->LastError()))
              return;
            }
          }
          
          if (mVideoCodec) {
            mError = mVideoCodec->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video codec") + ZS_PARAM("error", mVideoBase->LastError()))
              return;
            }
          }
#if 0
          if (mVideoFile) {
            mError = mVideoFile->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video file") + ZS_PARAM("error", mVideoBase->LastError()))
              return;
            }
          }
#endif
          if (!VideoEngine::Delete(mVideoEngine)) {
            ZS_LOG_ERROR(Detail, log("failed to delete video engine"))
            return;
          }
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setLogLevel()
      {
      }

      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => IMediaEngine
      #pragma mark

      //-----------------------------------------------------------------------
      MediaEnginePtr MediaEngine::create(IMediaEngineDelegatePtr delegate)
      {
        MediaEnginePtr pThis(new MediaEngine(UseStack::queueCore(), delegate));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      MediaEnginePtr MediaEngine::singleton(IMediaEngineDelegatePtr delegate)
      {
        static SingletonLazySharedPtr<MediaEngine> singleton(IMediaEngineFactory::singleton().createMediaEngine(delegate));
        MediaEnginePtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Debug, slog("singleton gone"))
        }
        return result;
      }

      //-------------------------------------------------------------------------
      void MediaEngine::setDefaultVideoOrientation(VideoOrientations orientation)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("set default video orientation") + ZS_PARAM("orientation", IMediaEngine::toString(orientation)))
          
          mLifetimeWantDefaultVideoOrientation = orientation;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-------------------------------------------------------------------------
      MediaEngine::VideoOrientations MediaEngine::getDefaultVideoOrientation()
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get default video orientation"))

        return mLifetimeWantDefaultVideoOrientation;
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::setRecordVideoOrientation(VideoOrientations orientation)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("set record video orientation") + ZS_PARAM("orientation", IMediaEngine::toString(orientation)))
          
          mLifetimeWantRecordVideoOrientation = orientation;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-------------------------------------------------------------------------
      MediaEngine::VideoOrientations MediaEngine::getRecordVideoOrientation()
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get record video orientation"))
        
        return mLifetimeWantRecordVideoOrientation;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setVideoOrientation()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantSetVideoOrientation = true;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setCaptureRenderView(void *renderView)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("set capture render view"))
          
          mLifetimeWantCaptureRenderView = renderView;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void *MediaEngine::getCaptureRenderView() const
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get capture render view"))
        
        return mLifetimeWantCaptureRenderView;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setChannelRenderView(void *renderView)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("set channel render view"))
          
          mLifetimeWantChannelRenderView = renderView;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void *MediaEngine::getChannelRenderView() const
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get channel render view"))
        
        return mLifetimeWantChannelRenderView;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setCaptureCapability(CaptureCapability capability, CameraTypes cameraType)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          if (cameraType == CameraType_Front)
            mLifetimeWantFrontCameraCaptureCapability = capability;
          else if (cameraType == CameraType_Back)
            mLifetimeWantBackCameraCaptureCapability = capability;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));

      }
      
      //-----------------------------------------------------------------------
      MediaEngine::CaptureCapabilityList MediaEngine::getCaptureCapabilities(CameraTypes cameraType)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          if (!mLifetimeInProgress) {
            mLifetimeInProgress = true;
          } else {
            ZS_LOG_WARNING(Debug, log("get capture capabilities - cached value returned"))
            if (cameraType == CameraType_Front)
              return mLifetimeFrontCameraCaptureCapabilityList;
            else if (cameraType == CameraType_Back)
              return mLifetimeBackCameraCaptureCapabilityList;
          }
        }
        
        bool lockAcquired = mLock.try_lock();
        
        if (!lockAcquired) {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeInProgress = false;
          ZS_LOG_WARNING(Debug, log("get capture capabilities - cached value returned"))
          if (cameraType == CameraType_Front)
            return mLifetimeFrontCameraCaptureCapabilityList;
          else if (cameraType == CameraType_Back)
            return mLifetimeBackCameraCaptureCapabilityList;
        }
        
        CaptureCapabilityList capabilityList = internalGetCaptureCapabilities(cameraType);
        
        mLock.unlock();
        
        {
          AutoRecursiveLock lock(mLifetimeLock);
          if (cameraType == CameraType_Front) {
            mLifetimeFrontCameraCaptureCapabilityList = capabilityList;
            mLifetimeInProgress = false;
            return mLifetimeFrontCameraCaptureCapabilityList;
          } else if (cameraType == CameraType_Back) {
            mLifetimeBackCameraCaptureCapabilityList = capabilityList;
            mLifetimeInProgress = false;
            return mLifetimeBackCameraCaptureCapabilityList;
          } else {
            return CaptureCapabilityList();
          }
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setCaptureRenderViewCropping(
                                                     float left,
                                                     float top,
                                                     float right,
                                                     float bottom
                                                     )
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantCaptureRenderViewCropLeft = left;
          mLifetimeWantCaptureRenderViewCropTop = top;
          mLifetimeWantCaptureRenderViewCropRight = right;
          mLifetimeWantCaptureRenderViewCropBottom = bottom;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setChannelRenderViewCropping(
                                                     float left,
                                                     float top,
                                                     float right,
                                                     float bottom
                                                     )
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantChannelRenderViewCropLeft = left;
          mLifetimeWantChannelRenderViewCropTop = top;
          mLifetimeWantChannelRenderViewCropRight = right;
          mLifetimeWantChannelRenderViewCropBottom = bottom;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setEcEnabled(bool enabled)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantEcEnabled = enabled;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setAgcEnabled(bool enabled)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantAgcEnabled = enabled;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setNsEnabled(bool enabled)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantNsEnabled = enabled;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::setVoiceRecordFile(String fileName)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("set voice record file") + ZS_PARAM("value", fileName))
          
          mLifetimeWantVoiceRecordFile = fileName;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-------------------------------------------------------------------------
      String MediaEngine::getVoiceRecordFile() const
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get voice record file") + ZS_PARAM("value", mVoiceRecordFile))
        
        return mLifetimeWantVoiceRecordFile;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setMuteEnabled(bool enabled)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantMuteEnabled = enabled;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngine::getMuteEnabled()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          if (!mLifetimeInProgress) {
            mLifetimeInProgress = true;
          } else {
            ZS_LOG_WARNING(Debug, log("get mute enabled - cached value returned"))
            return mLifetimeWantMuteEnabled;
          }
        }
        
        bool lockAcquired = mLock.try_lock();
        
        if (!lockAcquired) {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeInProgress = false;
          ZS_LOG_WARNING(Debug, log("get mute enabled - cached value returned"))
          return mLifetimeWantMuteEnabled;
        }
        
        bool enabled = internalGetMuteEnabled();
        mMuteEnabled = enabled;
        
        mLock.unlock();
        
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantMuteEnabled = enabled;
          mLifetimeInProgress = false;
          return mLifetimeWantMuteEnabled;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setLoudspeakerEnabled(bool enabled)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantLoudspeakerEnabled = enabled;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngine::getLoudspeakerEnabled()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          if (!mLifetimeInProgress) {
            mLifetimeInProgress = true;
          } else {
            ZS_LOG_WARNING(Debug, log("get loudspeaker enabled - cached value returned"))
            return mLifetimeWantLoudspeakerEnabled;
          }
        }
        
        bool lockAcquired = mLock.try_lock();
        
        if (!lockAcquired) {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeInProgress = false;
          ZS_LOG_WARNING(Debug, log("get loudspeaker enabled - cached value returned"))
          return mLifetimeWantLoudspeakerEnabled;
        }
        
        bool enabled = internalGetLoudspeakerEnabled();
        mLoudspeakerEnabled = enabled;
        
        mLock.unlock();
        
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantLoudspeakerEnabled = enabled;
          mLifetimeInProgress = false;
          return mLifetimeWantLoudspeakerEnabled;
        }
      }
      
      //-----------------------------------------------------------------------
      IMediaEngine::OutputAudioRoutes MediaEngine::getOutputAudioRoute()
      {
        bool lockAcquired = mLock.try_lock();
        
        if (!lockAcquired) {
          AutoRecursiveLock lock(mLifetimeLock);
          ZS_LOG_WARNING(Debug, log("get loudspeaker enabled - cached value returned"))
          return mLifetimeOutputAudioRoute;
        }
        
        OutputAudioRoutes route = internalGetOutputAudioRoute();
        
        mLock.unlock();
        
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeOutputAudioRoute = route;
          return mLifetimeOutputAudioRoute;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setContinuousVideoCapture(bool continuousVideoCapture)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("set continuous video capture") + ZS_PARAM("value", continuousVideoCapture))
          
          mLifetimeWantContinuousVideoCapture = continuousVideoCapture;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngine::getContinuousVideoCapture()
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get continuous video capture"))
        
        return mLifetimeWantContinuousVideoCapture;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::setFaceDetection(bool faceDetection)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("set face detection") + ZS_PARAM("value", faceDetection))
          
          mLifetimeWantFaceDetection = faceDetection;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngine::getFaceDetection()
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get face detection"))
        
        return mLifetimeWantFaceDetection;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::setCameraType(CameraTypes type)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantCameraType = type;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      IMediaEngine::CameraTypes MediaEngine::getCameraType() const
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get camera type"))
        
        return mLifetimeWantCameraType;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::startVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoCapture = true;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::stopVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoCapture = false;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::startRecordVideoCapture(String fileName, bool saveToLibrary)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantRecordVideoCapture = true;
          mLifetimeWantVideoRecordFile = fileName;
          mLifetimeWantSaveVideoToLibrary = saveToLibrary;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::stopRecordVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantRecordVideoCapture = false;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::getVideoTransportStatistics(RtpRtcpStatistics &stat)
      {
        bool lockAcquired = mLock.try_lock();
        
        if (!lockAcquired) {
          AutoRecursiveLock lock(mLifetimeLock);
          ZS_LOG_WARNING(Debug, log("get video transport statistics - cached value returned"))
          stat = mLifetimeVideoTransportStatistics;
          return 0;
        }
        
        RtpRtcpStatistics statValue;
        internalGetVideoTransportStatistics(statValue);
        
        mLock.unlock();
        
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeVideoTransportStatistics = stat = statValue;
          return 0;
        }
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::getVoiceTransportStatistics(RtpRtcpStatistics &stat)
      {
        bool lockAcquired = mLock.try_lock();
        
        if (!lockAcquired) {
          AutoRecursiveLock lock(mLifetimeLock);
          ZS_LOG_WARNING(Debug, log("get voice transport statistics - cached value returned"))
          stat = mLifetimeVoiceTransportStatistics;
          return 0;
        }
        
        RtpRtcpStatistics statValue;
        internalGetVoiceTransportStatistics(statValue);
        
        mLock.unlock();
        
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeVoiceTransportStatistics = stat = statValue;
          return 0;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::pauseVoice(bool pause)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          if (pause)
            mLifetimeWantAudio = false;
          else
            mLifetimeWantAudio = true;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => IMediaEngineForStack
      #pragma mark

      //-----------------------------------------------------------------------
      void MediaEngine::setup(IMediaEngineDelegatePtr delegate)
      {
        singleton(delegate);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => IMediaEngineForCallTransport
      #pragma mark

      //-----------------------------------------------------------------------
      void MediaEngine::startVoice()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantAudio = true;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      void MediaEngine::stopVoice()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantAudio = false;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      void MediaEngine::startVideoChannel()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoChannel = true;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      void MediaEngine::stopVideoChannel()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoChannel = false;
        }

        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }

      //-----------------------------------------------------------------------
      int MediaEngine::registerVoiceExternalTransport(Transport &transport)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("register voice external transport"))
          
          mLifetimeWantVoiceExternalTransport = &transport;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));

        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::deregisterVoiceExternalTransport()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("deregister voice external transport"))
          
          mLifetimeWantVoiceExternalTransport = NULL;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));

        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::receivedVoiceRTPPacket(const void *data, size_t length)
      {
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVoiceEngineReady)
            channel = mVoiceChannel;
        }

        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("voice channel is not ready yet"))
          return -1;
        }

        mError = mVoiceNetwork->ReceivedRTPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received voice RTP packet failed") + ZS_PARAM("error", mVoiceBase->LastError()))
          return mError;
        }

        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::receivedVoiceRTCPPacket(const void* data, size_t length)
      {
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVoiceEngineReady)
            channel = mVoiceChannel;
        }

        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("voice channel is not ready yet"))
          return -1;
        }

        mError = mVoiceNetwork->ReceivedRTCPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received voice RTCP packet failed") + ZS_PARAM("error", mVoiceBase->LastError()))
          return mError;
        }

        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::registerVideoExternalTransport(Transport &transport)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("register video external transport"))
          
          mLifetimeWantVideoExternalTransport = &transport;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));

        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::deregisterVideoExternalTransport()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          ZS_LOG_DEBUG(log("deregister video external transport"))
          
          mLifetimeWantVideoExternalTransport = NULL;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));

        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::receivedVideoRTPPacket(const void *data, size_t length)
      {
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVideoEngineReady)
            channel = mVideoChannel;
        }

        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("video channel is not ready yet"))
          return -1;
        }

        mError = mVideoNetwork->ReceivedRTPPacket(channel, data, length, webrtc::PacketTime());
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received video RTP packet failed") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }

        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::receivedVideoRTCPPacket(const void *data, size_t length)
      {
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVideoEngineReady)
            channel = mVideoChannel;
        }

        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("video channel is not ready yet"))
          return -1;
        }

        mError = mVideoNetwork->ReceivedRTCPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received video RTCP packet failed") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }

        return 0;
      }

      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => TraceCallback
      #pragma mark
      //-----------------------------------------------------------------------
      void MediaEngine::Print(const webrtc::TraceLevel level, const char *traceString, const int length)
      {
        switch (level) {
          case webrtc::kTraceApiCall:
          case webrtc::kTraceStateInfo:
            ZS_LOG_DETAIL(log(traceString))
            break;
          case webrtc::kTraceDebug:
          case webrtc::kTraceInfo:
            ZS_LOG_DEBUG(log(traceString))
            break;
          case webrtc::kTraceWarning:
            ZS_LOG_WARNING(Detail, log(traceString))
            break;
          case webrtc::kTraceError:
            ZS_LOG_ERROR(Detail, log(traceString))
            break;
          case webrtc::kTraceCritical:
            ZS_LOG_FATAL(Detail, log(traceString))
            break;
          case webrtc::kTraceModuleCall:
          case webrtc::kTraceMemory:
          case webrtc::kTraceTimer:
          case webrtc::kTraceStream:
            ZS_LOG_TRACE(log(traceString))
            break;
          default:
            ZS_LOG_TRACE(log(traceString))
            break;
        }
      }

      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => VoiceEngineObserver
      #pragma mark
      //-----------------------------------------------------------------------
      void MediaEngine::CallbackOnError(const int errCode, const int channel)
      {
        ZS_LOG_ERROR(Detail, log("Voice engine error") + ZS_PARAM("error", errCode))
      }

      //-----------------------------------------------------------------------
      void MediaEngine::CallbackOnOutputAudioRouteChange(const webrtc::OutputAudioRoute inRoute)
      {
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("audio route change callback igored as delegate was not specified"))
          return;
        }

        OutputAudioRoutes route = IMediaEngine::OutputAudioRoute_Headphone;

        switch (inRoute) {
          case webrtc::kOutputAudioRouteHeadphone:        route = IMediaEngine::OutputAudioRoute_Headphone;  break;
          case webrtc::kOutputAudioRouteBuiltInReceiver:  route = IMediaEngine::OutputAudioRoute_BuiltInReceiver; break;
          case webrtc::kOutputAudioRouteBuiltInSpeaker:   route = IMediaEngine::OutputAudioRoute_BuiltInSpeaker; break;
          default: {
            ZS_LOG_WARNING(Basic, log("media route changed to unknown type") + ZS_PARAM("value", inRoute))
            break;
          }
        }

        try {
          if (mDelegate)
            mDelegate->onMediaEngineAudioRouteChanged(route);
        } catch (IMediaEngineDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        ZS_LOG_DEBUG(log("Audio output route changed") + ZS_PARAM("route", IMediaEngine::toString(route)))
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::CallbackOnAudioSessionInterruptionBegin()
      {
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("audio session interruption began callback igored as delegate was not specified"))
          return;
        }
        
        ZS_LOG_WARNING(Detail, log("CallbackOnAudioSessionInterruptionBegin - escaped"))
        return;
        
        try {
          if (mDelegate)
            mDelegate->onMediaEngineAudioSessionInterruptionBegan();
        } catch (IMediaEngineDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
        
        ZS_LOG_DEBUG(log("Audio session interruption began"))
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::CallbackOnAudioSessionInterruptionEnd()
      {
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("audio session interruption ended callback igored as delegate was not specified"))
          return;
        }
        
        ZS_LOG_WARNING(Detail, log("CallbackOnAudioSessionInterruptionEnd - escaped"))
        return;
        
        try {
          if (mDelegate)
            mDelegate->onMediaEngineAudioSessionInterruptionEnded();
        } catch (IMediaEngineDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
        
        ZS_LOG_DEBUG(log("Audio session interruption ended"))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => ViECaptureObserver
      #pragma mark
      
      //-------------------------------------------------------------------------
      void MediaEngine::BrightnessAlarm(const int capture_id, const webrtc::Brightness brightness)
      {
        
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::CapturedFrameRate(const int capture_id, const unsigned char frame_rate)
      {
        
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::NoPictureAlarm(const int capture_id, const webrtc::CaptureAlarm alarm)
      {
        
      }
      
      //-------------------------------------------------------------------------
      void MediaEngine::FaceDetected(const int capture_id)
      {
        try {
          if (mDelegate)
            mDelegate->onMediaEngineFaceDetected();
        } catch (IMediaEngineDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      void MediaEngine::operator()()
      {
#if !defined(_ANDROID) && !defined(_LINUX)
#ifdef __QNX__
        pthread_setname_np(pthread_self(), "org.openpeer.core.mediaEngine");
#else
        pthread_setname_np("org.openpeer.core.mediaEngine");
#endif
#endif
        ZS_LOG_DEBUG(log("media engine lifetime thread spawned"))

        bool repeat = false;
        bool refreshStatus = false;

        bool firstAttempt = true;

        bool wantAudio = false;
        bool wantVideoCapture = false;
        bool wantVideoChannel = false;
        bool wantRecordVideoCapture = false;
        bool hasAudio = false;
        bool hasVideoCapture = false;
        bool hasVideoChannel = false;
        bool hasRecordVideoCapture = false;
        bool wantEcEnabled = false;
        bool wantAgcEnabled = false;
        bool wantNsEnabled = false;
        String wantVoiceRecordFile;
        bool wantMuteEnabled = false;
        bool wantLoudspeakerEnabled = false;
        OutputAudioRoutes outputAudioRoute(OutputAudioRoute_BuiltInSpeaker);
        VideoOrientations wantDefaultVideoOrientation(VideoOrientation_LandscapeLeft);
        VideoOrientations wantRecordVideoOrientation(VideoOrientation_LandscapeLeft);
        bool wantSetVideoOrientation = false;
        bool wantFaceDetection = false;
        CameraTypes wantCameraType = IMediaEngine::CameraType_None;
        void *wantCaptureRenderView = NULL;
        void *wantChannelRenderView = NULL;
        CaptureCapability wantFrontCameraCaptureCapability;
        CaptureCapability wantBackCameraCaptureCapability;
        CaptureCapabilityList frontCameraCaptureCapabilityList;
        CaptureCapabilityList backCameraCaptureCapabilityList;
        float wantCaptureRenderViewCropLeft;
        float wantCaptureRenderViewCropTop;
        float wantCaptureRenderViewCropRight;
        float wantCaptureRenderViewCropBottom;
        float wantChannelRenderViewCropLeft;
        float wantChannelRenderViewCropTop;
        float wantChannelRenderViewCropRight;
        float wantChannelRenderViewCropBottom;
        bool wantContinuousVideoCapture = false;
        String wantVideoRecordFile;
        bool wantSaveVideoToLibrary = false;
        RtpRtcpStatistics videoTransportStatistics;
        RtpRtcpStatistics voiceTransportStatistics;
        Transport *wantVoiceExternalTransport = NULL;
        Transport *wantVideoExternalTransport = NULL;

        // attempt to get the lifetime lock
        while (true)
        {
          if (!firstAttempt) {
            boost::this_thread::sleep(zsLib::Milliseconds(10));       // do not hammer CPU
          }
          firstAttempt = false;

          AutoRecursiveLock lock(mLifetimeLock);
          if (mLifetimeInProgress) {
            ZS_LOG_WARNING(Debug, log("could not obtain media lifetime lock"))
            continue;
          }

          mLifetimeInProgress = true;

          if (mLifetimeWantVideoChannel)
            mLifetimeWantVideoCapture = true;
          else if (mLifetimeHasVideoChannel && !mLifetimeWantContinuousVideoCapture)
            mLifetimeWantVideoCapture = false;
          if (!mLifetimeWantVideoCapture)
            mLifetimeWantRecordVideoCapture = false;
          wantAudio = mLifetimeWantAudio;
          wantVideoCapture = mLifetimeWantVideoCapture;
          wantVideoChannel = mLifetimeWantVideoChannel;
          wantRecordVideoCapture = mLifetimeWantRecordVideoCapture;
          hasAudio = mLifetimeHasAudio;
          hasVideoCapture = mLifetimeHasVideoCapture;
          hasVideoChannel = mLifetimeHasVideoChannel;
          hasRecordVideoCapture = mLifetimeHasRecordVideoCapture;
          wantEcEnabled = mLifetimeWantEcEnabled;
          wantAgcEnabled = mLifetimeWantAgcEnabled;
          wantNsEnabled = mLifetimeWantNsEnabled;
          wantVoiceRecordFile = mLifetimeWantVoiceRecordFile;
          wantMuteEnabled = mLifetimeWantMuteEnabled;
          wantLoudspeakerEnabled = mLifetimeWantLoudspeakerEnabled;
          wantDefaultVideoOrientation = mLifetimeWantDefaultVideoOrientation;
          wantRecordVideoOrientation = mLifetimeWantRecordVideoOrientation;
          wantSetVideoOrientation = mLifetimeWantSetVideoOrientation;
          if (wantSetVideoOrientation)
            mLifetimeWantSetVideoOrientation = false;
          wantFaceDetection = mLifetimeWantFaceDetection;
          wantCameraType = mLifetimeWantCameraType;
          wantCaptureRenderView = mLifetimeWantCaptureRenderView;
          wantChannelRenderView = mLifetimeWantChannelRenderView;
          wantFrontCameraCaptureCapability = mLifetimeWantFrontCameraCaptureCapability;
          wantBackCameraCaptureCapability = mLifetimeWantBackCameraCaptureCapability;
          wantCaptureRenderViewCropLeft = mLifetimeWantCaptureRenderViewCropLeft;
          wantCaptureRenderViewCropTop = mLifetimeWantCaptureRenderViewCropTop;
          wantCaptureRenderViewCropRight = mLifetimeWantCaptureRenderViewCropRight;
          wantCaptureRenderViewCropBottom = mLifetimeWantCaptureRenderViewCropBottom;
          wantChannelRenderViewCropLeft = mLifetimeWantChannelRenderViewCropLeft;
          wantChannelRenderViewCropTop = mLifetimeWantChannelRenderViewCropTop;
          wantChannelRenderViewCropRight = mLifetimeWantChannelRenderViewCropRight;
          wantChannelRenderViewCropBottom = mLifetimeWantChannelRenderViewCropBottom;
          wantContinuousVideoCapture = mLifetimeWantContinuousVideoCapture;
          wantVideoRecordFile = mLifetimeWantVideoRecordFile;
          wantSaveVideoToLibrary = mLifetimeWantSaveVideoToLibrary;
          wantVoiceExternalTransport = mLifetimeWantVoiceExternalTransport;
          wantVideoExternalTransport = mLifetimeWantVideoExternalTransport;
          break;
        }

        {
          AutoRecursiveLock lock(mLock);
          
          if (wantFrontCameraCaptureCapability.width != mFrontCameraCaptureCapability.width ||
              wantFrontCameraCaptureCapability.height != mFrontCameraCaptureCapability.height ||
              wantFrontCameraCaptureCapability.maxFPS != mFrontCameraCaptureCapability.maxFPS) {
            mFrontCameraCaptureCapability = wantFrontCameraCaptureCapability;
          }
          
          if (wantBackCameraCaptureCapability.width != mBackCameraCaptureCapability.width ||
              wantBackCameraCaptureCapability.height != mBackCameraCaptureCapability.height ||
              wantBackCameraCaptureCapability.maxFPS != mBackCameraCaptureCapability.maxFPS) {
            mBackCameraCaptureCapability = wantBackCameraCaptureCapability;
          }
          
          if (wantCaptureRenderViewCropLeft != mCaptureRenderViewCropLeft ||
              wantCaptureRenderViewCropTop != mCaptureRenderViewCropTop ||
              wantCaptureRenderViewCropRight != mCaptureRenderViewCropRight ||
              wantCaptureRenderViewCropBottom != mCaptureRenderViewCropBottom) {
            mCaptureRenderViewCropLeft = wantCaptureRenderViewCropLeft;
            mCaptureRenderViewCropTop = wantCaptureRenderViewCropTop;
            mCaptureRenderViewCropRight = wantCaptureRenderViewCropRight;
            mCaptureRenderViewCropBottom = wantCaptureRenderViewCropBottom;
          }
          
          if (wantChannelRenderViewCropLeft != mChannelRenderViewCropLeft ||
              wantChannelRenderViewCropTop != mChannelRenderViewCropTop ||
              wantChannelRenderViewCropRight != mChannelRenderViewCropRight ||
              wantChannelRenderViewCropBottom != mChannelRenderViewCropBottom) {
            mChannelRenderViewCropLeft = wantChannelRenderViewCropLeft;
            mChannelRenderViewCropTop = wantChannelRenderViewCropTop;
            mChannelRenderViewCropRight = wantChannelRenderViewCropRight;
            mChannelRenderViewCropBottom = wantChannelRenderViewCropBottom;
          }
          
          if (wantEcEnabled != mEcEnabled) {
            mEcEnabled = wantEcEnabled;
            internalSetEcEnabled(wantEcEnabled);
          }
          
          if (wantAgcEnabled != mAgcEnabled) {
            mAgcEnabled = wantAgcEnabled;
            internalSetAgcEnabled(wantAgcEnabled);
          }
          
          if (wantNsEnabled != mNsEnabled) {
            mNsEnabled = wantNsEnabled;
            internalSetNsEnabled(wantNsEnabled);
          }
          
          if (wantVoiceRecordFile != mVoiceRecordFile) {
            mVoiceRecordFile = wantVoiceRecordFile;
          }
          
          if (wantMuteEnabled != mMuteEnabled) {
            mMuteEnabled = wantMuteEnabled;
            internalSetMuteEnabled(wantMuteEnabled);
          }
          
          if (wantLoudspeakerEnabled != mLoudspeakerEnabled) {
            mLoudspeakerEnabled = wantLoudspeakerEnabled;
            internalSetLoudspeakerEnabled(wantLoudspeakerEnabled);
          }

          if (wantDefaultVideoOrientation != mDefaultVideoOrientation) {
            mDefaultVideoOrientation = wantDefaultVideoOrientation;
          }
          
          if (wantRecordVideoOrientation != mRecordVideoOrientation) {
            mRecordVideoOrientation = wantRecordVideoOrientation;
          }
          
          if (wantFaceDetection != mFaceDetection) {
            mFaceDetection = wantFaceDetection;
          }
          
          if (wantContinuousVideoCapture != mContinuousVideoCapture) {
            mContinuousVideoCapture = wantContinuousVideoCapture;
          }
          
          if (wantVoiceExternalTransport != mVoiceExternalTransport) {
            mVoiceExternalTransport = wantVoiceExternalTransport;
            mRedirectVoiceTransport.redirect(wantVoiceExternalTransport);
          }
          
          if (wantVideoExternalTransport != mVideoExternalTransport) {
            mVideoExternalTransport = wantVideoExternalTransport;
            mRedirectVideoTransport.redirect(wantVideoExternalTransport);
          }

          if (wantVideoCapture) {
            if (wantCameraType != mCameraType) {
              ZS_LOG_DEBUG(log("camera type needs to change") + ZS_PARAM("was", IMediaEngine::toString(mCameraType)) + ZS_PARAM("desired", IMediaEngine::toString(wantCameraType)))
              mCameraType = wantCameraType;
              if (hasVideoCapture) {
                ZS_LOG_DEBUG(log("video capture must be stopped first before camera type can be swapped (will try again)"))
                wantVideoCapture = false;  // pretend that we don't want video so it will be stopped
                repeat = true;      // repeat this thread operation again to start video back up again after
                if (hasVideoChannel) {
                  ZS_LOG_DEBUG(log("video channel must be stopped first before camera type can be swapped (will try again)"))
                  wantVideoChannel = false;  // pretend that we don't want video so it will be stopped
                }
              }
            }
          }
          
          if (wantVideoCapture) {
            if (wantCaptureRenderView != mCaptureRenderView) {
              if (hasVideoCapture) {
                if (wantCaptureRenderView == NULL || mCaptureRenderView != NULL) {
                  internalStopCaptureRenderer();
                  refreshStatus = true;
                }
                mCaptureRenderView = wantCaptureRenderView;
                if (wantCaptureRenderView != NULL) {
                  internalStartCaptureRenderer();
                  refreshStatus = true;
                }
              } else {
                mCaptureRenderView = wantCaptureRenderView;
              }
            }
          }
          
          if (wantVideoCapture) {
            if (!hasVideoCapture) {
              internalStartVideoCapture();
              refreshStatus = true;
            }
          }
          
          if (wantRecordVideoCapture) {
            if (!hasRecordVideoCapture) {
              internalStartRecordVideoCapture(wantVideoRecordFile, wantSaveVideoToLibrary);
              refreshStatus = true;
            }
          } else {
            if (hasRecordVideoCapture) {
              internalStopRecordVideoCapture();
              refreshStatus = true;
            }
          }

          if (wantAudio) {
            if (!hasAudio) {
              internalStartVoice();
              refreshStatus = true;
            }
          } else {
            if (hasAudio) {
              internalStopVoice();
              refreshStatus = true;
            }
          }

          if (wantVideoChannel) {
            if (wantChannelRenderView != mChannelRenderView) {
              if (hasVideoChannel) {
                if (wantChannelRenderView == NULL || mChannelRenderView != NULL) {
                  internalStopChannelRenderer();
                  refreshStatus = true;
                }
                mChannelRenderView = wantChannelRenderView;
                if (wantChannelRenderView != NULL) {
                  internalStartChannelRenderer();
                  refreshStatus = true;
                }
              } else {
                mChannelRenderView = wantChannelRenderView;
              }
            }
          }

          if (wantVideoChannel) {
            if (!hasVideoChannel) {
              internalStartVideoChannel();
              refreshStatus = true;
            }
          }
          
          if (wantSetVideoOrientation && hasVideoChannel) {
            internalSetVideoOrientation();
          }
          
          if (!wantVideoChannel) {
            if (hasVideoChannel) {
              internalStopVideoChannel();
              refreshStatus = true;
            }
          }
          
          if (!wantVideoCapture) {
            if (hasVideoCapture) {
              internalStopVideoCapture();
              refreshStatus = true;
            }
          }
          
          if (refreshStatus) {
            frontCameraCaptureCapabilityList = internalGetCaptureCapabilities(CameraType_Front);
            backCameraCaptureCapabilityList = internalGetCaptureCapabilities(CameraType_Back);
            wantMuteEnabled = internalGetMuteEnabled();
            wantLoudspeakerEnabled = internalGetLoudspeakerEnabled();
            outputAudioRoute = internalGetOutputAudioRoute();
            internalGetVideoTransportStatistics(videoTransportStatistics);
            internalGetVoiceTransportStatistics(voiceTransportStatistics);
          }
        }

        {
          AutoRecursiveLock lock(mLifetimeLock);

          mLifetimeHasAudio = wantAudio;
          mLifetimeHasVideoCapture = wantVideoCapture;
          mLifetimeHasVideoChannel = wantVideoChannel;
          mLifetimeHasRecordVideoCapture = wantRecordVideoCapture;
          
          if (refreshStatus) {
            // refresh lifetime status after execution of long running procedures
            mLifetimeFrontCameraCaptureCapabilityList = frontCameraCaptureCapabilityList;
            mLifetimeBackCameraCaptureCapabilityList = backCameraCaptureCapabilityList;
            mLifetimeWantMuteEnabled = wantMuteEnabled;
            mLifetimeWantLoudspeakerEnabled = wantLoudspeakerEnabled;
            mLifetimeOutputAudioRoute = outputAudioRoute;
            mLifetimeVideoTransportStatistics = videoTransportStatistics;
            mLifetimeVoiceTransportStatistics = voiceTransportStatistics;
          }

          mLifetimeInProgress = false;
        }

        if (repeat) {
          ZS_LOG_DEBUG(log("repeating media thread operation again"))
          (*this)();
          return;
        }

        ZS_LOG_DEBUG(log("media engine lifetime thread completed"))
      }
      
      //-----------------------------------------------------------------------
      MediaEngine::CaptureCapabilityList MediaEngine::internalGetCaptureCapabilities(CameraTypes cameraType)
      {
        ZS_LOG_DEBUG(log("get capture capabilities") + ZS_PARAM("camera type", IMediaEngine::toString(cameraType)))
        
        const unsigned int KMaxDeviceNameLength = 128;
        const unsigned int KMaxUniqueIdLength = 256;
        char deviceName[KMaxDeviceNameLength];
        memset(deviceName, 0, KMaxDeviceNameLength);
        char uniqueId[KMaxUniqueIdLength];
        memset(uniqueId, 0, KMaxUniqueIdLength);
        uint32_t captureIdx;
        
        webrtc::VideoCaptureModule::DeviceInfo *devInfo = webrtc::VideoCaptureFactory::CreateDeviceInfo(0);
        if (devInfo == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to create video capture device info"))
          return CaptureCapabilityList();
        }
        
        uint32_t numberOfDevices = devInfo->NumberOfDevices();
        
        if (cameraType == CameraType_Back)
        {
          if (numberOfDevices >= 2)
          {
            captureIdx = 0;
          }
          else
          {
            ZS_LOG_ERROR(Detail, log("back camera is not supported on single camera devices"))
            return CaptureCapabilityList();
          }
        }
        else if (cameraType == CameraType_Front)
        {
          if (numberOfDevices >= 2)
          {
            captureIdx = 1;
          }
          else
          {
            captureIdx = 0;
          }
        }
        else
        {
          ZS_LOG_ERROR(Detail, log("camera type is not set"))
          return CaptureCapabilityList();
        }
        
        mError = devInfo->GetDeviceName(captureIdx, deviceName,
                                        KMaxDeviceNameLength, uniqueId,
                                        KMaxUniqueIdLength);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get video device name"))
          return CaptureCapabilityList();
        }
        
        CaptureCapabilityList capabilityList;
        int capabilitiesNumber = mVideoCapture->NumberOfCapabilities(uniqueId, KMaxUniqueIdLength);
        
        for (int i = 0; i < capabilitiesNumber; i++) {
          webrtc::CaptureCapability engineCapability;
          CaptureCapability capability;
          
          mError = mVideoCapture->GetCaptureCapability(uniqueId, KMaxUniqueIdLength, i, engineCapability);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get capture capability"))
            return CaptureCapabilityList();
          }
          
          capability.width = engineCapability.width;
          capability.height = engineCapability.height;
          capability.maxFPS = engineCapability.maxFPS;
          capabilityList.push_back(capability);
        }
        
        return capabilityList;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalSetVideoOrientation()
      {
        ZS_LOG_DEBUG(log("set video orientation and codec parameters"))
        
        if (mVideoChannel == OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL) {
          mError = setVideoCaptureRotation();
        } else {
          mError = setVideoCodecParameters();
        }
      }

      //-----------------------------------------------------------------------
      void MediaEngine::internalSetEcEnabled(bool enabled)
      {
        ZS_LOG_DEBUG(log("set EC enabled") + ZS_PARAM("value", enabled))
        
        webrtc::EcModes ecMode = getEcMode();
        if (ecMode == webrtc::kEcUnchanged) {
          return;
        }
        mError = mVoiceAudioProcessing->SetEcStatus(enabled, ecMode);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller status") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        if (ecMode == webrtc::kEcAecm && enabled) {
          mError = mVoiceAudioProcessing->SetAecmMode(webrtc::kAecmSpeakerphone);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller mobile mode") + ZS_PARAM("error", mVoiceBase->LastError()))
            return;
          }
        }
        
        mEcEnabled = enabled;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalSetAgcEnabled(bool enabled)
      {
        ZS_LOG_DEBUG(log("set AGC enabled") + ZS_PARAM("value", enabled))
        
        mError = mVoiceAudioProcessing->SetAgcStatus(enabled, webrtc::kAgcAdaptiveDigital);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set automatic gain control status") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        
        mAgcEnabled = enabled;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalSetNsEnabled(bool enabled)
      {
        ZS_LOG_DEBUG(log("set NS enabled") + ZS_PARAM("value", enabled))
        
        mError = mVoiceAudioProcessing->SetNsStatus(enabled, webrtc::kNsLowSuppression);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set noise suppression status") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        
        mNsEnabled = enabled;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalSetMuteEnabled(bool enabled)
      {
        ZS_LOG_DEBUG(log("set microphone mute enabled") + ZS_PARAM("value", enabled))
        
        mError = mVoiceVolumeControl->SetInputMute(-1, enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set microphone mute") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngine::internalGetMuteEnabled()
      {
        ZS_LOG_DEBUG(log("get microphone mute enabled"))
        
        bool enabled;
        
        mError = mVoiceVolumeControl->GetInputMute(-1, enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set microphone mute") + ZS_PARAM("error", mVoiceBase->LastError()))
          return false;
        }
        
        return enabled;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalSetLoudspeakerEnabled(bool enabled)
      {
        ZS_LOG_DEBUG(log("set loudspeaker enabled") + ZS_PARAM("value", enabled))
        
        mError = mVoiceHardware->SetLoudspeakerStatus(enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set loudspeaker") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngine::internalGetLoudspeakerEnabled()
      {
        ZS_LOG_DEBUG(log("get loudspeaker enabled"))
        
        bool enabled;
        
        mError = mVoiceHardware->GetLoudspeakerStatus(enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get loudspeaker") + ZS_PARAM("error", mVoiceBase->LastError()))
          return false;
        }
        
        return enabled;
      }
      
      //-----------------------------------------------------------------------
      IMediaEngine::OutputAudioRoutes MediaEngine::internalGetOutputAudioRoute()
      {
        ZS_LOG_DEBUG(log("get output audio route"))
        
        OutputAudioRoute route;
        
        mError = mVoiceHardware->GetOutputAudioRoute(route);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get output audio route") + ZS_PARAM("error", mVoiceBase->LastError()))
          return OutputAudioRoute_BuiltInSpeaker;
        }
        
        switch (route) {
          case webrtc::kOutputAudioRouteHeadphone:
            return OutputAudioRoute_Headphone;
          case webrtc::kOutputAudioRouteBuiltInReceiver:
            return OutputAudioRoute_BuiltInReceiver;
          case webrtc::kOutputAudioRouteBuiltInSpeaker:
            return OutputAudioRoute_BuiltInSpeaker;
          default:
            return OutputAudioRoute_BuiltInSpeaker;
        }
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::internalGetVideoTransportStatistics(RtpRtcpStatistics &stat)
      {
        unsigned short fractionLost;
        unsigned int cumulativeLost;
        unsigned int extendedMax;
        unsigned int jitter;
        int rttMs;
        
        mError = mVideoRtpRtcp->GetReceivedRTCPStatistics(mVideoChannel, fractionLost, cumulativeLost, extendedMax, jitter, rttMs);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to get received RTCP statistics for video") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }
        
        unsigned int bytesSent;
        unsigned int packetsSent;
        unsigned int bytesReceived;
        unsigned int packetsReceived;
        
        mError = mVideoRtpRtcp->GetRTPStatistics(mVideoChannel, bytesSent, packetsSent, bytesReceived, packetsReceived);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to get RTP statistics for video") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }
        
        stat.fractionLost = fractionLost;
        stat.cumulativeLost = cumulativeLost;
        stat.extendedMax = extendedMax;
        stat.jitter = jitter;
        stat.rttMs = rttMs;
        stat.bytesSent = bytesSent;
        stat.packetsSent = packetsSent;
        stat.bytesReceived = bytesReceived;
        stat.packetsReceived = packetsReceived;
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::internalGetVoiceTransportStatistics(RtpRtcpStatistics &stat)
      {
        webrtc::CallStatistics callStat;
        
        mError = mVoiceRtpRtcp->GetRTCPStatistics(mVoiceChannel, callStat);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to get RTCP statistics for voice") + ZS_PARAM("error", mVoiceBase->LastError()))
          return mError;
        }
        
        stat.fractionLost = callStat.fractionLost;
        stat.cumulativeLost = callStat.cumulativeLost;
        stat.extendedMax = callStat.extendedMax;
        stat.jitter = callStat.jitterSamples;
        stat.rttMs = callStat.rttMs;
        stat.bytesSent = callStat.bytesSent;
        stat.packetsSent = callStat.packetsSent;
        stat.bytesReceived = callStat.bytesReceived;
        stat.packetsReceived = callStat.packetsReceived;
        
        return 0;
      }

      //-----------------------------------------------------------------------
      void MediaEngine::internalStartVoice()
      {
        ZS_LOG_DEBUG(log("start voice"))

        mVoiceChannel = mVoiceBase->CreateChannel();
        if (mVoiceChannel < 0) {
          ZS_LOG_ERROR(Detail, log("could not create voice channel") + ZS_PARAM("error", mVoiceBase->LastError()))
          mVoiceChannel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
          return;
        }

        mError = registerVoiceTransport();
        if (mError != 0)
          return;

        webrtc::EcModes ecMode = getEcMode();
        if (ecMode == webrtc::kEcUnchanged) {
          return;
        }
        mError = mVoiceAudioProcessing->SetEcStatus(mEcEnabled, ecMode);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller status") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        if (ecMode == webrtc::kEcAecm && mEcEnabled) {
          mError = mVoiceAudioProcessing->SetAecmMode(webrtc::kAecmSpeakerphone);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller mobile mode") + ZS_PARAM("error", mVoiceBase->LastError()))
            return;
          }
        }
        
        mError = mVoiceAudioProcessing->SetAgcStatus(mAgcEnabled, webrtc::kAgcAdaptiveDigital);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set automatic gain control status") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        
        mError = mVoiceAudioProcessing->SetNsStatus(mNsEnabled, webrtc::kNsLowSuppression);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set noise suppression status") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        
        mError = mVoiceVolumeControl->SetInputMute(-1, false);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set microphone mute") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
#ifdef TARGET_OS_IPHONE
        mError = mVoiceHardware->SetLoudspeakerStatus(false);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set loudspeaker") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
#endif
        webrtc::CodecInst cinst;
        memset(&cinst, 0, sizeof(webrtc::CodecInst));
        for (int idx = 0; idx < mVoiceCodec->NumOfCodecs(); idx++) {
          mError = mVoiceCodec->GetCodec(idx, cinst);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get voice codec") + ZS_PARAM("error", mVoiceBase->LastError()))
            return;
          }
#ifdef OPENPEER_MEDIA_ENGINE_VOICE_CODEC_ISAC
          if (strcmp(cinst.plname, "ISAC") == 0) {
            strcpy(cinst.plname, "ISAC");
            cinst.pltype = 103;
            cinst.rate = 32000;
            cinst.pacsize = 480; // 30ms
            cinst.plfreq = 16000;
            cinst.channels = 1;
            mError = mVoiceCodec->SetSendCodec(mVoiceChannel, cinst);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to set send voice codec") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
            break;
          }
#elif defined OPENPEER_MEDIA_ENGINE_VOICE_CODEC_OPUS
          if (strcmp(cinst.plname, "OPUS") == 0) {
            strcpy(cinst.plname, "OPUS");
            cinst.pltype = 110;
            cinst.rate = 20000;
            cinst.pacsize = 320; // 20ms
            cinst.plfreq = 16000;
            cinst.channels = 1;
            mError = mVoiceCodec->SetSendCodec(mVoiceChannel, cinst);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to set send voice codec") + ZS_PARAM("error", mVoiceBase->LastError()))
              return;
            }
            break;
          }
#endif
        }
        
        webrtc::CodecInst cfinst;
        memset(&cfinst, 0, sizeof(webrtc::CodecInst));
        for (int idx = 0; idx < mVoiceCodec->NumOfCodecs(); idx++) {
          mError = mVoiceCodec->GetCodec(idx, cfinst);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get voice codec") + ZS_PARAM("error", mVoiceBase->LastError()))
            return;
          }
          if (strcmp(cfinst.plname, "VORBIS") == 0) {
            strcpy(cfinst.plname, "VORBIS");
            cfinst.pltype = 109;
            cfinst.rate = 32000;
            cfinst.pacsize = 480; // 30ms
            cfinst.plfreq = 16000;
            cfinst.channels = 1;
            break;
          }
        }

        mError = setVoiceTransportParameters();
        if (mError != 0)
          return;
        
        mError = mVoiceBase->StartSend(mVoiceChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start sending voice") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }

        mError = mVoiceBase->StartReceive(mVoiceChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start receiving voice") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        mError = mVoiceBase->StartPlayout(mVoiceChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start playout") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        if (!mVoiceRecordFile.empty()) {
          mError = mVoiceFile->StartRecordingCall(mVoiceRecordFile, &cfinst);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start call recording") + ZS_PARAM("error", mVoiceBase->LastError()))
          }
        }
        
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVoiceEngineReady = true;
        }
      }

      //-----------------------------------------------------------------------
      void MediaEngine::internalStopVoice()
      {
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVoiceEngineReady = false;
        }

        ZS_LOG_DEBUG(log("stop voice"))

        mError = mVoiceBase->StopSend(mVoiceChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop sending voice") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        mError = mVoiceBase->StopPlayout(mVoiceChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop playout") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        mError = mVoiceBase->StopReceive(mVoiceChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop receiving voice") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        if (!mVoiceRecordFile.empty()) {
          mError = mVoiceFile->StopRecordingCall();
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop call recording") + ZS_PARAM("error", mVoiceBase->LastError()))
          }
          mVoiceRecordFile.erase();
        }
        mError = deregisterVoiceTransport();
        if (0 != mError)
          return;
        mError = mVoiceBase->DeleteChannel(mVoiceChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to delete voice channel") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }

        mVoiceChannel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::registerVoiceTransport()
      {
        if (NULL != mVoiceTransport) {
          mError = mVoiceNetwork->RegisterExternalTransport(mVoiceChannel, *mVoiceTransport);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to register voice external transport") + ZS_PARAM("error", mVoiceBase->LastError()))
            return mError;
          }
        } else {
          ZS_LOG_ERROR(Detail, log("external voice transport is not set"))
          return -1;
        }

        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::deregisterVoiceTransport()
      {
        mError = mVoiceNetwork->DeRegisterExternalTransport(mVoiceChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to deregister voice external transport") + ZS_PARAM("error", mVoiceBase->LastError()))
          return mError;
        }

        return 0;
      }

      //-----------------------------------------------------------------------
      int MediaEngine::setVoiceTransportParameters()
      {
        // No transport parameters for external transport.
        return 0;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStartCaptureRenderer()
      {
        ZS_LOG_DEBUG(log("start capture renderer"))
        
#if !defined(__QNX__)
        if (mCaptureRenderView == NULL) {
          ZS_LOG_WARNING(Detail, log("capture view is not set"))
          return;
        }

        mError = mVideoRender->AddRenderer(mCaptureId, mCaptureRenderView, 0, 0.0F, 0.0F, 1.0F, 1.0F);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to add renderer for video capture") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
#if defined(_ANDROID)
        mError = mVideoRender->SetStreamCropping(mCaptureId, mCaptureRenderViewCropLeft, mCaptureRenderViewCropTop,
            mCaptureRenderViewCropRight, mCaptureRenderViewCropBottom);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to set cropping parameters for video capture") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
#endif
        
        mError = mVideoRender->StartRender(mCaptureId);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start rendering video capture") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
#endif
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStopCaptureRenderer()
      {
        ZS_LOG_DEBUG(log("stop capture renderer"))
        
#if !defined(__QNX__)
        mError = mVideoRender->StopRender(mCaptureId);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop rendering video capture") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        mError = mVideoRender->RemoveRenderer(mCaptureId);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to remove renderer for video capture") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
#endif
      }
     
      //-----------------------------------------------------------------------
      void MediaEngine::internalStartVideoCapture()
      {
        ZS_LOG_DEBUG(log("start video capture") + ZS_PARAM("camera type", mCameraType == CameraType_Back ? "back" : "front"))
        
        const unsigned int KMaxDeviceNameLength = 128;
        const unsigned int KMaxUniqueIdLength = 256;
        char deviceName[KMaxDeviceNameLength];
        memset(deviceName, 0, KMaxDeviceNameLength);
        char uniqueId[KMaxUniqueIdLength];
        memset(uniqueId, 0, KMaxUniqueIdLength);
        uint32_t captureIdx;
        
        webrtc::VideoCaptureModule::DeviceInfo *devInfo = webrtc::VideoCaptureFactory::CreateDeviceInfo(0);
        if (devInfo == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to create video capture device info"))
          return;
        }

        uint32_t numberOfDevices = devInfo->NumberOfDevices();
        
        if (mCameraType == CameraType_Back)
        {
          if (numberOfDevices >= 2)
          {
            captureIdx = 0;
          }
          else
          {
            ZS_LOG_ERROR(Detail, log("back camera is not supported on single camera devices"))
            return;
          }
        }
        else if (mCameraType == CameraType_Front)
        {
          if (numberOfDevices >= 2)
          {
            captureIdx = 1;
          }
          else
          {
            captureIdx = 0;
          }
        }
        else
        {
          ZS_LOG_ERROR(Detail, log("camera type is not set"))
          return;
        }

        mError = devInfo->GetDeviceName(captureIdx, deviceName,
                                        KMaxDeviceNameLength, uniqueId,
                                        KMaxUniqueIdLength);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get video device name"))
          return;
        }
        
        strcpy(mDeviceUniqueId, uniqueId);

        mVcpm = webrtc::VideoCaptureFactory::Create(1, uniqueId);
        if (mVcpm == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to create video capture module"))
          return;
        }

        mError = mVideoCapture->AllocateCaptureDevice(*mVcpm, mCaptureId);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to allocate video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        mVcpm->AddRef();
        delete devInfo;
        
        mError = mVideoCapture->RegisterObserver(mCaptureId, *this);
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to register video capture observer") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
#ifdef TARGET_OS_IPHONE
        webrtc::CapturedFrameOrientation defaultOrientation;
        switch (mDefaultVideoOrientation) {
          case IMediaEngine::VideoOrientation_LandscapeLeft:
            defaultOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
            break;
          case IMediaEngine::VideoOrientation_PortraitUpsideDown:
            defaultOrientation = webrtc::CapturedFrameOrientation_PortraitUpsideDown;
            break;
          case IMediaEngine::VideoOrientation_LandscapeRight:
            defaultOrientation = webrtc::CapturedFrameOrientation_LandscapeRight;
            break;
          case IMediaEngine::VideoOrientation_Portrait:
            defaultOrientation = webrtc::CapturedFrameOrientation_Portrait;
            break;
          default:
            defaultOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
            break;
        }
        mError = mVideoCapture->SetDefaultCapturedFrameOrientation(mCaptureId, defaultOrientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set default orientation on video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
        setVideoCaptureRotation();
        
        webrtc::RotateCapturedFrame orientation;
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
#else
        webrtc::RotateCapturedFrame orientation = webrtc::RotateCapturedFrame_0;
#endif
        
        int width = 0, height = 0, maxFramerate = 0, maxBitrate = 0;
        mError = getVideoCaptureParameters(orientation, width, height, maxFramerate, maxBitrate);
        if (mError != 0)
          return;
        
        webrtc::CaptureCapability capability;
        capability.width = width;
        capability.height = height;
        capability.maxFPS = maxFramerate;
        capability.rawType = webrtc::kVideoI420;
        capability.faceDetection = mFaceDetection;
        mError = mVideoCapture->StartCapture(mCaptureId, capability);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start capturing") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
#if defined(_ANDROID)
        setVideoCaptureRotation();
#endif
        
        if (mCaptureRenderView != NULL) {
          internalStartCaptureRenderer();
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStopVideoCapture()
      {
        if (mCaptureRenderView != NULL) {
          internalStopCaptureRenderer();
        }
      
        ZS_LOG_DEBUG(log("stop video capture"))

        mError = mVideoCapture->StopCapture(mCaptureId);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop video capturing") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
        mError = mVideoCapture->DeregisterObserver(mCaptureId);
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to deregister video capture observer") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }

        mError = mVideoCapture->ReleaseCaptureDevice(mCaptureId);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to release video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
        if (mVcpm != NULL)
          mVcpm->Release();
        
        mVcpm = NULL;
        
        mCaptureId = OPENPEER_MEDIA_ENGINE_INVALID_CAPTURE;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStartChannelRenderer()
      {
        ZS_LOG_DEBUG(log("start channel renderer"))
        
        if (mChannelRenderView == NULL) {
          ZS_LOG_WARNING(Detail, log("channel view is not set"))
          return;
        }
        
        mError = mVideoRender->AddRenderer(mVideoChannel, mChannelRenderView, 0, 0.0F, 0.0F, 1.0F,
                                           1.0F);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to add renderer for video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
#if defined(_ANDROID)
        mError = mVideoRender->SetStreamCropping(mVideoChannel, mChannelRenderViewCropLeft, mChannelRenderViewCropTop,
            mChannelRenderViewCropRight, mChannelRenderViewCropBottom);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to set cropping parameters for video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
#endif
        
        mError = mVideoRender->StartRender(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start rendering video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStopChannelRenderer()
      {
        ZS_LOG_DEBUG(log("stop channel renderer"))
        
        mError = mVideoRender->StopRender(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop rendering video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
        mError = mVideoRender->RemoveRenderer(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to remove renderer for video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
      }

      //-----------------------------------------------------------------------
      void MediaEngine::internalStartVideoChannel()
      {
        ZS_LOG_DEBUG(log("start video channel"))

        mError = mVideoBase->CreateChannel(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("could not create video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }

        mError = registerVideoTransport();
        if (0 != mError)
          return;
        
        mError = mVideoNetwork->SetMTU(mVideoChannel, mMtu);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to set MTU for video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }

        mError = mVideoCapture->ConnectCaptureDevice(mCaptureId, mVideoChannel);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to connect capture device to video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
        mError = mVideoRtpRtcp->SetRTCPStatus(mVideoChannel, webrtc::kRtcpCompound_RFC4585);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to set video RTCP status") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }

        mError = mVideoRtpRtcp->SetKeyFrameRequestMethod(mVideoChannel,
                                                         webrtc::kViEKeyFrameRequestPliRtcp);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to set key frame request method") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }

        mError = mVideoRtpRtcp->SetTMMBRStatus(mVideoChannel, true);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to set temporary max media bit rate status") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }

#ifdef TARGET_OS_IPHONE
        OutputAudioRoute route;
        mError = mVoiceHardware->GetOutputAudioRoute(route);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get output audio route") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
        if (route != webrtc::kOutputAudioRouteHeadphone)
        {
          mError = mVoiceHardware->SetLoudspeakerStatus(true);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set loudspeaker") + ZS_PARAM("error", mVoiceBase->LastError()))
            return;
          }
        }
#endif

        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(VideoCodec));
        for (int idx = 0; idx < mVideoCodec->NumberOfCodecs(); idx++) {
          mError = mVideoCodec->GetCodec(idx, videoCodec);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get video codec") + ZS_PARAM("error", mVideoBase->LastError()))
            return;
          }
          if (videoCodec.codecType == webrtc::kVideoCodecVP8) {
            mError = mVideoCodec->SetSendCodec(mVideoChannel, videoCodec);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to set send video codec") + ZS_PARAM("error", mVideoBase->LastError()))
              return;
            }
            break;
          }
        }
        
        mError = setVideoCodecParameters();
        if (mError != 0) {
          return;
        }
        
        mError = setVideoTransportParameters();
        if (mError != 0)
          return;
        
        mError = mVideoBase->StartSend(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start sending video") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }

        mError = mVideoBase->StartReceive(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start receiving video") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
        if (mChannelRenderView != NULL) {
          internalStartChannelRenderer();
        }
        
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVideoEngineReady = true;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStopVideoChannel()
      {
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVideoEngineReady = false;
        }
        
        if (mChannelRenderView != NULL) {
          internalStopChannelRenderer();
        }
        
        ZS_LOG_DEBUG(log("stop video channel"))

        mError = mVideoBase->StopSend(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop sending video") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        mError = mVideoBase->StopReceive(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop receiving video") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        mError = mVideoCapture->DisconnectCaptureDevice(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to disconnect capture device from video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        mError = deregisterVideoTransport();
        if (0 != mError)
          return;
        mError = mVideoBase->DeleteChannel(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to delete video channel") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }

        mVideoChannel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStartRecordVideoCapture(String videoRecordFile, bool saveVideoToLibrary)
      {
        ZS_LOG_DEBUG(log("start video capture recording"))

        webrtc::CapturedFrameOrientation recordOrientation;
        switch (mRecordVideoOrientation) {
          case IMediaEngine::VideoOrientation_LandscapeLeft:
            recordOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
            break;
          case IMediaEngine::VideoOrientation_PortraitUpsideDown:
            recordOrientation = webrtc::CapturedFrameOrientation_PortraitUpsideDown;
            break;
          case IMediaEngine::VideoOrientation_LandscapeRight:
            recordOrientation = webrtc::CapturedFrameOrientation_LandscapeRight;
            break;
          case IMediaEngine::VideoOrientation_Portrait:
            recordOrientation = webrtc::CapturedFrameOrientation_Portrait;
            break;
          default:
            recordOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
            break;
        }
        mError = mVideoCapture->SetCapturedFrameLockedOrientation(mCaptureId, recordOrientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set record orientation on video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        mError = mVideoCapture->EnableCapturedFrameOrientationLock(mCaptureId, true);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to enable orientation lock on video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
        webrtc::RotateCapturedFrame orientation;
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }
        
        if (mVideoChannel == OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL)
          setVideoCaptureRotation();
        else
          setVideoCodecParameters();
        
        int width = 0, height = 0, maxFramerate = 0, maxBitrate = 0;
        mError = getVideoCaptureParameters(orientation, width, height, maxFramerate, maxBitrate);
        if (mError != 0)
          return;
        
#if 0
        webrtc::CodecInst audioCodec;
        memset(&audioCodec, 0, sizeof(webrtc::CodecInst));
        strcpy(audioCodec.plname, "AAC");
        audioCodec.rate = 32000;
        audioCodec.plfreq = 16000;
        audioCodec.channels = 1;
        
        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(VideoCodec));
        videoCodec.codecType = webrtc::kVideoCodecH264;
        videoCodec.width = width;
        videoCodec.height = height;
        videoCodec.maxFramerate = maxFramerate;
        videoCodec.maxBitrate = maxBitrate;
        
        mError = mVideoFile->StartRecordCaptureVideo(mCaptureId, videoRecordFile, webrtc::MICROPHONE, audioCodec, videoCodec, webrtc::kFileFormatMP4File, saveVideoToLibrary);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start video capture recording") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
#endif
      }
      
      //-----------------------------------------------------------------------
      void MediaEngine::internalStopRecordVideoCapture()
      {
        ZS_LOG_DEBUG(log("stop video capture recording"))

#if 0
        mError = mVideoFile->StopRecordCaptureVideo(mCaptureId);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop video capture recording") + ZS_PARAM("error", mVoiceBase->LastError()))
          return;
        }
#endif
        
        mError = mVideoCapture->EnableCapturedFrameOrientationLock(mCaptureId, false);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to disable orientation lock on video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return;
        }

        try {
          if (mDelegate)
            mDelegate->onMediaEngineVideoCaptureRecordStopped();
        } catch (IMediaEngineDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      int MediaEngine::registerVideoTransport()
      {
        if (NULL != mVideoTransport) {
          mError = mVideoNetwork->RegisterSendTransport(mVideoChannel, *mVideoTransport);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to register video external transport") + ZS_PARAM("error", mVideoBase->LastError()))
            return mError;
          }
        } else {
          ZS_LOG_ERROR(Detail, log("external video transport is not set"))
          return -1;
        }

        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::deregisterVideoTransport()
      {
        mError = mVideoNetwork->DeregisterSendTransport(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to deregister video external transport") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }

        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::setVideoTransportParameters()
      {
        // No transport parameters for external transport.
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::getVideoCaptureParameters(webrtc::RotateCapturedFrame orientation, int& width, int& height, int& maxFramerate, int& maxBitrate)
      {
#ifdef TARGET_OS_IPHONE
        String iPadString("iPad");
        String iPad2String("iPad2");
        String iPadMiniString("iPad2,5");
        String iPad3String("iPad3");
        String iPad4String("iPad3,4");
        String iPhoneString("iPhone");
        String iPhone4SString("iPhone4,1");
        String iPhone5String("iPhone5");
        String iPodString("iPod");
        String iPod4String("iPod4,1");
        String iPod5String("iPod5,1");
        if (mCameraType == CameraType_Back) {
          if (orientation == webrtc::RotateCapturedFrame_0 || orientation == webrtc::RotateCapturedFrame_180) {
            if (mMachineName.compare(0, iPod5String.size(), iPod5String) >= 0) {
              width = 960;
              height = 540;
              maxFramerate = 15;
              maxBitrate = 500;
            } else if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 640;
              height = 360;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 160;
              height = 90;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 960;
              height = 540;
              maxFramerate = 15;
              maxBitrate = 500;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 640;
              height = 360;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 160;
              height = 90;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 960;
              height = 540;
              maxFramerate = 15;
              maxBitrate = 500;
            } else if (mMachineName.compare(0, iPad2String.size(), iPad2String) >= 0) {
              width = 640;
              height = 360;
              maxFramerate = 15;
              maxBitrate = 400;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          } else if (orientation == webrtc::RotateCapturedFrame_90 || orientation == webrtc::RotateCapturedFrame_270) {
            if (mMachineName.compare(0, iPod5String.size(), iPod5String) >= 0) {
              width = 540;
              height = 960;
              maxFramerate = 15;
              maxBitrate = 500;
            } else if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 360;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 90;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 540;
              height = 960;
              maxFramerate = 15;
              maxBitrate = 500;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 360;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 90;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 540;
              height = 960;
              maxFramerate = 15;
              maxBitrate = 500;
            } else if (mMachineName.compare(0, iPad2String.size(), iPad2String) >= 0) {
              width = 360;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          }
        } else if (mCameraType == CameraType_Front) {
          if (orientation == webrtc::RotateCapturedFrame_0 || orientation == webrtc::RotateCapturedFrame_180) {
            if (mMachineName.compare(0, iPod5String.size(), iPod5String) >= 0) {
              width = 640;
              height = 360;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 640;
              height = 480;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 160;
              height = 120;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 640;
              height = 360;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 640;
              height = 480;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 160;
              height = 120;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPad4String.size(), iPad4String) >= 0) {
              width = 640;
              height = 360;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPad3String.size(), iPad3String) >= 0) {
              width = 640;
              height = 480;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 640;
              height = 360;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPadString.size(), iPadString) >= 0) {
              width = 640;
              height = 480;
              maxFramerate = 15;
              maxBitrate = 400;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          } else if (orientation == webrtc::RotateCapturedFrame_90 || orientation == webrtc::RotateCapturedFrame_270) {
            if (mMachineName.compare(0, iPod5String.size(), iPod5String) >= 0) {
              width = 360;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 480;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 120;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 360;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 480;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 120;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPad4String.size(), iPad4String) >= 0) {
              width = 360;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPad3String.size(), iPad3String) >= 0) {
              width = 480;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 360;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else if (mMachineName.compare(0, iPadString.size(), iPadString) >= 0) {
              width = 480;
              height = 640;
              maxFramerate = 15;
              maxBitrate = 400;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          }
        } else {
          ZS_LOG_ERROR(Detail, log("camera type is not set"))
          return -1;
        }
#else
        if (mCameraType == CameraType_Back) {
          if (mBackCameraCaptureCapability.width != 0 && mBackCameraCaptureCapability.height != 0) {
            width = mBackCameraCaptureCapability.width;
            height = mBackCameraCaptureCapability.height;
            if (mBackCameraCaptureCapability.maxFPS != 0) {
              maxFramerate = mBackCameraCaptureCapability.maxFPS;
            } else {
              maxFramerate = 15;
            }
            maxBitrate = 400;
          } else {
            width = 720;
            height = 480;
            maxFramerate = 15;
            maxBitrate = 400;
          }
        } else if (mCameraType == CameraType_Front) {
          if (mFrontCameraCaptureCapability.width != 0 && mFrontCameraCaptureCapability.height != 0) {
            width = mFrontCameraCaptureCapability.width;
            height = mFrontCameraCaptureCapability.height;
            if (mFrontCameraCaptureCapability.maxFPS != 0) {
              maxFramerate = mFrontCameraCaptureCapability.maxFPS;
            } else {
              maxFramerate = 15;
            }
            maxBitrate = 400;
          } else {
            width = 720;
            height = 480;
            maxFramerate = 15;
            maxBitrate = 400;
          }
        } else {
          ZS_LOG_ERROR(Detail, log("camera type is not set"))
          return -1;
        }
#endif
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::setVideoCaptureRotation()
      {
        webrtc::RotateCapturedFrame orientation;
#ifdef TARGET_OS_IPHONE
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }
#elif defined(_ANDROID)
        if (mCameraType == CameraType_Back)
          orientation = webrtc::RotateCapturedFrame_90;
        else if (mCameraType == CameraType_Front)
          orientation = webrtc::RotateCapturedFrame_270;
#else
        orientation = webrtc::RotateCapturedFrame_0;
#endif
        mError = mVideoCapture->SetRotateCapturedFrames(mCaptureId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set rotation for video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }
        
        const char *rotationString = NULL;
        switch (orientation) {
          case webrtc::RotateCapturedFrame_0:
            rotationString = "0 degrees";
            break;
          case webrtc::RotateCapturedFrame_90:
            rotationString = "90 degrees";
            break;
          case webrtc::RotateCapturedFrame_180:
            rotationString = "180 degrees";
            break;
          case webrtc::RotateCapturedFrame_270:
            rotationString = "270 degrees";
            break;
          default:
            break;
        }
        
        if (rotationString) {
          ZS_LOG_DEBUG(log("video capture rotation set") + ZS_PARAM("rotation", rotationString))
        }

        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngine::setVideoCodecParameters()
      {
#ifdef TARGET_OS_IPHONE
        webrtc::RotateCapturedFrame orientation;
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }
#else
        webrtc::RotateCapturedFrame orientation = webrtc::RotateCapturedFrame_0;
#endif
        
        int width = 0, height = 0, maxFramerate = 0, maxBitrate = 0;
        mError = getVideoCaptureParameters(orientation, width, height, maxFramerate, maxBitrate);
        if (mError != 0)
          return mError;
        
        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(VideoCodec));
        mError = mVideoCodec->GetSendCodec(mVideoChannel, videoCodec);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get video codec") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }
        videoCodec.width = width;
        videoCodec.height = height;
        videoCodec.maxFramerate = maxFramerate;
        videoCodec.maxBitrate = maxBitrate;
        mError = mVideoCodec->SetSendCodec(mVideoChannel, videoCodec);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set send video codec") + ZS_PARAM("error", mVideoBase->LastError()))
          return mError;
        }
        
        ZS_LOG_DEBUG(log("video codec size") + ZS_PARAM("width", width) + ZS_PARAM("height", height))
        
        return 0;
      }

      //-----------------------------------------------------------------------
      webrtc::EcModes MediaEngine::getEcMode()
      {
#if defined(TARGET_OS_IPHONE) || defined (_ANDROID)
        return webrtc::kEcAecm;
#elif defined(__QNX__)
        return webrtc::kEcAec;
#else
        return webrtc::kEcUnchanged;
#endif
      }

      //-----------------------------------------------------------------------
      Log::Params MediaEngine::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::MediaEngine");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params MediaEngine::slog(const char *message)
      {
        return Log::Params(message, "core::MediaEngine");
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine::RedirectTransport
      #pragma mark

      //-----------------------------------------------------------------------
      MediaEngine::RedirectTransport::RedirectTransport(const char *transportType) :
        mID(zsLib::createPUID()),
        mTransportType(transportType),
        mTransport(0)
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine::RedirectTransport => webrtc::Transport
      #pragma mark

      //-----------------------------------------------------------------------
      int MediaEngine::RedirectTransport::SendPacket(int channel, const void *data, int len)
      {
        Transport *transport = NULL;
        {
          AutoRecursiveLock lock(mLock);
          transport = mTransport;
        }
        if (!transport) {
          ZS_LOG_WARNING(Debug, log("RTP packet cannot be sent as no transport is not registered") + ZS_PARAM("channel", channel) + ZS_PARAM("length", len))
          return 0;
        }

        return transport->SendPacket(channel, data, len);
      }

      //-----------------------------------------------------------------------
      int MediaEngine::RedirectTransport::SendRTCPPacket(int channel, const void *data, int len)
      {
        Transport *transport = NULL;
        {
          AutoRecursiveLock lock(mLock);
          transport = mTransport;
        }
        if (!transport) {
          ZS_LOG_WARNING(Debug, log("RTCP packet cannot be sent as no transport is not registered") + ZS_PARAM("channel", channel) + ZS_PARAM("length", len))
          return 0;
        }

        return transport->SendRTCPPacket(channel, data, len);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine::RedirectTransport => friend MediaEngine
      #pragma mark

      //-----------------------------------------------------------------------
      void MediaEngine::RedirectTransport::redirect(Transport *transport)
      {
        AutoRecursiveLock lock(mLock);
        mTransport = transport;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MediaEngine::RedirectTransport => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params MediaEngine::RedirectTransport::log(const char *message)
      {
        ElementPtr objectEl = Element::create("core::MediaEngine::RedirectTransport");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        UseServicesHelper::debugAppend(objectEl, "type", mTransportType);
        return Log::Params(message, objectEl);
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IMediaEngine
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IMediaEngine::toString(CameraTypes type)
    {
      switch (type) {
        case CameraType_None:   return "None";
        case CameraType_Front:  return "Front";
        case CameraType_Back:   return "Back";
      }
      return "UNDEFINED";
    }
    
    //---------------------------------------------------------------------------
    const char *IMediaEngine::toString(VideoOrientations orientation)
    {
      switch (orientation) {
        case VideoOrientation_LandscapeLeft:        return "Landscape left";
        case VideoOrientation_PortraitUpsideDown:   return "Portrait upside down";
        case VideoOrientation_LandscapeRight:       return "Landscape right";
        case VideoOrientation_Portrait:             return "Portrait";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    const char *IMediaEngine::toString(OutputAudioRoutes route)
    {
      switch (route) {
        case OutputAudioRoute_Headphone:        return "Headphone";
        case OutputAudioRoute_BuiltInReceiver:  return "Built in receiver";
        case OutputAudioRoute_BuiltInSpeaker:   return "Built in speaker";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IMediaEnginePtr IMediaEngine::singleton()
    {
      return internal::MediaEngine::singleton();
    }
  }
}
