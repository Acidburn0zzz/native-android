APP_ABI := armeabi-v7a arm64-v8a
APP_PLATFORM := android-21 

#was static here 'APP_STL := gnustl_static'
# by reccomendations should be c++_static
# was changed to c++_shared  according to compile errors fix
APP_STL := c++_static
APP_CPPFLAGS += -std=c++14 -static-libstdc++ -fexceptions
CMAKE_CXX_FLAGS += -mapcs-frame -frtti #for debug
LOCAL_C_INCLUDES += ${ANDROID_NDK}/sources/cxx-stl/llvm-libc++/include
APP_ALLOW_MISSING_DEPS=true
