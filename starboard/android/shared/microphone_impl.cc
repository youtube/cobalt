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

// clang-format off
#include "starboard/shared/starboard/microphone/microphone_internal.h"
// clang-format on

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/thread_checker.h"

using starboard::android::shared::JniEnvExt;

namespace starboard::android::shared {
namespace {

const int kNumOfOpenSLESBuffers = 2;

bool CheckReturnValue(SLresult result) {
  return result == SL_RESULT_SUCCESS;
}
}  // namespace

class SbMicrophoneImpl : public SbMicrophonePrivate {
 public:
  SbMicrophoneImpl(int sample_rate_in_hz, int buffer_size_bytes);
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

  const int sample_rate_in_hz_;
  const int buffer_size_bytes_;
  const int samples_per_buffer_;
  const int buffer_size_in_bytes_;

  // Keeps track of the microphone's current state.
  State state_;

  // To avoid allocations in the audio callback, we use a fixed pool of buffers.
  std::vector<int16_t*> buffer_pool_;
  std::mutex pool_mutex_;
  std::queue<int16_t*> free_queue_;

  // Audio data that has been delivered to the OpenSL ES buffer queue.
  std::mutex delivered_queue_mutex_;
  std::queue<int16_t*> delivered_queue_;

  // Audio data that is ready to be read by Cobalt.
  std::mutex ready_queue_mutex_;
  std::queue<int16_t*> ready_queue_;

  int16_t* ready_buffer_ = nullptr;
  int ready_buffer_offset_ = 0;
};

SbMicrophoneImpl::SbMicrophoneImpl(int sample_rate_in_hz, int buffer_size_bytes)
    : engine_object_(nullptr),
      engine_(nullptr),
      recorder_object_(nullptr),
      recorder_(nullptr),
      buffer_object_(nullptr),
      config_object_(nullptr),
      sample_rate_in_hz_(sample_rate_in_hz),
      buffer_size_bytes_(buffer_size_bytes),
      samples_per_buffer_(sample_rate_in_hz / 100),  // 10ms per buffer
      buffer_size_in_bytes_(samples_per_buffer_ * sizeof(int16_t)),
      state_(kClosed) {
  // Pre-allocate a fixed pool of buffers to avoid new/delete in real-time path.
  // We need at least kNumOfOpenSLESBuffers plus a few extra for the ready
  // queue.
  const int kPoolSize = kNumOfOpenSLESBuffers + 10;
  for (int i = 0; i < kPoolSize; ++i) {
    int16_t* buffer = new int16_t[samples_per_buffer_];
    buffer_pool_.push_back(buffer);
    free_queue_.push(buffer);
  }
}

SbMicrophoneImpl::~SbMicrophoneImpl() {
  Close();
  // All buffers should be back in the pool or otherwise accounted for.
  for (int16_t* buffer : buffer_pool_) {
    delete[] buffer;
  }
}

bool SbMicrophoneImpl::RequestAudioPermission() {
  JniEnvExt* env = JniEnvExt::Get();
  jobject j_audio_permission_requester =
      static_cast<jobject>(env->CallStarboardObjectMethodOrAbort(
          "getAudioPermissionRequester",
          "()Ldev/cobalt/coat/AudioPermissionRequester;"));
  jboolean j_permission = env->CallBooleanMethodOrAbort(
      j_audio_permission_requester, "requestRecordAudioPermission", "()Z");
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
  SB_DLOG(INFO) << "SbMicrophoneImpl::Open state=" << state_;
  if (state_ == kOpened) {
    ClearBuffer();
    return true;
  }

  if (IsMicrophoneDisconnected()) {
    SB_DLOG(WARNING) << "No microphone connected.";
    return false;
  } else if (!RequestAudioPermission()) {
    state_ = kWaitPermission;
    SB_DLOG(INFO) << "SbMicrophoneImpl::Open waiting for permission";
    return true;
  } else if (!StartRecording()) {
    SB_DLOG(WARNING) << "SbMicrophoneImpl::Open StartRecording FAILED";
    return false;
  }

  SB_DLOG(INFO) << "SbMicrophoneImpl::Open SUCCESS";
  state_ = kOpened;
  return true;
}

bool SbMicrophoneImpl::StartRecording() {
  SB_DLOG(INFO) << "SbMicrophoneImpl::StartRecording";
  if (!CreateAudioRecorder()) {
    SB_DLOG(WARNING) << "Create audio recorder failed.";
    DeleteAudioRecorder();
    return false;
  }

  // Enqueue initial buffers from the pool.
  for (int i = 0; i < kNumOfOpenSLESBuffers; ++i) {
    int16_t* buffer = nullptr;
    {
      std::lock_guard lock(pool_mutex_);
      if (!free_queue_.empty()) {
        buffer = free_queue_.front();
        free_queue_.pop();
      }
    }

    if (buffer) {
      memset(buffer, 0, buffer_size_in_bytes_);
      {
        std::lock_guard lock(delivered_queue_mutex_);
        delivered_queue_.push(buffer);
      }
      SLresult result =
          (*buffer_object_)
              ->Enqueue(buffer_object_, buffer, buffer_size_in_bytes_);
      if (!CheckReturnValue(result)) {
        SB_DLOG(WARNING) << "Error adding buffers to the queue.";
        return false;
      }
    }
  }

  SLresult result =
      (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_RECORDING);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "SetRecordState(RECORDING) FAILED result=" << result;
    return false;
  }

  SB_DLOG(INFO) << "SbMicrophoneImpl::StartRecording SUCCESS";
  return true;
}

bool SbMicrophoneImpl::Close() {
  SB_DLOG(INFO) << "SbMicrophoneImpl::Close state=" << state_;
  if (state_ == kClosed) {
    return true;
  }

  if (state_ == kOpened && !StopRecording()) {
    SB_DLOG(WARNING) << "Error closing the microphone.";
    return false;
  }

  state_ = kClosed;
  return true;
}

bool SbMicrophoneImpl::StopRecording() {
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
    return -1;
  }

  if (!out_audio_data || audio_data_size == 0) {
    return 0;
  }

  if (state_ == kWaitPermission) {
    if (RequestAudioPermission()) {
      state_ = kPermissionGranted;
    } else {
      return 0;
    }
  }

  if (state_ == kPermissionGranted) {
    if (StartRecording()) {
      state_ = kOpened;
    } else {
      state_ = kClosed;
      return -1;
    }
  }

  int bytes_copied = 0;
  uint8_t* output = static_cast<uint8_t*>(out_audio_data);

  std::lock_guard lock(ready_queue_mutex_);

  while (bytes_copied < audio_data_size) {
    if (ready_buffer_ == nullptr) {
      if (ready_queue_.empty()) {
        break;
      }
      ready_buffer_ = ready_queue_.front();
      ready_queue_.pop();
      ready_buffer_offset_ = 0;
    }

    int remaining_in_buffer = buffer_size_in_bytes_ - ready_buffer_offset_;
    int remaining_to_copy = audio_data_size - bytes_copied;
    int to_copy = std::min(remaining_in_buffer, remaining_to_copy);

    memcpy(output + bytes_copied,
           reinterpret_cast<uint8_t*>(ready_buffer_) + ready_buffer_offset_,
           to_copy);

    bytes_copied += to_copy;
    ready_buffer_offset_ += to_copy;

    if (ready_buffer_offset_ >= buffer_size_in_bytes_) {
      // Return the buffer to the free pool instead of deleting it.
      {
        std::lock_guard lock(pool_mutex_);
        free_queue_.push(ready_buffer_);
      }
      ready_buffer_ = nullptr;
      ready_buffer_offset_ = 0;
    }
  }

  return bytes_copied;
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
  int16_t* filled_buffer = nullptr;
  {
    std::lock_guard lock(delivered_queue_mutex_);
    if (!delivered_queue_.empty()) {
      filled_buffer = delivered_queue_.front();
      delivered_queue_.pop();
    }
  }

  if (filled_buffer != NULL) {
    std::lock_guard lock(ready_queue_mutex_);
    ready_queue_.push(filled_buffer);
  }

  if (state_ == kOpened) {
    int16_t* next_buffer = nullptr;
    {
      std::lock_guard lock(pool_mutex_);
      if (!free_queue_.empty()) {
        next_buffer = free_queue_.front();
        free_queue_.pop();
      }
    }

    if (next_buffer) {
      memset(next_buffer, 0, buffer_size_in_bytes_);
      {
        std::lock_guard lock(delivered_queue_mutex_);
        delivered_queue_.push(next_buffer);
      }
      SLresult result =
          (*buffer_object_)
              ->Enqueue(buffer_object_, next_buffer, buffer_size_in_bytes_);
      CheckReturnValue(result);
    }
  }
}

bool SbMicrophoneImpl::CreateAudioRecorder() {
  SLresult result;
  result = slCreateEngine(&engine_object_, 0, nullptr, 0, nullptr, nullptr);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error creating the SL engine object.";
    return false;
  }

  result = (*engine_object_)
               ->Realize(engine_object_, /* async = */ SL_BOOLEAN_FALSE);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error realizing the SL engine object.";
    return false;
  }

  result =
      (*engine_object_)->GetInterface(engine_object_, SL_IID_ENGINE, &engine_);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error getting the SL engine interface.";
    return false;
  }

  SLDataLocator_IODevice input_dev_locator = {
      SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
      SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};

  SLDataSource audio_source = {&input_dev_locator, nullptr};
  SLDataLocator_AndroidSimpleBufferQueue simple_buffer_queue = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, kNumOfOpenSLESBuffers};

  SLAndroidDataFormat_PCM_EX format = {
      SL_ANDROID_DATAFORMAT_PCM_EX,
      1 /* numChannels */,
      static_cast<SLuint32>(sample_rate_in_hz_ * 1000),
      SL_PCMSAMPLEFORMAT_FIXED_16,
      SL_PCMSAMPLEFORMAT_FIXED_16,
      SL_SPEAKER_FRONT_CENTER,
      SL_BYTEORDER_LITTLEENDIAN,
      SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT};
  SLDataSink audio_sink = {&simple_buffer_queue, &format};

  const int kCount = 2;
  const SLInterfaceID kInterfaceId[kCount] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                              SL_IID_ANDROIDCONFIGURATION};
  const SLboolean kInterfaceRequired[kCount] = {SL_BOOLEAN_TRUE,
                                                SL_BOOLEAN_TRUE};
  result = (*engine_)->CreateAudioRecorder(engine_, &recorder_object_,
                                           &audio_source, &audio_sink, kCount,
                                           kInterfaceId, kInterfaceRequired);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error creating the audio recorder.";
    return false;
  }

  result = (*recorder_object_)
               ->GetInterface(recorder_object_, SL_IID_ANDROIDCONFIGURATION,
                              &config_object_);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error getting the audio recorder interface.";
    return false;
  }

  const SLuint32 kPresetValue = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
  result =
      (*config_object_)
          ->SetConfiguration(config_object_, SL_ANDROID_KEY_RECORDING_PRESET,
                             &kPresetValue, sizeof(SLuint32));
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error configuring the audio recorder.";
    return false;
  }

  result = (*recorder_object_)
               ->Realize(recorder_object_, /* async = */ SL_BOOLEAN_FALSE);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error realizing the audio recorder.";
    return false;
  }

  result = (*recorder_object_)
               ->GetInterface(recorder_object_, SL_IID_RECORD, &recorder_);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error getting the audio recorder interface.";
    return false;
  }

  result = (*recorder_object_)
               ->GetInterface(recorder_object_, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                              &buffer_object_);
  if (!CheckReturnValue(result)) {
    SB_DLOG(WARNING) << "Error getting the buffer queue interface.";
    return false;
  }

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

  if (engine_object_) {
    (*engine_object_)->Destroy(engine_object_);
  }
  engine_ = nullptr;
  engine_object_ = nullptr;
}

void SbMicrophoneImpl::ClearBuffer() {
  if (buffer_object_) {
    SLresult result = (*buffer_object_)->Clear(buffer_object_);
    if (!CheckReturnValue(result)) {
      SB_DLOG(WARNING) << "Error clearing the buffer.";
    }
  }

  {
    std::lock_guard lock(delivered_queue_mutex_);
    while (!delivered_queue_.empty()) {
      {
        std::lock_guard lock(pool_mutex_);
        free_queue_.push(delivered_queue_.front());
      }
      delivered_queue_.pop();
    }
  }

  {
    std::lock_guard lock(ready_queue_mutex_);
    while (!ready_queue_.empty()) {
      {
        std::lock_guard lock(pool_mutex_);
        free_queue_.push(ready_queue_.front());
      }
      ready_queue_.pop();
    }

    if (ready_buffer_) {
      {
        std::lock_guard lock(pool_mutex_);
        free_queue_.push(ready_buffer_);
      }
      ready_buffer_ = nullptr;
      ready_buffer_offset_ = 0;
    }
  }
}

}  // namespace starboard::android::shared

int SbMicrophonePrivate::GetAvailableMicrophones(
    SbMicrophoneInfo* out_info_array,
    int info_array_size) {
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
    out_info_array[0].id = reinterpret_cast<SbMicrophoneId>(1);
    out_info_array[0].type = kSbMicrophoneUnknown;
    out_info_array[0].max_sample_rate_hz = 48000;
    out_info_array[0].min_read_size = 128;
  }

  return 1;
}

bool SbMicrophonePrivate::IsMicrophoneSampleRateSupported(
    SbMicrophoneId id,
    int sample_rate_in_hz) {
  if (!SbMicrophoneIdIsValid(id)) {
    return false;
  }
  return sample_rate_in_hz >= 8000 && sample_rate_in_hz <= 48000;
}

namespace {
const int kUnusedBufferSize = 1024 * 1024;
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

  s_microphone = new starboard::android::shared::SbMicrophoneImpl(
      sample_rate_in_hz, buffer_size_bytes);
  return s_microphone;
}

void SbMicrophonePrivate::DestroyMicrophone(SbMicrophone microphone) {
  if (!SbMicrophoneIsValid(microphone)) {
    return;
  }

  SB_DCHECK_EQ(s_microphone, microphone);
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
