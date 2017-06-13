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

#include "starboard/shared/win32/audio_sink.h"

#include <basetyps.h>
#include <wrl.h>
#include <xaudio2.h>

#include <limits>

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "starboard/time.h"

using Microsoft::WRL::ComPtr;

namespace {
// Fails an SB_DCHECK if an HRESULT is not S_OK
void CHECK_HRESULT_OK(HRESULT hr) {
  SB_DCHECK(SUCCEEDED(hr)) << std::hex << hr;
}
}

namespace starboard {
namespace shared {
namespace win32 {

class XAudioAudioSink : public SbAudioSinkPrivate {
 public:
  XAudioAudioSink(Type* type,
                  const WAVEFORMATEX& wfx,
                  SbAudioSinkFrameBuffers frame_buffers,
                  int frame_buffers_size_in_frames,
                  SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                  SbAudioSinkConsumeFramesFunc consume_frame_func,
                  void* context);
  ~XAudioAudioSink() SB_OVERRIDE;

  bool IsType(Type* type) SB_OVERRIDE { return type_ == type; }
  void SetPlaybackRate(double playback_rate) SB_OVERRIDE {
    // TODO make sure to set appropriate MaxFrequencyRatio
    // and use that mechanism here
    SB_NOTIMPLEMENTED();
  }

 private:
  static void* ThreadEntryPoint(void* context);
  void AudioThreadFunc();
  void SubmitSourceBuffer(int offset_in_frames, int count_frames);

  XAudioAudioSinkType* type_;
  SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  SbAudioSinkConsumeFramesFunc consume_frame_func_;
  void* context_;

  SbThread audio_out_thread_;

  SbAudioSinkFrameBuffers frame_buffers_;
  int frame_buffers_size_in_frames_;
  WAVEFORMATEX wfx_;

  // Note: despite some documentation to the contrary, it appears
  // that IXAudio2SourceVoice cannot be a ComPtr.
  IXAudio2SourceVoice* source_voice_;

  // mutex_ protects only destroying_. Everything else is immutable
  // after the constructor.
  ::starboard::Mutex mutex_;
  bool destroying_;
};

XAudioAudioSink::XAudioAudioSink(
    Type* type,
    const WAVEFORMATEX& wfx,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frame_func,
    void* context)
    : type_(static_cast<XAudioAudioSinkType*>(type)),
      update_source_status_func_(update_source_status_func),
      consume_frame_func_(consume_frame_func),
      context_(context),
      audio_out_thread_(kSbThreadInvalid),
      frame_buffers_(frame_buffers),
      frame_buffers_size_in_frames_(frame_buffers_size_in_frames),
      wfx_(wfx),
      destroying_(false) {
  // TODO: Check MaxFrequencyRadio
  CHECK_HRESULT_OK(
      type_->x_audio2_->CreateSourceVoice(&source_voice_, &wfx, 0,
                                          /*MaxFrequencyRadio = */ 1.0));

  CHECK_HRESULT_OK(source_voice_->Start(0));

  audio_out_thread_ =
      SbThreadCreate(0, kSbThreadPriorityRealTime, kSbThreadNoAffinity, true,
                     "audio_out", &XAudioAudioSink::ThreadEntryPoint, this);
  SB_DCHECK(SbThreadIsValid(audio_out_thread_));
}

XAudioAudioSink::~XAudioAudioSink() {
  {
    ScopedLock lock(mutex_);
    destroying_ = true;
  }
  SbThreadJoin(audio_out_thread_, nullptr);
  source_voice_->DestroyVoice();
}

// static
void* XAudioAudioSink::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  XAudioAudioSink* sink = static_cast<XAudioAudioSink*>(context);
  sink->AudioThreadFunc();

  return nullptr;
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

void XAudioAudioSink::AudioThreadFunc() {
  const int kMaxFramesToConsumePerRequest = 1024;

  int submitted_frames = 0;
  uint64_t samples_played = 0;
  for (;;) {
    {
      ScopedLock lock(mutex_);
      if (destroying_) {
        break;
      }
    }
    int frames_in_buffer, offset_in_frames;
    bool is_playing, is_eos_reached;
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);
    // TODO: make sure that frames_in_buffer is large enough
    // that it exceeds the voice state pool interval
    if (!is_playing || frames_in_buffer == 0) {
      SbThreadSleep(kSbTimeMillisecond * 5);
      continue;
    }
    int unsubmitted_frames = frames_in_buffer - submitted_frames;
    int unsubmitted_start =
        (offset_in_frames + submitted_frames) % frame_buffers_size_in_frames_;
    if (unsubmitted_frames == 0) {
      // submit nothing
    } else if (unsubmitted_start + unsubmitted_frames <=
               frame_buffers_size_in_frames_) {
      SubmitSourceBuffer(unsubmitted_start, unsubmitted_frames);
    } else {
      int count_tail_frames = frame_buffers_size_in_frames_ - unsubmitted_start;
      SubmitSourceBuffer(unsubmitted_start, count_tail_frames);
      SubmitSourceBuffer(0, unsubmitted_frames - count_tail_frames);
    }
    submitted_frames = frames_in_buffer;

    SbThreadSleep(kSbTimeMillisecond);

    XAUDIO2_VOICE_STATE voice_state;
    source_voice_->GetState(&voice_state);

    int64_t consumed_frames = voice_state.SamplesPlayed - samples_played;
    SB_DCHECK(consumed_frames >= 0);
    SB_DCHECK(consumed_frames < std::numeric_limits<int>::max());
    int consumed_frames_int = static_cast<int>(consumed_frames);

    consume_frame_func_(consumed_frames_int, context_);
    submitted_frames -= consumed_frames_int;
    samples_played = voice_state.SamplesPlayed;
  }
}

namespace {

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

}  // namespace

XAudioAudioSinkType::XAudioAudioSinkType() {
  CHECK_HRESULT_OK(XAudio2Create(&x_audio2_, 0, XAUDIO2_DEFAULT_PROCESSOR));
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
  wfx.nChannels = channels;
  wfx.nSamplesPerSec = sampling_frequency_hz;
  wfx.nAvgBytesPerSec = channels *
                        SampleTypeToBitsPerSample(audio_sample_type) *
                        sampling_frequency_hz / 8;
  wfx.wBitsPerSample = SampleTypeToBitsPerSample(audio_sample_type);
  wfx.nBlockAlign = (channels * wfx.wBitsPerSample) / 8;
  wfx.cbSize = 0;

  return new XAudioAudioSink(
      this, wfx, frame_buffers, frame_buffers_size_in_frames,
      update_source_status_func, consume_frames_func, context);
}

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
