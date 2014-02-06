LOCAL_PATH := $(call my-dir)/../../
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_CFLAGS	:= -Wall \
-W \
-std=gnu++11 \
-O2 \
-pipe \
-fPIC \
-frtti \
-fexceptions \
-D_ANDROID \

LOCAL_MODULE    := hfcore_android

LOCAL_C_INCLUDES:= $(LOCAL_PATH) \
$(LOCAL_PATH)/openpeer/core/internal \
$(LOCAL_PATH)/../ortc-lib/libs/zsLib \
$(LOCAL_PATH)/../ortc-lib/libs/zsLib/internal \
$(LOCAL_PATH)/../ortc-lib/libs/op-services-cpp \
$(LOCAL_PATH)/../op-stack-cpp \
$(LOCAL_PATH)/../ortc-lib/libs \
$(LOCAL_PATH)/.. \
$(LOCAL_PATH)/../ortc-lib/libs/build/android/curl/include \
$(LOCAL_PATH)/../ortc-lib/libs/udns \
$(LOCAL_PATH)../ortc-lib/libs/build/android/boost/include/boost-1_53 \
$(LOCAL_PATH)/../ortc-lib/libs/webrtc \
$(LOCAL_PATH)/../ortc-lib/libs/webrtc/webrtc \
$(LOCAL_PATH)/../ortc-lib/libs/webrtc/webrtc/voice_engine/include \
$(LOCAL_PATH)/../ortc-lib/libs/webrtc/webrtc/video_engine/include \
$(LOCAL_PATH)/../ortc-lib/libs/webrtc/webrtc/modules/video_capture/include \
$(ANDROIDNDK_PATH)/sources/cxx-stl/gnu-libstdc++/4.7/include \
$(ANDROIDNDK_PATH)/sources/cxx-stl/gnu-libstdc++/4.7/libs/armeabi/include \

SOURCE_PATH := openpeer/core/cpp

LOCAL_SRC_FILES := $(SOURCE_PATH)/core_Account.cpp \
		   $(SOURCE_PATH)/core_Cache.cpp \
		   $(SOURCE_PATH)/core_Call.cpp \
		   $(SOURCE_PATH)/core_CallTransport.cpp \
		   $(SOURCE_PATH)/core_Contact.cpp \
		   $(SOURCE_PATH)/core_ConversationThread.cpp \
		   $(SOURCE_PATH)/core_ConversationThreadDocumentFetcher.cpp \
		   $(SOURCE_PATH)/core_ConversationThreadHost.cpp \
		   $(SOURCE_PATH)/core_ConversationThreadHost_PeerContact.cpp \
		   $(SOURCE_PATH)/core_ConversationThreadHost_PeerLocation.cpp \
		   $(SOURCE_PATH)/core_ConversationThreadSlave.cpp \
		   $(SOURCE_PATH)/core_Factory.cpp \
		   $(SOURCE_PATH)/core_Helper.cpp \
		   $(SOURCE_PATH)/core_Identity.cpp \
		   $(SOURCE_PATH)/core_IdentityLookup.cpp \
		   $(SOURCE_PATH)/core_Logger.cpp \
		   $(SOURCE_PATH)/core_Settings.cpp \
		   $(SOURCE_PATH)/core_Stack.cpp \
		   $(SOURCE_PATH)/core.cpp \
		   $(SOURCE_PATH)/core_thread.cpp \
		   $(SOURCE_PATH)/core_MediaEngine.cpp


include $(BUILD_STATIC_LIBRARY)

