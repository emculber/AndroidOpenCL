LOCAL_PATH := $(call my-dir)
OPENCV_ANDROID_SDK=/home/erik/Downloads/AndroidOpenCL-master/opencv-3.3.1-android-sdk/OpenCV-android-sdk
OPENCL_SDK=/home/erik/Downloads/AndroidOpenCL-master/opencl-sdk-1.2.2

# add OpenCV
include $(CLEAR_VARS)
OPENCV_INSTALL_MODULES:=on
ifdef OPENCV_ANDROID_SDK
  ifneq ("","$(wildcard $(OPENCV_ANDROID_SDK)/OpenCV.mk)")
    include ${OPENCV_ANDROID_SDK}/OpenCV.mk
  else
    include ${OPENCV_ANDROID_SDK}/sdk/native/jni/OpenCV.mk
  endif
else
  include ../../sdk/native/jni/OpenCV.mk
endif

ifndef OPENCL_SDK
  $(error Specify OPENCL_SDK to Android OpenCL SDK location)
endif

LOCAL_CPP_FEATURES += exceptions

# add OpenCL

#LOCAL_C_INCLUDES += $(OPENCL_SDK)/inc/
LOCAL_C_INCLUDES += src/main/jniLibs/OpenCL-Include
LOCAL_LDLIBS := -L$(OPENCL_SDK)/lib/ -lOpenCL -DWITH_OPENCL=YES

LOCAL_MODULE    := JNIpart
LOCAL_SRC_FILES := src/main/cpp/jni.c src/main/cpp/CLprocessor.cpp
LOCAL_LDLIBS    += -llog -lGLESv2 -lEGL
include $(BUILD_SHARED_LIBRARY)