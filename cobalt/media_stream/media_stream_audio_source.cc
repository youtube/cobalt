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

#include <vector>

#include "base/compiler_specific.h"

namespace cobalt {
namespace media_stream {

MediaStreamAudioSource::~MediaStreamAudioSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stop_callback_.is_null());
}

bool MediaStreamAudioSource::ConnectToTrack(MediaStreamAudioTrack* track) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(track);

  // Try to start the source, if has not been permanently stopped.
  // If we are unable to start the source, the MediaSteamTrack will stay in
  // the stopped/ended state.
  if (!is_stopped_) {
    if (!EnsureSourceIsStarted()) {
      StopSource();
    }
  }

  if (is_stopped_) {
    return false;
  }

  // It is safe to pass an unretained pointer, since the callback here
  // will be called in |MediaStreamAudioTrack::Stop|.
  track->Start(base::Bind(&MediaStreamAudioSource::StopAudioDeliveryTo,
                          weak_this_, base::Unretained(track)));
  deliverer_.AddConsumer(track);
  return true;
}

MediaTrackSettings MediaStreamAudioSource::GetMediaTrackSettings() const {
  // The following call is thread safe.
  AudioParameters parameters = deliverer_.GetAudioParameters();

  return MediaStreamAudioTrack::MediaTrackSettingsFromAudioParameters(
      parameters);
}

void MediaStreamAudioSource::DeliverDataToTracks(
    const MediaStreamAudioTrack::AudioBus& audio_bus,
    base::TimeTicks reference_time) {
  deliverer_.OnData(audio_bus, reference_time);
}

void MediaStreamAudioSource::NotifyTracksOfNewReadyState(
    MediaStreamAudioTrack::ReadyState new_ready_state) {
  std::vector<MediaStreamAudioTrack*> tracks_to_notify;
  deliverer_.GetConsumerList(&tracks_to_notify);
  for (MediaStreamAudioTrack* track : tracks_to_notify) {
    track->OnReadyStateChanged(new_ready_state);
  }
}

MediaStreamAudioSource::MediaStreamAudioSource()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_this_(weak_ptr_factory_.GetWeakPtr()) {}

void MediaStreamAudioSource::DoStopSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_stopped_ = true;

  // This function might result in the destruction of this object, so be
  // careful not to reference members after this call.
  EnsureSourceIsStopped();
}

void MediaStreamAudioSource::StopAudioDeliveryTo(MediaStreamAudioTrack* track) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool did_remove_last_track = deliverer_.RemoveConsumer(track);
  // Per spec at: https://www.w3.org/TR/mediacapture-streams/#mediastreamtrack
  // "When all tracks using a source have been stopped or ended by some other
  // means, the source is stopped."
  if (!is_stopped_ && did_remove_last_track) {
    StopSource();
  }
}

}  // namespace media_stream
}  // namespace cobalt
