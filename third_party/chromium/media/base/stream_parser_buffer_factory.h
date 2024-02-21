// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_BUFFER_FACTORY_H_
#define MEDIA_BASE_STREAM_PARSER_BUFFER_FACTORY_H_

#include "media/base/media_export.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/decoder_buffer_factory.h"

namespace media {

typedef DemuxerStream::Type Type;
typedef StreamParser::TrackId TrackId;

class MEDIA_EXPORT StreamParserBufferFactory : protected DecoderBufferFactory {
 public:
  StreamParserBufferFactory();
  StreamParserBufferFactory(const StreamParserBufferFactory&) = delete;
  StreamParserBufferFactory& operator=(const StreamParserBufferFactory&) = delete;
  ~StreamParserBufferFactory();

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
  class FactoryImpl;
  scoped_refptr<FactoryImpl> factory_;

};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_BUFFER_FACTORY_H_
