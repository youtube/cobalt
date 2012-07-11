// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "media/base/sinc_resampler.h"

#include <cmath>

#include "base/logging.h"

namespace media {

enum {
  // The kernel size can be adjusted for quality (higher is better) at the
  // expense of performance.  Must be an even number.
  // TODO(dalecurtis): Test performance to see if we can jack this up to 64+.
  kKernelSize = 32,

  // The number of destination frames generated per processing pass.  Affects
  // how often and for how much SincResampler calls back for input.  Must be
  // greater than kKernelSize.
  kBlockSize = 512,

  // The kernel offset count is used for interpolation and is the number of
  // sub-sample kernel shifts.  Can be adjusted for quality (higher is better)
  // at the expense of allocating more memory.
  kKernelOffsetCount = 32,
  kKernelStorageSize = kKernelSize * (kKernelOffsetCount + 1),

  // The size (in samples) of the internal buffer used by the resampler.
  kBufferSize = kBlockSize + kKernelSize
};

SincResampler::SincResampler(double io_sample_rate_ratio, const ReadCB& read_cb)
    : io_sample_rate_ratio_(io_sample_rate_ratio),
      virtual_source_idx_(0),
      buffer_primed_(false),
      read_cb_(read_cb),
      // TODO(dalecurtis): When we switch to AVX/SSE optimization, we'll need to
      // allocate with 32-byte alignment and ensure they're sized % 32 bytes.
      kernel_storage_(new float[kKernelStorageSize]),
      input_buffer_(new float[kBufferSize]),
      // Setup various region pointers in the buffer (see diagram above).
      r0_(input_buffer_.get() + kKernelSize / 2),
      r1_(input_buffer_.get()),
      r2_(r0_),
      r3_(r0_ + kBlockSize - kKernelSize / 2),
      r4_(r0_ + kBlockSize),
      r5_(r0_ + kKernelSize / 2) {
  DCHECK_EQ(kKernelSize % 2, 0) << "kKernelSize must be even!";
  DCHECK_GT(kBlockSize, kKernelSize)
      << "kBlockSize must be greater than kKernelSize!";
  // Basic sanity checks to ensure buffer regions are laid out correctly:
  // r0_ and r2_ should always be the same position.
  DCHECK_EQ(r0_, r2_);
  // r1_ at the beginning of the buffer.
  DCHECK_EQ(r1_, input_buffer_.get());
  // r1_ left of r2_, r2_ left of r5_ and r1_, r2_ size correct.
  DCHECK_EQ(r2_ - r1_, r5_ - r2_);
  // r3_ left of r4_, r5_ left of r0_ and r3_ size correct.
  DCHECK_EQ(r4_ - r3_, r5_ - r0_);
  // r3_, r4_ size correct and r4_ at the end of the buffer.
  DCHECK_EQ(r4_ + (r4_ - r3_), r1_ + kBufferSize);
  // r5_ size correct and at the end of the buffer.
  DCHECK_EQ(r5_ + kBlockSize, r1_ + kBufferSize);

  memset(kernel_storage_.get(), 0,
         sizeof(*kernel_storage_.get()) * kKernelStorageSize);
  memset(input_buffer_.get(), 0, sizeof(*input_buffer_.get()) * kBufferSize);

  InitializeKernel();
}

SincResampler::~SincResampler() {}

void SincResampler::InitializeKernel() {
  // Blackman window parameters.
  static const double kAlpha = 0.16;
  static const double kA0 = 0.5 * (1.0 - kAlpha);
  static const double kA1 = 0.5;
  static const double kA2 = 0.5 * kAlpha;

  // |sinc_scale_factor| is basically the normalized cutoff frequency of the
  // low-pass filter.
  double sinc_scale_factor =
      io_sample_rate_ratio_ > 1.0 ? 1.0 / io_sample_rate_ratio_ : 1.0;

  // The sinc function is an idealized brick-wall filter, but since we're
  // windowing it the transition from pass to stop does not happen right away.
  // So we should adjust the low pass filter cutoff slightly downward to avoid
  // some aliasing at the very high-end.
  // TODO(crogers): this value is empirical and to be more exact should vary
  // depending on kKernelSize.
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
      double window = kA0 - kA1 * cos(2.0 * M_PI * x) + kA2
          * cos(4.0 * M_PI * x);

      // Window the sinc() function and store at the correct offset.
      kernel_storage_[i + offset_idx * kKernelSize] = sinc * window;
    }
  }
}

void SincResampler::Resample(float* destination, int frames) {
  int remaining_frames = frames;

  // Step (1) -- Prime the input buffer at the start of the input stream.
  if (!buffer_primed_) {
    read_cb_.Run(r0_, kBlockSize + kKernelSize / 2);
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

      float* k1 = kernel_storage_.get() + offset_idx * kKernelSize;
      float* k2 = k1 + kKernelSize;

      // Initialize input pointer based on quantized |virtual_source_idx_|.
      float* input_ptr = r1_ + source_idx;

      // We'll compute "convolutions" for the two kernels which straddle
      // |virtual_source_idx_|.
      float sum1 = 0;
      float sum2 = 0;

      // Figure out how much to weight each kernel's "convolution".
      double kernel_interpolation_factor = virtual_offset_idx - offset_idx;

      // Generate a single output sample.
      int n = kKernelSize;
      float input;
      // TODO(dalecurtis): For initial commit, I've ripped out all the SSE
      // optimizations, these definitely need to go back in before release.
      while (n--) {
        input = *input_ptr++;
        sum1 += input * *k1++;
        sum2 += input * *k2++;
      }

      // Linearly interpolate the two "convolutions".
      double result = (1.0 - kernel_interpolation_factor) * sum1
          + kernel_interpolation_factor * sum2;

      *destination++ = result;

      // Advance the virtual index.
      virtual_source_idx_ += io_sample_rate_ratio_;

      if (!--remaining_frames)
        return;
    }

    // Wrap back around to the start.
    virtual_source_idx_ -= kBlockSize;

    // Step (3) Copy r3_ to r1_ and r4_ to r2_.
    // This wraps the last input frames back to the start of the buffer.
    memcpy(r1_, r3_, sizeof(*input_buffer_.get()) * (kKernelSize / 2));
    memcpy(r2_, r4_, sizeof(*input_buffer_.get()) * (kKernelSize / 2));

    // Step (4)
    // Refresh the buffer with more input.
    read_cb_.Run(r5_, kBlockSize);
  }
}

int SincResampler::ChunkSize() {
  return kBlockSize / io_sample_rate_ratio_;
}

}  // namespace media
