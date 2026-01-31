// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_H_
#define MEDIA_BASE_DECODER_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/demuxer_stream.h"
#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include "media/base/media_export.h"
#include "starboard/common/experimental/media_buffer_pool.h"
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)
#include "media/base/timestamp_constants.h"
#include "media/base/video_codecs.h"

namespace media {

// A specialized buffer for interfacing with audio / video decoders.
//
// Also includes decoder specific functionality for decryption.
//
// NOTE: It is illegal to call any method when end_of_stream() is true.
class MEDIA_EXPORT DecoderBuffer
    : public base::RefCountedThreadSafe<DecoderBuffer> {
 public:
  // ExternalMemory wraps a class owning a buffer and expose the data interface
  // through |span|. This class is derived by a class that owns the class owning
  // the buffer owner class. It is generally better to add the buffer class to
  // DecoderBuffer. ExternalMemory is for a class that cannot be added; for
  // instance, rtc::scoped_refptr<webrtc::EncodedImageBufferInterface>, webrtc
  // class cannot be included in //media/base.
  struct MEDIA_EXPORT ExternalMemory {
   public:
    explicit ExternalMemory(base::span<const uint8_t> span) : span_(span) {}
    virtual ~ExternalMemory() = default;
    const base::span<const uint8_t>& span() const { return span_; }

   protected:
    ExternalMemory() = default;
    base::span<const uint8_t> span_;
  };

  using DiscardPadding = std::pair<base::TimeDelta, base::TimeDelta>;

  struct MEDIA_EXPORT TimeInfo {
    TimeInfo();
    ~TimeInfo();
    TimeInfo(const TimeInfo&);
    TimeInfo& operator=(const TimeInfo&);

    // Presentation time of the frame.
    base::TimeDelta timestamp;

    // Presentation duration of the frame.
    base::TimeDelta duration;

    // Duration of (audio) samples from the beginning and end of this frame
    // which should be discarded after decoding. A value of kInfiniteDuration
    // for the first value indicates the entire frame should be discarded; the
    // second value must be base::TimeDelta() in this case.
    DiscardPadding discard_padding;
  };

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  class Allocator {
   public:
    // The class technically allocates opaque handles from the underlying memory
    // pool.  While these handles can sometimes be used as a pointer directly,
    // they may also be opaque handles that cannot be dereferenced, and can only
    // be written to using |Write|.
    // TODO(b/369245553): Currently only the surface functions below are using
    // Handle, and all the underlying Allocators are still using void* to avoid
    // massive changes.  Once this feature is proven to be working, we should
    // consider refactoring the underlying allocators.
    typedef intptr_t Handle;

    // This has to be 0 to be compatible with existing code checking for
    // nullptr.
    static constexpr Handle kInvalidHandle = 0;

    static void Set(Allocator* allocator);

    // The function should never return kInvalidHandle.  It may terminate the
    // app on allocation failure.
    virtual Handle Allocate(DemuxerStream::Type type, size_t size,
                            size_t alignment) = 0;
    virtual void Free(DemuxerStream::Type type, Handle handle, size_t size) = 0;
    virtual void Write(Handle handle, const void* data, size_t size) = 0;

    virtual int GetBufferAlignment() const = 0;
    virtual int GetBufferPadding() const = 0;
    virtual base::TimeDelta GetBufferGarbageCollectionDurationThreshold()
        const = 0;

    virtual void SetAllocateOnDemand(bool enabled) = 0;
    virtual void EnableMediaBufferPoolStrategy() = 0;

   protected:
    ~Allocator() {}
  };

  static void EnableAllocateOnDemand(bool enabled);
  static void EnableMediaBufferPoolStrategy();
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

  // Allocates buffer with |size| >= 0. |is_key_frame_| will default to false.
  explicit DecoderBuffer(size_t size);

  DecoderBuffer(const DecoderBuffer&) = delete;
  DecoderBuffer& operator=(const DecoderBuffer&) = delete;

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
      base::UnsafeSharedMemoryRegion region,
      uint64_t offset,
      size_t size);

  // Create a DecoderBuffer where data() of |size| bytes resides within the
  // ReadOnlySharedMemoryRegion referred to by |mapping| at non-negative offset
  // |offset|. The buffer's |is_key_frame_| will default to false.
  //
  // Ownership of |region| is transferred to the buffer.
  static scoped_refptr<DecoderBuffer> FromSharedMemoryRegion(
      base::ReadOnlySharedMemoryRegion region,
      uint64_t offset,
      size_t size);

  // Creates a DecoderBuffer with ExternalMemory. The buffer accessed through
  // the created DecoderBuffer is |span| of |external_memory||.
  // |external_memory| is owned by DecoderBuffer until it is destroyed.
  static scoped_refptr<DecoderBuffer> FromExternalMemory(
      std::unique_ptr<ExternalMemory> external_memory);

  // Create a DecoderBuffer indicating we've reached end of stream.
  //
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<DecoderBuffer> CreateEOSBuffer();

  // Method to verify if subsamples of a DecoderBuffer match.
  static bool DoSubsamplesMatch(const DecoderBuffer& encrypted);

  const TimeInfo& time_info() const {
    DCHECK(!end_of_stream());
    return time_info_;
  }

  base::TimeDelta timestamp() const {
    DCHECK(!end_of_stream());
    return time_info_.timestamp;
  }

  // TODO(dalecurtis): This should be renamed at some point, but to avoid a yak
  // shave keep as a virtual with hacker_style() for now.
  virtual void set_timestamp(base::TimeDelta timestamp);

  base::TimeDelta duration() const {
    DCHECK(!end_of_stream());
    return time_info_.duration;
  }

  void set_duration(base::TimeDelta duration) {
    DCHECK(!end_of_stream());
    DCHECK(duration == kNoTimestamp ||
           (duration >= base::TimeDelta() && duration != kInfiniteDuration))
        << duration.InSecondsF();
    time_info_.duration = duration;
  }

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  Allocator::Handle handle() const {
    return allocator_data_.handle;
  }
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)

  const uint8_t* data() const {
    DCHECK(!end_of_stream());
#if BUILDFLAG(USE_STARBOARD_MEDIA)
    // The function is used by unit tests and Chromium media stack, so we keep
    // it but CHECK() when the handle is annotated (e.g. cannot be converted
    // to a pointer).
#if !defined(OFFICIAL_BUILD)
    using starboard::common::experimental::IsPointerAnnotated;
    CHECK(!IsPointerAnnotated(allocator_data_.handle));
#endif  // !defined(OFFICIAL_BUILD)
    return reinterpret_cast<const uint8_t*>(allocator_data_.handle);
#else   // BUILDFLAG(USE_STARBOARD_MEDIA)
    if (read_only_mapping_.IsValid())
      return read_only_mapping_.GetMemoryAs<const uint8_t>();
    if (writable_mapping_.IsValid())
      return writable_mapping_.GetMemoryAs<const uint8_t>();
    if (external_memory_)
      return external_memory_->span().data();
    return data_.get();
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)
  }

  // TODO(sandersd): Remove writable_data(). https://crbug.com/834088
  uint8_t* writable_data() const {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
    // The function is used by unit tests and Chromium media stack, so we keep
    // it but CHECK() when the handle is annotated (e.g. cannot be converted
    // to a pointer).
#if !defined(OFFICIAL_BUILD)
    using starboard::common::experimental::IsPointerAnnotated;
    CHECK(!IsPointerAnnotated(allocator_data_.handle));
#endif  // !defined(OFFICIAL_BUILD)
    return reinterpret_cast<uint8_t*>(allocator_data_.handle);
#else   // BUILDFLAG(USE_STARBOARD_MEDIA)
    DCHECK(!end_of_stream());
    DCHECK(!read_only_mapping_.IsValid());
    DCHECK(!writable_mapping_.IsValid());
    DCHECK(!external_memory_);
    return data_.get();
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
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

  const DiscardPadding& discard_padding() const {
    DCHECK(!end_of_stream());
    return time_info_.discard_padding;
  }

  void set_discard_padding(const DiscardPadding& discard_padding) {
    DCHECK(!end_of_stream());
    time_info_.discard_padding = discard_padding;
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
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  void shrink_to(size_t size) {
    DCHECK_LE(size, size_);
    size_ = size;
  }
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

  bool end_of_stream() const {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
    return allocator_data_.handle == Allocator::kInvalidHandle;
#else   // BUILDFLAG(USE_STARBOARD_MEDIA)
    return !read_only_mapping_.IsValid() && !writable_mapping_.IsValid() &&
           !external_memory_ && !data_;
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
  }

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

  // As above, except that |data_| and |side_data_| are not compared.
  bool MatchesMetadataForTesting(const DecoderBuffer& buffer) const;

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString(bool verbose = false) const;

  // Replaces any existing side data with data copied from |side_data|.
  void CopySideDataFrom(const uint8_t* side_data, size_t side_data_size);

  // Returns total memory usage for both bookkeeping and buffered data. The
  // function is added for more accurately memory management.
  virtual size_t GetMemoryUsage() const;

 protected:
  friend class base::RefCountedThreadSafe<DecoderBuffer>;

  // Allocates a buffer of size |size| >= 0 and copies |data| into it. If |data|
  // is NULL then |data_| is set to NULL and |buffer_size_| to 0.
  // |is_key_frame_| will default to false.
  DecoderBuffer(const uint8_t* data,
                size_t size,
                const uint8_t* side_data,
                size_t side_data_size);
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  DecoderBuffer(DemuxerStream::Type type,
                const uint8_t* data,
                size_t size,
                const uint8_t* side_data,
                size_t side_data_size);
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)

  DecoderBuffer(std::unique_ptr<uint8_t[]> data, size_t size);

  DecoderBuffer(base::ReadOnlySharedMemoryMapping mapping, size_t size);

  DecoderBuffer(base::WritableSharedMemoryMapping mapping, size_t size);

  explicit DecoderBuffer(std::unique_ptr<ExternalMemory> external_memory);

  virtual ~DecoderBuffer();

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  struct AllocatorData {
    DemuxerStream::Type stream_type_ = DemuxerStream::UNKNOWN;
    Allocator::Handle handle = Allocator::kInvalidHandle;
    size_t size = 0;
  };
  // Encoded data, allocated from DecoderBuffer::Allocator.
  AllocatorData allocator_data_;
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
  // Encoded data, if it is stored on the heap.
  std::unique_ptr<uint8_t[]> data_;

 private:
  TimeInfo time_info_;

  // Size of the encoded data.
  size_t size_;

  // Side data. Used for alpha channel in VPx, and for text cues.
  size_t side_data_size_ = 0;
  std::unique_ptr<uint8_t[]> side_data_;

  // Encoded data, if it is stored in a read-only shared memory mapping.
  base::ReadOnlySharedMemoryMapping read_only_mapping_;

  // Encoded data, if it is stored in a writable shared memory mapping.
  base::WritableSharedMemoryMapping writable_mapping_;

  std::unique_ptr<ExternalMemory> external_memory_;

  // Encryption parameters for the encoded data.
  std::unique_ptr<DecryptConfig> decrypt_config_;

  // Whether the frame was marked as a keyframe in the container.
  bool is_key_frame_ = false;

  // Constructor helper method for memory allocations.
  void Initialize();
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  void Initialize(DemuxerStream::Type type);
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_H_
