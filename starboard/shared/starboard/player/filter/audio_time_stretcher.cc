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

#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/wsola_internal.h"

#include <cstring>

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

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
static const double kMinPlaybackRate = 0.5;
static const double kMaxPlaybackRate = 4.0;

// Overlap-and-add window size in milliseconds.
static const int kOlaWindowSizeMs = 20;

// Size of search interval in milliseconds. The search interval is
// [-delta delta] around |output_index_| * |playback_rate|. So the search
// interval is 2 * delta.
static const int kWsolaSearchIntervalMs = 30;

// The maximum size in seconds for the |audio_buffer_|. Arbitrarily determined.
static const int kMaxCapacityInSeconds = 3;

// The minimum size in ms for the |audio_buffer_|. Arbitrarily determined.
static const int kStartingCapacityInMs = 200;

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
                                    int samples_per_second) {
  SB_DCHECK(samples_per_second > 0);

  sample_type_ = sample_type;
  channels_ = channels;
  bytes_per_frame_ = media::GetBytesPerSample(sample_type_) * channels_;
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

  ola_window_.reset(new float[ola_window_size_]);
  internal::GetSymmetricHanningWindow(ola_window_size_, ola_window_.get());

  transition_window_.reset(new float[ola_window_size_ * 2]);
  internal::GetSymmetricHanningWindow(2 * ola_window_size_,
                                      transition_window_.get());

  wsola_output_ = new DecodedAudio(
      channels_, sample_type_, kSbMediaAudioFrameStorageTypeInterleaved, 0,
      (ola_window_size_ + ola_hop_size_) * bytes_per_frame_);
  // Initialize for overlap-and-add of the first block.
  memset(wsola_output_->buffer(), 0, wsola_output_->size());

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
  SB_DCHECK(bytes_per_frame_ > 0);
  SB_DCHECK(playback_rate >= 0);

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
    memset(dest->buffer(), 0, frames_to_render * bytes_per_frame_);
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
    SB_DCHECK(frames_read == frames_to_copy);
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
  memset(wsola_output_->buffer(), 0, wsola_output_->size());
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

// TODO: Make order of functions in .cc the same as order of functions in .h.
bool AudioTimeStretcher::CanPerformWsola() const {
  const int search_block_size = num_candidate_blocks_ + (ola_window_size_ - 1);
  const int frames = audio_buffer_.frames();
  return target_block_index_ + ola_window_size_ <= frames &&
         search_block_index_ + search_block_size <= frames;
}

int AudioTimeStretcher::ConvertMillisecondsToFrames(int ms) const {
  const double kMillisecondsPerSeconds =
      static_cast<double>(kSbTimeSecond / kSbTimeMillisecond);
  return static_cast<int>(ms * (samples_per_second_ / kMillisecondsPerSeconds));
}

bool AudioTimeStretcher::RunOneWsolaIteration(double playback_rate) {
  SB_DCHECK(bytes_per_frame_ > 0);

  if (!CanPerformWsola())
    return false;

  GetOptimalBlock();

  // Overlap-and-add.
  for (int k = 0; k < channels_; ++k) {
    const float* const ch_opt_frame =
        reinterpret_cast<const float*>(optimal_block_->buffer()) + k;
    float* ch_output = reinterpret_cast<float*>(wsola_output_->buffer()) + k +
                       num_complete_frames_ * sizeof(float);
    for (int n = 0; n < ola_hop_size_; ++n) {
      ch_output[n * channels_] =
          ch_output[n * channels_] * ola_window_[ola_hop_size_ + n] +
          ch_opt_frame[n * channels_] * ola_window_[n];
    }
  }
  // Copy the second half to the output.
  const float* const ch_opt_frame =
      reinterpret_cast<const float*>(optimal_block_->buffer());
  float* ch_output = reinterpret_cast<float*>(wsola_output_->buffer()) +
                     num_complete_frames_ * sizeof(float);
  memcpy(&ch_output[ola_hop_size_ * channels_],
               &ch_opt_frame[ola_hop_size_ * channels_],
               sizeof(*ch_opt_frame) * ola_hop_size_ * channels_);

  num_complete_frames_ += ola_hop_size_;
  UpdateOutputTime(playback_rate, ola_hop_size_);
  RemoveOldInputFrames(playback_rate);
  return true;
}

void AudioTimeStretcher::UpdateOutputTime(double playback_rate,
                                          double time_change) {
  output_time_ += time_change;
  // Center of the search region, in frames.
  const int search_block_center_index =
      static_cast<int>(output_time_ * playback_rate + 0.5);
  search_block_index_ = search_block_center_index - search_block_center_offset_;
}

void AudioTimeStretcher::RemoveOldInputFrames(double playback_rate) {
  const int earliest_used_index =
      std::min(target_block_index_, search_block_index_);
  if (earliest_used_index <= 0)
    return;  // Nothing to remove.

  // Remove frames from input and adjust indices accordingly.
  audio_buffer_.SeekFrames(earliest_used_index);
  target_block_index_ -= earliest_used_index;

  // Adjust output index.
  double output_time_change =
      static_cast<double>(earliest_used_index) / playback_rate;
  SB_CHECK(output_time_ >= output_time_change);
  UpdateOutputTime(playback_rate, -output_time_change);
}

int AudioTimeStretcher::WriteCompletedFramesTo(int requested_frames,
                                               int dest_offset,
                                               DecodedAudio* dest) {
  SB_DCHECK(bytes_per_frame_ > 0);

  int rendered_frames = std::min(num_complete_frames_, requested_frames);

  if (rendered_frames == 0)
    return 0;  // There is nothing to read from |wsola_output_|, return.

  memcpy(dest->buffer() + bytes_per_frame_ * dest_offset,
               wsola_output_->buffer(), rendered_frames * bytes_per_frame_);

  // Remove the frames which are read.
  int frames_to_move = wsola_output_->frames() - rendered_frames;
  memmove(wsola_output_->buffer(),
               wsola_output_->buffer() + rendered_frames * bytes_per_frame_,
               frames_to_move * bytes_per_frame_);
  num_complete_frames_ -= rendered_frames;
  return rendered_frames;
}

bool AudioTimeStretcher::TargetIsWithinSearchRegion() const {
  const int search_block_size = num_candidate_blocks_ + (ola_window_size_ - 1);

  return target_block_index_ >= search_block_index_ &&
         target_block_index_ + ola_window_size_ <=
             search_block_index_ + search_block_size;
}

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
    internal::Interval exclude_interval =
        std::make_pair(last_optimal - kExcludeIntervalLengthFrames / 2,
                       last_optimal + kExcludeIntervalLengthFrames / 2);

    // |optimal_index| is in frames and it is relative to the beginning of the
    // |search_block_|.
    optimal_index = internal::OptimalIndex(
        search_block_.get(), target_block_.get(),
        kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);

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
    for (int k = 0; k < channels_; ++k) {
      float* ch_opt = reinterpret_cast<float*>(optimal_block_->buffer()) + k;
      const float* const ch_target =
          reinterpret_cast<float*>(target_block_->buffer()) + k;
      for (int n = 0; n < ola_window_size_; ++n) {
        ch_opt[n * channels_] =
            ch_opt[n * channels_] * transition_window_[n] +
            ch_target[n * channels_] * transition_window_[ola_window_size_ + n];
      }
    }
  }

  // Next target is one hop ahead of the current optimal.
  target_block_index_ = optimal_index + ola_hop_size_;
}

void AudioTimeStretcher::PeekAudioWithZeroPrepend(int read_offset_frames,
                                                  DecodedAudio* dest) {
  SB_DCHECK(bytes_per_frame_ > 0);
  SB_CHECK(read_offset_frames + dest->frames() <= audio_buffer_.frames());

  int write_offset = 0;
  int num_frames_to_read = dest->frames();
  if (read_offset_frames < 0) {
    int num_zero_frames_appended =
        std::min(-read_offset_frames, num_frames_to_read);
    read_offset_frames = 0;
    num_frames_to_read -= num_zero_frames_appended;
    write_offset = num_zero_frames_appended;
    memset(dest->buffer(), 0, num_zero_frames_appended * bytes_per_frame_);
  }
  audio_buffer_.PeekFrames(num_frames_to_read, read_offset_frames, write_offset,
                           dest);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
