// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/win32/audio_transform.h"

#include <mfapi.h>

#include <memory>
#include <vector>

#include "starboard/memory.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_foundation_utils.h"
#include "starboard/shared/win32/wasapi_include.h"

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;
using starboard::media::AudioStreamInfo;

namespace {

GUID ConvertToWin32AudioCodec(SbMediaAudioCodec codec) {
  switch (codec) {
    case kSbMediaAudioCodecNone: {
      return MFAudioFormat_PCM;
    }
    case kSbMediaAudioCodecAac: {
      return MFAudioFormat_AAC;
    }
    case kSbMediaAudioCodecOpus: {
      return MFAudioFormat_Opus;
    }
    case kSbMediaAudioCodecAc3: {
      return MFAudioFormat_Dolby_AC3;
    }
    case kSbMediaAudioCodecEac3: {
      return MFAudioFormat_Dolby_DDPlus;
    }
    default: {
      SB_NOTIMPLEMENTED();
      return MFAudioFormat_PCM;
    }
  }
}

GUID ConvertToWin32TransformType(SbMediaAudioCodec codec) {
  switch (codec) {
    case kSbMediaAudioCodecAac: {
      return CLSID_MSAACDecMFT;
    }
    case kSbMediaAudioCodecAc3:
    case kSbMediaAudioCodecEac3: {
      return CLSID_MSDDPlusDecMFT;
    }
    default: {
      SB_NOTIMPLEMENTED();
      return MFAudioFormat_Float;
    }
  }
}

GUID ConvertToWin32OutputFormat(SbMediaAudioCodec codec) {
  switch (codec) {
    case kSbMediaAudioCodecAac:
    case kSbMediaAudioCodecOpus:
    case kSbMediaAudioCodecNone: {
      return MFAudioFormat_Float;
    }
    case kSbMediaAudioCodecAc3: {
      return MFAudioFormat_Dolby_AC3_SPDIF;
    }
    case kSbMediaAudioCodecEac3: {
      return KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS;
    }
    default: {
      SB_NOTIMPLEMENTED();
      return MFAudioFormat_Float;
    }
  }
}

class WinAudioFormat {
 public:
  explicit WinAudioFormat(const AudioStreamInfo& audio_stream_info) {
    if (audio_stream_info.codec == kSbMediaAudioCodecAac) {
      CreateAacAudioFormat(audio_stream_info);
    } else {
      SB_DCHECK(audio_stream_info.codec == kSbMediaAudioCodecAc3 ||
                audio_stream_info.codec == kSbMediaAudioCodecEac3);
      CreateAc3AudioFormat(audio_stream_info);
    }
  }

  void CreateAacAudioFormat(const AudioStreamInfo& audio_stream_info) {
    // The HEAACWAVEFORMAT structure has many specializations with varying data
    // appended at the end.
    // The "-1" is used to account for pbAudioSpecificConfig[1] at the end of
    // HEAACWAVEFORMAT.
    format_buffer_.resize(sizeof(HEAACWAVEFORMAT) +
                          audio_stream_info.audio_specific_config.size() - 1);
    HEAACWAVEFORMAT* wave_format =
        reinterpret_cast<HEAACWAVEFORMAT*>(format_buffer_.data());

    wave_format->wfInfo.wfx.nAvgBytesPerSec = 0;
    wave_format->wfInfo.wfx.nBlockAlign = 4;
    wave_format->wfInfo.wfx.nChannels = audio_stream_info.number_of_channels;
    wave_format->wfInfo.wfx.nSamplesPerSec =
        audio_stream_info.samples_per_second;
    wave_format->wfInfo.wfx.wBitsPerSample = audio_stream_info.bits_per_sample;
    wave_format->wfInfo.wfx.wFormatTag = WAVE_FORMAT_MPEG_HEAAC;
    // The "-1" is used to account for pbAudioSpecificConfig[1] at the end of
    // HEAACWAVEFORMAT.
    wave_format->wfInfo.wfx.cbSize =
        static_cast<WORD>(sizeof(HEAACWAVEFORMAT) - sizeof(WAVEFORMATEX) +
                          audio_stream_info.audio_specific_config.size() - 1);

    wave_format->wfInfo.wPayloadType = 0;                     // RAW
    wave_format->wfInfo.wAudioProfileLevelIndication = 0xfe;  // Unknown Profile
    wave_format->wfInfo.wStructType = 0;  // AudioSpecificConfig()

    if (!audio_stream_info.audio_specific_config.empty()) {
      memcpy(wave_format->pbAudioSpecificConfig,
             audio_stream_info.audio_specific_config.data(),
             audio_stream_info.audio_specific_config.size());
    }
  }

  void CreateAc3AudioFormat(const AudioStreamInfo& audio_stream_info) {
    format_buffer_.resize(sizeof(WAVEFORMATEXTENSIBLE));
    WAVEFORMATEXTENSIBLE* wave_format =
        reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format_buffer_.data());

    wave_format->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wave_format->Format.nChannels = audio_stream_info.number_of_channels;
    wave_format->Format.wBitsPerSample = audio_stream_info.bits_per_sample;
    wave_format->Format.nSamplesPerSec = audio_stream_info.samples_per_second;
    wave_format->Format.nBlockAlign = 4;
    wave_format->Format.nAvgBytesPerSec = 0;
    wave_format->Format.cbSize =
        sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wave_format->Samples.wValidBitsPerSample =
        wave_format->Format.wBitsPerSample;
    wave_format->dwChannelMask = audio_stream_info.number_of_channels > 2
                                     ? KSAUDIO_SPEAKER_5POINT1
                                     : KSAUDIO_SPEAKER_STEREO;
    wave_format->SubFormat = ConvertToWin32AudioCodec(audio_stream_info.codec);
  }

  WAVEFORMATEX* WaveFormatData() {
    return reinterpret_cast<WAVEFORMATEX*>(format_buffer_.data());
  }
  UINT32 Size() const { return static_cast<UINT32>(format_buffer_.size()); }

 private:
  std::vector<uint8_t> format_buffer_;
};

}  // namespace.

std::unique_ptr<MediaTransform> CreateAudioTransform(
    const AudioStreamInfo& audio_stream_info) {
  SB_DCHECK(audio_stream_info.codec == kSbMediaAudioCodecAac ||
            audio_stream_info.codec == kSbMediaAudioCodecAc3 ||
            audio_stream_info.codec == kSbMediaAudioCodecEac3);
  ComPtr<IMFTransform> transform;
  HRESULT hr = CreateDecoderTransform(
      ConvertToWin32TransformType(audio_stream_info.codec), &transform);

  CheckResult(hr);

  ComPtr<IMFMediaType> input_type;
  hr = MFCreateMediaType(&input_type);
  CheckResult(hr);

  WinAudioFormat audio_fmt(audio_stream_info);
  hr = MFInitMediaTypeFromWaveFormatEx(
      input_type.Get(), audio_fmt.WaveFormatData(), audio_fmt.Size());
  CheckResult(hr);

  GUID win32_audio_type = ConvertToWin32AudioCodec(audio_stream_info.codec);

  std::vector<ComPtr<IMFMediaType>> available_types =
      GetAllInputMediaTypes(MediaTransform::kStreamId, transform.Get());

  available_types = FilterMediaBySubType(available_types, win32_audio_type);
  SB_DCHECK(available_types.size());

  ComPtr<IMFMediaType> selected = available_types[0];
  CopyProperties(input_type.Get(), selected.Get());

  std::unique_ptr<MediaTransform> output(new MediaTransform(transform));
  output->SetInputType(selected);
  output->SetOutputTypeBySubType(
      ConvertToWin32OutputFormat(audio_stream_info.codec));

  return output;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
