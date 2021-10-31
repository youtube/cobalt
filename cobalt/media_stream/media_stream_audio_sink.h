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

#ifndef COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_SINK_H_
#define COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_SINK_H_

#include "cobalt/media/base/audio_bus.h"

#include "cobalt/media_stream/audio_parameters.h"

#include "cobalt/media_stream/media_stream_source.h"
#include "cobalt/media_stream/media_stream_track.h"


namespace cobalt {
namespace media_stream {

// Abstract base class that is used to represent a sink for the audio data.
// Note: users of this class will call OnSetFormat is before OnData.
class MediaStreamAudioSink {
 public:
  typedef media::AudioBus AudioBus;
  MediaStreamAudioSink() = default;

  // These are called on the same thread.
  virtual void OnData(const AudioBus& audio_bus,
                      base::TimeTicks reference_time) = 0;
  virtual void OnSetFormat(const media_stream::AudioParameters& params) = 0;
  virtual void OnReadyStateChanged(
      media_stream::MediaStreamTrack::ReadyState new_state) {
  }

 private:
  MediaStreamAudioSink(const MediaStreamAudioSink&) = delete;
  MediaStreamAudioSink& operator=(const MediaStreamAudioSink&) = delete;
};

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_SINK_H_
