// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_time_stretcher.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/wsola_internal.h"

#if (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
#include <arm_neon.h>
#endif  // (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)

namespace starboard {

// Waveform Similarity Overlap-and-add (WSOLA).
//
// One WSOLA iteration
//
// 1) Extract |target_block_| as input frames at indices
//    [|target_block_index_|, |target_block_index_| + |ola_window_size_|).
//    Note that |target_block_| is the "natural" continuation of the output.
//
// 2) Extract |search_block_| as input frames at indices
//    [|search_block_index_|,
//     |search_block_index_| + |num_candidate_blocks_| + |ola_window_size_|).
//
// 3) Find a block within the |search_block_| that is most similar
//    to |target_block_|. Let |optimal_index| be the index of such block and
//    write it to |optimal_block_|.
//
// 4) Update:
//    |optimal_block_| = |transition_window_| * |target_block_| +
//    (1 - |transition_window_|) * |optimal_block_|.
//
// 5) Overlap-and-add |optimal_block_| to the |wsola_output_|.
//
// 6) Update:
//    |target_block_| = |optimal_index| + |ola_window_size_| / 2.
//    |output_index_| = |output_index_| + |ola_window_size_| / 2,
//    |search_block_center_offset_| = |output_index_| * |playback_rate|, and
//    |search_block_index_| = |search_block_center_offset_| -
//        |search_block_center_offset_|.

// Max/min supported playback rates for fast/slow audio. Audio outside of these
// ranges are muted.
// Audio at these speeds would sound better under a frequency domain algorithm.
constexpr double kMinPlaybackRate = 0.5;
constexpr double kMaxPlaybackRate = 4.0;

// Overlap-and-add window size in milliseconds.
constexpr int kOlaWindowSizeMs = 20;

// Size of search interval in milliseconds. The search interval is
// [-delta delta] around |output_index_| * |playback_rate|. So the search
// interval is 2 * delta.
constexpr int kWsolaSearchIntervalMs = 30;

// The maximum size in seconds for the |audio_buffer_|. Arbitrarily determined.
constexpr int kMaxCapacityInSeconds = 3;

// The minimum size in ms for the |audio_buffer_|. Arbitrarily determined.
constexpr int kStartingCapacityInMs = 200;

AudioTimeStretcher::AudioTimeStretcher()
    : sample_type_(kSbMediaAudioSampleTypeFloat32),
      channels_(0),
      bytes_per_frame_(0),
      samples_per_second_(0),
      muted_partial_frame_(0),
      capacity_(0),
      output_time_(0.0),
      search_block_center_offset_(0),
      search_block_index_(0),
      num_candidate_blocks_(0),
      target_block_index_(0),
      ola_window_size_(0),
      ola_hop_size_(0),
      num_complete_frames_(0),
      initial_capacity_(0),
      max_capacity_(0) {}

AudioTimeStretcher::~AudioTimeStretcher() {}

void AudioTimeStretcher::Initialize(SbMediaAudioSampleType sample_type,
                                    int channels,
                                    int samples_per_second,
                                    bool enable_optimized) {
  SB_DCHECK_GT(samples_per_second, 0);

  sample_type_ = sample_type;
  channels_ = channels;
#if SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
  // Disable optimizations on x86/x64 because the optimized path is currently
  // only implemented on ARM (using NEON SIMD). Using it on x86/x64 would fall
  // back to scalar C++ loops, causing a performance regression for Mono audio.
  enable_optimized_wsola_ = false;
#else   // SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
  enable_optimized_wsola_ = enable_optimized;
#endif  // SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
  bytes_per_frame_ = GetBytesPerSample(sample_type_) * channels_;
  samples_per_second_ = samples_per_second;
  initial_capacity_ = capacity_ =
      ConvertMillisecondsToFrames(kStartingCapacityInMs);
  max_capacity_ =
      std::max(initial_capacity_, kMaxCapacityInSeconds * samples_per_second_);
  num_candidate_blocks_ = ConvertMillisecondsToFrames(kWsolaSearchIntervalMs);
  ola_window_size_ = ConvertMillisecondsToFrames(kOlaWindowSizeMs);

  // Make sure window size in an even number.
  ola_window_size_ += ola_window_size_ & 1;
  ola_hop_size_ = ola_window_size_ / 2;

  // |num_candidate_blocks_| / 2 is the offset of the center of the search
  // block to the center of the first (left most) candidate block. The offset
  // of the center of a candidate block to its left most point is
  // |ola_window_size_| / 2 - 1. Note that |ola_window_size_| is even and in
  // our convention the center belongs to the left half, so we need to subtract
  // one frame to get the correct offset.
  //
  //                             Search Block
  //              <------------------------------------------->
  //
  //   |ola_window_size_| / 2 - 1
  //              <----
  //
  //             |num_candidate_blocks_| / 2
  //                   <----------------
  //                                 center
  //              X----X----------------X---------------X-----X
  //              <---------->                     <---------->
  //                Candidate      ...               Candidate
  //                   1,          ...         |num_candidate_blocks_|
  search_block_center_offset_ =
      num_candidate_blocks_ / 2 + (ola_window_size_ / 2 - 1);

  ola_window_ = std::make_unique<float[]>(ola_window_size_);
  GetPeriodicHanningWindow(ola_window_size_, ola_window_.get());

  transition_window_ = std::make_unique<float[]>(ola_window_size_ * 2);
  GetPeriodicHanningWindow(2 * ola_window_size_, transition_window_.get());

  wsola_output_ = new DecodedAudio(
      channels_, sample_type_, kSbMediaAudioFrameStorageTypeInterleaved, 0,
      (ola_window_size_ + ola_hop_size_) * bytes_per_frame_);
  // Initialize for overlap-and-add of the first block.
  memset(wsola_output_->data(), 0, wsola_output_->size_in_bytes());

  // Auxiliary containers.
  optimal_block_ = new DecodedAudio(channels_, sample_type_,
                                    kSbMediaAudioFrameStorageTypeInterleaved, 0,
                                    ola_window_size_ * bytes_per_frame_);
  search_block_ = new DecodedAudio(
      channels_, sample_type_, kSbMediaAudioFrameStorageTypeInterleaved, 0,
      (num_candidate_blocks_ + (ola_window_size_ - 1)) * bytes_per_frame_);
  target_block_ = new DecodedAudio(channels_, sample_type_,
                                   kSbMediaAudioFrameStorageTypeInterleaved, 0,
                                   ola_window_size_ * bytes_per_frame_);
}

scoped_refptr<DecodedAudio> AudioTimeStretcher::Read(int requested_frames,
                                                     double playback_rate) {
  SB_DCHECK_GT(bytes_per_frame_, 0);
  SB_DCHECK_GE(playback_rate, 0);

  scoped_refptr<DecodedAudio> dest = new DecodedAudio(
      channels_, sample_type_, kSbMediaAudioFrameStorageTypeInterleaved, 0,
      requested_frames * bytes_per_frame_);

  if (playback_rate == 0) {
    dest->ShrinkTo(0);
    return dest;
  }

  // Optimize the muted case to issue a single clear instead of performing
  // the full crossfade and clearing each crossfaded frame.
  if (playback_rate < kMinPlaybackRate || playback_rate > kMaxPlaybackRate) {
    int frames_to_render =
        std::min(static_cast<int>(audio_buffer_.frames() / playback_rate),
                 requested_frames);

    // Compute accurate number of frames to actually skip in the source data.
    // Includes the leftover partial frame from last request. However, we can
    // only skip over complete frames, so a partial frame may remain for next
    // time.
    muted_partial_frame_ += frames_to_render * playback_rate;
    // Handle the case where muted_partial_frame_ rounds up to
    // audio_buffer_.frames()+1.
    int seek_frames = std::min(static_cast<int>(muted_partial_frame_),
                               audio_buffer_.frames());
    memset(dest->data(), 0, frames_to_render * bytes_per_frame_);
    audio_buffer_.SeekFrames(seek_frames);

    // Determine the partial frame that remains to be skipped for next call. If
    // the user switches back to playing, it may be off time by this partial
    // frame, which would be undetectable. If they subsequently switch to
    // another playback rate that mutes, the code will attempt to line up the
    // frames again.
    muted_partial_frame_ -= seek_frames;
    dest->ShrinkTo(frames_to_render * bytes_per_frame_);
    return dest;
  }

  int slower_step = static_cast<int>(ceil(ola_window_size_ * playback_rate));
  int faster_step = static_cast<int>(ceil(ola_window_size_ / playback_rate));

  // Optimize the most common |playback_rate| ~= 1 case to use a single copy
  // instead of copying frame by frame.
  if (ola_window_size_ <= faster_step && slower_step >= ola_window_size_) {
    const int frames_to_copy =
        std::min(audio_buffer_.frames(), requested_frames);
    const int frames_read = audio_buffer_.ReadFrames(frames_to_copy, 0, dest);
    SB_DCHECK_EQ(frames_read, frames_to_copy);
    dest->ShrinkTo(frames_read * bytes_per_frame_);
    return dest;
  }

  int rendered_frames = 0;
  do {
    rendered_frames += WriteCompletedFramesTo(
        requested_frames - rendered_frames, rendered_frames, dest);
  } while (rendered_frames < requested_frames &&
           RunOneWsolaIteration(playback_rate));
  dest->ShrinkTo(rendered_frames * bytes_per_frame_);
  return dest;
}

void AudioTimeStretcher::FlushBuffers() {
  // Clear the queue of decoded packets (releasing the buffers).
  audio_buffer_.Clear();
  output_time_ = 0.0;
  search_block_index_ = 0;
  target_block_index_ = 0;
  memset(wsola_output_->data(), 0, wsola_output_->size_in_bytes());
  num_complete_frames_ = 0;

  // Reset |capacity_| so growth triggered by underflows doesn't penalize seek
  // time.
  capacity_ = initial_capacity_;
}

void AudioTimeStretcher::EnqueueBuffer(
    const scoped_refptr<DecodedAudio>& audio_data) {
  SB_DCHECK(!audio_data->is_end_of_stream());
  audio_buffer_.Append(audio_data);
}

bool AudioTimeStretcher::IsQueueFull() const {
  return audio_buffer_.frames() >= capacity_;
}

namespace {

#if (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
void BlendTransitionMono_NEON(float* opt,
                              const float* target,
                              const float* w1,
                              const float* w2,
                              int size) {
  const int last_index = size - (size % 4);
  for (int n = 0; n < last_index; n += 4) {
    float32x4_t opt_reg = vld1q_f32(opt + n);
    float32x4_t target_reg = vld1q_f32(target + n);
    float32x4_t w1_reg = vld1q_f32(w1 + n);
    float32x4_t w2_reg = vld1q_f32(w2 + n);
    vst1q_f32(opt + n,
              vmlaq_f32(vmulq_f32(opt_reg, w1_reg), target_reg, w2_reg));
  }
  for (int n = last_index; n < size; ++n) {
    opt[n] = opt[n] * w1[n] + target[n] * w2[n];
  }
}

void BlendTransitionStereo_NEON(float* opt,
                                const float* target,
                                const float* w1,
                                const float* w2,
                                int size) {
  const int last_index = size - (size % 2);
  for (int n = 0; n < last_index; n += 2) {
    float32x4_t opt_reg = vld1q_f32(opt + n * 2);
    float32x4_t target_reg = vld1q_f32(target + n * 2);
    float32x2_t w1_2 = vld1_f32(w1 + n);
    float32x2_t w2_2 = vld1_f32(w2 + n);
    float32x2x2_t w1_zip = vzip_f32(w1_2, w1_2);
    float32x2x2_t w2_zip = vzip_f32(w2_2, w2_2);
    float32x4_t w1_reg = vcombine_f32(w1_zip.val[0], w1_zip.val[1]);
    float32x4_t w2_reg = vcombine_f32(w2_zip.val[0], w2_zip.val[1]);
    vst1q_f32(opt + n * 2,
              vmlaq_f32(vmulq_f32(opt_reg, w1_reg), target_reg, w2_reg));
  }
  for (int n = last_index; n < size; ++n) {
    opt[n * 2] = opt[n * 2] * w1[n] + target[n * 2] * w2[n];
    opt[n * 2 + 1] = opt[n * 2 + 1] * w1[n] + target[n * 2 + 1] * w2[n];
  }
}

void OverlapAndAddMono_NEON(const float* opt,
                            float* output,
                            const float* w_out,
                            const float* w_opt,
                            int size) {
  const int last_index = size - (size % 4);
  for (int n = 0; n < last_index; n += 4) {
    float32x4_t out_reg = vld1q_f32(output + n);
    float32x4_t opt_reg = vld1q_f32(opt + n);
    float32x4_t w_out_reg = vld1q_f32(w_out + n);
    float32x4_t w_opt_reg = vld1q_f32(w_opt + n);
    vst1q_f32(output + n,
              vmlaq_f32(vmulq_f32(out_reg, w_out_reg), opt_reg, w_opt_reg));
  }
  for (int n = last_index; n < size; ++n) {
    output[n] = output[n] * w_out[n] + opt[n] * w_opt[n];
  }
}

void OverlapAndAddStereo_NEON(const float* opt,
                              float* output,
                              const float* w_out,
                              const float* w_opt,
                              int size) {
  const int last_index = size - (size % 2);
  for (int n = 0; n < last_index; n += 2) {
    float32x4_t out_reg = vld1q_f32(output + n * 2);
    float32x4_t opt_reg = vld1q_f32(opt + n * 2);
    float32x2_t w_out_2 = vld1_f32(w_out + n);
    float32x2_t w_opt_2 = vld1_f32(w_opt + n);
    float32x2x2_t w_out_zip = vzip_f32(w_out_2, w_out_2);
    float32x2x2_t w_opt_zip = vzip_f32(w_opt_2, w_opt_2);
    float32x4_t w_out_reg = vcombine_f32(w_out_zip.val[0], w_out_zip.val[1]);
    float32x4_t w_opt_reg = vcombine_f32(w_opt_zip.val[0], w_opt_zip.val[1]);
    vst1q_f32(output + n * 2,
              vmlaq_f32(vmulq_f32(out_reg, w_out_reg), opt_reg, w_opt_reg));
  }
  for (int n = last_index; n < size; ++n) {
    output[n * 2] = output[n * 2] * w_out[n] + opt[n * 2] * w_opt[n];
    output[n * 2 + 1] =
        output[n * 2 + 1] * w_out[n] + opt[n * 2 + 1] * w_opt[n];
  }
}
#endif  // (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)

}  // namespace

void AudioTimeStretcher::GetOptimalBlock() {
  int optimal_index = 0;

  // An interval around last optimal block which is excluded from the search.
  // This is to reduce the buzzy sound. The number 160 is rather arbitrary and
  // derived heuristically.
  const int kExcludeIntervalLengthFrames = 160;
  if (TargetIsWithinSearchRegion()) {
    optimal_index = target_block_index_;
    PeekAudioWithZeroPrepend(optimal_index, optimal_block_.get());
  } else {
    PeekAudioWithZeroPrepend(target_block_index_, target_block_.get());
    PeekAudioWithZeroPrepend(search_block_index_, search_block_.get());
    int last_optimal =
        target_block_index_ - ola_hop_size_ - search_block_index_;
    Interval exclude_interval =
        std::make_pair(last_optimal - kExcludeIntervalLengthFrames / 2,
                       last_optimal + kExcludeIntervalLengthFrames / 2);

    // |optimal_index| is in frames and it is relative to the beginning of the
    // |search_block_|.
    optimal_index = OptimalIndex(search_block_.get(), target_block_.get(),
                                 kSbMediaAudioFrameStorageTypeInterleaved,
                                 exclude_interval, enable_optimized_wsola_);

    // Translate |index| w.r.t. the beginning of |audio_buffer_| and extract the
    // optimal block.
    optimal_index += search_block_index_;
    PeekAudioWithZeroPrepend(optimal_index, optimal_block_.get());

    // Make a transition from target block to the optimal block if different.
    // Target block has the best continuation to the current output.
    // Optimal block is the most similar block to the target, however, it might
    // introduce some discontinuity when over-lap-added. Therefore, we combine
    // them for a smoother transition. The length of transition window is twice
    // as that of the optimal-block which makes it like a weighting function
    // where target-block has higher weight close to zero (weight of 1 at index
    // 0) and lower weight close the end.
    if (enable_optimized_wsola_ && channels_ == 1) {
      bool handled = false;
#if (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
      BlendTransitionMono_NEON(
          optimal_block_->data_as_float32(), target_block_->data_as_float32(),
          transition_window_.get(), transition_window_.get() + ola_window_size_,
          ola_window_size_);
      handled = true;
#endif  // (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
      if (!handled) {
        float* opt = optimal_block_->data_as_float32();
        const float* target = target_block_->data_as_float32();
        const float* w1 = transition_window_.get();
        const float* w2 = transition_window_.get() + ola_window_size_;
        for (int n = 0; n < ola_window_size_; ++n) {
          opt[n] = opt[n] * w1[n] + target[n] * w2[n];
        }
      }
    } else if (enable_optimized_wsola_ && channels_ == 2) {
      bool handled = false;
#if (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
      BlendTransitionStereo_NEON(
          optimal_block_->data_as_float32(), target_block_->data_as_float32(),
          transition_window_.get(), transition_window_.get() + ola_window_size_,
          ola_window_size_);
      handled = true;
#endif  // (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
      if (!handled) {
        float* opt = optimal_block_->data_as_float32();
        const float* target = target_block_->data_as_float32();
        const float* w1 = transition_window_.get();
        const float* w2 = transition_window_.get() + ola_window_size_;
        for (int n = 0; n < ola_window_size_; ++n) {
          opt[n * 2] = opt[n * 2] * w1[n] + target[n * 2] * w2[n];
          opt[n * 2 + 1] = opt[n * 2 + 1] * w1[n] + target[n * 2 + 1] * w2[n];
        }
      }
    } else {
      for (int n = 0; n < ola_window_size_; ++n) {
        float* ch_opt = optimal_block_->data_as_float32() + n * channels_;
        const float* const ch_target =
            target_block_->data_as_float32() + n * channels_;
        float w1 = transition_window_[n];
        float w2 = transition_window_[ola_window_size_ + n];
        for (int k = 0; k < channels_; ++k) {
          ch_opt[k] = ch_opt[k] * w1 + ch_target[k] * w2;
        }
      }
    }
  }

  // Next target is one hop ahead of the current optimal.
  target_block_index_ = optimal_index + ola_hop_size_;
}

int AudioTimeStretcher::WriteCompletedFramesTo(int requested_frames,
                                               int dest_offset,
                                               DecodedAudio* dest) {
  SB_DCHECK_GT(bytes_per_frame_, 0);

  int rendered_frames = std::min(num_complete_frames_, requested_frames);

  if (rendered_frames == 0) {
    return 0;  // There is nothing to read from |wsola_output_|, return.
  }

  memcpy(dest->data() + bytes_per_frame_ * dest_offset, wsola_output_->data(),
         rendered_frames * bytes_per_frame_);

  // Remove the frames which are read.
  int frames_to_move = wsola_output_->frames() - rendered_frames;
  memmove(wsola_output_->data(),
          wsola_output_->data() + rendered_frames * bytes_per_frame_,
          frames_to_move * bytes_per_frame_);
  num_complete_frames_ -= rendered_frames;
  return rendered_frames;
}

void AudioTimeStretcher::PeekAudioWithZeroPrepend(int read_offset_frames,
                                                  DecodedAudio* dest) {
  SB_DCHECK_GT(bytes_per_frame_, 0);
  SB_CHECK_LE(read_offset_frames + dest->frames(), audio_buffer_.frames());

  int write_offset = 0;
  int num_frames_to_read = dest->frames();
  if (read_offset_frames < 0) {
    int num_zero_frames_appended =
        std::min(-read_offset_frames, num_frames_to_read);
    read_offset_frames = 0;
    num_frames_to_read -= num_zero_frames_appended;
    write_offset = num_zero_frames_appended;
    memset(dest->data(), 0, num_zero_frames_appended * bytes_per_frame_);
  }
  audio_buffer_.PeekFrames(num_frames_to_read, read_offset_frames, write_offset,
                           dest);
}

bool AudioTimeStretcher::RunOneWsolaIteration(double playback_rate) {
  SB_DCHECK_GT(bytes_per_frame_, 0);

  if (!CanPerformWsola()) {
    return false;
  }

  GetOptimalBlock();

  // Overlap-and-add.
  if (enable_optimized_wsola_ && channels_ == 1) {
    bool handled = false;
#if (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
    OverlapAndAddMono_NEON(
        optimal_block_->data_as_float32(),
        wsola_output_->data_as_float32() + num_complete_frames_,
        ola_window_.get() + ola_hop_size_, ola_window_.get(), ola_hop_size_);
    handled = true;
#endif  // (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
    if (!handled) {
      const float* opt = optimal_block_->data_as_float32();
      float* output = wsola_output_->data_as_float32() + num_complete_frames_;
      const float* w_out = ola_window_.get() + ola_hop_size_;
      const float* w_opt = ola_window_.get();
      for (int n = 0; n < ola_hop_size_; ++n) {
        output[n] = output[n] * w_out[n] + opt[n] * w_opt[n];
      }
    }
  } else if (enable_optimized_wsola_ && channels_ == 2) {
    bool handled = false;
#if (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
    OverlapAndAddStereo_NEON(
        optimal_block_->data_as_float32(),
        wsola_output_->data_as_float32() + num_complete_frames_ * 2,
        ola_window_.get() + ola_hop_size_, ola_window_.get(), ola_hop_size_);
    handled = true;
#endif  // (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
    if (!handled) {
      const float* opt = optimal_block_->data_as_float32();
      float* output =
          wsola_output_->data_as_float32() + num_complete_frames_ * 2;
      const float* w_out = ola_window_.get() + ola_hop_size_;
      const float* w_opt = ola_window_.get();
      for (int n = 0; n < ola_hop_size_; ++n) {
        output[n * 2] = output[n * 2] * w_out[n] + opt[n * 2] * w_opt[n];
        output[n * 2 + 1] =
            output[n * 2 + 1] * w_out[n] + opt[n * 2 + 1] * w_opt[n];
      }
    }
  } else {
    for (int n = 0; n < ola_hop_size_; ++n) {
      float* ch_output = wsola_output_->data_as_float32() +
                         (num_complete_frames_ + n) * channels_;
      const float* const ch_opt_frame =
          optimal_block_->data_as_float32() + n * channels_;
      float w_out = ola_window_[ola_hop_size_ + n];
      float w_opt = ola_window_[n];
      for (int k = 0; k < channels_; ++k) {
        ch_output[k] = ch_output[k] * w_out + ch_opt_frame[k] * w_opt;
      }
    }
  }
  // Copy the second half to the output.
  const float* const ch_opt_frame = optimal_block_->data_as_float32();
  float* ch_output =
      wsola_output_->data_as_float32() + num_complete_frames_ * channels_;
  memcpy(&ch_output[ola_hop_size_ * channels_],
         &ch_opt_frame[ola_hop_size_ * channels_],
         sizeof(*ch_opt_frame) * ola_hop_size_ * channels_);

  num_complete_frames_ += ola_hop_size_;
  UpdateOutputTime(playback_rate, ola_hop_size_);
  RemoveOldInputFrames(playback_rate);
  return true;
}

void AudioTimeStretcher::RemoveOldInputFrames(double playback_rate) {
  const int earliest_used_index =
      std::min(target_block_index_, search_block_index_);
  if (earliest_used_index <= 0) {
    return;  // Nothing to remove.
  }

  // Remove frames from input and adjust indices accordingly.
  audio_buffer_.SeekFrames(earliest_used_index);
  target_block_index_ -= earliest_used_index;

  // Adjust output index.
  double output_time_change =
      static_cast<double>(earliest_used_index) / playback_rate;
  SB_CHECK_GE(output_time_, output_time_change);
  UpdateOutputTime(playback_rate, -output_time_change);
}

void AudioTimeStretcher::UpdateOutputTime(double playback_rate,
                                          double time_change) {
  output_time_ += time_change;
  // Center of the search region, in frames.
  const int search_block_center_index =
      static_cast<int>(output_time_ * playback_rate + 0.5);
  search_block_index_ = search_block_center_index - search_block_center_offset_;
}

bool AudioTimeStretcher::TargetIsWithinSearchRegion() const {
  const int search_block_size = num_candidate_blocks_ + (ola_window_size_ - 1);

  return target_block_index_ >= search_block_index_ &&
         target_block_index_ + ola_window_size_ <=
             search_block_index_ + search_block_size;
}

bool AudioTimeStretcher::CanPerformWsola() const {
  const int search_block_size = num_candidate_blocks_ + (ola_window_size_ - 1);
  const int frames = audio_buffer_.frames();
  return target_block_index_ + ola_window_size_ <= frames &&
         search_block_index_ + search_block_size <= frames;
}

int AudioTimeStretcher::ConvertMillisecondsToFrames(int ms) const {
  const double kMillisecondsPerSeconds = 1000;
  return static_cast<int>(ms * (samples_per_second_ / kMillisecondsPerSeconds));
}

}  // namespace starboard
