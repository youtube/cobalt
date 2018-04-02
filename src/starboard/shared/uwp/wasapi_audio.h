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

#ifndef STARBOARD_SHARED_UWP_WASAPI_AUDIO_H_
#define STARBOARD_SHARED_UWP_WASAPI_AUDIO_H_

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl\implements.h>

namespace starboard {
namespace shared {
namespace uwp {

class WASAPIAudioDevice
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::FtmBase,
          IActivateAudioInterfaceCompletionHandler> {
 public:
  WASAPIAudioDevice::WASAPIAudioDevice();

  int GetNumChannels() const { return channels_; }
  int GetBitrate() const { return bitrate_; }

 private:
  STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation*);
  void InitializeAudioDevice();

  HANDLE activate_completed_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  Microsoft::WRL::ComPtr<IAudioClient3> audio_client_ = nullptr;
  int bitrate_ = -1;
  int channels_ = -1;
};

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_WASAPI_AUDIO_H_
