// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/stream_parser_buffer.h"

#include "base/logging.h"

namespace media {

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CreateEOSBuffer() {
  return make_scoped_refptr(new StreamParserBuffer(NULL, 0, false));
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CopyFrom(
    const uint8* data, int data_size, bool is_keyframe) {
  return make_scoped_refptr(
      new StreamParserBuffer(data, data_size, is_keyframe));
}

base::TimeDelta StreamParserBuffer::GetDecodeTimestamp() const {
  if (decode_timestamp_ == kNoTimestamp())
    return GetTimestamp();
  return decode_timestamp_;
}

void StreamParserBuffer::SetDecodeTimestamp(const base::TimeDelta& timestamp) {
  decode_timestamp_ = timestamp;
}

base::TimeDelta StreamParserBuffer::GetEndTimestamp() const {
  DCHECK(GetDecodeTimestamp() != kNoTimestamp());
  DCHECK(GetDuration() != kNoTimestamp());
  return GetDecodeTimestamp() + GetDuration();
}

StreamParserBuffer::StreamParserBuffer(const uint8* data, int data_size,
                                       bool is_keyframe)
    : DecoderBuffer(data, data_size),
      is_keyframe_(is_keyframe),
      decode_timestamp_(kNoTimestamp()) {
  SetDuration(kNoTimestamp());
}


StreamParserBuffer::~StreamParserBuffer() {
}

}  // namespace media
