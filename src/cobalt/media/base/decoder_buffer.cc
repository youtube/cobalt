// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/decoder_buffer.h"

#include <vector>

#include "cobalt/build/build_config.h"
#include "starboard/media.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {
namespace {
SbMediaType DemuxerStreamTypeToSbMediaType(DemuxerStream::Type type) {
  switch (type) {
    case DemuxerStream::AUDIO:
      return kSbMediaTypeAudio;
    case DemuxerStream::VIDEO:
      return kSbMediaTypeVideo;
    case DemuxerStream::UNKNOWN:
    case DemuxerStream::TEXT:
    case DemuxerStream::NUM_TYPES:
      break;
  }
  NOTREACHED();
  return kSbMediaTypeAudio;
}
}  // namespace

DecoderBuffer::ScopedAllocatorPtr::ScopedAllocatorPtr(Allocator* allocator,
                                                      Type type, size_t size)
    : allocator_(allocator), type_(type) {
  if (size > 0) {
    DCHECK(allocator_);
#if SB_API_VERSION >= 10
    int padding = SbMediaGetBufferPadding(DemuxerStreamTypeToSbMediaType(type));
    int alignment =
        SbMediaGetBufferAlignment(DemuxerStreamTypeToSbMediaType(type));
#else   // SB_API_VERSION >= 10
    int padding = COBALT_MEDIA_BUFFER_PADDING;
    int alignment = COBALT_MEDIA_BUFFER_ALIGNMENT;
#endif  // SB_API_VERSION >= 10
    allocations_ = allocator_->Allocate(size + padding, alignment,
                                        static_cast<intptr_t>(type));
    static bool logged_warning = false;
    if (padding > 0) {
      if (allocations_.number_of_buffers() > 0) {
        const int kMaxPadding = 1024;
        if (padding > kMaxPadding) {
          if (!logged_warning) {
            LOG(WARNING) << "Media buffer padding is larger than "
                         << kMaxPadding
                         << ", this will cause extra allocations.";
            logged_warning = true;
          }
          std::vector<char> zeros(padding + 1, 0);
          allocations_.Write(size, zeros.data(), padding);
        } else {
          char zeros[kMaxPadding + 1] = {0};
          allocations_.Write(size, zeros, padding);
        }
        allocations_.ShrinkTo(size);
      }
    }
  }
}

DecoderBuffer::ScopedAllocatorPtr::~ScopedAllocatorPtr() {
  // |allocator_| can be NULL for EOS buffer.
  if (allocator_) {
    allocator_->Free(allocations_);
  }
}

DecoderBuffer::DecoderBuffer()
    : data_(NULL, DemuxerStream::UNKNOWN, 0),
      splice_timestamp_(kNoTimestamp),
      is_key_frame_(false) {}

DecoderBuffer::DecoderBuffer(Allocator* allocator, Type type, size_t size)
    : data_(allocator, type, size),
      splice_timestamp_(kNoTimestamp),
      is_key_frame_(false) {}

DecoderBuffer::DecoderBuffer(Allocator* allocator, Type type,
                             const uint8_t* data, size_t size)
    : data_(allocator, type, size),
      splice_timestamp_(kNoTimestamp),
      is_key_frame_(false) {
  if (!data) {
    CHECK_EQ(size, 0u);
    return;
  }
  if (allocations().number_of_buffers()) {
    allocations().Write(0, data, size);
  }
}

DecoderBuffer::DecoderBuffer(Allocator* allocator, Type type,
                             Allocator::Allocations allocations)
    : data_(allocator, type, allocations.size()),
      splice_timestamp_(kNoTimestamp),
      is_key_frame_(false) {
  int offset = 0;
  for (int i = 0; i < allocations.number_of_buffers(); ++i) {
    this->allocations().Write(offset, allocations.buffers()[i],
                              allocations.buffer_sizes()[i]);
    offset += allocations.buffer_sizes()[i];
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
      new DecoderBuffer(allocator, type, data, data_size);
  if (decoder_buffer->has_data()) {
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
    << " duration: " << duration_.InMicroseconds() << " size: " << data_size()
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
