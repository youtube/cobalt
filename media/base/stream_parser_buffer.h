// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_BUFFER_H_
#define MEDIA_BASE_STREAM_PARSER_BUFFER_H_

#include "media/base/data_buffer.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT StreamParserBuffer : public DataBuffer {
 public:
  static scoped_refptr<StreamParserBuffer> CreateEOSBuffer();
  static scoped_refptr<StreamParserBuffer> CopyFrom(
      const uint8* data, int data_size, bool is_keyframe);
  bool IsKeyframe() const { return is_keyframe_; }

  // Returns this buffer's timestamp + duration, assuming both are valid.
  base::TimeDelta GetEndTimestamp() const;

 private:
  StreamParserBuffer(const uint8* data, int data_size, bool is_keyframe);

  bool is_keyframe_;
  DISALLOW_COPY_AND_ASSIGN(StreamParserBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_BUFFER_H_
