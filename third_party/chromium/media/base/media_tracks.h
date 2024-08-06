// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_TRACKS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_TRACKS_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/base/media_track.h"

namespace media_m96 {

class AudioDecoderConfig;
class VideoDecoderConfig;

class MEDIA_EXPORT MediaTracks {
 public:
  using MediaTracksCollection = std::vector<std::unique_ptr<MediaTrack>>;

  MediaTracks();

  MediaTracks(const MediaTracks&) = delete;
  MediaTracks& operator=(const MediaTracks&) = delete;

  ~MediaTracks();

  // Adds a new audio track. The |bytestreamTrackId| must uniquely identify the
  // track within the bytestream.
  MediaTrack* AddAudioTrack(const AudioDecoderConfig& config,
                            StreamParser::TrackId bytestream_track_id,
                            const MediaTrack::Kind& kind,
                            const MediaTrack::Label& label,
                            const MediaTrack::Language& language);
  // Adds a new video track. The |bytestreamTrackId| must uniquely identify the
  // track within the bytestream.
  MediaTrack* AddVideoTrack(const VideoDecoderConfig& config,
                            StreamParser::TrackId bytestream_track_id,
                            const MediaTrack::Kind& kind,
                            const MediaTrack::Label& label,
                            const MediaTrack::Language& language);

  const MediaTracksCollection& tracks() const { return tracks_; }

  const AudioDecoderConfig& getAudioConfig(
      StreamParser::TrackId bytestream_track_id) const;
  const VideoDecoderConfig& getVideoConfig(
      StreamParser::TrackId bytestream_track_id) const;

 private:
  MediaTracksCollection tracks_;
  std::map<StreamParser::TrackId, AudioDecoderConfig> audio_configs_;
  std::map<StreamParser::TrackId, VideoDecoderConfig> video_configs_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_TRACKS_H_
