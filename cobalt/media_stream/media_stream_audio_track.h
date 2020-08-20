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

#ifndef COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_TRACK_H_
#define COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_TRACK_H_

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media/base/audio_bus.h"
#include "cobalt/media_stream/audio_parameters.h"
#include "cobalt/media_stream/media_stream_audio_deliverer.h"
#include "cobalt/media_stream/media_stream_audio_sink.h"
#include "cobalt/media_stream/media_stream_track.h"
#include "cobalt/media_stream/media_track_settings.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {

namespace media_capture {

class MediaRecorderTest;
}

namespace media_stream {

class MediaStreamAudioSource;

// This class represents a MediaStreamTrack, and implements the specification
// at: https://www.w3.org/TR/mediacapture-streams/#dom-mediastreamtrack
class MediaStreamAudioTrack : public MediaStreamTrack {
 public:
  static MediaTrackSettings MediaTrackSettingsFromAudioParameters(
      const AudioParameters& parameters) {
    MediaTrackSettings settings;
    settings.set_channel_count(parameters.channel_count());
    settings.set_sample_rate(parameters.sample_rate());
    settings.set_sample_size(parameters.bits_per_sample());
    return settings;
  }
  typedef media::AudioBus AudioBus;
  explicit MediaStreamAudioTrack(script::EnvironmentSettings* settings)
      : MediaStreamTrack(settings) {}

  ~MediaStreamAudioTrack() override { Stop(); }

  void AddSink(MediaStreamAudioSink* sink);
  void RemoveSink(MediaStreamAudioSink* sink);

  void Stop() override;

  void OnReadyStateChanged(
      media_stream::MediaStreamTrack::ReadyState new_state) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioTrackTest, OnSetFormatAndData);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioTrackTest, OneTrackMultipleSinks);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioTrackTest, TwoTracksWithOneSinkEach);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioTrackTest, AddRemoveSink);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioTrackTest, Stop);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioTrackTest,
                           ReadyStateEndedNotifyIfAlreadyStopped);

  friend class media_capture::MediaRecorderTest;
  friend class MediaStreamAudioSource;
  friend class MediaStreamAudioDeliverer<MediaStreamAudioTrack>;

  // Called by MediaStreamAudioSource.
  void Start(const base::Closure& stop_callback);

  // Called by MediaStreamAudioDeliverer.
  void OnData(const AudioBus& audio_bus, base::TimeTicks reference_time);
  void OnSetFormat(const media_stream::AudioParameters& params);

  THREAD_CHECKER(thread_checker_);
  MediaStreamAudioDeliverer<MediaStreamAudioSink> deliverer_;

  MediaStreamAudioTrack(const MediaStreamAudioTrack&) = delete;
  MediaStreamAudioTrack& operator=(const MediaStreamAudioTrack&) = delete;

  base::Closure stop_callback_;
};

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_TRACK_H_
