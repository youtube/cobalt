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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_INTERLEAVED_SINC_RESAMPLER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_INTERLEAVED_SINC_RESAMPLER_H_

#include <math.h>

#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class InterleavedSincResampler {
 public:
  // |io_sample_rate_ratio| is the ratio of input / output sample rates.
  // |channel_count| is the number of channels in the interleaved audio stream.
  InterleavedSincResampler(double io_sample_rate_ratio, int channel_count);

  // Append a buffer to the queue. The samples in the buffer has to be floats.
  void QueueBuffer(const float* data, int frames);

  // Resample |frames| of data from enqueued buffers.  Return false if no sample
  // is read.  Return true if all requested samples have been written into
  // |destination|.  It will never do a partial read.  After the stream reaches
  // the end, the function will fill the rest of buffer with 0.
  void Resample(float* destination, int frames);

  // Flush all buffered data and reset internal indices.
  void Flush();

  // Return false if we shouldn't queue more buffers to the resampler.
  bool CanQueueBuffer() const;

  // Return true if the enqueued buffers have enough data for us to resample the
  // requested number of frames.
  bool HasEnoughData(int frames_to_resample) const;

  double GetSampleRateRatio() const { return io_sample_rate_ratio_; }

  int GetNumberOfCachedFrames() const;

  int channels() const { return channel_count_; }

 private:
  class Buffer : public ::starboard::RefCountedThreadSafe<Buffer> {
   public:
    Buffer() {}
    Buffer(const float* data, int frames, int channel_count) : frames_(frames) {
      data_.reset(new float[frames * channel_count]);
      memcpy(data_.get(), data, frames * channel_count * sizeof(float));
    }
    Buffer(scoped_array<float> data, int frames)
        : data_(data.Pass()), frames_(frames) {}

    const float* GetData() const { return data_.get(); }

    int GetNumFrames() const { return frames_; }

    bool IsEndOfStream() const { return GetData() == NULL; }

   private:
    friend class ::starboard::RefCountedThreadSafe<Buffer>;
    scoped_array<float> data_;
    int frames_ = 0;

    Buffer(const Buffer&) = delete;
    void operator=(const Buffer&) = delete;
  };

  // The kernel size can be adjusted for quality (higher is better) at the
  // expense of performance.  Must be a multiple of 32.
  static const int kKernelSize = 32;

  // The number of destination frames generated per processing pass.  Affects
  // how often and for how much AudioResampler calls back for input.
  // Must be greater than kKernelSize.
  static const int kBlockSize = 512;

  // The kernel offset count is used for interpolation and is the number of
  // sub-sample kernel shifts.  Can be adjusted for quality (higher is better)
  // at the expense of allocating more memory.
  static const int kKernelOffsetCount = 32;
  static const int kKernelStorageSize = kKernelSize * (kKernelOffsetCount + 1);

  // The size (in samples) of the internal buffer used by the resampler.
  static const int kBufferSize = kBlockSize + kKernelSize;

  // The maximum numbers of buffer can be queued.
  static const int kMaximumPendingBuffers = 8;

  static const int kMaxChannels = 8;
  static const int kInputBufferSize = kMaxChannels * kBufferSize;

  void InitializeKernel();
  void Read(float* destination, int frames);

  float Convolve(const float* input_ptr,
                 const float* k1,
                 const float* k2,
                 double kernel_interpolation_factor);

  // The ratio of input / output sample rates.
  double io_sample_rate_ratio_ = 0.0;

  // An index on the source input buffer with sub-sample precision.  It must be
  // double precision to avoid drift.
  double virtual_source_idx_ = 0.0;

  // The buffer is primed once at the very beginning of processing.
  bool buffer_primed_ = false;

  // Number of audio channels.
  int channel_count_ = 0;

  // The size of bytes for an audio frame.
  const int frame_size_in_bytes_;

  // Contains kKernelOffsetCount kernels back-to-back, each of size kKernelSize.
  // The kernel offsets are sub-sample shifts of a windowed sinc shifted from
  // 0.0 to 1.0 sample.
  float kernel_storage_[kKernelStorageSize] = {0};

  // Data from the source is copied into this buffer for each processing pass.
  float input_buffer_[kInputBufferSize] = {0};

  // A queue of buffers to be resampled.
  std::queue<scoped_refptr<Buffer>> pending_buffers_;

  // The current offset to read when reading from the first pending buffer.
  int offset_in_frames_ = 0;

  // The following two variables are used to calculate EOS and in HasEnoughData.
  int frames_resampled_ = 0;
  int frames_queued_ = 0;

  // Pointers to the various regions inside |input_buffer_|.  See the diagram at
  // the top of the .cc file for more information.
  float* r0_;
  float* r1_;
  float* r2_;
  float* r3_;
  float* r4_;
  float* r5_;

  InterleavedSincResampler(const InterleavedSincResampler&) = delete;
  void operator=(const InterleavedSincResampler&) = delete;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_INTERLEAVED_SINC_RESAMPLER_H_
