// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_AUDIO_AUDIO_HELPERS_H_
#define COBALT_AUDIO_AUDIO_HELPERS_H_

#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_audio_bus.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/shell_audio_bus.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

#if defined(OS_STARBOARD)
#include "starboard/audio_sink.h"
#include "starboard/media.h"
#endif

namespace cobalt {
namespace audio {

#if defined(COBALT_MEDIA_SOURCE_2016)
typedef media::ShellAudioBus::SampleType SampleType;
const SampleType kSampleTypeInt16 = media::ShellAudioBus::kInt16;
const SampleType kSampleTypeFloat32 = media::ShellAudioBus::kFloat32;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::ShellAudioBus::SampleType SampleType;
const SampleType kSampleTypeInt16 = ::media::ShellAudioBus::kInt16;
const SampleType kSampleTypeFloat32 = ::media::ShellAudioBus::kFloat32;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

const float kMaxInt16AsFloat32 = 32767.0f;

#if defined(OS_STARBOARD)
// Get the size in bytes of an SbMediaAudioSampleType.
inline size_t GetStarboardSampleTypeSize(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeFloat32:
      return sizeof(float);
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return sizeof(int16);
  }
  NOTREACHED();
  return 0u;
}
#endif

// Get the size in bytes of an internal sample type, which is an alias for
// media::ShellAudioBus::SampleType.
inline size_t GetSampleTypeSize(SampleType sample_type) {
  switch (sample_type) {
    case kSampleTypeInt16:
      return sizeof(int16);
    case kSampleTypeFloat32:
      return sizeof(float);
  }
  NOTREACHED();
  return 0u;
}

// Get the sample type that we would prefer to output in using starboard, as
// an internal SampleType.  If we are not running on starboard or using the
// starboard media pipeline, then the preferred sample type is always float32.
inline SampleType GetPreferredOutputSampleType() {
#if defined(OS_STARBOARD)
#if SB_CAN(MEDIA_USE_STARBOARD_PIPELINE)
  if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
    return kSampleTypeFloat32;
  }
  DCHECK(SbAudioSinkIsAudioSampleTypeSupported(
      kSbMediaAudioSampleTypeInt16Deprecated))
      << "At least one starboard audio sample type must be supported if using "
         "starboard media pipeline.";
  return kSampleTypeInt16;
#else   // SB_CAN(MEDIA_USE_STARBOARD_PIPELINE)
  return kSampleTypeFloat32;
#endif  // SB_CAN(MEDIA_USE_STARBOARD_PIPELINE)
#else   // defined(OS_STARBOARD)
  return kSampleTypeFloat32;
#endif  // defined(OS_STARBOARD)
}

#if defined(OS_STARBOARD)
// The same as GetPreferredOutputSampleType, only as an SbMediaAudioSample
// rather than an internal SampleType.
inline SbMediaAudioSampleType GetPreferredOutputStarboardSampleType() {
  if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
    return kSbMediaAudioSampleTypeFloat32;
  }
  return kSbMediaAudioSampleTypeInt16Deprecated;
}
#endif

// Convert a sample value from {int16,float} to {int16,float}.
template <typename SourceType, typename DestType>
inline DestType ConvertSample(SourceType sample);

template <>
inline int16 ConvertSample<float, int16>(float sample) {
  if (!(-1.0 <= sample && sample <= 1.0)) {
    DLOG(WARNING)
        << "Sample of type float32 must lie on interval [-1.0, 1.0], got: "
        << sample << ".";
  }
  return static_cast<int16>(sample * kMaxInt16AsFloat32);
}

template <>
inline float ConvertSample<int16, float>(int16 sample) {
  float int16_sample_as_float = static_cast<float>(sample);
  return int16_sample_as_float / kMaxInt16AsFloat32;
}

template <>
inline float ConvertSample<float, float>(float sample) {
  return sample;
}

template <>
inline int16 ConvertSample<int16, int16>(int16 sample) {
  return sample;
}

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_HELPERS_H_
