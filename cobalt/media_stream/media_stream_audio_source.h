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

#ifndef COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_SOURCE_H_
#define COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_SOURCE_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/media_stream/audio_parameters.h"
#include "cobalt/media_stream/media_stream_audio_deliverer.h"
#include "cobalt/media_stream/media_stream_audio_track.h"
#include "cobalt/media_stream/media_stream_source.h"
#include "cobalt/media_stream/media_track_settings.h"

namespace cobalt {
namespace media_stream {

// Base class that provides functionality to connect tracks
// and have audio data delivered to them.
// Subclasses should:
//   implement |EnsureSourceIsStarted()| and |EnsureSourceIsStopped()|
//   call |SetFormat()| and |DeliverDataToTracks()|
class MediaStreamAudioSource : public MediaStreamSource {
 public:
  MediaStreamAudioSource();
  ~MediaStreamAudioSource() override;

  // Connects this source to the track. One source can be connected to multiple
  // tracks.  Returns true if successful.
  bool ConnectToTrack(MediaStreamAudioTrack* track);

  MediaTrackSettings GetMediaTrackSettings() const;

 protected:
  // Subclasses should override these Ensure* methods.

  // Returns true if the source has been successfully started.
  // After the source has been started, subclasses should call
  // |DeliverDataToTracks| to deliver the data.
  virtual bool EnsureSourceIsStarted() = 0;
  // Stops the source. |DeliverDataToTracks| must not be called after
  // the source is stopped.
  virtual void EnsureSourceIsStopped() = 0;

  // Subclasses should call these methods. |DeliverDataToTracks| can be
  // called from a different thread than where MediaStreamAudioSource
  // was created.
  void DeliverDataToTracks(const MediaStreamAudioTrack::AudioBus& audio_bus,
                           base::TimeTicks reference_time);

  void SetFormat(const media_stream::AudioParameters& params) {
    DLOG(INFO) << "MediaStreamAudioSource@" << this << "::SetFormat("
               << params.AsHumanReadableString() << "), was previously set to {"
               << deliverer_.GetAudioParameters().AsHumanReadableString()
               << "}.";
    deliverer_.OnSetFormat(params);
  }

 protected:
  void NotifyTracksOfNewReadyState(
      MediaStreamAudioTrack::ReadyState new_ready_state);

 private:
  MediaStreamAudioSource(const MediaStreamAudioSource&) = delete;
  MediaStreamAudioSource& operator=(const MediaStreamAudioSource&) = delete;

  void DoStopSource() final;
  void StopAudioDeliveryTo(MediaStreamAudioTrack* track);

  THREAD_CHECKER(thread_checker_);
  base::Closure stop_callback_;

  base::WeakPtrFactory<MediaStreamAudioSource> weak_ptr_factory_;
  base::WeakPtr<MediaStreamAudioSource> weak_this_;

  MediaStreamAudioDeliverer<MediaStreamAudioTrack> deliverer_;

  bool is_stopped_ = false;

  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioSourceTest, DeliverData);
};

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_MEDIA_STREAM_AUDIO_SOURCE_H_
