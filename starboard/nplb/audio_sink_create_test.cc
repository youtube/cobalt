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

#include "starboard/audio_sink.h"

#include <vector>

#include "starboard/nplb/audio_sink_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

namespace {

void UpdateSourceStatusFuncStub(int* frames_in_buffer,
                                int* offset_in_frames,
                                bool* is_playing,
                                bool* is_eos_reached,
                                void* context) {
  *frames_in_buffer = 0;
  *offset_in_frames = 0;
  *is_playing = false;
  *is_eos_reached = false;
}

void ConsumeFramesFuncStub(int frames_consumed,
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                           SbTime frames_consumed_at,
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                           void* context) {
}

}  // namespace

TEST(SbAudioSinkCreateTest, SunnyDay) {
  ASSERT_GE(SbAudioSinkGetMaxChannels(), 1);

  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());

  SbAudioSink audio_sink = SbAudioSinkCreate(
      frame_buffers.channels(),
      SbAudioSinkGetNearestSupportedSampleFrequency(44100),
      frame_buffers.sample_type(), frame_buffers.storage_type(),
      frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
      UpdateSourceStatusFuncStub, ConsumeFramesFuncStub,
      reinterpret_cast<void*>(1));
  EXPECT_TRUE(SbAudioSinkIsValid(audio_sink));
  SbAudioSinkDestroy(audio_sink);
}

#if SB_API_VERSION >= SB_MULTI_PLAYER_API_VERSION
TEST(SbAudioSinkCreateTest, MultiSink) {
  ASSERT_GE(SbAudioSinkGetMaxChannels(), 1);

  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());

  const int kMaxSinks = 16;
  std::vector<SbAudioSink> created_sinks;
  for (int i = 0; i < kMaxSinks; ++i) {
    created_sinks.push_back(SbAudioSinkCreate(
        frame_buffers.channels(),
        SbAudioSinkGetNearestSupportedSampleFrequency(44100),
        frame_buffers.sample_type(), frame_buffers.storage_type(),
        frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
        UpdateSourceStatusFuncStub, ConsumeFramesFuncStub,
        reinterpret_cast<void*>(1)));
    if (!SbAudioSinkIsValid(created_sinks[i])) {
      created_sinks.pop_back();
      break;
    }
  }
  SB_DLOG(INFO) << "Created " << created_sinks.size() << " valid audio sinks";
  for (auto sink : created_sinks) {
    SbAudioSinkDestroy(sink);
  }
}
#endif  // SB_API_VERSION >= SB_MULTI_PLAYER_API_VERSION

TEST(SbAudioSinkCreateTest, SunnyDayAllCombinations) {
  std::vector<SbMediaAudioSampleType> sample_types;
  if (SbAudioSinkIsAudioSampleTypeSupported(
          kSbMediaAudioSampleTypeInt16Deprecated)) {
    sample_types.push_back(kSbMediaAudioSampleTypeInt16Deprecated);
  }
  if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
    sample_types.push_back(kSbMediaAudioSampleTypeFloat32);
  }

  std::vector<SbMediaAudioFrameStorageType> storage_types;
  if (SbAudioSinkIsAudioFrameStorageTypeSupported(
          kSbMediaAudioFrameStorageTypeInterleaved)) {
    storage_types.push_back(kSbMediaAudioFrameStorageTypeInterleaved);
  }
  if (SbAudioSinkIsAudioFrameStorageTypeSupported(
          kSbMediaAudioFrameStorageTypePlanar)) {
    storage_types.push_back(kSbMediaAudioFrameStorageTypePlanar);
  }

  for (size_t sample_type = 0; sample_type < sample_types.size();
       ++sample_type) {
    for (size_t storage_type = 0; storage_type < storage_types.size();
         ++storage_type) {
      AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels(),
                                              sample_types[sample_type],
                                              storage_types[storage_type]);
      // It should at least support stereo and the claimed max channels.
      SbAudioSink audio_sink = SbAudioSinkCreate(
          frame_buffers.channels(),
          SbAudioSinkGetNearestSupportedSampleFrequency(44100),
          frame_buffers.sample_type(), frame_buffers.storage_type(),
          frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
          UpdateSourceStatusFuncStub, ConsumeFramesFuncStub, NULL);
      EXPECT_TRUE(SbAudioSinkIsValid(audio_sink));
      SbAudioSinkDestroy(audio_sink);

      if (SbAudioSinkGetMaxChannels() > 2) {
        AudioSinkTestFrameBuffers frame_buffers(2, sample_types[sample_type],
                                                storage_types[storage_type]);
        audio_sink = SbAudioSinkCreate(
            frame_buffers.channels(),
            SbAudioSinkGetNearestSupportedSampleFrequency(44100),
            frame_buffers.sample_type(), frame_buffers.storage_type(),
            frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
            UpdateSourceStatusFuncStub, ConsumeFramesFuncStub, NULL);
        EXPECT_TRUE(SbAudioSinkIsValid(audio_sink));
        SbAudioSinkDestroy(audio_sink);
      }
    }
  }
}

TEST(SbAudioSinkCreateTest, RainyDayInvalidChannels) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  SbAudioSink audio_sink = SbAudioSinkCreate(
      0,  // |channels| is 0
      SbAudioSinkGetNearestSupportedSampleFrequency(44100),
      frame_buffers.sample_type(), frame_buffers.storage_type(),
      frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
      UpdateSourceStatusFuncStub, ConsumeFramesFuncStub, NULL);
  EXPECT_FALSE(SbAudioSinkIsValid(audio_sink));
}

TEST(SbAudioSinkCreateTest, RainyDayInvalidFrequency) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  SbAudioSink audio_sink = SbAudioSinkCreate(
      frame_buffers.channels(), 0,  // |sampling_frequency_hz| is 0
      frame_buffers.sample_type(), frame_buffers.storage_type(),
      frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
      UpdateSourceStatusFuncStub, ConsumeFramesFuncStub, NULL);
  EXPECT_FALSE(SbAudioSinkIsValid(audio_sink));

  const int kOddFrequency = 12345;
  if (SbAudioSinkGetNearestSupportedSampleFrequency(kOddFrequency) !=
      kOddFrequency) {
    audio_sink = SbAudioSinkCreate(
        frame_buffers.channels(),
        kOddFrequency,  // |sampling_frequency_hz| is unsupported
        frame_buffers.sample_type(), frame_buffers.storage_type(),
        frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
        UpdateSourceStatusFuncStub, ConsumeFramesFuncStub, NULL);
    EXPECT_FALSE(SbAudioSinkIsValid(audio_sink));
  }
}

TEST(SbAudioSinkCreateTest, RainyDayInvalidSampleType) {
  SbMediaAudioSampleType invalid_sample_type;
  if (SbAudioSinkIsAudioSampleTypeSupported(
          kSbMediaAudioSampleTypeInt16Deprecated)) {
    if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
      return;
    }
    invalid_sample_type = kSbMediaAudioSampleTypeFloat32;
  } else {
    invalid_sample_type = kSbMediaAudioSampleTypeInt16Deprecated;
  }

  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels(),
                                          invalid_sample_type);
  SbAudioSink audio_sink = SbAudioSinkCreate(
      SbAudioSinkGetMaxChannels(),
      SbAudioSinkGetNearestSupportedSampleFrequency(44100),
      invalid_sample_type,  // |sample_type| is invalid
      frame_buffers.storage_type(), frame_buffers.frame_buffers(),
      frame_buffers.frames_per_channel(), UpdateSourceStatusFuncStub,
      ConsumeFramesFuncStub, NULL);
  EXPECT_FALSE(SbAudioSinkIsValid(audio_sink));
}

TEST(SbAudioSinkCreateTest, RainyDayInvalidFrameStorageType) {
  SbMediaAudioFrameStorageType invalid_storage_type;
  if (SbAudioSinkIsAudioFrameStorageTypeSupported(
          kSbMediaAudioFrameStorageTypeInterleaved)) {
    if (SbAudioSinkIsAudioFrameStorageTypeSupported(
            kSbMediaAudioFrameStorageTypePlanar)) {
      return;
    }
    invalid_storage_type = kSbMediaAudioFrameStorageTypePlanar;
  } else {
    invalid_storage_type = kSbMediaAudioFrameStorageTypeInterleaved;
  }

  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels(),
                                          invalid_storage_type);
  SbAudioSink audio_sink = SbAudioSinkCreate(
      SbAudioSinkGetMaxChannels(),
      SbAudioSinkGetNearestSupportedSampleFrequency(44100),
      frame_buffers.sample_type(),
      invalid_storage_type,  // |storage_type| is invalid
      frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
      UpdateSourceStatusFuncStub, ConsumeFramesFuncStub, NULL);
  EXPECT_FALSE(SbAudioSinkIsValid(audio_sink));
}

TEST(SbAudioSinkCreateTest, RainyDayInvalidFrameBuffers) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  SbAudioSink audio_sink = SbAudioSinkCreate(
      frame_buffers.channels(),
      SbAudioSinkGetNearestSupportedSampleFrequency(44100),
      frame_buffers.sample_type(), frame_buffers.storage_type(),
      NULL,  // |frame_buffers| is NULL
      frame_buffers.frames_per_channel(), UpdateSourceStatusFuncStub,
      ConsumeFramesFuncStub, NULL);
  EXPECT_FALSE(SbAudioSinkIsValid(audio_sink));

  audio_sink = SbAudioSinkCreate(
      frame_buffers.channels(),
      SbAudioSinkGetNearestSupportedSampleFrequency(44100),
      frame_buffers.sample_type(), frame_buffers.storage_type(),
      frame_buffers.frame_buffers(),
      0,  // |frames_per_channel| is 0
      UpdateSourceStatusFuncStub, ConsumeFramesFuncStub, NULL);
  EXPECT_FALSE(SbAudioSinkIsValid(audio_sink));
}

TEST(SbAudioSinkCreateTest, RainyDayInvalidCallback) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  SbAudioSink audio_sink = SbAudioSinkCreate(
      frame_buffers.channels(),
      SbAudioSinkGetNearestSupportedSampleFrequency(44100),
      frame_buffers.sample_type(), frame_buffers.storage_type(),
      frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
      NULL,  // |update_source_status_func| is NULL
      ConsumeFramesFuncStub, NULL);
  EXPECT_FALSE(SbAudioSinkIsValid(audio_sink));

  audio_sink = SbAudioSinkCreate(
      frame_buffers.channels(),
      SbAudioSinkGetNearestSupportedSampleFrequency(44100),
      frame_buffers.sample_type(), frame_buffers.storage_type(),
      frame_buffers.frame_buffers(), frame_buffers.frames_per_channel(),
      UpdateSourceStatusFuncStub,
      NULL,  // |consume_frames_func| is NULL
      NULL);
  EXPECT_FALSE(SbAudioSinkIsValid(audio_sink));
}

}  // namespace nplb
}  // namespace starboard
