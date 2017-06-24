// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/starboard/player/input_buffer_internal.h"

#include <vector>

#include "starboard/atomic.h"
#include "starboard/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

class InputBuffer::ReferenceCountedBuffer {
 public:
  ReferenceCountedBuffer(SbMediaType sample_type,
                         SbPlayerDeallocateSampleFunc deallocate_sample_func,
                         SbPlayer player,
                         void* context,
                         const void* sample_buffer,
                         int sample_buffer_size,
                         SbMediaTime sample_pts,
                         const SbMediaVideoSampleInfo* video_sample_info,
                         const SbDrmSampleInfo* sample_drm_info)
      : ref_count_(0),
        sample_type_(sample_type),
        deallocate_sample_func_(deallocate_sample_func),
        player_(player),
        context_(context),
        data_(static_cast<const uint8_t*>(sample_buffer)),
        size_(sample_buffer_size),
        pts_(sample_pts),
        has_video_sample_info_(video_sample_info != NULL),
        has_drm_info_(sample_drm_info != NULL) {
    SB_DCHECK(deallocate_sample_func);
    if (has_video_sample_info_) {
      video_sample_info_ = *video_sample_info;
    }
    if (has_drm_info_) {
      SB_DCHECK(sample_drm_info->subsample_count > 0);

      subsamples_.assign(sample_drm_info->subsample_mapping,
                         sample_drm_info->subsample_mapping +
                             sample_drm_info->subsample_count);
      drm_info_ = *sample_drm_info;
      drm_info_.subsample_mapping =
          subsamples_.empty() ? NULL : &subsamples_[0];
    }
  }

  void AddRef() const { SbAtomicBarrier_Increment(&ref_count_, 1); }
  void Release() const {
    if (SbAtomicBarrier_Increment(&ref_count_, -1) == 0) {
      delete this;
    }
  }

  SbMediaType sample_type() const { return sample_type_; }
  const uint8_t* data() const { return data_; }
  int size() const { return size_; }
  SbMediaTime pts() const { return pts_; }
  const SbMediaVideoSampleInfo* video_sample_info() const {
    return has_video_sample_info_ ? &video_sample_info_ : NULL;
  }
  const SbDrmSampleInfo* drm_info() const {
    return has_drm_info_ ? &drm_info_ : NULL;
  }
  void SetDecryptedContent(const void* buffer, int size) {
    SB_DCHECK(size == size_);
    SB_DCHECK(deallocate_sample_func_);
    DeallocateSampleBuffer();

    if (size > 0) {
      decrypted_data_.resize(size);
      SbMemoryCopy(&decrypted_data_[0], buffer, size);
      data_ = &decrypted_data_[0];
    } else {
      data_ = NULL;
    }
    size_ = size;
    has_drm_info_ = false;
  }

 private:
  ~ReferenceCountedBuffer() { DeallocateSampleBuffer(); }

  void DeallocateSampleBuffer() {
    if (deallocate_sample_func_) {
      deallocate_sample_func_(player_, context_, const_cast<uint8_t*>(data_));
      deallocate_sample_func_ = NULL;
    }
  }

  mutable SbAtomic32 ref_count_;
  SbMediaType sample_type_;
  SbPlayerDeallocateSampleFunc deallocate_sample_func_;
  SbPlayer player_;
  void* context_;
  const uint8_t* data_;
  int size_;
  SbMediaTime pts_;
  bool has_video_sample_info_;
  SbMediaVideoSampleInfo video_sample_info_;
  bool has_drm_info_;
  SbDrmSampleInfo drm_info_;
  std::vector<uint8_t> decrypted_data_;
  std::vector<SbDrmSubSampleMapping> subsamples_;

  SB_DISALLOW_COPY_AND_ASSIGN(ReferenceCountedBuffer);
};

InputBuffer::InputBuffer() : buffer_(NULL) {}

InputBuffer::InputBuffer(SbMediaType sample_type,
                         SbPlayerDeallocateSampleFunc deallocate_sample_func,
                         SbPlayer player,
                         void* context,
                         const void* sample_buffer,
                         int sample_buffer_size,
                         SbMediaTime sample_pts,
                         const SbMediaVideoSampleInfo* video_sample_info,
                         const SbDrmSampleInfo* sample_drm_info) {
  buffer_ = new ReferenceCountedBuffer(
      sample_type, deallocate_sample_func, player, context, sample_buffer,
      sample_buffer_size, sample_pts, video_sample_info, sample_drm_info);
  buffer_->AddRef();
}

InputBuffer::InputBuffer(const InputBuffer& that) {
  buffer_ = that.buffer_;
  if (buffer_) {
    buffer_->AddRef();
  }
}

InputBuffer::~InputBuffer() {
  reset();
}

InputBuffer& InputBuffer::operator=(const InputBuffer& that) {
  if (that.buffer_) {
    that.buffer_->AddRef();
  }
  if (buffer_) {
    buffer_->Release();
  }
  buffer_ = that.buffer_;

  return *this;
}

void InputBuffer::reset() {
  if (buffer_) {
    buffer_->Release();
    buffer_ = NULL;
  }
}

SbMediaType InputBuffer::sample_type() const {
  SB_DCHECK(buffer_);
  return buffer_->sample_type();
}

const uint8_t* InputBuffer::data() const {
  SB_DCHECK(buffer_);
  return buffer_->data();
}

int InputBuffer::size() const {
  SB_DCHECK(buffer_);
  return buffer_->size();
}

SbMediaTime InputBuffer::pts() const {
  SB_DCHECK(buffer_);
  return buffer_->pts();
}

const SbMediaVideoSampleInfo* InputBuffer::video_sample_info() const {
  SB_DCHECK(buffer_);
  return buffer_->video_sample_info();
}

const SbDrmSampleInfo* InputBuffer::drm_info() const {
  SB_DCHECK(buffer_);
  return buffer_->drm_info();
}

void InputBuffer::SetDecryptedContent(const void* buffer, int size) {
  SB_DCHECK(buffer_);
  buffer_->SetDecryptedContent(buffer, size);
}

bool operator==(const InputBuffer& lhs, const InputBuffer& rhs) {
  if (!lhs.is_valid() && !rhs.is_valid()) {
    return true;
  }
  if (lhs.is_valid() && rhs.is_valid()) {
    return lhs.sample_type() == rhs.sample_type() && lhs.data() == rhs.data() &&
           lhs.size() == rhs.size() && lhs.pts() == rhs.pts() &&
           lhs.video_sample_info() == rhs.video_sample_info() &&
           lhs.drm_info() == rhs.drm_info();
  }

  return false;
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
