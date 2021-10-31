// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/starboard/microphone/microphone_internal.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <algorithm>
#include <cstddef>
#include <queue>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/thread_checker.h"

using starboard::android::shared::JniEnvExt;

namespace starboard {
namespace android {
namespace shared {
namespace {

const int kSampleRateInHz = 16000;
const int kSampleRateInMillihertz = kSampleRateInHz * 1000;
const int kNumOfOpenSLESBuffers = 2;
const int kSamplesPerBuffer = 128;
const int kBufferSizeInBytes = kSamplesPerBuffer * sizeof(int16_t);

bool CheckReturnValue(SLresult result) {
  return result == SL_RESULT_SUCCESS;
}
}  // namespace

class SbMicrophoneImpl : public SbMicrophonePrivate {
 public:
  SbMicrophoneImpl();
  ~SbMicrophoneImpl() override;

  bool Open() override;
  bool Close() override;
  int Read(void* out_audio_data, int audio_data_size) override;

  void SetPermission(bool is_granted);
  static bool IsMicrophoneDisconnected();
  static bool IsMicrophoneMute();

 private:
  enum State { kWaitPermission, kPermissionGranted, kOpened, kClosed };

  static void SwapAndPublishBuffer(SLAndroidSimpleBufferQueueItf buffer_object,
                                   void* context);
  void SwapAndPublishBuffer();

  bool CreateAudioRecorder();
  void DeleteAudioRecorder();

  void ClearBuffer();

  bool RequestAudioPermission();
  bool StartRecording();
  bool StopRecording();

  SLObjectItf engine_object_;
  SLEngineItf engine_;
  SLObjectItf recorder_object_;
  SLRecordItf recorder_;
  SLAndroidSimpleBufferQueueItf buffer_object_;
  SLAndroidConfigurationItf config_object_;

  // Keeps track of the microphone's current state.
  State state_;
  // Audio data that has been delivered to the buffer queue.
  Mutex delivered_queue_mutex_;
  std::queue<int16_t*> delivered_queue_;
  // Audio data that is ready to be read.
  Mutex ready_queue_mutex_;
  std::queue<int16_t*> ready_queue_;
};

SbMicrophoneImpl::SbMicrophoneImpl()
    : engine_object_(nullptr),
      engine_(nullptr),
      recorder_object_(nullptr),
      recorder_(nullptr),
      buffer_object_(nullptr),
      config_object_(nullptr),
      state_(kClosed) {}

SbMicrophoneImpl::~SbMicrophoneImpl() {
  Close();
}

bool SbMicrophoneImpl::RequestAudioPermission() {
  JniEnvExt* env = JniEnvExt::Get();
  jobject j_audio_permission_requester =
      static_cast<jobject>(env->CallStarboardObjectMethodOrAbort(
          "getAudioPermissionRequester",
          "()Ldev/cobalt/coat/AudioPermissionRequester;"));
  jboolean j_permission = env->CallBooleanMethodOrAbort(
      j_audio_permission_requester, "requestRecordAudioPermission", "(J)Z",
      reinterpret_cast<intptr_t>(this));
  return j_permission;
}

// static
bool SbMicrophoneImpl::IsMicrophoneDisconnected() {
  JniEnvExt* env = JniEnvExt::Get();
  jboolean j_microphone =
      env->CallStarboardBooleanMethodOrAbort("isMicrophoneDisconnected", "()Z");
  return j_microphone;
}

// static
bool SbMicrophoneImpl::IsMicrophoneMute() {
  JniEnvExt* env = JniEnvExt::Get();
  jboolean j_microphone =
      env->CallStarboardBooleanMethodOrAbort("isMicrophoneMute", "()Z");
  return j_microphone;
}

bool SbMicrophoneImpl::Open() {
  if (state_ == kOpened) {
    // The microphone has already been opened; clear the unread buffer. See
    // starboard/microphone.h for more info.
    ClearBuffer();
    return true;
  }

  if (IsMicrophoneDisconnected()) {
    SB_DLOG(WARNING) << "No microphone connected.";
    return false;
  } else if (!RequestAudioPermission()) {
    state_ = kWaitPermission;
    SB_DLOG(INFO) << "Waiting for audio permission.";
    // The permission is not set; this causes the MicrophoneManager to call
    // read() repeatedly and wait for the user's response.
    return true;
  } else if (!StartRecording()) {
    SB_DLOG(WARNING) << "Error starting recording.";
    return false;
  }

  // Successfully opened the microphone and started recording.
  state_ = kOpened;
  return true;
}

bool SbMicrophoneImpl::StartRecording() {
  if (!CreateAudioRecorder()) {
    SB_DLOG(WARNING) << "Create audio recorder failed.";
    DeleteAudioRecorder();
    return false;
  }

  // Enqueues kNumOfOpenSLESBuffers zero buffers to start.
  // Adds buffers to the queue before changing state to ensure that recording
  // starts as soon as the state is modified.
  for (int i = 0; i < kNumOfOpenSLESBuffers; ++i) {
    int16_t* buffer = new int16_t[kSamplesPerBuffer];
    memset(buffer, 0, kBufferSizeInBytes);
    {
      ScopedLock lock(delivered_queue_mutex_);
      delivered_queue_.push(buffer);
    }
    SLresult result =
        (*buffer_object_)->Enqueue(buffer_object_, buffer, kBufferSizeInBytes);
    if (!CheckReturnValue(result)) {
      SB_DLOG(WARNING) << "Error adding buffers to the queue.";
      return false;
    }
  }

  // Start the recording by setting the state to |SL_RECORDSTATE_RECORDING|.
  // When the object is in the SL_RECORDSTATE_RECORDING state, adding buffers
  // will implicitly start the recording process.
  SLresult result =
      (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_RECORDING);
  if (!CheckReturnValue(result)) {
    return false;
  }

  return true;
}

bool SbMicrophoneImpl::Close() {
  if (state_ == kClosed) {
    // The microphone has already been closed.
    return true;
  }

  if (state_ == kOpened && !StopRecording()) {
    SB_DLOG(WARNING) << "Error closing the microphone.";
    return false;
  }

  // Successfully closed the microphone and stopped recording.
  state_ = kClosed;
  return true;
}

bool SbMicrophoneImpl::StopRecording() {
  // Stop recording by setting the record state to |SL_RECORDSTATE_STOPPED|.
  SLresult result =
      (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_STOPPED);
  if (!CheckReturnValue(result)) {
    return false;
  }

  ClearBuffer();

  DeleteAudioRecorder();

  return true;
}

int SbMicrophoneImpl::Read(void* out_audio_data, int audio_data_size) {
  if (state_ == kClosed || IsMicrophoneMute()) {
    // No audio data is read from a stopped or muted microphone; return an
    // error.
    return -1;
  }

  if (!out_audio_data || audio_data_size == 0 || state_ == kWaitPermission) {
    // No data to be read.
    return 0;
  }

  if (state_ == kPermissionGranted) {
    if (StartRecording()) {
      state_ = kOpened;
    } else {
      // Could not start recording; return an error.
      state_ = kClosed;
      return -1;
    }
  }

  int read_bytes = 0;
  scoped_ptr<int16_t> buffer;
  {
    ScopedLock lock(ready_queue_mutex_);
    // Go through the ready queue, reading and sending audio data.
    while (!ready_queue_.empty() &&
           audio_data_size - read_bytes >= kBufferSizeInBytes) {
      buffer.reset(ready_queue_.front());
      memcpy(static_cast<uint8_t*>(out_audio_data) + read_bytes, buffer.get(),
             kBufferSizeInBytes);
      ready_queue_.pop();
      read_bytes += kBufferSizeInBytes;
    }
  }

  buffer.reset();
  return read_bytes;
}

void SbMicrophoneImpl::SetPermission(bool is_granted) {
  state_ = is_granted ? kPermissionGranted : kClosed;
}

// static
void SbMicrophoneImpl::SwapAndPublishBuffer(
    SLAndroidSimpleBufferQueueItf buffer_object,
    void* context) {
  SbMicrophoneImpl* recorder = static_cast<SbMicrophoneImpl*>(context);
  recorder->SwapAndPublishBuffer();
}

void SbMicrophoneImpl::SwapAndPublishBuffer() {
  int16_t* buffer = nullptr;
  {
    ScopedLock lock(delivered_queue_mutex_);
    if (!delivered_queue_.empty()) {
      // The front item in the delivered queue already has the buffered data, so
      // move it from the delivered queue to the ready queue for future reads.
      buffer = delivered_queue_.front();
      delivered_queue_.pop();
    }
  }

  if (buffer != NULL) {
    ScopedLock lock(ready_queue_mutex_);
    ready_queue_.push(buffer);
  }

  if (state_ == kOpened) {
    int16_t* buffer = new int16_t[kSamplesPerBuffer];
    memset(buffer, 0, kBufferSizeInBytes);
    {
      ScopedLock lock(delivered_queue_mutex_);
      delivered_queue_.push(buffer);
    }
    SLresult result =
        (*buffer_object_)->Enqueue(buffer_object_, buffer, kBufferSizeInBytes);
    CheckReturnValue(result);
  }
}

bool SbMicrophoneImpl::CreateAudioRecorder() {
  SLresult result;
  // Initializes the SL engine object with specific options.
  // OpenSL ES for Android is designed for multi-threaded applications and
  // is thread-safe.
  result = slCreateEngine(&engine_object_, 0, nullptr, 0, nullptr, nullptr);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error creating the SL engine object.";
    return false;
  }

  // Realize the SL engine object in synchronous mode.
  result = (*engine_object_)
               ->Realize(engine_object_, /* async = */ SL_BOOLEAN_FALSE);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error realizing the SL engine object.";
    return false;
  }

  // Get the SL engine interface.
  result =
      (*engine_object_)->GetInterface(engine_object_, SL_IID_ENGINE, &engine_);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error getting the SL engine interface.";
    return false;
  }

  // Audio source configuration; the audio source is an I/O device data locator.
  SLDataLocator_IODevice input_dev_locator = {
      SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
      SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};

  SLDataSource audio_source = {&input_dev_locator, nullptr};
  SLDataLocator_AndroidSimpleBufferQueue simple_buffer_queue = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, kNumOfOpenSLESBuffers};

  // Audio sink configuration; the audio sink is a simple buffer queue. PCM is
  // the only data format allowed with buffer queues.
  SLAndroidDataFormat_PCM_EX format = {
      SL_ANDROID_DATAFORMAT_PCM_EX, 1 /* numChannels */,
      kSampleRateInMillihertz,      SL_PCMSAMPLEFORMAT_FIXED_16,
      SL_PCMSAMPLEFORMAT_FIXED_16,  SL_SPEAKER_FRONT_CENTER,
      SL_BYTEORDER_LITTLEENDIAN,    SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT};
  SLDataSink audio_sink = {&simple_buffer_queue, &format};

  const int kCount = 2;
  const SLInterfaceID kInterfaceId[kCount] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                              SL_IID_ANDROIDCONFIGURATION};
  const SLboolean kInterfaceRequired[kCount] = {SL_BOOLEAN_TRUE,
                                                SL_BOOLEAN_TRUE};
  // Create the audio recorder.
  result = (*engine_)->CreateAudioRecorder(engine_, &recorder_object_,
                                           &audio_source, &audio_sink, kCount,
                                           kInterfaceId, kInterfaceRequired);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error creating the audio recorder.";
    return false;
  }

  // Configure the audio recorder (before it is realized); get the configuration
  // interface.
  result = (*recorder_object_)
               ->GetInterface(recorder_object_, SL_IID_ANDROIDCONFIGURATION,
                              &config_object_);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error getting the audio recorder interface.";
    return false;
  }

  // Use the main microphone tuned for voice recognition.
  const SLuint32 kPresetValue = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
  result =
      (*config_object_)
          ->SetConfiguration(config_object_, SL_ANDROID_KEY_RECORDING_PRESET,
                             &kPresetValue, sizeof(SLuint32));
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error configuring the audio recorder.";
    return false;
  }

  // Realize the recorder in synchronous mode.
  result = (*recorder_object_)
               ->Realize(recorder_object_, /* async = */ SL_BOOLEAN_FALSE);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error realizing the audio recorder. Double check that "
                        "the microphone is connected to the device.";
    return false;
  }

  // Get the record interface (an implicit interface).
  result = (*recorder_object_)
               ->GetInterface(recorder_object_, SL_IID_RECORD, &recorder_);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error getting the audio recorder interface.";
    return false;
  }

  // Get the buffer queue interface which was explicitly requested.
  result = (*recorder_object_)
               ->GetInterface(recorder_object_, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                              &buffer_object_);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error getting the buffer queue interface.";
    return false;
  }

  // Setup to receive buffer queue event callbacks for when a buffer in the
  // queue is completed.
  result =
      (*buffer_object_)
          ->RegisterCallback(buffer_object_,
                             &SbMicrophoneImpl::SwapAndPublishBuffer, this);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error registering buffer queue callbacks.";
    return false;
  }

  return true;
}

void SbMicrophoneImpl::DeleteAudioRecorder() {
  if (recorder_object_) {
    (*recorder_object_)->Destroy(recorder_object_);
  }

  config_object_ = nullptr;
  buffer_object_ = nullptr;
  recorder_ = nullptr;
  recorder_object_ = nullptr;

  // Destroy the engine object.
  if (engine_object_) {
    (*engine_object_)->Destroy(engine_object_);
  }
  engine_ = nullptr;
  engine_object_ = nullptr;
}

void SbMicrophoneImpl::ClearBuffer() {
  // Clear the buffer queue to get rid of old data.
  if (buffer_object_) {
    SLresult result = (*buffer_object_)->Clear(buffer_object_);
    if (!CheckReturnValue(result)) {
      SB_DLOG(WARNING) << "Error clearing the buffer.";
    }
  }

  {
    ScopedLock lock(delivered_queue_mutex_);
    while (!delivered_queue_.empty()) {
      delete[] delivered_queue_.front();
      delivered_queue_.pop();
    }
  }

  {
    ScopedLock lock(ready_queue_mutex_);
    while (!ready_queue_.empty()) {
      delete[] ready_queue_.front();
      ready_queue_.pop();
    }
  }
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

int SbMicrophonePrivate::GetAvailableMicrophones(
    SbMicrophoneInfo* out_info_array,
    int info_array_size) {
  // Note that there is no way of checking for a connected microphone/device
  // before API 23, so GetAvailableMicrophones() will assume a microphone is
  // connected and always return 1 on APIs < 23.
  if (starboard::android::shared::SbMicrophoneImpl::
          IsMicrophoneDisconnected()) {
    SB_DLOG(WARNING) << "No microphone connected.";
    return 0;
  }
  if (starboard::android::shared::SbMicrophoneImpl::IsMicrophoneMute()) {
    SB_DLOG(WARNING) << "Microphone is muted.";
    return 0;
  }

  if (out_info_array && info_array_size > 0) {
    // Only support one microphone.
    out_info_array[0].id = reinterpret_cast<SbMicrophoneId>(1);
    out_info_array[0].type = kSbMicrophoneUnknown;
    out_info_array[0].max_sample_rate_hz =
        starboard::android::shared::kSampleRateInHz;
    out_info_array[0].min_read_size =
        starboard::android::shared::kSamplesPerBuffer;
  }

  return 1;
}

bool SbMicrophonePrivate::IsMicrophoneSampleRateSupported(
    SbMicrophoneId id,
    int sample_rate_in_hz) {
  if (!SbMicrophoneIdIsValid(id)) {
    return false;
  }

  return sample_rate_in_hz == starboard::android::shared::kSampleRateInHz;
}

namespace {
const int kUnusedBufferSize = 32 * 1024;
// Only a single microphone is supported.
SbMicrophone s_microphone = kSbMicrophoneInvalid;

}  // namespace

SbMicrophone SbMicrophonePrivate::CreateMicrophone(SbMicrophoneId id,
                                                   int sample_rate_in_hz,
                                                   int buffer_size_bytes) {
  if (!SbMicrophoneIdIsValid(id) ||
      !IsMicrophoneSampleRateSupported(id, sample_rate_in_hz) ||
      buffer_size_bytes > kUnusedBufferSize || buffer_size_bytes <= 0) {
    return kSbMicrophoneInvalid;
  }

  if (s_microphone != kSbMicrophoneInvalid) {
    return kSbMicrophoneInvalid;
  }

  s_microphone = new starboard::android::shared::SbMicrophoneImpl();
  return s_microphone;
}

void SbMicrophonePrivate::DestroyMicrophone(SbMicrophone microphone) {
  if (!SbMicrophoneIsValid(microphone)) {
    return;
  }

  SB_DCHECK(s_microphone == microphone);
  s_microphone->Close();

  delete s_microphone;
  s_microphone = kSbMicrophoneInvalid;
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_AudioPermissionRequester_nativeHandlePermission(
    JNIEnv* env,
    jobject unused_this,
    jlong nativeSbMicrophoneImpl,
    jboolean is_granted) {
  starboard::android::shared::SbMicrophoneImpl* native =
      reinterpret_cast<starboard::android::shared::SbMicrophoneImpl*>(
          nativeSbMicrophoneImpl);
  native->SetPermission(is_granted);
}
