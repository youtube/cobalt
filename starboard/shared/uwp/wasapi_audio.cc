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

#include "starboard/shared/uwp/wasapi_audio.h"

#include <mfapi.h>

#include "starboard/common/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/once.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "starboard/shared/win32/wasapi_include.h"

namespace starboard {
namespace shared {
namespace uwp {

using Microsoft::WRL::ComPtr;
using ::starboard::shared::starboard::media::MimeSupportabilityCache;
using Windows::Foundation::TypedEventHandler;
using Windows::Media::Devices::DefaultAudioRenderDeviceChangedEventArgs;
using Windows::Media::Devices::MediaDevice;

namespace {

const int kWaitForActivateTimeout = 500;  // 0.5 sec

void SetPassthroughWaveformat(WAVEFORMATEXTENSIBLE* wfext,
                              SbMediaAudioCodec audio_codec) {
  SB_DCHECK(audio_codec == kSbMediaAudioCodecAc3 ||
            audio_codec == kSbMediaAudioCodecEac3);

  wfext->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  wfext->Format.nChannels = kIec60958Channels;
  wfext->Format.wBitsPerSample = kIec60958BitsPerSample;
  wfext->Format.nSamplesPerSec = audio_codec == kSbMediaAudioCodecAc3
                                     ? kAc3SamplesPerSecond
                                     : kEac3SamplesPerSecond;
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

class DefaultAudioRenderParams {
 public:
  DefaultAudioRenderParams() {
    device_changed_token_ = MediaDevice::DefaultAudioRenderDeviceChanged +=
        ref new TypedEventHandler<Platform::Object ^,
                                  DefaultAudioRenderDeviceChangedEventArgs ^>(
            [this](Platform::Object ^ sender,
                   DefaultAudioRenderDeviceChangedEventArgs ^ args) {
              OnDefaultAudioRenderDeviceChanged(sender, args);
            });
  }

  ~DefaultAudioRenderParams() {
    MediaDevice::DefaultAudioRenderDeviceChanged -= device_changed_token_;
  }

  int GetBitrate() {
    ScopedLock lock(mutex_);
    RefreshCachedParamsIfNeeded_Locked();
    return cached_bitrate_;
  }

  int GetNumChannels() {
    ScopedLock lock(mutex_);
    RefreshCachedParamsIfNeeded_Locked();
    return cached_channels_;
  }

  bool GetPassthroughCodecSupport(SbMediaAudioCodec audio_codec) {
    SB_DCHECK(audio_codec == kSbMediaAudioCodecAc3 ||
              audio_codec == kSbMediaAudioCodecEac3);
    ScopedLock lock(mutex_);
    RefreshCachedParamsIfNeeded_Locked();
    return audio_codec == kSbMediaAudioCodecAc3 ? supports_ac3_passthrough_
                                                : supports_eac3_passthrough_;
  }

 private:
  atomic_bool is_dirty_{true};
  Mutex mutex_;
  int cached_bitrate_ = 0;
  int cached_channels_ = 0;
  bool supports_ac3_passthrough_ = false;
  bool supports_eac3_passthrough_ = false;
  Windows::Foundation::EventRegistrationToken device_changed_token_{};

  void OnDefaultAudioRenderDeviceChanged(
      Platform::Object ^ sender,
      Windows::Media::Devices::DefaultAudioRenderDeviceChangedEventArgs ^
          event_args) {
    is_dirty_.store(true);
    MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
  }

  void RefreshCachedParamsIfNeeded_Locked() {
    if (is_dirty_.load()) {
      Microsoft::WRL::ComPtr<WASAPIAudioDevice> audio_device =
          Microsoft::WRL::Make<WASAPIAudioDevice>();
      cached_bitrate_ = audio_device->GetBitrate();
      cached_channels_ = audio_device->GetNumChannels();
      supports_ac3_passthrough_ =
          audio_device->GetPassthroughCodecSupport(kSbMediaAudioCodecAc3);
      supports_eac3_passthrough_ =
          audio_device->GetPassthroughCodecSupport(kSbMediaAudioCodecEac3);
      // when a timeout occurres before WASAPIAudioDevice::ActivateCompleted()
      // has been called the cached values are not set with correct values. We
      // need to initializa again for getting correct values nex time.
      if (cached_bitrate_ != kInitialValue && cached_channels_ != kInitialValue)
        is_dirty_.store(false);
    }
  }
};

SB_ONCE_INITIALIZE_FUNCTION(DefaultAudioRenderParams,
                            GetDefaultAudioRenderParams);

}  // namespace

// static
int WASAPIAudioDevice::GetCachedBitrateOfDefaultAudioRenderer() {
  return GetDefaultAudioRenderParams()->GetBitrate();
}

// static
int WASAPIAudioDevice::GetCachedNumChannelsOfDefaultAudioRenderer() {
  return GetDefaultAudioRenderParams()->GetNumChannels();
}

// static
bool WASAPIAudioDevice::GetPassthroughSupportOfDefaultAudioRenderer(
    SbMediaAudioCodec audio_codec) {
  return GetDefaultAudioRenderParams()->GetPassthroughCodecSupport(audio_codec);
}

WASAPIAudioDevice::WASAPIAudioDevice() {
  InitializeAudioDevice();
}

WASAPIAudioDevice::~WASAPIAudioDevice() {
  CloseHandle(activate_completed_);
}

void WASAPIAudioDevice::InitializeAudioDevice() {
  ComPtr<IActivateAudioInterfaceAsyncOperation> async_operation;

  // Default Audio Device Renderer
  Platform::String ^ deviceIdString = MediaDevice::GetDefaultAudioRenderId(
      Windows::Media::Devices::AudioDeviceRole::Default);

  if (FAILED(ActivateAudioInterfaceAsync(deviceIdString->Data(),
                                         __uuidof(IAudioClient3), nullptr, this,
                                         &async_operation))) {
    return;
  }
  WaitForSingleObject(activate_completed_, kWaitForActivateTimeout);
}

HRESULT WASAPIAudioDevice::ActivateCompleted(
    IActivateAudioInterfaceAsyncOperation* operation) {
  HRESULT hr = S_OK;
  HRESULT hr_activate = S_OK;
  ComPtr<IUnknown> audio_interface;

  // Check for a successful activation result
  hr = operation->GetActivateResult(&hr_activate, &audio_interface);

  if (SUCCEEDED(hr) && SUCCEEDED(hr_activate)) {
    // Get the pointer for the Audio Client.
    hr = audio_interface->QueryInterface(IID_PPV_ARGS(&audio_client_));
    SB_DCHECK(audio_client_);
    if (SUCCEEDED(hr)) {
      AudioClientProperties audio_props = {0};
      audio_props.cbSize = sizeof(AudioClientProperties);
      audio_props.bIsOffload = false;
      audio_props.eCategory = AudioCategory_Media;

      hr = audio_client_->SetClientProperties(&audio_props);
      if (SUCCEEDED(hr)) {
        WAVEFORMATEX* format;
        hr = audio_client_->GetMixFormat(&format);
        SB_DCHECK(format);
        if (SUCCEEDED(hr)) {
          bitrate_ = format->nSamplesPerSec * format->wBitsPerSample;
          channels_ = format->nChannels;
          CoTaskMemFree(format);

          if (channels_ > 2) {
            WAVEFORMATEXTENSIBLE passthrough_format;
            SetPassthroughWaveformat(&passthrough_format,
                                     kSbMediaAudioCodecAc3);
            hr = audio_client_->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                  &passthrough_format.Format,
                                                  nullptr);
            supports_ac3_passthrough_ = hr == S_OK;
            SetPassthroughWaveformat(&passthrough_format,
                                     kSbMediaAudioCodecEac3);
            hr = audio_client_->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                  &passthrough_format.Format,
                                                  nullptr);
            supports_eac3_passthrough_ = hr == S_OK;
          }
        }  // audio_client_->GetMixFormat
      }  // audio_client_->SetClientProperties
    }  // audio_interface->QueryInterface
  }

  audio_client_ = nullptr;
  SetEvent(activate_completed_);

  // Need to return S_OK
  return S_OK;
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
