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
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

namespace {

const int kFrameBufferSizeInFrames = 4096;

void UpdateSourceStatusFuncStub(int* frames_in_buffer,
                                int* offset_in_frames,
                                bool* is_playing,
                                bool* is_eos_reached,
                                void* context) {}
void ConsumeFramesFuncStub(int frames_consumed, void* context) {}

}  // namespace

TEST(SbAudioSinkTest, SunnyDay) {
  ASSERT_GE(SbAudioSinkGetMaxChannels(), 2);

  SbMediaAudioSampleType sample_type = kSbMediaAudioSampleTypeInt16;
  if (!SbAudioSinkIsAudioSampleTypeSupported(sample_type)) {
    sample_type = kSbMediaAudioSampleTypeFloat32;
  }

  SbMediaAudioFrameStorageType storage_type =
      kSbMediaAudioFrameStorageTypeInterleaved;
  if (!SbAudioSinkIsAudioFrameStorageTypeSupported(storage_type)) {
    storage_type = kSbMediaAudioFrameStorageTypePlanar;
  }

  float frame_buffer[kFrameBufferSizeInFrames];
  float* frame_buffers[1] = {frame_buffer};
  SbAudioSink audio_sink = SbAudioSinkCreate(
      2, SbAudioSinkGetNearestSupportedSampleFrequency(44100), sample_type,
      storage_type, reinterpret_cast<void**>(frame_buffers), 4096,
      UpdateSourceStatusFuncStub, ConsumeFramesFuncStub, NULL);
  EXPECT_TRUE(SbAudioSinkIsValid(audio_sink));
  SbAudioSinkDestroy(audio_sink);
}

}  // namespace nplb
}  // namespace starboard
