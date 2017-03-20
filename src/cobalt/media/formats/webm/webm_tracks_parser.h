// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_
#define COBALT_MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/base/media_tracks.h"
#include "cobalt/media/base/text_track_config.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "cobalt/media/formats/webm/webm_audio_client.h"
#include "cobalt/media/formats/webm/webm_content_encodings_client.h"
#include "cobalt/media/formats/webm/webm_parser.h"
#include "cobalt/media/formats/webm/webm_video_client.h"

namespace media {

// Parser for WebM Tracks element.
class MEDIA_EXPORT WebMTracksParser : public WebMParserClient {
 public:
  WebMTracksParser(const scoped_refptr<MediaLog>& media_log,
                   bool ignore_text_tracks);
  ~WebMTracksParser() OVERRIDE;

  // Parses a WebM Tracks element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8_t* buf, int size);

  int64_t audio_track_num() const { return audio_track_num_; }
  int64_t video_track_num() const { return video_track_num_; }

  // If TrackEntry DefaultDuration field existed for the associated audio or
  // video track, returns that value converted from ns to base::TimeDelta with
  // precision not greater than |timecode_scale_in_us|. Defaults to
  // kNoTimestamp.
  base::TimeDelta GetAudioDefaultDuration(
      const double timecode_scale_in_us) const;
  base::TimeDelta GetVideoDefaultDuration(
      const double timecode_scale_in_us) const;

  const std::set<int64_t>& ignored_tracks() const { return ignored_tracks_; }

  const std::string& audio_encryption_key_id() const {
    return audio_encryption_key_id_;
  }

  const AudioDecoderConfig& audio_decoder_config() {
    return audio_decoder_config_;
  }

  const std::string& video_encryption_key_id() const {
    return video_encryption_key_id_;
  }

  const VideoDecoderConfig& video_decoder_config() {
    return video_decoder_config_;
  }

  typedef std::map<int, TextTrackConfig> TextTracks;

  const TextTracks& text_tracks() const { return text_tracks_; }

  int detected_audio_track_count() const { return detected_audio_track_count_; }

  int detected_video_track_count() const { return detected_video_track_count_; }

  int detected_text_track_count() const { return detected_text_track_count_; }

  // Note: Calling media_tracks() method passes the ownership of the MediaTracks
  // object from WebMTracksParser to the caller (which is typically
  // WebMStreamParser object). So this method must be called only once, after
  // track parsing has been completed.
  scoped_ptr<MediaTracks> media_tracks() {
    CHECK(media_tracks_.get());
    return media_tracks_.Pass();
  }

 private:
  void Reset();
  void ResetTrackEntry();

  // WebMParserClient implementation.
  WebMParserClient* OnListStart(int id) OVERRIDE;
  bool OnListEnd(int id) OVERRIDE;
  bool OnUInt(int id, int64_t val) OVERRIDE;
  bool OnFloat(int id, double val) OVERRIDE;
  bool OnBinary(int id, const uint8_t* data, int size) OVERRIDE;
  bool OnString(int id, const std::string& str) OVERRIDE;

  bool reset_on_next_parse_;
  int64_t track_type_;
  int64_t track_num_;
  std::string track_name_;
  std::string track_language_;
  std::string codec_id_;
  std::vector<uint8_t> codec_private_;
  int64_t seek_preroll_;
  int64_t codec_delay_;
  int64_t default_duration_;
  scoped_ptr<WebMContentEncodingsClient> track_content_encodings_client_;

  int64_t audio_track_num_;
  int64_t audio_default_duration_;
  int64_t video_track_num_;
  int64_t video_default_duration_;
  bool ignore_text_tracks_;
  TextTracks text_tracks_;
  std::set<int64_t> ignored_tracks_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;
  scoped_refptr<MediaLog> media_log_;

  WebMAudioClient audio_client_;
  AudioDecoderConfig audio_decoder_config_;

  WebMVideoClient video_client_;
  VideoDecoderConfig video_decoder_config_;

  int detected_audio_track_count_;
  int detected_video_track_count_;
  int detected_text_track_count_;
  scoped_ptr<MediaTracks> media_tracks_;

  DISALLOW_COPY_AND_ASSIGN(WebMTracksParser);
};

}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_
