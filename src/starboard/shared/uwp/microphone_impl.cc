// Copyright 2017 Google Inc. All Rights Reserved.
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

// Windows headers.
#include <collection.h>
#include <MemoryBuffer.h>
#include <ppltasks.h>

// C++ headers.
#include <algorithm>
#include <deque>
#include <memory>
#include <sstream>
#include <vector>

#include "starboard/atomic.h"
#include "starboard/common/semaphore.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/shared/uwp/app_accessors.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/uwp/async_utils.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/simple_thread.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"
#include "starboard/time.h"
#include "starboard/user.h"

#if !SB_HAS(MICROPHONE)
#error Microphone expected to be enabled when compiling a microphone impl.
#endif

using concurrency::task_continuation_context;
using Microsoft::WRL::ComPtr;
using starboard::Mutex;
using starboard::scoped_ptr;
using starboard::ScopedLock;
using starboard::Semaphore;
using starboard::shared::uwp::ApplicationUwp;
using starboard::shared::win32::CheckResult;
using starboard::shared::win32::platformStringToString;
using Windows::Devices::Enumeration::DeviceInformation;
using Windows::Devices::Enumeration::DeviceInformationCollection;
using Windows::Foundation::EventRegistrationToken;
using Windows::Foundation::IMemoryBufferByteAccess;
using Windows::Foundation::IMemoryBufferReference;
using Windows::Foundation::TypedEventHandler;
using Windows::Media::Audio::AudioDeviceInputNode;
using Windows::Media::Audio::AudioDeviceNodeCreationStatus;
using Windows::Media::Audio::AudioFrameOutputNode;
using Windows::Media::Audio::AudioGraph;
using Windows::Media::Audio::AudioGraphCreationStatus;
using Windows::Media::Audio::AudioGraphSettings;
using Windows::Media::Audio::CreateAudioDeviceInputNodeResult;
using Windows::Media::Audio::CreateAudioGraphResult;
using Windows::Media::Audio::QuantumSizeSelectionMode;
using Windows::Media::AudioBuffer;
using Windows::Media::AudioBufferAccessMode;
using Windows::Media::AudioFrame;
using Windows::Media::Capture::MediaCategory;
using Windows::Media::Devices::MediaDevice;
using Windows::Media::MediaProperties::AudioEncodingProperties;
using Windows::Media::Render::AudioRenderCategory;

namespace {

// It appears that cobalt will only request 16khz.
const int kMinSampleRate = 16000;
const int kMaxSampleRate = 44100;
const int kNumChannels = 1;
const int kOutputBytesPerSample = sizeof(int16_t);
const int kMinReadSizeBytes = 4096;
const int kMicGain = 1;

// Controls the amount of time that a microphone will record muted audio
// before it signals a read error. Without this trigger, the app
// will continuously wait for audio data. This happens with the Kinect
// device, which when disconnected will still record 0-value samples.
const SbTime kTimeMutedThreshold = 3 * kSbTimeSecond;

// Maps [-1.0f, 1.0f] -> [-32768, 32767]
// Values outside of [-1.0f, 1.0] are clamped.
int16_t To16BitPcm(float val) {
  static const float kMaxFloatValue = std::numeric_limits<int16_t>::max();
  static const float kLowFloatValue = std::numeric_limits<int16_t>::lowest();
  if (val == 0.0f) {
    return 0;
  } else if (val > 0.0f) {
    if (val > 1.0f) {
      val = 1.0;
    }
    return static_cast<int16_t>(val * kMaxFloatValue);
  } else {
    if (val < -1.0f) {
      val = -1.0;
    }
    return static_cast<int16_t>(-1.0f * val * kLowFloatValue);
  }
}

const char* ToString(AudioDeviceNodeCreationStatus status) {
  switch (status) {
    case AudioDeviceNodeCreationStatus::AccessDenied:
      return "AccessDenied";
    case AudioDeviceNodeCreationStatus::DeviceNotAvailable:
      return "DeviceNotAvailable";
    case AudioDeviceNodeCreationStatus::FormatNotSupported:
      return "FormatNotSupported";
    case AudioDeviceNodeCreationStatus::Success:
      return "Success";
    case AudioDeviceNodeCreationStatus::UnknownFailure:
      return "UnknownFailure";
  }
  return "Unknown";
}

bool IsUiThread() {
  auto dispatcher = starboard::shared::uwp::GetDispatcher();
  // Is UI thread.
  return dispatcher->HasThreadAccess;
}

std::vector<DeviceInformation^> GetAllMicrophoneDevices() {
  std::vector<DeviceInformation^> output;
  Platform::String^ audio_str = MediaDevice::GetAudioCaptureSelector();
  DeviceInformationCollection^ all_devices =
      starboard::shared::uwp::WaitForResult(
          DeviceInformation::FindAllAsync(audio_str));
  for (DeviceInformation^ dev_info : all_devices) {
    output.push_back(dev_info);
  }

  return output;
}

AudioGraph^ CreateAudioGraph(AudioRenderCategory category,
                             QuantumSizeSelectionMode selection_mode) {
  AudioGraphSettings^ settings = ref new AudioGraphSettings(category);
  settings->QuantumSizeSelectionMode = selection_mode;
  CreateAudioGraphResult^ result =
      starboard::shared::uwp::WaitForResult(AudioGraph::CreateAsync(settings));
  SB_DCHECK(result->Status == AudioGraphCreationStatus::Success);
  AudioGraph^ graph = result->Graph;
  return graph;
}

std::vector<AudioDeviceInputNode^> GenerateAudioInputNodes(
    const std::vector<DeviceInformation^>& microphone_devices,
    AudioEncodingProperties^ encoding_properties,
    AudioGraph^ graph) {
  std::vector<AudioDeviceInputNode^> output;
  for (DeviceInformation^ mic : microphone_devices) {
    auto create_microphone_input_task = graph->CreateDeviceInputNodeAsync(
        MediaCategory::Speech, encoding_properties, mic);
    CreateAudioDeviceInputNodeResult^ deviceInputNodeResult =
        starboard::shared::uwp::WaitForResult(create_microphone_input_task);

    auto status = deviceInputNodeResult->Status;
    AudioDeviceInputNode^ input_node = deviceInputNodeResult->DeviceInputNode;

    if (status != AudioDeviceNodeCreationStatus::Success) {
      SB_LOG(INFO) << "Failed to create microphone with device name \""
                   << platformStringToString(mic->Name) << "\" because "
                   << ToString(status);
      continue;
    }

    SB_LOG(INFO) << "Created a microphone with device \""
                 << platformStringToString(mic->Name) << "\"";

    input_node->ConsumeInput = true;
    input_node->OutgoingGain = kMicGain;
    output.push_back(input_node);
  }
  return output;
}

// Reinterprets underlying buffer type to match destination vector.
void ExtractRawAudioData(AudioFrameOutputNode^ node,
                         std::vector<float>* destination) {
  AudioFrame^ audio_frame = node->GetFrame();
  AudioBuffer^ audio_buffer =
      audio_frame->LockBuffer(AudioBufferAccessMode::Read);
  IMemoryBufferReference^ memory_buffer_reference =
      audio_buffer->CreateReference();

  ComPtr<IMemoryBufferByteAccess> memory_byte_access;
  HRESULT hr = reinterpret_cast<IInspectable*>(memory_buffer_reference)
                   ->QueryInterface(IID_PPV_ARGS(&memory_byte_access));
  CheckResult(hr);

  BYTE* data = nullptr;
  UINT32 capacity = 0;
  hr = memory_byte_access->GetBuffer(&data, &capacity);
  CheckResult(hr);

  // Audio data is float data, so the buffer must be a multiple of 4.
  SB_DCHECK(capacity % sizeof(float) == 0);

  if (capacity > 0) {
    float* typed_data = reinterpret_cast<float*>(data);
    const size_t typed_data_size = capacity / sizeof(float);
    destination->insert(destination->end(), typed_data,
                        typed_data + typed_data_size);
  }
}

// Timer useful for detecting that the microphone has been muted for a certain
// amount of time.
class MutedTrigger {
 public:
  void SignalMuted() {
    if (state_ != kInitialized) {
      return;
    }
    state_ = kIsMuted;
    time_start_ = SbTimeGetMonotonicNow();
  }

  void SignalSound() {
    state_ = kFoundSound;
  }

  bool IsMuted(SbTimeMonotonic duration_theshold) const {
    if (state_ != kIsMuted) {
      return false;
    }
    SbTimeMonotonic duration = SbTimeGetMonotonicNow() - time_start_;
    return duration > duration_theshold;
  }

 private:
  enum State {
    kInitialized,
    kIsMuted,
    kFoundSound
  };
  State state_ = kInitialized;
  SbTimeMonotonic time_start_ = 0;
};

// MicrophoneProcessor encapsulates Microsoft's audio api. All available
// microphones are queried and instantiated. This class will mix the audio
// together into one signed 16-bit pcm stream.
//
// When the microphone is created it will find all available microphones and
// immediately start recording. A callback will be created which will process
// audio data when new samples are available. The Microphone will stop
// recording when Close() is called.
ref class MicrophoneProcessor sealed {
 public:
  // This will try and create a microphone. This will fail (return null) if
  // there are not available microphones.
  static MicrophoneProcessor^ TryCreateAndStartRecording(
      size_t max_num_samples,
      int sample_rate) {
    std::vector<DeviceInformation^> microphone_devices =
        GetAllMicrophoneDevices();
    if (microphone_devices.empty()) {  // Unexpected condition.
      return nullptr;
    }

    MicrophoneProcessor^ output = ref new MicrophoneProcessor(
        max_num_samples, sample_rate, microphone_devices);

    if (output->input_nodes_.empty()) {
      output = nullptr;
    }
    return output;
  }

  virtual ~MicrophoneProcessor() {
  }

  void Close() {
    audio_graph_->QuantumStarted -= removal_token_;
    ScopedLock lock(mutex_);
    audio_graph_->Stop();
  }

  // Returns the number of elements that have been written, or -1 if there
  // was a read error.
  int Read(int16_t* out_audio_data, size_t out_audio_count) {
    ScopedLock lock(mutex_);
    if (muted_timer_.IsMuted(kTimeMutedThreshold)) {
      return -1;
    }

    out_audio_count = std::min(out_audio_count,
                               pcm_audio_data_.size());
    using iter = std::vector<int16_t>::iterator;
    iter it_begin = pcm_audio_data_.begin();
    iter it_end = pcm_audio_data_.begin() + out_audio_count;
    std::copy(it_begin, it_end, out_audio_data);
    pcm_audio_data_.erase(it_begin, it_end);
    return static_cast<int>(out_audio_count);
  }

 private:
  explicit MicrophoneProcessor(
      size_t max_num_samples,
      int sample_rate,
      const std::vector<DeviceInformation^>& microphone_devices)
          : max_num_samples_(max_num_samples) {
    audio_graph_ = CreateAudioGraph(AudioRenderCategory::Speech,
                                    QuantumSizeSelectionMode::SystemDefault);
    wave_encoder_ =
        AudioEncodingProperties::CreatePcm(sample_rate, kNumChannels,
                                           16);  // 4-byte float.
    SB_DCHECK(audio_graph_);
    input_nodes_ = GenerateAudioInputNodes(microphone_devices, wave_encoder_,
                                           audio_graph_);
    for (AudioDeviceInputNode^ input_node : input_nodes_) {
      AudioFrameOutputNode^ audio_frame_node =
          audio_graph_->CreateFrameOutputNode(wave_encoder_);
      audio_frame_node->ConsumeInput = true;
      input_node->AddOutgoingConnection(audio_frame_node);
      audio_channel_.emplace_back(new std::vector<float>());
      audio_frame_nodes_.push_back(audio_frame_node);
    }
    // Update the audio data whenever a new audio sample has been finished.
    removal_token_ =
        audio_graph_->QuantumStarted +=
            ref new TypedEventHandler<AudioGraph^, Object^>(
                this, &MicrophoneProcessor::OnQuantumStarted);
    audio_graph_->Start();
  }

  void OnQuantumStarted(AudioGraph^, Object^) {
    Process();
  }

  void Process() {
    ScopedLock lock(mutex_);
    if (audio_frame_nodes_.empty()) {
      return;
    }
    for (size_t i = 0; i < audio_frame_nodes_.size(); ++i) {
      ExtractRawAudioData(audio_frame_nodes_[i], audio_channel_[i].get());
    }

    size_t num_elements = max_num_samples_;
    for (const auto& audio_datum : audio_channel_) {
      num_elements = std::min(audio_datum->size(), num_elements);
    }
    if (num_elements == 0) {
      return;
    }

    bool is_muted = true;
    // Mix all available audio channels together and convert to output buffer
    // format. Detect if audio is muted.
    for (int i = 0; i < num_elements; ++i) {
      float mixed_sample = 0.0f;
      for (const auto& audio_datum : audio_channel_) {
        float sample = (*audio_datum)[i];
        if (sample != 0.0) {
          is_muted = false;
        }
        mixed_sample += sample;
      }
      pcm_audio_data_.push_back(To16BitPcm(mixed_sample));
    }

    // Trim values from finished pcm_data if the buffer has exceeded it's
    // allowed size.
    if (pcm_audio_data_.size() > max_num_samples_) {
      size_t num_delete = pcm_audio_data_.size() - max_num_samples_;
      pcm_audio_data_.erase(pcm_audio_data_.begin(),
                            pcm_audio_data_.begin() + num_delete);
    }

    if (is_muted) {
      muted_timer_.SignalMuted();
    } else {
      muted_timer_.SignalSound();
    }
    // Trim values from source channels that were just transfered to
    // pcm_audio_data.
    for (const auto& audio_datum : audio_channel_) {
      audio_datum->erase(audio_datum->begin(),
                         audio_datum->begin() + num_elements);
    }
  }

  AudioGraph^ audio_graph_ = nullptr;
  AudioEncodingProperties^ wave_encoder_;
  std::vector<AudioDeviceInputNode^> input_nodes_;
  std::vector<AudioFrameOutputNode^> audio_frame_nodes_;
  std::vector<std::unique_ptr<std::vector<float>>> audio_channel_;
  std::vector<int16_t> pcm_audio_data_;
  EventRegistrationToken removal_token_;
  size_t max_num_samples_ = 0;
  MutedTrigger muted_timer_;
  Mutex mutex_;
};

// Implements the SbMicrophonePrivate interface.
class MicrophoneImpl : public SbMicrophonePrivate {
 public:
  MicrophoneImpl(int sample_rate, int buffer_size_bytes)
      : buffer_size_bytes_(buffer_size_bytes),
        sample_rate_(sample_rate) {
  }

  ~MicrophoneImpl() { Close(); }

  bool Open() override {
    if (!microphone_.Get()) {
      if (IsUiThread()) {
        SB_LOG(INFO) << "Could not open microphone from UI thread.";
        return false;
      }
      microphone_ = MicrophoneProcessor::TryCreateAndStartRecording(
          buffer_size_bytes_ / kOutputBytesPerSample,
          sample_rate_);
    }
    return microphone_ != nullptr;
  }

  bool Close() override {
    microphone_->Close();
    microphone_ = nullptr;
    return true;
  }

  int Read(void* out_audio_data, int audio_data_size) override {
    if (!microphone_.Get()) {
      return -1;
    }
    int16_t* pcm_buffer = reinterpret_cast<int16*>(out_audio_data);
    size_t pcm_buffer_count = audio_data_size / kOutputBytesPerSample;
    int n_samples = microphone_->Read(pcm_buffer, pcm_buffer_count);
    if (n_samples < 0) {
      return -1;  // Is error.
    } else {
      return n_samples * kOutputBytesPerSample;
    }
  }

 private:
  const int buffer_size_bytes_;
  const int sample_rate_;
  Platform::Agile<MicrophoneProcessor> microphone_;
};

// Singleton access is required by the microphone interface as specified by
// nplb.
const SbMicrophoneId kSingletonId =
    reinterpret_cast<SbMicrophoneId>(0x1);
starboard::atomic_pointer<MicrophoneImpl*> s_singleton_pointer;

}  // namespace.

int SbMicrophonePrivate::GetAvailableMicrophones(
    SbMicrophoneInfo* out_info_array,
    int info_array_size) {
  if (GetAllMicrophoneDevices().empty()) {
    return 0;
  }
  if (out_info_array && (info_array_size >= 1)) {
    SbMicrophoneInfo info = {kSingletonId, kSBMicrophoneAnalogHeadset,
                             kMaxSampleRate, kMinReadSizeBytes};
    out_info_array[0] = info;
  }
  return 1;
}

bool SbMicrophonePrivate::IsMicrophoneSampleRateSupported(
    SbMicrophoneId id,
    int sample_rate_in_hz) {
  if (!SbMicrophoneIdIsValid(id)) {
    return false;
  }
  return (kMinSampleRate <= sample_rate_in_hz) &&
         (sample_rate_in_hz <= kMaxSampleRate);
}

SbMicrophone SbMicrophonePrivate::CreateMicrophone(SbMicrophoneId id,
                                                   int sample_rate_in_hz,
                                                   int buffer_size_bytes) {
  if (!SbMicrophoneIdIsValid(id)) {
    return kSbMicrophoneInvalid;
  }
  if (sample_rate_in_hz < kMinSampleRate) {
    return kSbMicrophoneInvalid;
  }
  if (sample_rate_in_hz > kMaxSampleRate) {
    return kSbMicrophoneInvalid;
  }
  if (buffer_size_bytes <= 0) {
    return kSbMicrophoneInvalid;
  }
  // Required to conform to nplb test.
  if (buffer_size_bytes >= (std::numeric_limits<int>::max() - 1)) {
    return kSbMicrophoneInvalid;
  }
  // Id will either by 1 or 0. At this time there is only one microphone.
  SB_DCHECK(id == kSingletonId);
  if (s_singleton_pointer.load()) {
    return kSbMicrophoneInvalid;
  }
  MicrophoneImpl* new_microphone =
      new MicrophoneImpl(sample_rate_in_hz, buffer_size_bytes);

  s_singleton_pointer.store(new_microphone);
  return new_microphone;
}

void SbMicrophonePrivate::DestroyMicrophone(SbMicrophone microphone) {
  SB_DCHECK(microphone == s_singleton_pointer.load());
  s_singleton_pointer.store(nullptr);
  delete microphone;
}
