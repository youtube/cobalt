// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace uwp {

using Microsoft::WRL::ComPtr;
using Windows::Media::Devices::MediaDevice;

namespace {
const int kWaitForActivateTimeout = 500;  // 0.5 sec
}  // namespace

WASAPIAudioDevice::WASAPIAudioDevice() {
  InitializeAudioDevice();
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
        }  // audio_client_->GetMixFormat
      }    // audio_client_->SetClientProperties
    }      // audio_interface->QueryInterface
  }

  audio_client_ = nullptr;
  SetEvent(activate_completed_);

  // Need to return S_OK
  return S_OK;
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
