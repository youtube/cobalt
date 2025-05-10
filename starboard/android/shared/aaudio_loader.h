#ifndef STARBOARD_ANDROID_SHARED_AAUDIO_LOADER_H_
#define STARBOARD_ANDROID_SHARED_AAUDIO_LOADER_H_

#include <aaudio/AAudio.h>

#include <dlfcn.h>
#include <unistd.h>

// Function signatures are from AAudio.h header (introduced in Android O).
// https://cs.corp.google.com/android/frameworks/av/media/libaaudio/include/aaudio/AAudio.h

// Define AAudio function signatures.

namespace starboard::android::shared {
// Anonymous namespace prevents external linkage of local aaudio symbols.
namespace {  // NOLINT

// Define AAudioStreamBuilder function signatures.

#define DEFINE_METHOD(api_name)             \
  using PF##api_name = decltype(&api_name); \
  PF##api_name api_name;

DEFINE_METHOD(AAudio_createStreamBuilder);

DEFINE_METHOD(AAudioStreamBuilder_delete);
DEFINE_METHOD(AAudioStreamBuilder_openStream);
DEFINE_METHOD(AAudioStreamBuilder_setBufferCapacityInFrames);
DEFINE_METHOD(AAudioStreamBuilder_setDataCallback);
DEFINE_METHOD(AAudioStreamBuilder_setDirection);
DEFINE_METHOD(AAudioStreamBuilder_setErrorCallback);
DEFINE_METHOD(AAudioStreamBuilder_setChannelCount);
DEFINE_METHOD(AAudioStreamBuilder_setFormat);
DEFINE_METHOD(AAudioStreamBuilder_setFramesPerDataCallback);
DEFINE_METHOD(AAudioStreamBuilder_setPerformanceMode);
DEFINE_METHOD(AAudioStreamBuilder_setSamplesPerFrame);
DEFINE_METHOD(AAudioStreamBuilder_setSampleRate);

DEFINE_METHOD(AAudioStream_close);
DEFINE_METHOD(AAudioStream_getFormat);
DEFINE_METHOD(AAudioStream_getFramesPerBurst);
DEFINE_METHOD(AAudioStream_getPerformanceMode);

DEFINE_METHOD(AAudioStream_getSampleRate);
DEFINE_METHOD(AAudioStream_getSamplesPerFrame);
DEFINE_METHOD(AAudioStream_getState);
DEFINE_METHOD(AAudioStream_requestFlush);
DEFINE_METHOD(AAudioStream_requestPause);
DEFINE_METHOD(AAudioStream_requestStart);
DEFINE_METHOD(AAudioStream_requestStop);
DEFINE_METHOD(AAudioStream_waitForStateChange);
DEFINE_METHOD(AAudio_convertResultToText);

// Loads required AAudio symbols.
//
// @return libdl handle, nullptr on failure
void* LoadAAudioSymbols() {
  SB_LOG(INFO) << __func__;
  void* dl_handle = dlopen("libaaudio.so", RTLD_NOW);
  if (dl_handle == nullptr) {
    const char* error_msg = dlerror();
    SB_LOG(ERROR) << "Unable to open libaaudio.so: "
                  << (error_msg ? error_msg : "");
    return nullptr;
  }
  SB_LOG(INFO) << "Opened libaaudio.so";

#define LOAD_AAUDIO_FUNCTION(name)                            \
  *reinterpret_cast<void**>(&name) = dlsym(dl_handle, #name); \
  if (!name) {                                                \
    SB_LOG(ERROR) << "Unable to load method " #name;          \
    dlclose(dl_handle);                                       \
    return nullptr;                                           \
  }                                                           \
  SB_LOG(INFO) << "Loaded method " #name;

  LOAD_AAUDIO_FUNCTION(AAudio_createStreamBuilder);

  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_delete);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setBufferCapacityInFrames);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setDataCallback);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setDirection);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setErrorCallback);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setChannelCount);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setFormat);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setFramesPerDataCallback);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setPerformanceMode);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setSamplesPerFrame);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setSampleRate);

  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_openStream);

  LOAD_AAUDIO_FUNCTION(AAudioStream_close);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getFormat);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getFramesPerBurst);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getPerformanceMode);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getSampleRate);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getSamplesPerFrame);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getState);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestFlush);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestPause);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestStart);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestStop);
  LOAD_AAUDIO_FUNCTION(AAudioStream_waitForStateChange);

  LOAD_AAUDIO_FUNCTION(AAudio_convertResultToText);

#undef LOAD_AAUDIO_FUNCTION

  SB_LOG(INFO) << "Successfully loaded AAudio symbols";
  return dl_handle;
}

}  // namespace
}  // namespace starboard::android::shared
#endif  // STARBOARD_ANDROID_SHARED_AAUDIO_LOADER_H_
