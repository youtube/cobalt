// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_TRACKS_PARSER_H_
#define MEDIA_WEBM_WEBM_TRACKS_PARSER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_log.h"
#include "media/base/video_decoder_config.h"
#include "media/webm/webm_audio_client.h"
#include "media/webm/webm_content_encodings_client.h"
#include "media/webm/webm_parser.h"
#include "media/webm/webm_video_client.h"

namespace media {

// Parser for WebM Tracks element.
class WebMTracksParser : public WebMParserClient {
 public:
  explicit WebMTracksParser(const LogCB& log_cb);
  virtual ~WebMTracksParser();

  // Parses a WebM Tracks element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8* buf, int size);

  int64 audio_track_num() const { return audio_track_num_; }
  int64 video_track_num() const { return video_track_num_; }
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

 private:
  // WebMParserClient methods
  virtual WebMParserClient* OnListStart(int id) override;
  virtual bool OnListEnd(int id) override;
  virtual bool OnUInt(int id, int64 val) override;
  virtual bool OnFloat(int id, double val) override;
  virtual bool OnBinary(int id, const uint8* data, int size) override;
  virtual bool OnString(int id, const std::string& str) override;

  int64 track_type_;
  int64 track_num_;
  std::string codec_id_;
  std::vector<uint8> codec_private_;
  scoped_ptr<WebMContentEncodingsClient> track_content_encodings_client_;

  int64 audio_track_num_;
  int64 video_track_num_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;
  LogCB log_cb_;

  WebMAudioClient audio_client_;
  AudioDecoderConfig audio_decoder_config_;

  WebMVideoClient video_client_;
  VideoDecoderConfig video_decoder_config_;

  DISALLOW_COPY_AND_ASSIGN(WebMTracksParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_TRACKS_PARSER_H_
