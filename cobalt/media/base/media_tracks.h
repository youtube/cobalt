// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_MEDIA_TRACKS_H_
#define COBALT_MEDIA_BASE_MEDIA_TRACKS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/media_track.h"

namespace cobalt {
namespace media {

class AudioDecoderConfig;
class VideoDecoderConfig;

class MEDIA_EXPORT MediaTracks {
 public:
  typedef std::vector<std::unique_ptr<MediaTrack>> MediaTracksCollection;

  MediaTracks();
  ~MediaTracks();

  // Adds a new audio track. The |bytestreamTrackId| must uniquely identify the
  // track within the bytestream.
  MediaTrack* AddAudioTrack(const AudioDecoderConfig& config,
                            StreamParser::TrackId bytestream_track_id,
                            const std::string& kind, const std::string& label,
                            const std::string& language);
  // Adds a new video track. The |bytestreamTrackId| must uniquely identify the
  // track within the bytestream.
  MediaTrack* AddVideoTrack(const VideoDecoderConfig& config,
                            StreamParser::TrackId bytestream_track_id,
                            const std::string& kind, const std::string& label,
                            const std::string& language);

  const MediaTracksCollection& tracks() const { return tracks_; }
  MediaTracksCollection& tracks() { return tracks_; }

  const AudioDecoderConfig& getAudioConfig(
      StreamParser::TrackId bytestream_track_id) const;
  const VideoDecoderConfig& getVideoConfig(
      StreamParser::TrackId bytestream_track_id) const;

 private:
  MediaTracksCollection tracks_;
  std::map<StreamParser::TrackId, AudioDecoderConfig> audio_configs_;
  std::map<StreamParser::TrackId, VideoDecoderConfig> video_configs_;

  DISALLOW_COPY_AND_ASSIGN(MediaTracks);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_MEDIA_TRACKS_H_
