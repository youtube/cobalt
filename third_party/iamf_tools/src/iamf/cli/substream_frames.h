/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_SUBSTREAM_FRAMES_H_
#define CLI_SUBSTREAM_FRAMES_H_

#include <algorithm>
#include <cstddef>
#include <list>
#include <vector>

#include "absl/log/absl_check.h"

namespace iamf_tools {

/*!\brief Stores samples in frames and supports FIFO accesses.
 *
 * Each frame is a 2D vector storing samples in (channel, time) axes.
 * Use pattern:
 *
 * for (all channels corresponding to a substream) {
 *   for (all samples in a frame) {
 *     PushSamples(channel_index, sample);
 *   }
 * }
 *
 * // Retrieve the oldest frame.
 * auto frame = Front();
 *
 * // Release the storage space for future use.
 * PopFront();
 *
 */
// TODO(b/427616513): Test this class in a unit test.
template <typename SampleType>
class SubstreamFrames {
 public:
  /*!\brief Constructor.
   *
   * \param num_channels Number of channels.
   * \param num_samples_per_frame Number of samples per frame.
   */
  SubstreamFrames(size_t num_channels, size_t num_samples_per_frame)
      : num_channels_(num_channels),
        num_samples_per_frame_(num_samples_per_frame),
        current_frame_iters_(num_channels) {
    EnsureNonEmpty();
  }

  /*!\brief Returns whether this data structure is empty.
   *
   * \return True if there is no sample. Either there is no frame or all the
   *         channels in the frame are empty.
   */
  bool Empty() const {
    if (frames_.empty()) {
      return true;
    }
    for (const auto& frame : frames_) {
      if (!frame.empty()) {
        for (const auto& channel : frame) {
          if (!channel.empty()) {
            return false;
          }
        }
      }
    }
    return true;
  }

  /*!\brief Gets the front of the (guaranteed non-empty) list of frames.
   *
   * \return Front of the list of frames.
   */
  std::vector<std::vector<SampleType>>& Front() {
    EnsureNonEmpty();
    return frames_.front();
  }

  /*!\brief Pushes a sample to the specified channel in the current frame.
   *
   * \param channel_index Index of the channel to push to.
   * \param sample Sample value to push
   */
  void PushSample(int channel_index, const SampleType sample) {
    ABSL_CHECK_LT(channel_index, static_cast<int>(num_channels_));
    auto& channel = GetChannelInNonFullFrame(channel_index);
    channel.push_back(sample);
  }

  /*!\brief Pads zeros to all the channels in the current frame.
   *
   * \param num_samples_to_pad Number of zeros to pad.
   */
  void PadZeros(const int num_samples_to_pad) {
    ABSL_CHECK_GE(num_samples_to_pad, 0);
    int padded_samples = 0;
    while (padded_samples < num_samples_to_pad) {
      int num_samples_to_pad_in_frame = 0;
      for (int channel_index = 0;
           channel_index < static_cast<int>(num_channels_); channel_index++) {
        auto& channel = GetChannelInNonFullFrame(channel_index);
        num_samples_to_pad_in_frame =
            std::min(static_cast<int>(num_samples_per_frame_) -
                         static_cast<int>(channel.size()),
                     num_samples_to_pad - padded_samples);
        ABSL_CHECK_GT(num_samples_to_pad_in_frame, 0);
        channel.insert(channel.end(), num_samples_to_pad_in_frame, 0);
      }
      padded_samples += num_samples_to_pad_in_frame;
    }
  }

  /*!\brief Pops the front of the list of frames.
   *
   * Used when the frame is "consumed" and the space can be reused.
   */
  void PopFront() {
    if (frames_.empty()) {
      return;
    }
    recycled_frames_.splice(recycled_frames_.end(), frames_, frames_.begin(),
                            ++frames_.begin());
  }

 private:
  typedef std::list<std::vector<std::vector<SampleType>>> ListOfFrames;
  typedef ListOfFrames::iterator ListOfFramesIter;

  // Gets the back frame of the (guaranteed non-empty) list.
  std::vector<std::vector<SampleType>>& Back() {
    EnsureNonEmpty();
    return frames_.back();
  }

  // Gets the channel in the last non-full frame (a new frame will be appended
  // in the end if it's full).
  std::vector<SampleType>& GetChannelInNonFullFrame(int channel_index) {
    EnsureNonEmpty();
    auto& current_frame_iter_for_channel = current_frame_iters_[channel_index];
    if ((*current_frame_iter_for_channel)[channel_index].size() ==
        num_samples_per_frame_) {
      if (current_frame_iter_for_channel->data() == Back().data()) {
        AppendEmptyFrame();
      }
      current_frame_iter_for_channel++;
    }
    return (*current_frame_iter_for_channel)[channel_index];
  }

  // Ensures that there is at least one frame in the list.
  void EnsureNonEmpty() {
    if (frames_.empty()) {
      AppendEmptyFrame();
      for (auto& current_frame_iter : current_frame_iters_) {
        current_frame_iter = frames_.begin();
      }
    }
  }

  // Appends an empty frame in the end of the list. May reuse frames in
  // `recycled_frames_` to save the re-allocation cost.
  void AppendEmptyFrame() {
    if (recycled_frames_.empty()) {
      frames_.push_back({num_channels_, std::vector<SampleType>()});
    } else {
      frames_.splice(frames_.end(), recycled_frames_, recycled_frames_.begin(),
                     ++recycled_frames_.begin());
    }
    auto& back_frame = frames_.back();
    for (auto& channel : back_frame) {
      channel.reserve(num_samples_per_frame_);
      channel.clear();
    }
  }

  size_t num_channels_;
  size_t num_samples_per_frame_;

  // Frames to write sample to.
  ListOfFrames frames_;

  // Discarded frames that can be reused if needed.
  ListOfFrames recycled_frames_;

  // Iterators pointing to the current frame to write to, for each channel
  // this iterator might point to a different frame in the list of frames.
  std::vector<ListOfFramesIter> current_frame_iters_;
};

}  // namespace iamf_tools

#endif  // CLI_SUBSTREAM_FRAMES_H_
