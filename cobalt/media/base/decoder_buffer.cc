// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/decoder_buffer.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

DecoderBuffer::ScopedAllocatorPtr::ScopedAllocatorPtr(Allocator* allocator,
                                                      Type type, size_t size)
    : allocator_(allocator), type_(type), ptr_(NULL) {
  if (size > 0) {
    DCHECK(allocator_);
    ptr_ = static_cast<uint8_t*>(
        allocator_->Allocate(type_, size + kPaddingSize, kAlignmentSize));
    if (ptr_) {
      SbMemorySet(ptr_ + size, 0, kPaddingSize);
    }
  }
}

DecoderBuffer::ScopedAllocatorPtr::~ScopedAllocatorPtr() {
  // |allocator_| can be NULL for EOS buffer.
  if (allocator_ && ptr_) {
    allocator_->Free(type_, ptr_);
  }
}

DecoderBuffer::DecoderBuffer()
    : allocator_(NULL),
      type_(DemuxerStream::UNKNOWN),
      allocated_size_(0),
      size_(0),
      data_(NULL, DemuxerStream::UNKNOWN, 0),
      side_data_size_(0),
      side_data_(NULL, DemuxerStream::UNKNOWN, 0),
      splice_timestamp_(kNoTimestamp),
      is_key_frame_(false) {}

DecoderBuffer::DecoderBuffer(Allocator* allocator, Type type, size_t size)
    : allocator_(allocator),
      type_(type),
      allocated_size_(size),
      size_(size),
      data_(allocator_, type, size),
      side_data_size_(0),
      side_data_(allocator_, type, 0),
      splice_timestamp_(kNoTimestamp),
      is_key_frame_(false) {}

DecoderBuffer::DecoderBuffer(Allocator* allocator, Type type,
                             const uint8_t* data, size_t size,
                             const uint8_t* side_data, size_t side_data_size)
    : allocator_(allocator),
      type_(type),
      allocated_size_(size),
      size_(size),
      data_(allocator_, type, size),
      side_data_size_(side_data_size),
      side_data_(allocator_, type, side_data_size),
      splice_timestamp_(kNoTimestamp),
      is_key_frame_(false) {
  if (!data) {
    CHECK_EQ(size_, 0u);
    CHECK(!side_data);
    return;
  }

  if (has_data()) {
    SbMemoryCopy(data_.get(), data, size_);
  }

  if (!side_data) {
    CHECK_EQ(side_data_size, 0u);
    return;
  }

  if (has_side_data()) {
    DCHECK_GT(side_data_size_, 0u);
    SbMemoryCopy(side_data_.get(), side_data, side_data_size_);
  }
}

DecoderBuffer::~DecoderBuffer() {}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::Create(Allocator* allocator,
                                                   Type type, size_t size) {
  DCHECK_GT(size, 0);
  scoped_refptr<DecoderBuffer> decoder_buffer =
      new DecoderBuffer(allocator, type, size);
  if (decoder_buffer->has_data()) {
    return decoder_buffer;
  }
  return NULL;
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(Allocator* allocator,
                                                     Type type,
                                                     const uint8_t* data,
                                                     size_t data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  scoped_refptr<DecoderBuffer> decoder_buffer =
      new DecoderBuffer(allocator, type, data, data_size, NULL, 0);
  if (decoder_buffer->has_data()) {
    return decoder_buffer;
  }
  return NULL;
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(
    Allocator* allocator, Type type, const uint8_t* data, size_t data_size,
    const uint8_t* side_data, size_t side_data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  CHECK(side_data);
  scoped_refptr<DecoderBuffer> decoder_buffer = new DecoderBuffer(
      allocator, type, data, data_size, side_data, side_data_size);
  if (decoder_buffer->has_data() && decoder_buffer->has_side_data()) {
    return decoder_buffer;
  }
  return NULL;
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer() {
  return make_scoped_refptr(new DecoderBuffer);
}

const char* DecoderBuffer::GetTypeName() const {
  switch (type()) {
    case DemuxerStream::AUDIO:
      return "audio";
    case DemuxerStream::VIDEO:
      return "video";
    case DemuxerStream::TEXT:
      return "text";
    case DemuxerStream::UNKNOWN:
      return "unknown";
    case DemuxerStream::NUM_TYPES:
      // Fall-through to NOTREACHED().
      break;
  }
  NOTREACHED();
  return "";
}

std::string DecoderBuffer::AsHumanReadableString() {
  if (end_of_stream()) {
    return "end of stream";
  }

  std::ostringstream s;
  s << "type: " << GetTypeName()
    << " timestamp: " << timestamp_.InMicroseconds()
    << " duration: " << duration_.InMicroseconds() << " size: " << size_
    << " side_data_size: " << side_data_size_
    << " is_key_frame: " << is_key_frame_
    << " encrypted: " << (decrypt_config_ != NULL) << " discard_padding (ms): ("
    << discard_padding_.first.InMilliseconds() << ", "
    << discard_padding_.second.InMilliseconds() << ")";

  if (decrypt_config_) s << " decrypt:" << (*decrypt_config_);

  return s.str();
}

void DecoderBuffer::set_timestamp(base::TimeDelta timestamp) {
  DCHECK(!end_of_stream());
  timestamp_ = timestamp;
}

}  // namespace media
}  // namespace cobalt
