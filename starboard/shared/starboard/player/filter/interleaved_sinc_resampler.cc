// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Modifications Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
//
// Input buffer layout, dividing the total buffer into regions (r0_ - r5_):
//
// |----------------|-----------------------------------------|----------------|
//
//                                   kBlockSize + kKernelSize / 2
//                   <--------------------------------------------------------->
//                                              r0_
//
//  kKernelSize / 2   kKernelSize / 2         kKernelSize / 2   kKernelSize / 2
// <---------------> <--------------->       <---------------> <--------------->
//        r1_               r2_                     r3_               r4_
//
//                                                     kBlockSize
//                                     <--------------------------------------->
//                                                        r5_
//
// The algorithm:
//
// 1) Consume input frames into r0_ (r1_ is zero-initialized).
// 2) Position kernel centered at start of r0_ (r2_) and generate output frames
//    until kernel is centered at start of r4_ or we've finished generating all
//    the output frames.
// 3) Copy r3_ to r1_ and r4_ to r2_.
// 4) Consume input frames into r5_ (zero-pad if we run out of input).
// 5) Goto (2) until all of input is consumed.
//
// Note: we're glossing over how the sub-sample handling works with
// |virtual_source_idx_|, etc.

#include "starboard/shared/starboard/player/filter/interleaved_sinc_resampler.h"

#include <algorithm>

#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

InterleavedSincResampler::InterleavedSincResampler(double io_sample_rate_ratio,
                                                   int channel_count)
    : io_sample_rate_ratio_(io_sample_rate_ratio),
      channel_count_(channel_count),
      frame_size_in_bytes_(sizeof(float) * channel_count_) {
  // Setup various region pointers in the buffer (see diagram above).
  r0_ = input_buffer_ + kKernelSize / 2 * channel_count_;
  r1_ = input_buffer_;
  r2_ = r0_;
  r3_ = r0_ + (kBlockSize - kKernelSize / 2) * channel_count_;
  r4_ = r0_ + kBlockSize * channel_count_;
  r5_ = r0_ + kKernelSize / 2 * channel_count_;
  // Ensure kKernelSize is a multiple of 32 for easy SSE optimizations; causes
  // r0_ and r5_ (used for input) to always be 16-byte aligned by virtue of
  // input_buffer_ being 16-byte aligned.
  SB_DCHECK(kKernelSize % 32 == 0) << "kKernelSize must be a multiple of 32!";
  SB_DCHECK(kBlockSize > kKernelSize)
      << "kBlockSize must be greater than kKernelSize!";
  // Basic sanity checks to ensure buffer regions are laid out correctly:
  // r0_ and r2_ should always be the same position.
  SB_DCHECK(r0_ == r2_);
  // r1_ at the beginning of the buffer.
  SB_DCHECK(r1_ == input_buffer_);
  // r1_ left of r2_, r2_ left of r5_ and r1_, r2_ size correct.
  SB_DCHECK(r2_ - r1_ == r5_ - r2_);
  // r3_ left of r4_, r5_ left of r0_ and r3_ size correct.
  SB_DCHECK(r4_ - r3_ == r5_ - r0_);
  // r3_, r4_ size correct and r4_ at the end of the buffer.
  SB_DCHECK(r4_ + (r4_ - r3_) == r1_ + kBufferSize * channel_count_);
  // r5_ size correct and at the end of the buffer.
  SB_DCHECK(r5_ + kBlockSize * channel_count_ ==
            r1_ + kBufferSize * channel_count_);

  InitializeKernel();
}

void InterleavedSincResampler::InitializeKernel() {
  // Blackman window parameters.
  const double kAlpha = 0.16;
  const double kA0 = 0.5 * (1.0 - kAlpha);
  const double kA1 = 0.5;
  const double kA2 = 0.5 * kAlpha;

  // |sinc_scale_factor| is basically the normalized cutoff frequency of the
  // low-pass filter.
  double sinc_scale_factor =
      io_sample_rate_ratio_ > 1.0 ? 1.0 / io_sample_rate_ratio_ : 1.0;

  // The sinc function is an idealized brick-wall filter, but since we're
  // windowing it the transition from pass to stop does not happen right away.
  // So we should adjust the low pass filter cutoff slightly downward to avoid
  // some aliasing at the very high-end.
  // TODO: this value is empirical and to be more exact should vary depending
  // on kKernelSize.
  sinc_scale_factor *= 0.9;

  // Generates a set of windowed sinc() kernels.
  // We generate a range of sub-sample offsets from 0.0 to 1.0.
  for (int offset_idx = 0; offset_idx <= kKernelOffsetCount; ++offset_idx) {
    double subsample_offset =
        static_cast<double>(offset_idx) / kKernelOffsetCount;

    for (int i = 0; i < kKernelSize; ++i) {
      // Compute the sinc with offset.
      double s =
          sinc_scale_factor * M_PI * (i - kKernelSize / 2 - subsample_offset);
      double sinc = (!s ? 1.0 : sin(s) / s) * sinc_scale_factor;

      // Compute Blackman window, matching the offset of the sinc().
      double x = (i - subsample_offset) / kKernelSize;
      double window =
          kA0 - kA1 * cos(2.0 * M_PI * x) + kA2 * cos(4.0 * M_PI * x);

      // Window the sinc() function and store at the correct offset.
      kernel_storage_[i + offset_idx * kKernelSize] = sinc * window;
    }
  }
}

int InterleavedSincResampler::GetNumberOfCachedFrames() const {
  if (pending_buffers_.empty()) {
    return 0;
  }

  SB_DCHECK(frames_queued_ - kBufferSize >
            frames_resampled_ * io_sample_rate_ratio_);
  return frames_queued_ - kBufferSize -
         frames_resampled_ * io_sample_rate_ratio_;
}

void InterleavedSincResampler::QueueBuffer(const float* data, int frames) {
  SB_DCHECK(CanQueueBuffer());

  if (!pending_buffers_.empty() && pending_buffers_.back()->IsEndOfStream()) {
    SB_DCHECK(!data);
    return;
  }

  if (data) {
    frames_queued_ += frames;
    pending_buffers_.push(new Buffer(data, frames, channel_count_));
  } else {
    pending_buffers_.push(new Buffer);
  }
}

void InterleavedSincResampler::Resample(float* destination, int frames) {
  int remaining_frames = frames;

  // Step (1) -- Prime the input buffer at the start of the input stream.
  if (!buffer_primed_) {
    Read(r0_, kBlockSize + kKernelSize / 2);
    buffer_primed_ = true;
  }

  // Step (2) -- Resample!
  while (remaining_frames) {
    while (virtual_source_idx_ < kBlockSize) {
      // |virtual_source_idx_| lies in between two kernel offsets so figure out
      // what they are.
      int source_idx = static_cast<int>(virtual_source_idx_);
      double subsample_remainder = virtual_source_idx_ - source_idx;

      double virtual_offset_idx = subsample_remainder * kKernelOffsetCount;
      int offset_idx = static_cast<int>(virtual_offset_idx);

      // We'll compute "convolutions" for the two kernels which straddle
      // |virtual_source_idx_|.
      float* k1 = kernel_storage_ + offset_idx * kKernelSize;
      float* k2 = k1 + kKernelSize;

      // Initialize input pointer based on quantized |virtual_source_idx_|.
      float* input_ptr = r1_ + source_idx * channel_count_;

      // Figure out how much to weight each kernel's "convolution".
      double kernel_interpolation_factor = virtual_offset_idx - offset_idx;
      for (int i = 0; i < channel_count_; ++i) {
        *destination++ =
            Convolve(input_ptr + i, k1, k2, kernel_interpolation_factor);
      }

      // Advance the virtual index.
      virtual_source_idx_ += io_sample_rate_ratio_;

      if (!--remaining_frames) {
        frames_resampled_ += frames;
        return;
      }
    }

    // Wrap back around to the start.
    virtual_source_idx_ -= kBlockSize;

    // Step (3) Copy r3_ to r1_ and r4_ to r2_.
    // This wraps the last input frames back to the start of the buffer.
    memcpy(r1_, r3_, frame_size_in_bytes_ * (kKernelSize / 2));
    memcpy(r2_, r4_, frame_size_in_bytes_ * (kKernelSize / 2));

    // Step (4)
    // Refresh the buffer with more input.
    Read(r5_, kBlockSize);
  }

  SB_NOTREACHED();
}

void InterleavedSincResampler::Flush() {
  virtual_source_idx_ = 0;
  buffer_primed_ = false;
  while (!pending_buffers_.empty()) {
    pending_buffers_.pop();
  }
  offset_in_frames_ = 0;
  frames_resampled_ = 0;
  frames_queued_ = 0;
}

bool InterleavedSincResampler::CanQueueBuffer() const {
  if (pending_buffers_.empty()) {
    return true;
  }
  if (pending_buffers_.back()->IsEndOfStream()) {
    return false;
  }
  return pending_buffers_.size() < kMaximumPendingBuffers;
}

bool InterleavedSincResampler::HasEnoughData(int frames_to_resample) const {
  // Always return true if EOS is seen, as in this case we will just fill 0.
  if (!pending_buffers_.empty() && pending_buffers_.back()->IsEndOfStream()) {
    return true;
  }

  // We have to decrease frames_queued_ down as the Read()s are always done in
  // blocks of kBlockSize or kBufferSize. We have to ensure that there is buffer
  // for an extra Read().
  return (frames_resampled_ + frames_to_resample) * io_sample_rate_ratio_ <
         frames_queued_ - kBufferSize;
}

void InterleavedSincResampler::Read(float* destination, int frames) {
  while (frames > 0 && !pending_buffers_.empty()) {
    scoped_refptr<Buffer> buffer = pending_buffers_.front();
    if (buffer->IsEndOfStream()) {
      // Zero fill the buffer after EOS has reached.
      memset(destination, 0, frame_size_in_bytes_ * frames);
      return;
    }
    // Copy the data over.
    int frames_in_buffer = buffer->GetNumFrames();
    int frames_to_copy = std::min(frames_in_buffer - offset_in_frames_, frames);
    const float* source = buffer->GetData();
    source += offset_in_frames_ * channel_count_;
    memcpy(destination, source, frame_size_in_bytes_ * frames_to_copy);
    offset_in_frames_ += frames_to_copy;
    // Pop the first buffer if all its content has been read.
    if (offset_in_frames_ == frames_in_buffer) {
      offset_in_frames_ = 0;
      pending_buffers_.pop();
    }
    frames -= frames_to_copy;
    destination += frames_to_copy * channel_count_;
  }

  // Read should always be satisfied as otherwise Resample should return false
  // to the caller directly.
  SB_DCHECK(frames == 0);
}

float InterleavedSincResampler::Convolve(const float* input_ptr,
                                         const float* k1,
                                         const float* k2,
                                         double kernel_interpolation_factor) {
  float sum1 = 0;
  float sum2 = 0;

  // Generate a single output sample.  Unrolling this loop hurt performance in
  // local testing.
  int n = kKernelSize;
  while (n--) {
    sum1 += *input_ptr * *k1++;
    sum2 += *input_ptr * *k2++;
    input_ptr += channel_count_;
  }

  // Linearly interpolate the two "convolutions".
  return (1.0 - kernel_interpolation_factor) * sum1 +
         kernel_interpolation_factor * sum2;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
