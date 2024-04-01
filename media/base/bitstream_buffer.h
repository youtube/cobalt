// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BITSTREAM_BUFFER_H_
#define MEDIA_BASE_BITSTREAM_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"

namespace IPC {
template <class P>
struct ParamTraits;
}

namespace media {

// Class for passing bitstream buffers around.  Does not take ownership of the
// data.  This is the media-namespace equivalent of PP_VideoBitstreamBuffer_Dev.
class MEDIA_EXPORT BitstreamBuffer {
 public:
  BitstreamBuffer();

  // Constructs a new BitstreamBuffer. The content of the bitstream is located
  // at |offset| bytes away from the start of the shared memory and the payload
  // is |size| bytes. When not provided, the default value for |offset| is 0.
  // |presentation_timestamp| is when the decoded frame should be displayed.
  // When not provided, |presentation_timestamp| will be
  // |media::kNoTimestamp|.
  BitstreamBuffer(int32_t id,
                  base::subtle::PlatformSharedMemoryRegion region,
                  size_t size,
                  off_t offset = 0,
                  base::TimeDelta presentation_timestamp = kNoTimestamp);

  // As above, creating by unwrapping a base::UnsafeSharedMemoryRegion.
  BitstreamBuffer(int32_t id,
                  base::UnsafeSharedMemoryRegion region,
                  size_t size,
                  off_t offset = 0,
                  base::TimeDelta presentation_timestamp = kNoTimestamp);

  // Move operations are allowed.
  BitstreamBuffer(BitstreamBuffer&&);
  BitstreamBuffer& operator=(BitstreamBuffer&&);

  BitstreamBuffer(const BitstreamBuffer&) = delete;
  BitstreamBuffer& operator=(const BitstreamBuffer&) = delete;

  ~BitstreamBuffer();

  // Produce an equivalent DecoderBuffer. This consumes region(), even if
  // nullptr is returned.
  //
  // This method is only intended to be used by VDAs that are being converted to
  // use DecoderBuffer.
  //
  // TODO(sandersd): Remove once all VDAs are converted.
  scoped_refptr<DecoderBuffer> ToDecoderBuffer();

  // TODO(crbug.com/813845): As this is only used by Android, include
  // EncryptionScheme and optional EncryptionPattern when updating for Android.
  void SetDecryptionSettings(const std::string& key_id,
                             const std::string& iv,
                             const std::vector<SubsampleEntry>& subsamples);

  // Taking the region invalides the one in this BitstreamBuffer.
  base::subtle::PlatformSharedMemoryRegion TakeRegion() {
    return std::move(region_);
  }

  // If a region needs to be taken from a const BitstreamBuffer, it must be
  // duplicated. This function makes that explicit.
  // TODO(crbug.com/793446): this is probably only needed by legacy IPC, and can
  // be removed once that is converted to the new shared memory API.
  base::subtle::PlatformSharedMemoryRegion DuplicateRegion() const {
    return region_.Duplicate();
  }

  const base::subtle::PlatformSharedMemoryRegion& region() const {
    return region_;
  }

  int32_t id() const { return id_; }

  // The number of bytes of the actual bitstream data. It is the size of the
  // content instead of the whole shared memory.
  size_t size() const { return size_; }

  // The offset to the start of actual bitstream data in the shared memory.
  off_t offset() const { return offset_; }

  // The timestamp is only valid if it's not equal to |media::kNoTimestamp|.
  base::TimeDelta presentation_timestamp() const {
    return presentation_timestamp_;
  }

  void set_region(base::subtle::PlatformSharedMemoryRegion region) {
    region_ = std::move(region);
  }

  // The following methods come from SetDecryptionSettings().
  const std::string& key_id() const { return key_id_; }
  const std::string& iv() const { return iv_; }
  const std::vector<SubsampleEntry>& subsamples() const { return subsamples_; }

 private:
  int32_t id_;
  base::subtle::PlatformSharedMemoryRegion region_;
  size_t size_;
  off_t offset_;

  // Note: Not set by all clients.
  base::TimeDelta presentation_timestamp_;

  // Note that BitstreamBuffer uses the settings in Audio/VideoDecoderConfig
  // to determine the encryption mode and pattern (if required by the encryption
  // scheme).
  // TODO(timav): Try to DISALLOW_COPY_AND_ASSIGN and include these params as
  // std::unique_ptr<DecryptConfig> or explain why copy & assign is needed.
  std::string key_id_;                      // key ID.
  std::string iv_;                          // initialization vector
  std::vector<SubsampleEntry> subsamples_;  // clear/cypher sizes

  friend struct IPC::ParamTraits<media::BitstreamBuffer>;
};

}  // namespace media

#endif  // MEDIA_BASE_BITSTREAM_BUFFER_H_
