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

#include <numeric>
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
        pts_(sample_pts) {
    SB_DCHECK(deallocate_sample_func);
    TryToAssignVideoSampleInfo(video_sample_info);
    TryToAssignDrmSampleInfo(sample_drm_info);
  }

  ReferenceCountedBuffer(SbMediaType sample_type,
                         SbPlayerDeallocateSampleFunc deallocate_sample_func,
                         SbPlayer player,
                         void* context,
                         const void** sample_buffers,
                         int* sample_buffer_sizes,
                         int number_of_sample_buffers,
                         SbMediaTime sample_pts,
                         const SbMediaVideoSampleInfo* video_sample_info,
                         const SbDrmSampleInfo* sample_drm_info)
      : ref_count_(0),
        sample_type_(sample_type),
        deallocate_sample_func_(deallocate_sample_func),
        player_(player),
        context_(context),
        pts_(sample_pts) {
    SB_DCHECK(deallocate_sample_func);
    SB_DCHECK(number_of_sample_buffers > 0);

    TryToAssignVideoSampleInfo(video_sample_info);
    TryToAssignDrmSampleInfo(sample_drm_info);

    if (number_of_sample_buffers == 1) {
      data_ = static_cast<const uint8_t*>(sample_buffers[0]);
      size_ = sample_buffer_sizes[0];
    } else if (number_of_sample_buffers > 1) {
      // TODO: This simply concatenating multi-part buffers into one large
      // buffer.  It serves the purpose to test the Cobalt media code work with
      // multi-part sample buffer but we should proper implement InputBuffer to
      // ensure that the concatenating of multi-part buffers is handled inside
      // the renderers or the decoders so the SbPlayer implementation won't use
      // too much memory.
      size_ =
          std::accumulate(sample_buffer_sizes,
                          sample_buffer_sizes + number_of_sample_buffers, 0);
      flattened_data_.reserve(size_);
      for (int i = 0; i < number_of_sample_buffers; ++i) {
        const uint8_t* data = static_cast<const uint8_t*>(sample_buffers[i]);
        flattened_data_.insert(flattened_data_.end(), data,
                               data + sample_buffer_sizes[i]);
      }
      DeallocateSampleBuffer(sample_buffers[0]);
      data_ = flattened_data_.data();
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
    DeallocateSampleBuffer(data_);

    if (size > 0) {
      flattened_data_.resize(size);
      SbMemoryCopy(flattened_data_.data(), buffer, size);
      data_ = flattened_data_.data();
    } else {
      data_ = NULL;
    }
    size_ = size;
    has_drm_info_ = false;
  }

 private:
  ~ReferenceCountedBuffer() { DeallocateSampleBuffer(data_); }

  void TryToAssignVideoSampleInfo(
      const SbMediaVideoSampleInfo* video_sample_info) {
    has_video_sample_info_ = video_sample_info != NULL;

    if (!has_video_sample_info_) {
      return;
    }

    video_sample_info_ = *video_sample_info;
    if (video_sample_info_.color_metadata) {
      color_metadata_ = *video_sample_info_.color_metadata;
      video_sample_info_.color_metadata = &color_metadata_;
    } else {
      video_sample_info_.color_metadata = NULL;
    }
  }
  void TryToAssignDrmSampleInfo(const SbDrmSampleInfo* sample_drm_info) {
    has_drm_info_ = sample_drm_info != NULL;

    if (!has_drm_info_) {
      return;
    }

    SB_DCHECK(sample_drm_info->subsample_count > 0);

    subsamples_.assign(
        sample_drm_info->subsample_mapping,
        sample_drm_info->subsample_mapping + sample_drm_info->subsample_count);
    drm_info_ = *sample_drm_info;
    drm_info_.subsample_mapping = subsamples_.empty() ? NULL : &subsamples_[0];
  }

  void DeallocateSampleBuffer(const void* buffer) {
    if (deallocate_sample_func_) {
      deallocate_sample_func_(player_, context_, buffer);
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
  SbMediaColorMetadata color_metadata_;
  SbMediaVideoSampleInfo video_sample_info_;
  bool has_drm_info_;
  SbDrmSampleInfo drm_info_;
  std::vector<uint8_t> flattened_data_;
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

InputBuffer::InputBuffer(SbMediaType sample_type,
                         SbPlayerDeallocateSampleFunc deallocate_sample_func,
                         SbPlayer player,
                         void* context,
                         const void** sample_buffers,
                         int* sample_buffer_sizes,
                         int number_of_sample_buffers,
                         SbMediaTime sample_pts,
                         const SbMediaVideoSampleInfo* video_sample_info,
                         const SbDrmSampleInfo* sample_drm_info) {
  buffer_ = new ReferenceCountedBuffer(
      sample_type, deallocate_sample_func, player, context, sample_buffers,
      sample_buffer_sizes, number_of_sample_buffers, sample_pts,
      video_sample_info, sample_drm_info);
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
