// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/uwp/wasapi_audio_sink.h"

#include <mfapi.h>

#include <utility>

namespace starboard {
namespace shared {
namespace uwp {
namespace {

#define CHECK_HRESULT_OK(hr)                                         \
  do {                                                               \
    SB_DCHECK(hr == S_OK) << "WASAPI audio sink error, error code: " \
                          << std::hex << hr;                         \
  } while (false)

void SetPassthroughWaveFormat(WAVEFORMATEXTENSIBLE* wfext,
                              SbMediaAudioCodec audio_codec,
                              int channels,
                              int sample_rate) {
  SB_DCHECK(audio_codec == kSbMediaAudioCodecAc3 ||
            audio_codec == kSbMediaAudioCodecEac3);

  wfext->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  wfext->Format.nChannels = kIec60958Channels;
  wfext->Format.wBitsPerSample = kIec60958BitsPerSample;
  wfext->Format.nSamplesPerSec = sample_rate;
  wfext->Format.nBlockAlign = kIec60958BlockAlign;
  wfext->Format.nAvgBytesPerSec =
      wfext->Format.nSamplesPerSec * wfext->Format.nBlockAlign;
  wfext->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  wfext->Samples.wValidBitsPerSample = wfext->Format.wBitsPerSample;
  wfext->dwChannelMask = KSAUDIO_SPEAKER_DIRECTOUT;
  wfext->SubFormat = audio_codec == kSbMediaAudioCodecAc3
                         ? MFAudioFormat_Dolby_AC3_SPDIF
                         : KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS;
}

}  // namespace

WASAPIAudioSink::WASAPIAudioSink()
    : thread_checker_(starboard::ThreadChecker::kSetThreadIdOnFirstCheck) {
  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> device_enumerator;
  HRESULT hr = CoCreateInstance(
      CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator,
      reinterpret_cast<void**>(device_enumerator.GetAddressOf()));
  CHECK_HRESULT_OK(hr);
  hr = device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                  device_.GetAddressOf());
  CHECK_HRESULT_OK(hr);
}

bool WASAPIAudioSink::Initialize(int channels,
                                 int sample_rate,
                                 SbMediaAudioCodec audio_codec) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(audio_codec == kSbMediaAudioCodecAc3 ||
            audio_codec == kSbMediaAudioCodecEac3);

  HRESULT hr;
  WAVEFORMATEXTENSIBLE wave_format;
  SetPassthroughWaveFormat(&wave_format, audio_codec, channels, sample_rate);

  hr =
      device_->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL,
                        reinterpret_cast<void**>(audio_client_.GetAddressOf()));
  if (hr != S_OK) {
    SB_LOG(ERROR) << "Failed to activate audio client, error code: " << std::hex
                  << hr;
    return false;
  }

  // 300 milliseconds, as REFERENCE_TIME is in units of 100 nanoseconds.
  REFERENCE_TIME buffer_duration = 300 * 10000;
  hr =
      audio_client_->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, 0, buffer_duration,
                                buffer_duration, &wave_format.Format, NULL);

  if (hr != S_OK) {
    SB_LOG(ERROR) << "Failed to initialize audio client, error code: "
                  << std::hex << hr;
    return false;
  }

  hr = audio_client_->GetService(
      IID_ISimpleAudioVolume,
      reinterpret_cast<void**>(audio_volume_.GetAddressOf()));
  if (hr != S_OK) {
    SB_LOG(ERROR) << "Failed to initialize volume handler, error code: "
                  << std::hex << hr;
    return false;
  }

  hr = audio_client_->GetService(
      IID_IAudioRenderClient,
      reinterpret_cast<void**>(render_client_.GetAddressOf()));
  if (hr != S_OK) {
    SB_LOG(ERROR) << "Failed to initialize render client, error code: "
                  << std::hex << hr;
    return false;
  }

  hr = audio_client_->GetService(
      IID_IAudioClock, reinterpret_cast<void**>(audio_clock_.GetAddressOf()));
  if (hr != S_OK) {
    SB_LOG(ERROR) << "Failed to initialize audio clock, error code: "
                  << std::hex << hr;
    return false;
  }
  audio_clock_->GetFrequency(&audio_clock_frequency_);
  SB_DCHECK(audio_clock_frequency_ > 0);

  audio_client_->GetBufferSize(&client_buffer_size_in_frames_);

  frames_per_audio_buffer_ = audio_codec == kSbMediaAudioCodecAc3
                                 ? kAc3BufferSizeInFrames
                                 : kEac3BufferSizeInFrames;

  SB_DCHECK(client_buffer_size_in_frames_ >= frames_per_audio_buffer_);

  return true;
}

bool WASAPIAudioSink::WriteBuffer(scoped_refptr<DecodedAudio> decoded_audio) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(decoded_audio->size_in_bytes() == kAc3BufferSize ||
            decoded_audio->size_in_bytes() == kEac3BufferSize);
  SB_DCHECK(decoded_audio);

  if (!job_thread_) {
    job_thread_.reset(new JobThread("wasapi_audio_sink"));
  }

  ScopedLock lock(output_frames_mutex_);
  bool queued_decoded_audio = false;
  if (pending_decoded_audios_.size() < kMaxDecodedAudios) {
    pending_decoded_audios_.push(std::move(decoded_audio));
    queued_decoded_audio = true;
  }

  job_thread_->job_queue()->Schedule(
      std::bind(&WASAPIAudioSink::OutputFrames, this));

  return queued_decoded_audio;
}

void WASAPIAudioSink::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  job_thread_.reset();
  was_playing_ = false;
  ScopedLock decoded_audios_lock(output_frames_mutex_);
  pending_decoded_audios_ = std::queue<scoped_refptr<DecodedAudio>>();
  audio_client_->Stop();
  ScopedLock audio_clock_lock(audio_clock_mutex_);
  audio_client_->Reset();
}

void WASAPIAudioSink::Pause() {
  paused_.store(true);
  if (job_thread_) {
    job_thread_->job_queue()->Schedule(
        std::bind(&WASAPIAudioSink::UpdatePlaybackState, this));
  }
}
void WASAPIAudioSink::Play() {
  paused_.store(false);
  if (job_thread_) {
    job_thread_->job_queue()->Schedule(
        std::bind(&WASAPIAudioSink::UpdatePlaybackState, this));
  }
}

void WASAPIAudioSink::SetVolume(double volume) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (volume > 1.0 || volume < 0.0) {
    SB_LOG(WARNING) << "volume " << volume << " is not between 0.0 and 1.0";
    volume = volume > 1.0 ? 1.0 : 0.0;
  }
  volume_.store(volume);
  if (job_thread_) {
    job_thread_->job_queue()->Schedule(
        std::bind(&WASAPIAudioSink::UpdatePlaybackState, this));
  }
}

void WASAPIAudioSink::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  SB_DCHECK(playback_rate == 0.0 || playback_rate == 1.0)
      << "Playback rate " << playback_rate
      << " is unsupported by WASAPIAudioSink.";
  playback_rate_.store(playback_rate);
  if (job_thread_) {
    job_thread_->job_queue()->Schedule(
        std::bind(&WASAPIAudioSink::UpdatePlaybackState, this));
  }
}

double WASAPIAudioSink::GetCurrentPlaybackTime(uint64_t* updated_at) {
  SB_DCHECK(audio_clock_);

  ScopedLock lock(audio_clock_mutex_);
  uint64_t pos;
  HRESULT hr = audio_clock_->GetPosition(&pos, updated_at);
  if (hr != S_OK) {
    SB_LOG(INFO) << "WASAPI audio clock error, error code: " << std::hex << hr;
    return -1;
  }

  return (static_cast<double>(pos) /
          static_cast<double>(audio_clock_frequency_)) *
         1'000'000;
}

void WASAPIAudioSink::OutputFrames() {
  SB_DCHECK(job_thread_->job_queue()->BelongsToCurrentThread());

  ScopedLock lock(output_frames_mutex_);

  if (pending_decoded_audios_.empty()) {
    return;
  }

  int frames_copied = 0;
  uint32_t frames_in_client_buffer = 0;
  int64_t output_frames_job_delay_usec = 5'000;  // 5ms
  HRESULT hr = audio_client_->GetCurrentPadding(&frames_in_client_buffer);
  CHECK_HRESULT_OK(hr);
  int frames_available = static_cast<int>(client_buffer_size_in_frames_) -
                         static_cast<int>(frames_in_client_buffer);
  if (frames_available >= frames_per_audio_buffer_) {
    BYTE* client_buffer = nullptr;
    hr = render_client_->GetBuffer(frames_available, &client_buffer);
    CHECK_HRESULT_OK(hr);

    int client_buffer_offset = 0;
    while (frames_available >= frames_per_audio_buffer_ &&
           !pending_decoded_audios_.empty()) {
      scoped_refptr<DecodedAudio> decoded_audio =
          pending_decoded_audios_.front();
      SB_DCHECK(decoded_audio);
      memcpy(client_buffer + client_buffer_offset,
             reinterpret_cast<BYTE*>(decoded_audio->data()),
             decoded_audio->size_in_bytes());
      frames_copied += frames_per_audio_buffer_;
      client_buffer_offset += decoded_audio->size_in_bytes();
      pending_decoded_audios_.pop();
      frames_available -= frames_per_audio_buffer_;
    }

    hr = render_client_->ReleaseBuffer(frames_copied, 0 /* dwFlags */);
    CHECK_HRESULT_OK(hr);
    output_frames_job_delay_usec = 0;
  }

  if (!pending_decoded_audios_.empty()) {
    job_thread_->job_queue()->Schedule(
        std::bind(&WASAPIAudioSink::OutputFrames, this),
        output_frames_job_delay_usec);
  }
}

void WASAPIAudioSink::UpdatePlaybackState() {
  SB_DCHECK(job_thread_->job_queue()->BelongsToCurrentThread());

  bool is_playing = playing();
  HRESULT hr;
  if (is_playing != was_playing_) {
    if (is_playing) {
      hr = audio_client_->Start();
      CHECK_HRESULT_OK(hr);
    } else {
      hr = audio_client_->Stop();
      CHECK_HRESULT_OK(hr);
    }
    was_playing_ = is_playing;
  }
  double volume = volume_.load();
  if (current_volume_ != volume) {
    hr = audio_volume_->SetMasterVolume(volume, NULL);
    CHECK_HRESULT_OK(hr);
    current_volume_ = volume;
  }
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
