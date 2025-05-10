#ifndef STARBOARD_ANDROID_SHARED_AAUDIO_LOADER_H_
#define STARBOARD_ANDROID_SHARED_AAUDIO_LOADER_H_

#include <aaudio/AAudio.h>

#include <dlfcn.h>
#include <unistd.h>

// Function signatures are from AAudio.h header (introduced in Android O).
// https://cs.corp.google.com/android/frameworks/av/media/libaaudio/include/aaudio/AAudio.h

// Define AAudio function signatures.

// Anonymous namespace prevents external linkage of local aaudio symbols.
namespace {  // NOLINT

using PFAAudio_createStreamBuilder = aaudio_result_t (*)(AAudioStreamBuilder**);
PFAAudio_createStreamBuilder AAudio_createStreamBuilder;

// Define AAudioStreamBuilder function signatures.

using PFAAudioStreamBuilder_delete = int32_t (*)(AAudioStreamBuilder*);
PFAAudioStreamBuilder_delete AAudioStreamBuilder_delete;

using PFAAudioStreamBuilder_openStream =
    aaudio_result_t (*)(AAudioStreamBuilder* builder, AAudioStream**);
PFAAudioStreamBuilder_openStream AAudioStreamBuilder_openStream;

using PFAAudioStreamBuilder_setBufferCapacityInFrames =
    void (*)(AAudioStreamBuilder*, int32_t);
PFAAudioStreamBuilder_setBufferCapacityInFrames
    AAudioStreamBuilder_setBufferCapacityInFrames;

using PFAAudioStreamBuilder_setDataCallback =
    void (*)(AAudioStreamBuilder* builder,
             AAudioStream_dataCallback callback,
             void* userData);
PFAAudioStreamBuilder_setDataCallback AAudioStreamBuilder_setDataCallback;

using PFAAudioStreamBuilder_setDirection =
    void (*)(AAudioStreamBuilder* builder, aaudio_direction_t direction);
PFAAudioStreamBuilder_setDirection AAudioStreamBuilder_setDirection;

using PFAAudioStreamBuilder_setErrorCallback =
    void (*)(AAudioStreamBuilder* builder,
             AAudioStream_errorCallback callback,
             void* userData);
PFAAudioStreamBuilder_setErrorCallback AAudioStreamBuilder_setErrorCallback;

using PFAAudioStreamBuilder_setFormat = void (*)(AAudioStreamBuilder*, int32_t);
PFAAudioStreamBuilder_setFormat AAudioStreamBuilder_setFormat;

using PFAAudioStreamBuilder_setFramesPerDataCallback =
    void (*)(AAudioStreamBuilder* builder, int32_t numFrames);
PFAAudioStreamBuilder_setFramesPerDataCallback
    AAudioStreamBuilder_setFramesPerDataCallback;

using PFAAudioStreamBuilder_setPerformanceMode =
    void (*)(AAudioStreamBuilder* builder, aaudio_performance_mode_t mode);
PFAAudioStreamBuilder_setPerformanceMode AAudioStreamBuilder_setPerformanceMode;

using PFAAudioStreamBuilder_setSamplesPerFrame = void (*)(AAudioStreamBuilder*,
                                                          int32_t);
PFAAudioStreamBuilder_setSamplesPerFrame AAudioStreamBuilder_setSamplesPerFrame;

using PFAAudioStreamBuilder_setSampleRate = void (*)(AAudioStreamBuilder*,
                                                     int32_t);
PFAAudioStreamBuilder_setSampleRate AAudioStreamBuilder_setSampleRate;

// Define AAudioStream function signatures.

using PFAAudioStream_close = int32_t (*)(AAudioStream* stream);
PFAAudioStream_close AAudioStream_close;

using PFAAudioStream_getFormat = aaudio_format_t (*)(AAudioStream*);
PFAAudioStream_getFormat AAudioStream_getFormat;

using PFAAudioStream_getFramesPerBurst = int32_t (*)(AAudioStream* stream);
PFAAudioStream_getFramesPerBurst AAudioStream_getFramesPerBurst;

using PFAAudioStream_getPerformanceMode =
    aaudio_performance_mode_t (*)(AAudioStream* stream);
PFAAudioStream_getPerformanceMode AAudioStream_getPerformanceMode;

using PFAAudioStream_getSampleRate = int32_t (*)(AAudioStream* stream);
PFAAudioStream_getSampleRate AAudioStream_getSampleRate;

using PFAAudioStream_getSamplesPerFrame = int32_t (*)(AAudioStream* stream);
PFAAudioStream_getSamplesPerFrame AAudioStream_getSamplesPerFrame;

using PFAAudioStream_getState = aaudio_stream_state_t (*)(AAudioStream* stream);
PFAAudioStream_getState AAudioStream_getState;

using PFAAudioStream_requestFlush = aaudio_result_t (*)(AAudioStream* stream);
PFAAudioStream_requestFlush AAudioStream_requestFlush;

using PFAAudioStream_requestPause = aaudio_result_t (*)(AAudioStream* stream);
PFAAudioStream_requestPause AAudioStream_requestPause;

using PFAAudioStream_requestStart = aaudio_result_t (*)(AAudioStream* stream);
PFAAudioStream_requestStart AAudioStream_requestStart;

using PFAAudioStream_requestStop = aaudio_result_t (*)(AAudioStream* stream);
PFAAudioStream_requestStop AAudioStream_requestStop;

using PFAAudioStream_waitForStateChange =
    aaudio_result_t (*)(AAudioStream* stream,
                        aaudio_stream_state_t inputState,
                        aaudio_stream_state_t* nextState,
                        int64_t timeoutNanoseconds);
PFAAudioStream_waitForStateChange AAudioStream_waitForStateChange;

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
  // return dl_handle;

  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_delete);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setBufferCapacityInFrames);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setDataCallback);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setDirection);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setErrorCallback);
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

#undef LOAD_AAUDIO_FUNCTION

  SB_LOG(INFO) << "Successfully loaded AAudio symbols";
  return dl_handle;
}

}  // namespace

#endif  // STARBOARD_ANDROID_SHARED_AAUDIO_LOADER_H_
