// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_H_
#define MEDIA_BASE_DECODER_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"
#if defined(STARBOARD)
#include "starboard/media.h"
#else  // defined(STARBOARD)
#include "media/base/unaligned_shared_memory.h"
#endif  // defined(STARBOARD)

namespace media {

// A specialized buffer for interfacing with audio / video decoders.
//
// Also includes decoder specific functionality for decryption.
//
// NOTE: It is illegal to call any method when end_of_stream() is true.
class MEDIA_EXPORT DecoderBuffer
    : public base::RefCountedThreadSafe<DecoderBuffer> {
 public:
  enum {
    kPaddingSize = 64,
#if defined(ARCH_CPU_ARM_FAMILY)
    kAlignmentSize = 16
#else
    kAlignmentSize = 32
#endif
  };

#if defined(STARBOARD)
  class Allocator {
   public:
    static Allocator* GetInstance();

    // The function should never return nullptr.  It may terminate the app on
    // allocation failure.
    virtual void* Allocate(size_t size, size_t alignment) = 0;
    virtual void Free(void* p, size_t size) = 0;

    virtual int GetAudioBufferBudget() const = 0;
    virtual int GetBufferAlignment() const = 0;
    virtual int GetBufferPadding() const = 0;
    virtual base::TimeDelta GetBufferGarbageCollectionDurationThreshold() const = 0;
    virtual int GetProgressiveBufferBudget(SbMediaVideoCodec codec,
                                           int resolution_width,
                                           int resolution_height,
                                           int bits_per_pixel) const = 0;
    virtual int GetVideoBufferBudget(SbMediaVideoCodec codec,
                                     int resolution_width,
                                     int resolution_height,
                                     int bits_per_pixel) const = 0;

   protected:
    ~Allocator() {}

    static void Set(Allocator* allocator);
  };
#endif  // defined(STARBOARD)

  // Allocates buffer with |size| >= 0. |is_key_frame_| will default to false.
  explicit DecoderBuffer(size_t size);

  // Create a DecoderBuffer whose |data_| is copied from |data|. |data| must not
  // be NULL and |size| >= 0. The buffer's |is_key_frame_| will default to
  // false.
  static scoped_refptr<DecoderBuffer> CopyFrom(const uint8_t* data,
                                               size_t size);

  // Create a DecoderBuffer whose |data_| is copied from |data| and |side_data_|
  // is copied from |side_data|. Data pointers must not be NULL and sizes must
  // be >= 0. The buffer's |is_key_frame_| will default to false.
  static scoped_refptr<DecoderBuffer> CopyFrom(const uint8_t* data,
                                               size_t size,
                                               const uint8_t* side_data,
                                               size_t side_data_size);

#if !defined(STARBOARD)
  // Create a DecoderBuffer where data() of |size| bytes resides within the heap
  // as byte array. The buffer's |is_key_frame_| will default to false.
  //
  // Ownership of |data| is transferred to the buffer.
  static scoped_refptr<DecoderBuffer> FromArray(std::unique_ptr<uint8_t[]> data,
                                                size_t size);

  // Create a DecoderBuffer where data() of |size| bytes resides within the
  // memory referred to by |region| at non-negative offset |offset|. The
  // buffer's |is_key_frame_| will default to false.
  //
  // The shared memory will be mapped read-only.
  //
  // If mapping fails, nullptr will be returned.
  static scoped_refptr<DecoderBuffer> FromSharedMemoryRegion(
      base::subtle::PlatformSharedMemoryRegion region,
      off_t offset,
      size_t size);

  // Create a DecoderBuffer where data() of |size| bytes resides within the
  // ReadOnlySharedMemoryRegion referred to by |mapping| at non-negative offset
  // |offset|. The buffer's |is_key_frame_| will default to false.
  //
  // Ownership of |region| is transferred to the buffer.
  static scoped_refptr<DecoderBuffer> FromSharedMemoryRegion(
      base::ReadOnlySharedMemoryRegion region,
      off_t offset,
      size_t size);
#endif  // !defined(STARBOARD)

  // Create a DecoderBuffer indicating we've reached end of stream.
  //
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<DecoderBuffer> CreateEOSBuffer();

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

  const uint8_t* data() const {
    DCHECK(!end_of_stream());

#if defined(STARBOARD)
    return data_;
#else  // defined(STARBOARD)
    if (shared_mem_mapping_ && shared_mem_mapping_->IsValid())
      return static_cast<const uint8_t*>(shared_mem_mapping_->memory());
    if (shm_)
      return static_cast<uint8_t*>(shm_->memory());
    return data_.get();
#endif  // defined(STARBOARD)
  }

  // TODO(sandersd): Remove writable_data(). https://crbug.com/834088
  uint8_t* writable_data() const {
    DCHECK(!end_of_stream());

#if defined(STARBOARD)
    return data_;
#else  // defined(STARBOARD)
    DCHECK(!shm_);
    DCHECK(!shared_mem_mapping_);
    return data_.get();
#endif  // defined(STARBOARD)
  }

  size_t data_size() const {
    DCHECK(!end_of_stream());
    return size_;
  }

  const uint8_t* side_data() const {
    DCHECK(!end_of_stream());
    return side_data_.get();
  }

  size_t side_data_size() const {
    DCHECK(!end_of_stream());
    return side_data_size_;
  }

  typedef std::pair<base::TimeDelta, base::TimeDelta> DiscardPadding;
  const DiscardPadding& discard_padding() const {
    DCHECK(!end_of_stream());
    return discard_padding_;
  }

  void set_discard_padding(const DiscardPadding& discard_padding) {
    DCHECK(!end_of_stream());
    discard_padding_ = discard_padding;
  }

  // Returns DecryptConfig associated with |this|. Returns null iff |this| is
  // not encrypted.
  const DecryptConfig* decrypt_config() const {
    DCHECK(!end_of_stream());
    return decrypt_config_.get();
  }

  void set_decrypt_config(std::unique_ptr<DecryptConfig> decrypt_config) {
    DCHECK(!end_of_stream());
    decrypt_config_ = std::move(decrypt_config);
  }

  // If there's no data in this buffer, it represents end of stream.
#if defined(STARBOARD)
  bool end_of_stream() const { return !data_; }
  void shrink_to(size_t size) { DCHECK_LE(size, size_); size_ = size; }
#else  // defined(STARBOARD)
  bool end_of_stream() const { return !shared_mem_mapping_ && !shm_ && !data_; }
#endif  // defined(STARBOARD)

  bool is_key_frame() const {
    DCHECK(!end_of_stream());
    return is_key_frame_;
  }

  void set_is_key_frame(bool is_key_frame) {
    DCHECK(!end_of_stream());
    is_key_frame_ = is_key_frame;
  }

  // Returns true if all fields in |buffer| matches this buffer
  // including |data_| and |side_data_|.
  bool MatchesForTesting(const DecoderBuffer& buffer) const;

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString(bool verbose = false) const;

  // Replaces any existing side data with data copied from |side_data|.
  void CopySideDataFrom(const uint8_t* side_data, size_t side_data_size);

 protected:
  friend class base::RefCountedThreadSafe<DecoderBuffer>;

  // Allocates a buffer of size |size| >= 0 and copies |data| into it. If |data|
  // is NULL then |data_| is set to NULL and |buffer_size_| to 0.
  // |is_key_frame_| will default to false.
  DecoderBuffer(const uint8_t* data,
                size_t size,
                const uint8_t* side_data,
                size_t side_data_size);

#if !defined(STARBOARD)
  DecoderBuffer(std::unique_ptr<uint8_t[]> data, size_t size);

  DecoderBuffer(std::unique_ptr<UnalignedSharedMemory> shm, size_t size);

  DecoderBuffer(std::unique_ptr<ReadOnlyUnalignedMapping> shared_mem_mapping,
                size_t size);
#endif  // !defined(STARBOARD)

  virtual ~DecoderBuffer();

#if defined(STARBOARD)
  // Encoded data, allocated from DecoderBuffer::Allocator.
  uint8_t* data_ = nullptr;
  size_t allocated_size_ = 0;
#else  // defined(STARBOARD)
  // Encoded data, if it is stored on the heap.
  std::unique_ptr<uint8_t[]> data_;
#endif  // defined(STARBOARD)

 private:
  // Presentation time of the frame.
  base::TimeDelta timestamp_;
  // Presentation duration of the frame.
  base::TimeDelta duration_;

  // Size of the encoded data.
  size_t size_;

  // Side data. Used for alpha channel in VPx, and for text cues.
  size_t side_data_size_;
  std::unique_ptr<uint8_t[]> side_data_;

#if !defined(STARBOARD)
  // Encoded data, if it is stored in a shared memory mapping.
  std::unique_ptr<ReadOnlyUnalignedMapping> shared_mem_mapping_;

  // Encoded data, if it is stored in SHM.
  std::unique_ptr<UnalignedSharedMemory> shm_;
#endif  // !defined(STARBOARD)

  // Encryption parameters for the encoded data.
  std::unique_ptr<DecryptConfig> decrypt_config_;

  // Duration of (audio) samples from the beginning and end of this frame which
  // should be discarded after decoding. A value of kInfiniteDuration for the
  // first value indicates the entire frame should be discarded; the second
  // value must be base::TimeDelta() in this case.
  DiscardPadding discard_padding_;

  // Whether the frame was marked as a keyframe in the container.
  bool is_key_frame_;

  // Constructor helper method for memory allocations.
  void Initialize();

  DISALLOW_COPY_AND_ASSIGN(DecoderBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_H_
