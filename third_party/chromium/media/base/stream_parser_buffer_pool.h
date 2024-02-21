// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_BUFFER_POOL_H_
#define MEDIA_BASE_STREAM_PARSER_BUFFER_POOL_H_

#include <stddef.h>

#include "media/base/media_export.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/decoder_buffer_pool.h"

namespace base {
class TickClock;
}

namespace media {

typedef DemuxerStream::Type Type;
typedef StreamParser::TrackId TrackId;

class MEDIA_EXPORT StreamParserBufferPool : protected DecoderBufferPool {
 public:
  StreamParserBufferPool();
  StreamParserBufferPool(const StreamParserBufferPool&) = delete;
  StreamParserBufferPool& operator=(const StreamParserBufferPool&) = delete;
  ~StreamParserBufferPool();

  scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
                                             size_t data_size,
                                             bool is_key_frame,
                                             Type type,
                                             TrackId track_id);

  scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
                                             size_t data_size,
                                             const uint8_t* side_data,
                                             size_t side_data_size,
                                             bool is_key_frame,
                                             Type type,
                                             TrackId track_id);
 private:
  class PoolImpl;
  scoped_refptr<PoolImpl> pool_;

};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_BUFFER_POOL_H_
