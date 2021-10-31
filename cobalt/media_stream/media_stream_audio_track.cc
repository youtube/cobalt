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

#include <vector>

#include "base/callback_helpers.h"
#include "cobalt/media_stream/media_track_settings.h"

namespace cobalt {
namespace media_stream {

void MediaStreamAudioTrack::AddSink(MediaStreamAudioSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (stop_callback_.is_null()) {
    sink->OnReadyStateChanged(MediaStreamTrack::kReadyStateEnded);
    return;
  }

  deliverer_.AddConsumer(sink);
}

void MediaStreamAudioTrack::RemoveSink(MediaStreamAudioSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool last_consumer = deliverer_.RemoveConsumer(sink);
  if (last_consumer) {
    Stop();
  }
}

void MediaStreamAudioTrack::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::ResetAndRunIfNotNull(&stop_callback_);

  std::vector<MediaStreamAudioSink*> consumer_sinks_to_remove;
  deliverer_.GetConsumerList(&consumer_sinks_to_remove);
  for (MediaStreamAudioSink* sink : consumer_sinks_to_remove) {
    deliverer_.RemoveConsumer(sink);
    sink->OnReadyStateChanged(MediaStreamTrack::kReadyStateEnded);
  }
}

void MediaStreamAudioTrack::OnReadyStateChanged(
    media_stream::MediaStreamTrack::ReadyState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<MediaStreamAudioSink*> sinks_to_notify;
  deliverer_.GetConsumerList(&sinks_to_notify);
  for (MediaStreamAudioSink* sink : sinks_to_notify) {
    sink->OnReadyStateChanged(new_state);
  }
}

void MediaStreamAudioTrack::Start(const base::Closure& stop_callback) {
  DCHECK(!stop_callback.is_null());  // This variable is passed in.
  DCHECK(stop_callback_.is_null());  // This is the member variable.
  stop_callback_ = stop_callback;
}

void MediaStreamAudioTrack::OnData(const AudioBus& audio_bus,
                                   base::TimeTicks reference_time) {
  deliverer_.OnData(audio_bus, reference_time);
}

void MediaStreamAudioTrack::OnSetFormat(
    const media_stream::AudioParameters& params) {
  MediaTrackSettings settings =
      MediaStreamAudioTrack::MediaTrackSettingsFromAudioParameters(params);
  SetMediaTrackSettings(settings);
  deliverer_.OnSetFormat(params);
}

}  // namespace media_stream
}  // namespace cobalt
