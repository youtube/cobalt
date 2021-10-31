// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>

#include "starboard/nplb/audio_sink_helpers.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

TEST(SbAudioSinkTest, UpdateStatusCalled) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  AudioSinkTestEnvironment environment(frame_buffers);
  ASSERT_TRUE(environment.is_valid());

  EXPECT_TRUE(environment.WaitUntilUpdateStatusCalled());
  EXPECT_TRUE(environment.WaitUntilUpdateStatusCalled());
}

TEST(SbAudioSinkTest, SomeFramesConsumed) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  AudioSinkTestEnvironment environment(frame_buffers);
  ASSERT_TRUE(environment.is_valid());

  // Audio sink need to be fully filled once to ensure it can start working.
  int frames_to_append = frame_buffers.frames_per_channel();
  environment.AppendFrame(frames_to_append);

  EXPECT_TRUE(environment.WaitUntilSomeFramesAreConsumed());
}

TEST(SbAudioSinkTest, AllFramesConsumed) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  AudioSinkTestEnvironment environment(frame_buffers);
  ASSERT_TRUE(environment.is_valid());

  int frames_to_append = frame_buffers.frames_per_channel();
  environment.AppendFrame(frames_to_append / 2);

  EXPECT_TRUE(environment.WaitUntilAllFramesAreConsumed());
}

TEST(SbAudioSinkTest, MultipleAppendAndConsume) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  AudioSinkTestEnvironment environment(frame_buffers);
  ASSERT_TRUE(environment.is_valid());

  // Audio sink need to be fully filled once to ensure it can start working.
  int frames_to_append = frame_buffers.frames_per_channel();
  environment.AppendFrame(frames_to_append);

  EXPECT_TRUE(environment.WaitUntilSomeFramesAreConsumed());
  ASSERT_GT(environment.GetFrameBufferFreeSpaceInFrames(), 0);
  environment.AppendFrame(environment.GetFrameBufferFreeSpaceInFrames());
  EXPECT_TRUE(environment.WaitUntilAllFramesAreConsumed());
}

TEST(SbAudioSinkTest, Pause) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  AudioSinkTestEnvironment environment(frame_buffers);
  ASSERT_TRUE(environment.is_valid());

  environment.SetIsPlaying(false);

  // Audio sink need to be fully filled once to ensure it can start working.
  int frames_to_append = frame_buffers.frames_per_channel();
  environment.AppendFrame(frames_to_append);

  int free_space = environment.GetFrameBufferFreeSpaceInFrames();
  EXPECT_TRUE(environment.WaitUntilUpdateStatusCalled());
  EXPECT_TRUE(environment.WaitUntilUpdateStatusCalled());
  EXPECT_EQ(free_space, environment.GetFrameBufferFreeSpaceInFrames());
  environment.SetIsPlaying(true);
  EXPECT_TRUE(environment.WaitUntilSomeFramesAreConsumed());
}

TEST(SbAudioSinkTest, Underflow) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());
  AudioSinkTestEnvironment environment(frame_buffers);
  ASSERT_TRUE(environment.is_valid());

  // Audio sink need to be fully filled once to ensure it can start working.
  int frames_to_append = frame_buffers.frames_per_channel();
  environment.AppendFrame(frames_to_append);

  EXPECT_TRUE(environment.WaitUntilSomeFramesAreConsumed());
  SbThreadSleep(250 * kSbTimeMillisecond);
  ASSERT_GT(environment.GetFrameBufferFreeSpaceInFrames(), 0);
  environment.AppendFrame(environment.GetFrameBufferFreeSpaceInFrames());
  EXPECT_TRUE(environment.WaitUntilAllFramesAreConsumed());
}

TEST(SbAudioSinkTest, ContinuousAppend) {
  AudioSinkTestFrameBuffers frame_buffers(SbAudioSinkGetMaxChannels());

  AudioSinkTestEnvironment environment(frame_buffers);
  ASSERT_TRUE(environment.is_valid());

  int sample_rate = environment.sample_rate();
  // We are trying to send 1/4s worth of audio samples.
  int frames_to_append = sample_rate / 4;

  while (frames_to_append > 0) {
    int free_space = environment.GetFrameBufferFreeSpaceInFrames();
    environment.AppendFrame(std::min(free_space, frames_to_append));
    frames_to_append -= std::min(free_space, frames_to_append);
    ASSERT_TRUE(environment.WaitUntilSomeFramesAreConsumed());
  }
  EXPECT_TRUE(environment.WaitUntilAllFramesAreConsumed());
}

}  // namespace nplb
}  // namespace starboard
