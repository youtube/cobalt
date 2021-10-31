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

#include "cobalt/media_stream/media_stream_audio_track.h"

#include "base/bind_helpers.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/media/base/audio_bus.h"
#include "cobalt/media_stream/media_stream_audio_deliverer.h"
#include "cobalt/media_stream/testing/mock_media_stream_audio_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace {

const int kFrameCount = 20;
const int kChannelCount = 1;
const int kSampleRate = 1000;
const int kBitsPerSample = 8;

}  // namespace

namespace cobalt {
namespace media_stream {

typedef media::AudioBus AudioBus;

// This fixture is created, so it can be added as a friend to
// |MediaStreamAudioTrack|.  This enables calling a private method
// called|Start()| on the track, without instantiating an audio source.
class MediaStreamAudioTrackTest : public testing::Test {
 public:
  MediaStreamAudioTrackTest() = default;

 protected:
  dom::testing::StubEnvironmentSettings environment_settings_;
};

TEST_F(MediaStreamAudioTrackTest, OnSetFormatAndData) {
  media_stream::AudioParameters expected_params(kChannelCount, kSampleRate,
                                                kBitsPerSample);
  base::TimeTicks expected_time = base::TimeTicks::Now();

  StrictMock<MockMediaStreamAudioSink> mock_sink;
  ::testing::InSequence seq;
  EXPECT_CALL(mock_sink, OnSetFormat(expected_params));
  EXPECT_CALL(mock_sink, OnData(_, expected_time));
  EXPECT_CALL(mock_sink, OnReadyStateChanged(
                             MediaStreamTrack::ReadyState::kReadyStateEnded));

  scoped_refptr<MediaStreamAudioTrack> track =
      new MediaStreamAudioTrack(&environment_settings_);
  track->Start(base::Closure(base::Bind([]() {} /*Do nothing*/)));
  track->AddSink(&mock_sink);

  MediaStreamAudioDeliverer<MediaStreamAudioTrack> deliverer;
  deliverer.AddConsumer(track);

  deliverer.OnSetFormat(expected_params);

  AudioBus audio_bus(kChannelCount, kFrameCount, AudioBus::kInt16,
                     AudioBus::kInterleaved);

  deliverer.OnData(audio_bus, expected_time);
}

TEST_F(MediaStreamAudioTrackTest, OneTrackMultipleSinks) {
  media_stream::AudioParameters expected_params(kChannelCount, kSampleRate,
                                                kBitsPerSample);
  base::TimeTicks expected_time = base::TimeTicks::Now();

  StrictMock<MockMediaStreamAudioSink> mock_sink1;
  EXPECT_CALL(mock_sink1, OnSetFormat(expected_params));
  EXPECT_CALL(mock_sink1, OnData(_, expected_time));
  EXPECT_CALL(mock_sink1, OnReadyStateChanged(
                              MediaStreamTrack::ReadyState::kReadyStateEnded));
  StrictMock<MockMediaStreamAudioSink> mock_sink2;
  EXPECT_CALL(mock_sink2, OnSetFormat(expected_params));
  EXPECT_CALL(mock_sink2, OnData(_, expected_time));
  EXPECT_CALL(mock_sink2, OnReadyStateChanged(
                              MediaStreamTrack::ReadyState::kReadyStateEnded));

  scoped_refptr<MediaStreamAudioTrack> track =
      new MediaStreamAudioTrack(&environment_settings_);
  track->Start(base::Closure(base::Bind([]() {} /*Do nothing*/)));
  track->AddSink(&mock_sink1);
  track->AddSink(&mock_sink2);

  MediaStreamAudioDeliverer<MediaStreamAudioTrack> deliverer;
  deliverer.AddConsumer(track);

  deliverer.OnSetFormat(expected_params);

  AudioBus audio_bus(kChannelCount, kFrameCount, AudioBus::kInt16,
                     AudioBus::kInterleaved);

  deliverer.OnData(audio_bus, expected_time);
}

TEST_F(MediaStreamAudioTrackTest, TwoTracksWithOneSinkEach) {
  media_stream::AudioParameters expected_params(kChannelCount, kSampleRate,
                                                kBitsPerSample);
  base::TimeTicks expected_time = base::TimeTicks::Now();

  StrictMock<MockMediaStreamAudioSink> mock_sink1;
  EXPECT_CALL(mock_sink1, OnSetFormat(expected_params));
  EXPECT_CALL(mock_sink1, OnData(_, expected_time));
  EXPECT_CALL(mock_sink1, OnReadyStateChanged(
                              MediaStreamTrack::ReadyState::kReadyStateEnded));
  StrictMock<MockMediaStreamAudioSink> mock_sink2;
  EXPECT_CALL(mock_sink2, OnSetFormat(expected_params));
  EXPECT_CALL(mock_sink2, OnData(_, expected_time));
  EXPECT_CALL(mock_sink2, OnReadyStateChanged(
                              MediaStreamTrack::ReadyState::kReadyStateEnded));

  scoped_refptr<MediaStreamAudioTrack> track1 =
      new MediaStreamAudioTrack(&environment_settings_);
  scoped_refptr<MediaStreamAudioTrack> track2 =
      new MediaStreamAudioTrack(&environment_settings_);
  track1->Start(base::Closure(base::Bind([]() {} /*Do nothing*/)));
  track2->Start(base::Closure(base::Bind([]() {} /*Do nothing*/)));
  track1->AddSink(&mock_sink1);
  track2->AddSink(&mock_sink2);

  MediaStreamAudioDeliverer<MediaStreamAudioTrack> deliverer;
  deliverer.AddConsumer(track1);
  deliverer.AddConsumer(track2);

  deliverer.OnSetFormat(expected_params);

  AudioBus audio_bus(kChannelCount, kFrameCount, AudioBus::kInt16,
                     AudioBus::kInterleaved);

  deliverer.OnData(audio_bus, expected_time);
}

TEST_F(MediaStreamAudioTrackTest, AddRemoveSink) {
  media_stream::AudioParameters expected_params(kChannelCount, kSampleRate,
                                                kBitsPerSample);
  base::TimeTicks expected_time = base::TimeTicks::Now();

  // This never receives any data because the sink is removed before the data
  // is delivered.
  StrictMock<MockMediaStreamAudioSink> mock_sink;

  scoped_refptr<MediaStreamAudioTrack> track =
      new MediaStreamAudioTrack(&environment_settings_);
  track->Start(base::Closure(base::Bind([]() {} /*Do nothing*/)));
  track->AddSink(&mock_sink);

  MediaStreamAudioDeliverer<MediaStreamAudioTrack> deliverer;
  deliverer.AddConsumer(track);

  deliverer.OnSetFormat(expected_params);

  AudioBus audio_bus(kChannelCount, kFrameCount, AudioBus::kInt16,
                     AudioBus::kInterleaved);

  track->RemoveSink(&mock_sink);
  deliverer.OnData(audio_bus, expected_time);
}

TEST_F(MediaStreamAudioTrackTest, Stop) {
  StrictMock<MockMediaStreamAudioSink> mock_sink;

  scoped_refptr<MediaStreamAudioTrack> track =
      new MediaStreamAudioTrack(&environment_settings_);
  track->Start(base::Closure(base::Bind([]() {} /*Do nothing*/)));
  track->AddSink(&mock_sink);

  MediaStreamAudioDeliverer<MediaStreamAudioTrack> deliverer;
  deliverer.AddConsumer(track);

  EXPECT_CALL(mock_sink, OnReadyStateChanged(
                             MediaStreamTrack::ReadyState::kReadyStateEnded));

  // This should call |OnReadyStateChanged| with |ReadyState::kReadyStateEnded|,
  // and |OnReadyStateChanged| should NOT be called when track gets destroyed,
  // since the track is already stopped.
  track->Stop();

  EXPECT_CALL(mock_sink, OnReadyStateChanged(_)).Times(0);
}

TEST_F(MediaStreamAudioTrackTest, ReadyStateEndedNotifyIfAlreadyStopped) {
  scoped_refptr<MediaStreamAudioTrack> track =
      new MediaStreamAudioTrack(&environment_settings_);
  track->Start(base::Closure(base::Bind([]() {} /*Do nothing*/)));
  track->Stop();

  StrictMock<MockMediaStreamAudioSink> mock_sink;
  EXPECT_CALL(mock_sink, OnReadyStateChanged(
                             MediaStreamTrack::ReadyState::kReadyStateEnded));
  track->AddSink(&mock_sink);
}

TEST_F(MediaStreamAudioTrackTest, ReadyStateEndedNotifyIfNeverStarted) {
  scoped_refptr<MediaStreamAudioTrack> track =
      new MediaStreamAudioTrack(&environment_settings_);

  StrictMock<MockMediaStreamAudioSink> mock_sink;
  EXPECT_CALL(mock_sink, OnReadyStateChanged(
                             MediaStreamTrack::ReadyState::kReadyStateEnded));
  track->AddSink(&mock_sink);
}

}  // namespace media_stream
}  // namespace cobalt
