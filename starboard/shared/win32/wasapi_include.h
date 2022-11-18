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

#ifndef STARBOARD_SHARED_WIN32_WASAPI_INCLUDE_H_
#define STARBOARD_SHARED_WIN32_WASAPI_INCLUDE_H_

#include <guiddef.h>
#include <initguid.h>

#define KSAUDIO_SPEAKER_DIRECTOUT 0
#define KSAUDIO_SPEAKER_STEREO (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
#define KSAUDIO_SPEAKER_5POINT1                                      \
  (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | \
   SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT)

DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS,
            0x0000000a,
            0x0cea,
            0x0010,
            0x80,
            0x00,
            0x00,
            0xaa,
            0x00,
            0x38,
            0x9b,
            0x71);

// IEC 60958 specific values.
constexpr int kAc3SamplesPerSecond = 48000;
constexpr int kEac3SamplesPerSecond = 192000;
constexpr int kIec60958Channels = 2;
constexpr int kIec60958BitsPerSample = 16;
constexpr int kIec60958BlockAlign = 4;
// Values taken from the Dolby Audio Decoder MFT documentation
// https://docs.microsoft.com/en-us/windows/win32/medfound/dolby-audio-decoder
constexpr size_t kAc3BufferSize = 6144;
constexpr size_t kEac3BufferSize = 24576;

namespace starboard {
namespace shared {
namespace win32 {

typedef struct {
  WAVEFORMATEXTENSIBLE FormatExt;

  DWORD dwEncodedSamplesPerSec;
  DWORD dwEncodedChannelCount;
  DWORD dwAverageBytesPerSec;
} WAVEFORMATEXTENSIBLE_IEC61937, *PWAVEFORMATEXTENSIBLE_IEC61937;

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_WASAPI_INCLUDE_H_
