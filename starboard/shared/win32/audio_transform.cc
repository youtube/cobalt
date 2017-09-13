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

#include "starboard/shared/win32/audio_transform.h"

#include <vector>

#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_foundation_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;

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
    default: {
      SB_NOTIMPLEMENTED();
      return MFAudioFormat_PCM;
    }
  }
}

class WinAudioFormat {
 public:
  explicit WinAudioFormat(const SbMediaAudioHeader& audio_header) {
    WAVEFORMATEX* wave_format = WaveFormatTexPtr();
    wave_format->nAvgBytesPerSec = audio_header.average_bytes_per_second;
    wave_format->nBlockAlign = audio_header.block_alignment;
    wave_format->nChannels = audio_header.number_of_channels;
    wave_format->nSamplesPerSec = audio_header.samples_per_second;
    wave_format->wBitsPerSample = audio_header.bits_per_sample;
    wave_format->wFormatTag = audio_header.format_tag;

    // TODO: Investigate this more.
    wave_format->cbSize = kAudioExtraFormatBytes;
    std::uint8_t* audio_specific_config = AudioSpecificConfigPtr();

    // These are hard-coded audio specif audio configuration.
    // Use |SbMediaAudioHeader::audio_specific_config| instead.
    SB_DCHECK(kAudioExtraFormatBytes == 2);
    // TODO: What do these values do?
    audio_specific_config[0] = 0x12;
    audio_specific_config[1] = 0x10;
  }
  WAVEFORMATEX* WaveFormatTexPtr() {
    return reinterpret_cast<WAVEFORMATEX*>(full_structure);
  }
  uint8_t* AudioSpecificConfigPtr() {
    return full_structure + sizeof(WAVEFORMATEX);
  }

  UINT32 Size() const { return sizeof(full_structure); }

 private:
  static const UINT32 kAudioExtraFormatBytes = 2;
  uint8_t full_structure[sizeof(WAVEFORMATEX) + kAudioExtraFormatBytes];
};

}  // namespace.

scoped_ptr<MediaTransform> CreateAudioTransform(
    const SbMediaAudioHeader& audio,
    SbMediaAudioCodec codec) {
  ComPtr<IMFTransform> transform;
  HRESULT hr = CreateDecoderTransform(CLSID_MSAACDecMFT, &transform);
  CheckResult(hr);

  ComPtr<IMFMediaType> input_type;
  hr = MFCreateMediaType(&input_type);
  CheckResult(hr);

  WinAudioFormat audio_fmt(audio);
  hr = MFInitMediaTypeFromWaveFormatEx(
      input_type.Get(), audio_fmt.WaveFormatTexPtr(), audio_fmt.Size());

  CheckResult(hr);

  hr = input_type->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);  // raw aac
  CheckResult(hr);

  GUID win32_audio_type = ConvertToWin32AudioCodec(codec);
  hr = input_type->SetGUID(MF_MT_SUBTYPE, win32_audio_type);
  CheckResult(hr);

  std::vector<ComPtr<IMFMediaType>> available_types =
      GetAllInputMediaTypes(MediaTransform::kStreamId, transform.Get());

  available_types = FilterMediaBySubType(available_types, win32_audio_type);
  SB_DCHECK(available_types.size());

  ComPtr<IMFMediaType> selected = available_types[0];
  std::vector<GUID> attribs = {
      MF_MT_AUDIO_BLOCK_ALIGNMENT, MF_MT_AUDIO_SAMPLES_PER_SECOND,
      MF_MT_AUDIO_AVG_BYTES_PER_SECOND, MF_MT_AUDIO_NUM_CHANNELS,
  };

  for (auto it = attribs.begin(); it != attribs.end(); ++it) {
    CopyUint32Property(*it, input_type.Get(), selected.Get());
  }

  scoped_ptr<MediaTransform> output(new MediaTransform(transform));
  output->SetInputType(selected);
  output->SetOutputTypeBySubType(MFAudioFormat_Float);

  return output.Pass();
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
