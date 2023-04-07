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

#ifndef STARBOARD_SHARED_UWP_WASAPI_AUDIO_H_
#define STARBOARD_SHARED_UWP_WASAPI_AUDIO_H_

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl\implements.h>

#include "starboard/media.h"

constexpr int kInitialValue = -1;

namespace starboard {
namespace shared {
namespace uwp {

class WASAPIAudioDevice
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::FtmBase,
          IActivateAudioInterfaceCompletionHandler> {
 public:
  // For accessing values of the bitrate and the number of channels of the
  // default audio render device it is necessary to instantiate
  // WASAPIAudioDevice and call its methods GetNumChannels() and GetBitrate().
  // The default audio render device may be changed by system or user but
  // WASAPIAudioDevice does not care about it and for accessing actual values of
  // the bitrate and the number of channels it is necessary to create the new
  // instance of WASAPIAudioDevice every time before calling to GetNumChannels()
  // and GetBitrate().
  // Generally WASAPIAudioDevice initialization makes some work which may take
  // a while. If some code frequently instantiates WASAPIAudioDevice to get the
  // bitrate and the number of channels, it may take some significant period of
  // time which will cause a long delay for the caller.
  // Due to this for accessing actual values of the bitrate and the number of
  // channels, instead of calling GetNumChannels() and GetBitrate() please
  // consider using static functions GetCachedBitrateOfDefaultAudioRenderer()
  // and GetCachedNumChannelsOfDefaultAudioRenderer() which are accessing the
  // cached values of the bitrate and the number of channels of the default
  // audio render device. Their implementations handle the system event
  // DefaultAudioRenderDeviceChanged which notifies when the default audio
  // device is changed.
  static int GetCachedBitrateOfDefaultAudioRenderer();
  static int GetCachedNumChannelsOfDefaultAudioRenderer();
  static bool GetPassthroughSupportOfDefaultAudioRenderer(
      SbMediaAudioCodec audio_codec);

  WASAPIAudioDevice();
  virtual ~WASAPIAudioDevice();

  int GetNumChannels() const { return channels_; }
  int GetBitrate() const { return bitrate_; }
  bool GetPassthroughCodecSupport(SbMediaAudioCodec audio_codec) const {
    if (audio_codec == kSbMediaAudioCodecAc3) {
      return supports_ac3_passthrough_;
    } else if (audio_codec == kSbMediaAudioCodecEac3) {
      return supports_eac3_passthrough_;
    }
    return false;
  }

 private:
  friend class WASAPIAudioDeviceTest;
  STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation*);
  void InitializeAudioDevice();

  HANDLE activate_completed_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  Microsoft::WRL::ComPtr<IAudioClient3> audio_client_ = nullptr;
  int bitrate_ = kInitialValue;
  int channels_ = kInitialValue;
  bool supports_ac3_passthrough_ = false;
  bool supports_eac3_passthrough_ = false;
};

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_WASAPI_AUDIO_H_
