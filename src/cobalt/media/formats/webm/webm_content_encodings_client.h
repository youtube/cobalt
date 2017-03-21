// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_CLIENT_H_
#define COBALT_MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/formats/webm/webm_content_encodings.h"
#include "cobalt/media/formats/webm/webm_parser.h"

namespace media {

typedef std::vector<ContentEncoding> ContentEncodings;

// Parser for WebM ContentEncodings element.
class MEDIA_EXPORT WebMContentEncodingsClient : public WebMParserClient {
 public:
  explicit WebMContentEncodingsClient(const scoped_refptr<MediaLog>& media_log);
  ~WebMContentEncodingsClient() OVERRIDE;

  const ContentEncodings& content_encodings() const;

  // WebMParserClient methods
  WebMParserClient* OnListStart(int id) OVERRIDE;
  bool OnListEnd(int id) OVERRIDE;
  bool OnUInt(int id, int64_t val) OVERRIDE;
  bool OnBinary(int id, const uint8_t* data, int size) OVERRIDE;

 private:
  scoped_refptr<MediaLog> media_log_;
  scoped_ptr<ContentEncoding> cur_content_encoding_;
  bool content_encryption_encountered_;
  ContentEncodings content_encodings_;

  // |content_encodings_| is ready. For debugging purpose.
  bool content_encodings_ready_;

  DISALLOW_COPY_AND_ASSIGN(WebMContentEncodingsClient);
};

}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_CLIENT_H_
