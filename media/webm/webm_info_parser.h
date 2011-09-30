// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_INFO_PARSER_H_
#define MEDIA_WEBM_WEBM_INFO_PARSER_H_

#include "media/webm/webm_parser.h"

namespace media {

// Parser for WebM Info element.
class WebMInfoParser : public WebMParserClient {
 public:
  WebMInfoParser();
  virtual ~WebMInfoParser();

  // Parses a WebM Info element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8* buf, int size);

  int64 timecode_scale() const { return timecode_scale_; }
  double duration() const { return duration_; }

 private:
  // WebMParserClient methods
  virtual bool OnListStart(int id);
  virtual bool OnListEnd(int id);
  virtual bool OnUInt(int id, int64 val);
  virtual bool OnFloat(int id, double val);
  virtual bool OnBinary(int id, const uint8* data, int size);
  virtual bool OnString(int id, const std::string& str);
  virtual bool OnSimpleBlock(int track_num, int timecode, int flags,
                             const uint8* data, int size);

  int64 timecode_scale_;
  double duration_;

  DISALLOW_COPY_AND_ASSIGN(WebMInfoParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_INFO_PARSER_H_
