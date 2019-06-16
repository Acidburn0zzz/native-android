APP_ABI :=  arm64-v8a armeabi-v7a
# do not put this comment from the right of the 1st line: armeabi-v7a arm64-v8a


APP_PLATFORM := android-21 
APP_STL := c++_static
APP_CPPFLAGS += -std=c++14
CMAKE_CXX_FLAGS +=  -frtti
ifeq (${RELEASE}, 1)

else
CMAKE_CXX_FLAGS +=  -mapcs-frame 
endif

#LOCAL_C_INCLUDES += ${ANDROID_NDK}/sources/cxx-stl/llvm-libc++/include
APP_ALLOW_MISSING_DEPS=true