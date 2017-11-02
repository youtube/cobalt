// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_DECODER_BUFFER_H_
#define COBALT_MEDIA_BASE_DECODER_BUFFER_H_

#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
#include "cobalt/media/base/decrypt_config.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/timestamp_constants.h"
#include "nb/multipart_allocator.h"
#include "starboard/memory.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// A specialized buffer for interfacing with audio / video decoders.
//
// Specifically ensures that data is aligned and padded as necessary by the
// underlying decoding framework.  On desktop platforms this means memory is
// allocated using FFmpeg with particular alignment and padding requirements.
//
// Also includes decoder specific functionality for decryption.
//
// NOTE: It is illegal to call any method when end_of_stream() is true.
class MEDIA_EXPORT DecoderBuffer
    : public base::RefCountedThreadSafe<DecoderBuffer> {
 public:
  typedef DemuxerStream::Type Type;
  class Allocator : public nb::MultipartAllocator {
   public:
    // Update the video config so that constraints are appropriate for the
    // current resolution.
    virtual void UpdateVideoConfig(const VideoDecoderConfig& config) = 0;
  };

  // Create a DecoderBuffer whose |data_| points to a memory with at least
  // |size| bytes.  Buffer will be padded and aligned as necessary.
  // The buffer's |is_key_frame_| will default to false.
  static scoped_refptr<DecoderBuffer> Create(Allocator* allocator, Type type,
                                             size_t size);

  // Create a DecoderBuffer whose |data_| is copied from |data|.  Buffer will be
  // padded and aligned as necessary.  |data| must not be NULL and |size| >= 0.
  // The buffer's |is_key_frame_| will default to false.
  static scoped_refptr<DecoderBuffer> CopyFrom(Allocator* allocator, Type type,
                                               const uint8_t* data,
                                               size_t size);

  // Create a DecoderBuffer indicating we've reached end of stream.
  //
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<DecoderBuffer> CreateEOSBuffer();

  // Returns the allocator.  This is usually used when creating a copy of the
  // buffer.
  Allocator* allocator() const { return data_.allocator(); }

  // Gets the parser's media type associated with this buffer. Value is
  // meaningless for EOS buffers.
  Type type() const { return data_.type(); }
  const char* GetTypeName() const;

  base::TimeDelta timestamp() const {
    DCHECK(!end_of_stream());
    return timestamp_;
  }

  // TODO(dalecurtis): This should be renamed at some point, but to avoid a yak
  // shave keep as a virtual with hacker_style() for now.
  virtual void set_timestamp(base::TimeDelta timestamp);

  base::TimeDelta duration() const {
    DCHECK(!end_of_stream());
    return duration_;
  }

  void set_duration(base::TimeDelta duration) {
    DCHECK(!end_of_stream());
    DCHECK(duration == kNoTimestamp ||
           (duration >= base::TimeDelta() && duration != kInfiniteDuration))
        << duration.InSecondsF();
    duration_ = duration;
  }

  bool has_data() const { return allocations().number_of_buffers() > 0; }

  const Allocator::Allocations& allocations() const {
    return data_.allocations();
  }
  Allocator::Allocations& allocations() { return data_.allocations(); }

  size_t data_size() const {
    DCHECK(!end_of_stream());
    return data_.allocations().size();
  }

  void shrink_to(size_t size) {
    DCHECK_GE(static_cast<int>(size), 0);
    allocations().ShrinkTo(static_cast<int>(size));
  }

  // A discard window indicates the amount of data which should be discard from
  // this buffer after decoding.  The first value is the amount of the front and
  // the second the amount off the back.  A value of kInfiniteDuration for the
  // first value indicates the entire buffer should be discarded; the second
  // value must be base::TimeDelta() in this case.
  typedef std::pair<base::TimeDelta, base::TimeDelta> DiscardPadding;
  const DiscardPadding& discard_padding() const {
    DCHECK(!end_of_stream());
    return discard_padding_;
  }

  void set_discard_padding(const DiscardPadding& discard_padding) {
    DCHECK(!end_of_stream());
    discard_padding_ = discard_padding;
  }

  const DecryptConfig* decrypt_config() const {
    DCHECK(!end_of_stream());
    return decrypt_config_.get();
  }

  void set_decrypt_config(scoped_ptr<DecryptConfig> decrypt_config) {
    DCHECK(!end_of_stream());
    decrypt_config_ = decrypt_config.Pass();
  }

  // If there's no data in this buffer, it represents end of stream.
  bool end_of_stream() const { return !has_data(); }

  // Indicates this buffer is part of a splice around |splice_timestamp_|.
  // Returns kNoTimestamp if the buffer is not part of a splice.
  base::TimeDelta splice_timestamp() const {
    DCHECK(!end_of_stream());
    return splice_timestamp_;
  }

  // When set to anything but kNoTimestamp indicates this buffer is part of a
  // splice around |splice_timestamp|.
  void set_splice_timestamp(base::TimeDelta splice_timestamp) {
    DCHECK(!end_of_stream());
    splice_timestamp_ = splice_timestamp;
  }

  bool is_key_frame() const {
    DCHECK(!end_of_stream());
    return is_key_frame_;
  }

  void set_is_key_frame(bool is_key_frame) {
    DCHECK(!end_of_stream());
    is_key_frame_ = is_key_frame;
  }

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString();

 protected:
  friend class base::RefCountedThreadSafe<DecoderBuffer>;

  // The default ctor creates an EOS buffer without specific stream type.
  DecoderBuffer();

  // Allocates buffer with |size| >= 0.  Buffer will be padded and aligned
  // as necessary, and |is_key_frame_| will default to false.
  DecoderBuffer(Allocator* allocator, Type type, size_t size);

  // Allocates a buffer of size |size| >= 0 and copies |data| into it.  Buffer
  // will be padded and aligned as necessary.  If |data| is NULL then |data_| is
  // set to NULL and |buffer_size_| to 0.  |is_key_frame_| will default to
  // false.
  DecoderBuffer(Allocator* allocator, Type type, const uint8_t* data,
                size_t size);

  // Allocates a buffer to copy the data in |allocations|.  Buffer will be
  // padded and aligned as necessary.  |is_key_frame_| will default to false.
  DecoderBuffer(Allocator* allocator, Type type,
                Allocator::Allocations allocations);

  virtual ~DecoderBuffer();

 private:
  class ScopedAllocatorPtr {
   public:
    ScopedAllocatorPtr(Allocator* allocator, Type type, size_t size);
    ~ScopedAllocatorPtr();

    const Allocator::Allocations& allocations() const { return allocations_; }
    Allocator::Allocations& allocations() { return allocations_; }

    Allocator* allocator() const { return allocator_; }
    Type type() const { return type_; }

   private:
    Allocator* allocator_;
    Type type_;
    Allocator::Allocations allocations_;

    DISALLOW_COPY_AND_ASSIGN(ScopedAllocatorPtr);
  };

  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  ScopedAllocatorPtr data_;
  scoped_ptr<DecryptConfig> decrypt_config_;
  DiscardPadding discard_padding_;
  base::TimeDelta splice_timestamp_;
  bool is_key_frame_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBuffer);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_DECODER_BUFFER_H_
