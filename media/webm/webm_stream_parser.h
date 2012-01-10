// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_STREAM_PARSER_H_
#define MEDIA_WEBM_WEBM_STREAM_PARSER_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffers.h"
#include "media/base/stream_parser.h"
#include "media/base/video_decoder_config.h"
#include "media/webm/webm_cluster_parser.h"

namespace media {

class WebMStreamParser : public StreamParser {
 public:
  WebMStreamParser();
  virtual ~WebMStreamParser();

  // StreamParser implementation.
  virtual void Init(const InitCB& init_cb, StreamParserHost* host) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual int Parse(const uint8* buf, int size) OVERRIDE;

 private:
  enum State {
    WAITING_FOR_INIT,
    PARSING_HEADERS,
    PARSING_CLUSTERS
  };

  void ChangeState(State new_state);

  // Parses WebM Header, Info, Tracks elements. It also skips other level 1
  // elements that are not used right now. Once the Info & Tracks elements have
  // been parsed, this method will transition the parser from PARSING_HEADERS to
  // PARSING_CLUSTERS.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseInfoAndTracks(const uint8* data, int size);

  // Incrementally parses WebM cluster elements. This method also skips
  // CUES elements if they are encountered since we currently don't use the
  // data in these elements.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseCluster(const uint8* data, int size);

  State state_;
  InitCB init_cb_;
  StreamParserHost* host_;

  scoped_ptr<WebMClusterParser> cluster_parser_;

  DISALLOW_COPY_AND_ASSIGN(WebMStreamParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_STREAM_PARSER_H_
