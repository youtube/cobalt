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

#include "cobalt/media_stream/media_stream_audio_source.h"

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/media/base/audio_bus.h"
#include "cobalt/media_stream/media_stream_audio_track.h"
#include "cobalt/media_stream/testing/mock_media_stream_audio_source.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

void Increment(int* value) { ++(*value); }

}  // namespace

namespace cobalt {
namespace media_stream {

typedef media::AudioBus AudioBus;

class MediaStreamAudioSourceTest : public testing::Test {
 public:
  MediaStreamAudioSourceTest()
      : audio_source_(new StrictMock<MockMediaStreamAudioSource>()),
        track_(new MediaStreamAudioTrack(&environment_settings_)) {}

 protected:
  dom::testing::StubEnvironmentSettings environment_settings_;
  scoped_refptr<StrictMock<MockMediaStreamAudioSource>> audio_source_;
  scoped_refptr<MediaStreamAudioTrack> track_;
};

TEST_F(MediaStreamAudioSourceTest, SourceCanStart) {
  EXPECT_CALL(*audio_source_, EnsureSourceIsStarted()).WillOnce(Return(true));
  // Source will be stopped when the last track is disconnected.
  // This happens when |track_| is destructed.
  EXPECT_CALL(*audio_source_, EnsureSourceIsStopped());

  // Adding a track ensures that the source is started.
  bool source_started = audio_source_->ConnectToTrack(track_.get());
  EXPECT_TRUE(source_started);
}

TEST_F(MediaStreamAudioSourceTest, SourceCannotStart) {
  EXPECT_CALL(*audio_source_, EnsureSourceIsStarted()).WillOnce(Return(false));
  EXPECT_CALL(*audio_source_, EnsureSourceIsStopped());

  // Adding a track ensures that the source is started (if it is not stopped).
  bool source_started = audio_source_->ConnectToTrack(track_.get());
  EXPECT_FALSE(source_started);
}

TEST_F(MediaStreamAudioSourceTest, CallStopCallback) {
  EXPECT_CALL(*audio_source_, EnsureSourceIsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*audio_source_, EnsureSourceIsStopped());

  int times_callback_is_called = 0;
  base::Closure closure = base::Bind(Increment, &times_callback_is_called);
  audio_source_->SetStopCallback(closure);

  // Adding a track ensures that the source is started.
  audio_source_->ConnectToTrack(track_);
  audio_source_->StopSource();

  EXPECT_EQ(1, times_callback_is_called);
}

TEST_F(MediaStreamAudioSourceTest, EmptyStopCallback) {
  // This test should not crash.
  EXPECT_CALL(*audio_source_, EnsureSourceIsStarted()).WillOnce(Return(true));
  EXPECT_CALL(*audio_source_, EnsureSourceIsStopped());

  // Adding a track ensures that the source is started.
  audio_source_->ConnectToTrack(track_);
  audio_source_->StopSource();
}

TEST_F(MediaStreamAudioSourceTest, AddMultipleTracks) {
  // This test should not crash.
  EXPECT_CALL(*audio_source_, EnsureSourceIsStarted())
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*audio_source_, EnsureSourceIsStopped());

  // Adding a track ensures that the source is started.
  audio_source_->ConnectToTrack(track_);
  auto track2 =
      base::WrapRefCounted(new MediaStreamAudioTrack(&environment_settings_));
  audio_source_->ConnectToTrack(track2);
  audio_source_->StopSource();
}

}  // namespace media_stream
}  // namespace cobalt
