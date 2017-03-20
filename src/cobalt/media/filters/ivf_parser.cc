// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "cobalt/media/filters/ivf_parser.h"

namespace media {

void IvfFileHeader::ByteSwap() {
  version = base::ByteSwapToLE16(version);
  header_size = base::ByteSwapToLE16(header_size);
  fourcc = base::ByteSwapToLE32(fourcc);
  width = base::ByteSwapToLE16(width);
  height = base::ByteSwapToLE16(height);
  timebase_denum = base::ByteSwapToLE32(timebase_denum);
  timebase_num = base::ByteSwapToLE32(timebase_num);
  num_frames = base::ByteSwapToLE32(num_frames);
  unused = base::ByteSwapToLE32(unused);
}

void IvfFrameHeader::ByteSwap() {
  frame_size = base::ByteSwapToLE32(frame_size);
  timestamp = base::ByteSwapToLE64(timestamp);
}

IvfParser::IvfParser() : ptr_(NULL), end_(NULL) {}

bool IvfParser::Initialize(const uint8_t* stream, size_t size,
                           IvfFileHeader* file_header) {
  DCHECK(stream);
  DCHECK(file_header);
  ptr_ = stream;
  end_ = stream + size;

  if (size < sizeof(IvfFileHeader)) {
    DLOG(ERROR) << "EOF before file header";
    return false;
  }

  memcpy(file_header, ptr_, sizeof(IvfFileHeader));
  file_header->ByteSwap();

  if (memcmp(file_header->signature, kIvfHeaderSignature,
             sizeof(file_header->signature)) != 0) {
    DLOG(ERROR) << "IVF signature mismatch";
    return false;
  }
  DLOG_IF(WARNING, file_header->version != 0)
      << "IVF version unknown: " << file_header->version
      << ", the parser may not be able to parse correctly";
  if (file_header->header_size != sizeof(IvfFileHeader)) {
    DLOG(ERROR) << "IVF file header size mismatch";
    return false;
  }

  ptr_ += sizeof(IvfFileHeader);

  return true;
}

bool IvfParser::ParseNextFrame(IvfFrameHeader* frame_header,
                               const uint8_t** payload) {
  DCHECK(ptr_);
  DCHECK(payload);

  if (end_ < ptr_ + sizeof(IvfFrameHeader)) {
    DLOG_IF(ERROR, ptr_ != end_) << "Incomplete frame header";
    return false;
  }

  memcpy(frame_header, ptr_, sizeof(IvfFrameHeader));
  frame_header->ByteSwap();
  ptr_ += sizeof(IvfFrameHeader);

  if (end_ < ptr_ + frame_header->frame_size) {
    DLOG(ERROR) << "Not enough frame data";
    return false;
  }

  *payload = ptr_;
  ptr_ += frame_header->frame_size;

  return true;
}

}  // namespace media
