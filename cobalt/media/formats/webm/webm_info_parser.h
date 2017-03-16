// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_WEBM_WEBM_INFO_PARSER_H_
#define COBALT_MEDIA_FORMATS_WEBM_WEBM_INFO_PARSER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/formats/webm/webm_parser.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// Parser for WebM Info element.
class MEDIA_EXPORT WebMInfoParser : public WebMParserClient {
 public:
  WebMInfoParser();
  ~WebMInfoParser() OVERRIDE;

  // Parses a WebM Info element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8_t* buf, int size);

  int64_t timecode_scale() const { return timecode_scale_; }
  double duration() const { return duration_; }
  base::Time date_utc() const { return date_utc_; }

 private:
  // WebMParserClient methods
  WebMParserClient* OnListStart(int id) OVERRIDE;
  bool OnListEnd(int id) OVERRIDE;
  bool OnUInt(int id, int64_t val) OVERRIDE;
  bool OnFloat(int id, double val) OVERRIDE;
  bool OnBinary(int id, const uint8_t* data, int size) OVERRIDE;
  bool OnString(int id, const std::string& str) OVERRIDE;

  int64_t timecode_scale_;
  double duration_;
  base::Time date_utc_;

  DISALLOW_COPY_AND_ASSIGN(WebMInfoParser);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_FORMATS_WEBM_WEBM_INFO_PARSER_H_
