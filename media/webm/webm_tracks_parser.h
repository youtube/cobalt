// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_TRACKS_PARSER_H_
#define MEDIA_WEBM_WEBM_TRACKS_PARSER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "media/webm/webm_content_encodings_client.h"
#include "media/webm/webm_parser.h"

namespace media {

// Parser for WebM Tracks element.
class WebMTracksParser : public WebMParserClient {
 public:
  explicit WebMTracksParser(int64 timecode_scale);
  virtual ~WebMTracksParser();

  // Parses a WebM Tracks element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8* buf, int size);

  int64 audio_track_num() const { return audio_track_num_; }
  int64 video_track_num() const { return video_track_num_; }

  const uint8* video_encryption_key_id() const;
  int video_encryption_key_id_size() const;

 private:
  // WebMParserClient methods
  virtual WebMParserClient* OnListStart(int id) OVERRIDE;
  virtual bool OnListEnd(int id) OVERRIDE;
  virtual bool OnUInt(int id, int64 val) OVERRIDE;
  virtual bool OnFloat(int id, double val) OVERRIDE;
  virtual bool OnBinary(int id, const uint8* data, int size) OVERRIDE;
  virtual bool OnString(int id, const std::string& str) OVERRIDE;

  int64 timecode_scale_;

  int64 track_type_;
  int64 track_num_;
  scoped_ptr<WebMContentEncodingsClient> track_content_encodings_client_;

  int64 audio_track_num_;
  scoped_ptr<WebMContentEncodingsClient> audio_content_encodings_client_;

  int64 video_track_num_;
  scoped_ptr<WebMContentEncodingsClient> video_content_encodings_client_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMTracksParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_TRACKS_PARSER_H_
