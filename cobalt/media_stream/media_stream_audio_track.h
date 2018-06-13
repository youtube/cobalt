// Copyright 2018 Google Inc. All Rights Reserved.
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
#include "base/string_piece.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media_stream/audio_parameters.h"
#include "cobalt/media_stream/media_stream_audio_deliverer.h"
#include "cobalt/media_stream/media_stream_audio_sink.h"
#include "cobalt/media_stream/media_stream_track.h"
#include "cobalt/media_stream/media_track_settings.h"

#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_audio_bus.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/shell_audio_bus.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace media_stream {

class MediaStreamAudioSource;

// This class represents a MediaStreamTrack, and implements the specification
// at: https://www.w3.org/TR/mediacapture-streams/#dom-mediastreamtrack
class MediaStreamAudioTrack : public MediaStreamTrack {
 public:
#if defined(COBALT_MEDIA_SOURCE_2016)
  typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
  MediaStreamAudioTrack() = default;

  void AddSink(MediaStreamAudioSink* sink);
  void RemoveSink(MediaStreamAudioSink* sink);

 private:
  friend class MediaStreamAudioSource;
  friend class MediaStreamAudioDeliverer<MediaStreamAudioTrack>;

  // Called by MediaStreamAudioSource.
  void Start(const base::Closure& stop_callback);
  void UpdateTrackSettings(int sample_rate, int sample_size_in_bits,
                           int channel_count);

  // Called by MediaStreamAudioDeliverer.
  void OnData(const ShellAudioBus& audio_bus, base::TimeTicks reference_time);
  void OnSetFormat(const media_stream::AudioParameters& params);

  base::ThreadChecker thread_checker_;
  MediaStreamAudioDeliverer<MediaStreamAudioSink> deliverer_;

  MediaStreamAudioTrack(const MediaStreamAudioTrack&) = delete;
  MediaStreamAudioTrack& operator=(const MediaStreamAudioTrack&) = delete;

  base::Closure stop_callback_;
};

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_TRACK_H_
