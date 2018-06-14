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

#include "cobalt/media_stream/media_stream_audio_track.h"

namespace cobalt {
namespace media_stream {

void MediaStreamAudioTrack::AddSink(MediaStreamAudioSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  deliverer_.AddConsumer(sink);
}

void MediaStreamAudioTrack::RemoveSink(MediaStreamAudioSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  deliverer_.RemoveConsumer(sink);
}

void MediaStreamAudioTrack::Start(const base::Closure& stop_callback) {
  DCHECK(!stop_callback.is_null());  // This variable is passed in.
  DCHECK(stop_callback_.is_null());  // This is the member variable.
  stop_callback_ = stop_callback;
}

void MediaStreamAudioTrack::OnData(const ShellAudioBus& audio_bus,
                                   base::TimeTicks reference_time) {
  deliverer_.OnData(audio_bus, reference_time);
}

void MediaStreamAudioTrack::OnSetFormat(
    const media_stream::AudioParameters& params) {
  settings_.set_channel_count(params.channel_count());
  settings_.set_sample_rate(params.sample_rate());
  settings_.set_sample_size(params.bits_per_sample());
  deliverer_.OnSetFormat(params);
}

}  // namespace media_stream
}  // namespace cobalt
