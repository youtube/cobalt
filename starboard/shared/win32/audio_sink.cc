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

#include <basetyps.h>
#include <wrl.h>
#include <xaudio2.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "starboard/condition_variable.h"
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/thread.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace win32 {
namespace {

using Microsoft::WRL::ComPtr;

const int kMaxBuffersSubmittedPerLoop = 2;

// Fails an SB_DCHECK if an HRESULT is not S_OK
void CHECK_HRESULT_OK(HRESULT hr) {
  SB_DCHECK(SUCCEEDED(hr)) << std::hex << hr;
}

WORD SampleTypeToFormatTag(SbMediaAudioSampleType type) {
  switch (type) {
    case kSbMediaAudioSampleTypeInt16:
      return WAVE_FORMAT_PCM;
    case kSbMediaAudioSampleTypeFloat32:
      return WAVE_FORMAT_IEEE_FLOAT;
    default:
      SB_NOTREACHED();
      return 0;
  }
}

WORD SampleTypeToBitsPerSample(SbMediaAudioSampleType type) {
  switch (type) {
    case kSbMediaAudioSampleTypeInt16:
      return 16;
    case kSbMediaAudioSampleTypeFloat32:
      return 32;
    default:
      SB_NOTREACHED();
      return 0;
  }
}

class XAudioAudioSinkType;

class XAudioAudioSink : public SbAudioSinkPrivate {
 public:
  XAudioAudioSink(XAudioAudioSinkType* type,
                  IXAudio2SourceVoice* source_voice,
                  const WAVEFORMATEX& wfx,
                  SbAudioSinkFrameBuffers frame_buffers,
                  int frame_buffers_size_in_frames,
                  SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                  SbAudioSinkConsumeFramesFunc consume_frame_func,
                  void* context);
  ~XAudioAudioSink() override;

  bool IsType(Type* type) override;
  void SetPlaybackRate(double playback_rate) override {
    SB_DCHECK(playback_rate >= 0.0);
    if (playback_rate != 0.0 && playback_rate != 1.0) {
      SB_NOTIMPLEMENTED() << "TODO: Only playback rates of 0.0 and 1.0 are "
                             "currently supported.";
      playback_rate = (playback_rate > 0.0) ? 1.0 : 0.0;
    }
    ScopedLock lock(mutex_);
    playback_rate_ = playback_rate;
  }
  void SetVolume(double volume) override {
    ScopedLock lock(mutex_);
    volume_ = volume;
  }
  void Process();

 private:
  void SubmitSourceBuffer(int offset_in_frames, int count_frames);

  XAudioAudioSinkType* const type_;
  const SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  const SbAudioSinkConsumeFramesFunc consume_frame_func_;
  void* const context_;

  SbAudioSinkFrameBuffers frame_buffers_;
  const int frame_buffers_size_in_frames_;
  const WAVEFORMATEX wfx_;

  // Note: despite some documentation to the contrary, it appears
  // that IXAudio2SourceVoice cannot be a ComPtr.
  IXAudio2SourceVoice* source_voice_;

  // |mutex_| protects only |destroying_| and |playback_rate_|.
  Mutex mutex_;
  double playback_rate_;
  double volume_;
  // The following variables are only used inside Process().  To keep it in the
  // class simply to allow them to be kept between Process() calls.
  int submited_frames_;
  int samples_played_;
  int queued_buffers_;
  bool was_playing_;
  double current_volume_;
};

class XAudioAudioSinkType : public SbAudioSinkPrivate::Type,
                            private IXAudio2EngineCallback {
 public:
  XAudioAudioSinkType();

  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbMediaAudioFrameStorageType audio_frame_storage_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frame_buffers_size_in_frames,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkConsumeFramesFunc consume_frames_func,
      void* context);

  bool IsValid(SbAudioSink audio_sink) override {
    return audio_sink != kSbAudioSinkInvalid && audio_sink->IsType(this);
  }

  void Destroy(SbAudioSink audio_sink) override;

 private:
  // IXAudio2EngineCallback methods
  // This function will be called periodically with an interval of ~10ms.
  void OnProcessingPassStart() override;
  void OnProcessingPassEnd() override {}
  void OnCriticalError(HRESULT) override {}

  ComPtr<IXAudio2> x_audio2_;
  IXAudio2MasteringVoice* mastering_voice_;

  Mutex mutex_;
  std::vector<XAudioAudioSink*> audio_sinks_to_add_;
  std::vector<SbAudioSink> audio_sinks_to_delete_;
  ConditionVariable sink_removed_signal_;
  std::vector<XAudioAudioSink*> audio_sinks_on_xaudio_callbacks_;
};

XAudioAudioSink::XAudioAudioSink(
    XAudioAudioSinkType* type,
    IXAudio2SourceVoice* source_voice,
    const WAVEFORMATEX& wfx,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frame_func,
    void* context)
    : type_(type),
      source_voice_(source_voice),
      update_source_status_func_(update_source_status_func),
      consume_frame_func_(consume_frame_func),
      context_(context),
      frame_buffers_(frame_buffers),
      frame_buffers_size_in_frames_(frame_buffers_size_in_frames),
      wfx_(wfx),
      playback_rate_(1.0),
      volume_(1.0),
      submited_frames_(0),
      samples_played_(0),
      queued_buffers_(0),
      was_playing_(false),
      current_volume_(1.0) {
  CHECK_HRESULT_OK(source_voice_->Stop(0));
}

XAudioAudioSink::~XAudioAudioSink() {
  source_voice_->DestroyVoice();
}

bool XAudioAudioSink::IsType(Type* type) {
  return type_ == type;
}

void XAudioAudioSink::Process() {
  int frames_in_buffer, offset_in_frames;
  bool is_playing, is_eos_reached;
  bool is_playback_rate_zero = false;
  bool should_set_volume = false;

  // This function is run on the XAudio thread and shouldn't be blocked.
  if (mutex_.AcquireTry()) {
    is_playback_rate_zero = playback_rate_ == 0.0;
    should_set_volume = current_volume_ != volume_;
    current_volume_ = volume_;
    mutex_.Release();
  }

  if (should_set_volume) {
    CHECK_HRESULT_OK(source_voice_->SetVolume(current_volume_));
  }

  update_source_status_func_(&frames_in_buffer, &offset_in_frames, &is_playing,
                             &is_eos_reached, context_);
  if (is_playback_rate_zero) {
    is_playing = false;
  }

  if (is_playing != was_playing_) {
    if (is_playing) {
      CHECK_HRESULT_OK(source_voice_->Start(0));
    } else {
      CHECK_HRESULT_OK(source_voice_->Stop(0));
    }
  }
  was_playing_ = is_playing;

  // TODO: make sure that frames_in_buffer is large enough
  // that it exceeds the voice state pool interval
  if (!is_playing || frames_in_buffer == 0 || is_playback_rate_zero) {
    return;
  }
  int unsubmitted_frames = frames_in_buffer - submited_frames_;
  int unsubmitted_start =
      (offset_in_frames + submited_frames_) % frame_buffers_size_in_frames_;
  if (unsubmitted_frames == 0 || queued_buffers_ + kMaxBuffersSubmittedPerLoop >
                                     XAUDIO2_MAX_QUEUED_BUFFERS) {
    // submit nothing
  } else if (unsubmitted_start + unsubmitted_frames <=
             frame_buffers_size_in_frames_) {
    SubmitSourceBuffer(unsubmitted_start, unsubmitted_frames);
  } else {
    int count_tail_frames = frame_buffers_size_in_frames_ - unsubmitted_start;
    // Note since we can submit up to two source buffers at a time,
    // kMaxBuffersSubmittedPerLoop = 2.
    SubmitSourceBuffer(unsubmitted_start, count_tail_frames);
    SubmitSourceBuffer(0, unsubmitted_frames - count_tail_frames);
  }
  submited_frames_ = frames_in_buffer;

  XAUDIO2_VOICE_STATE voice_state;
  source_voice_->GetState(&voice_state);

  int64_t consumed_frames = voice_state.SamplesPlayed - samples_played_;
  SB_DCHECK(consumed_frames >= 0);
  SB_DCHECK(consumed_frames <= std::numeric_limits<int>::max());
  int consumed_frames_int = static_cast<int>(consumed_frames);

  consume_frame_func_(consumed_frames_int, context_);
  submited_frames_ -= consumed_frames_int;
  samples_played_ = voice_state.SamplesPlayed;
  queued_buffers_ = voice_state.BuffersQueued;
}

void XAudioAudioSink::SubmitSourceBuffer(int offset_in_frames,
                                         int count_frames) {
  XAUDIO2_BUFFER audio_buffer_info;

  audio_buffer_info.Flags = 0;
  audio_buffer_info.AudioBytes = wfx_.nChannels *
                                 frame_buffers_size_in_frames_ *
                                 (wfx_.wBitsPerSample / 8);
  audio_buffer_info.pAudioData = static_cast<const BYTE*>(frame_buffers_[0]);
  audio_buffer_info.PlayBegin = offset_in_frames;
  audio_buffer_info.PlayLength = count_frames;
  audio_buffer_info.LoopBegin = 0;
  audio_buffer_info.LoopLength = 0;
  audio_buffer_info.LoopCount = 0;
  audio_buffer_info.pContext = nullptr;
  CHECK_HRESULT_OK(source_voice_->SubmitSourceBuffer(&audio_buffer_info));
}

XAudioAudioSinkType::XAudioAudioSinkType() : sink_removed_signal_(mutex_) {
  CHECK_HRESULT_OK(XAudio2Create(&x_audio2_, 0, XAUDIO2_DEFAULT_PROCESSOR));

#if !defined(COBALT_BUILD_TYPE_GOLD)
  XAUDIO2_DEBUG_CONFIGURATION debug_config = {};
  debug_config.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS |
                           XAUDIO2_LOG_INFO | XAUDIO2_LOG_DETAIL |
                           XAUDIO2_LOG_TIMING | XAUDIO2_LOG_LOCKS;
  debug_config.LogThreadID = TRUE;
  debug_config.LogFileline = TRUE;
  debug_config.LogFunctionName = TRUE;
  debug_config.LogTiming = TRUE;
  x_audio2_->SetDebugConfiguration(&debug_config, NULL);
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

  x_audio2_->RegisterForCallbacks(this);
  CHECK_HRESULT_OK(x_audio2_->CreateMasteringVoice(&mastering_voice_));
}

SbAudioSink XAudioAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frames_func,
    void* context) {
  SB_DCHECK(audio_frame_storage_type ==
            kSbMediaAudioFrameStorageTypeInterleaved);

  WAVEFORMATEX wfx;

  wfx.wFormatTag = SampleTypeToFormatTag(audio_sample_type);
  wfx.nChannels = static_cast<WORD>(channels);
  wfx.nSamplesPerSec = sampling_frequency_hz;
  wfx.nAvgBytesPerSec = channels *
                        SampleTypeToBitsPerSample(audio_sample_type) *
                        sampling_frequency_hz / 8;
  wfx.wBitsPerSample = SampleTypeToBitsPerSample(audio_sample_type);
  wfx.nBlockAlign = static_cast<WORD>((channels * wfx.wBitsPerSample) / 8);
  wfx.cbSize = 0;

  IXAudio2SourceVoice* source_voice;
  CHECK_HRESULT_OK(x_audio2_->CreateSourceVoice(&source_voice, &wfx,
                                                XAUDIO2_VOICE_NOPITCH, 1.f));

  XAudioAudioSink* audio_sink = new XAudioAudioSink(
      this, source_voice, wfx, frame_buffers, frame_buffers_size_in_frames,
      update_source_status_func, consume_frames_func, context);

  ScopedLock lock(mutex_);
  audio_sinks_to_add_.push_back(audio_sink);
  return audio_sink;
}

void XAudioAudioSinkType::Destroy(SbAudioSink audio_sink) {
  if (audio_sink == kSbAudioSinkInvalid) {
    return;
  }
  if (!IsValid(audio_sink)) {
    SB_LOG(WARNING) << "audio_sink is invalid.";
    return;
  }
  ScopedLock lock(mutex_);
  audio_sinks_to_delete_.push_back(audio_sink);
  while (!audio_sinks_to_delete_.empty()) {
    sink_removed_signal_.Wait();
  }
  delete audio_sink;
}

void XAudioAudioSinkType::OnProcessingPassStart() {
  if (mutex_.AcquireTry()) {
    if (!audio_sinks_to_add_.empty()) {
      audio_sinks_on_xaudio_callbacks_.insert(
          audio_sinks_on_xaudio_callbacks_.end(), audio_sinks_to_add_.begin(),
          audio_sinks_to_add_.end());
      audio_sinks_to_add_.clear();
    }
    if (!audio_sinks_to_delete_.empty()) {
      for (auto sink : audio_sinks_to_delete_) {
        audio_sinks_on_xaudio_callbacks_.erase(
            std::find(audio_sinks_on_xaudio_callbacks_.begin(),
                      audio_sinks_on_xaudio_callbacks_.end(), sink));
      }
      audio_sinks_to_delete_.clear();
      sink_removed_signal_.Broadcast();
    }
    mutex_.Release();
  }

  for (auto sink : audio_sinks_on_xaudio_callbacks_) {
    sink->Process();
  }
}

}  // namespace
}  // namespace win32
}  // namespace shared
}  // namespace starboard

namespace {
SbAudioSinkPrivate::Type* audio_sink_;
}  // namespace

// static
void SbAudioSinkPrivate::PlatformInitialize() {
  SB_DCHECK(!audio_sink_);
  audio_sink_ = new starboard::shared::win32::XAudioAudioSinkType();
  SetPrimaryType(audio_sink_);
  EnableFallbackToStub();
}

// static
void SbAudioSinkPrivate::PlatformTearDown() {
  SB_DCHECK(audio_sink_ == GetPrimaryType());
  SetPrimaryType(nullptr);
  delete audio_sink_;
  audio_sink_ = nullptr;
}
