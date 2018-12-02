cmake_minimum_required(VERSION 3.6)

include_directories(
        src/main/jniLibs/OpenCV-Include/
        src/main/jniLibs/OpenCL-Include/
)

add_library(
        lib_opencv SHARED IMPORTED
        lib_opencl SHARED IMPORTED
)

set_target_properties(lib_opencv PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libopencv_java3.so)

add_library(
    native-lib
    SHARED
    src/main/cpp/jni.c
    src/main/cpp/common.hpp
    src/main/cpp/CLprocessor.cpp

)

target_link_libraries(
        native-lib
        lib_opencv
        log
)

set(sample example-tutorial-4-opencl)
if(NOT DEFINED ANDROID_OPENCL_SDK)
  message(STATUS "Sample ${sample} is disabled, because ANDROID_OPENCL_SDK is not specified")
  return()
endif()
if(ANDROID_NATIVE_API_LEVEL LESS 14)
  message(STATUS "Sample ${sample} is disabled, because ANDROID_NATIVE_API_LEVEL < 14")
  return()
endif()

if(BUILD_FAT_JAVA_LIB)
  set(native_deps opencv_java)
else()
  set(native_deps opencv_imgproc)
endif()

include_directories(${ANDROID_OPENCL_SDK}/include)
link_directories(${ANDROID_OPENCL_SDK}/lib/${ANDROID_NDK_ABI_NAME})
add_android_project(${sample} "${CMAKE_CURRENT_SOURCE_DIR}"
    LIBRARY_DEPS "${OPENCV_ANDROID_LIB_DIR}"
    SDK_TARGET 28 "${ANDROID_SDK_TARGET}"
    NATIVE_DEPS ${native_deps} -lGLESv2 -lEGL -lOpenCL
    COPY_LIBS YES
)
if(TARGET ${sample})
  add_dependencies(opencv_android_examples ${sample})
endif()


