// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/stream_parser_buffer.h"

#include "base/logging.h"
#include "media/base/shell_buffer_factory.h"

namespace media {

#if defined(__LB_SHELL__) || defined(COBALT)
scoped_refptr<StreamParserBuffer> StreamParserBuffer::CreateEOSBuffer() {
  return make_scoped_refptr(new StreamParserBuffer(NULL, 0, false));
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CopyFrom(
    const uint8* data,
    int data_size,
    bool is_keyframe) {
  if (!data || data_size == 0) {
    return CreateEOSBuffer();
  }
  DCHECK(data);
  DCHECK_GT(data_size, 0);

  uint8* buffer_bytes = ShellBufferFactory::Instance()->AllocateNow(data_size);
  if (!buffer_bytes) {
    return NULL;
  }
  // copy the data over to the resuable buffer area
  memcpy(buffer_bytes, data, data_size);
  return make_scoped_refptr(
      new StreamParserBuffer(buffer_bytes,
                             data_size,
                             is_keyframe));
}
#else
scoped_refptr<StreamParserBuffer> StreamParserBuffer::CopyFrom(
    const uint8* data, int data_size, bool is_keyframe) {
  return make_scoped_refptr(
      new StreamParserBuffer(data, data_size, is_keyframe));
}
#endif

base::TimeDelta StreamParserBuffer::GetDecodeTimestamp() const {
  if (decode_timestamp_ == kNoTimestamp())
    return GetTimestamp();
  return decode_timestamp_;
}

void StreamParserBuffer::SetDecodeTimestamp(const base::TimeDelta& timestamp) {
  decode_timestamp_ = timestamp;
}

#if defined(__LB_SHELL__) || defined(COBALT)
StreamParserBuffer::StreamParserBuffer(uint8* data,
                                       int data_size,
                                       bool is_keyframe)
    : DecoderBuffer(data, data_size, is_keyframe),
      decode_timestamp_(kNoTimestamp()),
      config_id_(kInvalidConfigId) {
  SetDuration(kNoTimestamp());
}
#else
StreamParserBuffer::StreamParserBuffer(const uint8* data,
                                       int data_size,
                                       bool is_keyframe)
    : DecoderBuffer(data, data_size, is_keyframe),
      decode_timestamp_(kNoTimestamp()),
      config_id_(kInvalidConfigId) {
  SetDuration(kNoTimestamp());
}
#endif

StreamParserBuffer::~StreamParserBuffer() {
}

int StreamParserBuffer::GetConfigId() const {
  return config_id_;
}

void StreamParserBuffer::SetConfigId(int config_id) {
  config_id_ = config_id;
}

}  // namespace media
