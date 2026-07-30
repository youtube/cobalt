// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"

#include <queue>
#include <sstream>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {

void AudioFrameTracker::Reset() {
  frame_records_.clear();
  last_playback_rate_ = 1.0;
  overflowed_frames_ = 0;
  current_original_frame_head_position_ = 0;
  current_adjusted_frame_head_position_ = 0;
}

void AudioFrameTracker::AddFrames(int number_of_frames, double playback_rate) {
  SB_DCHECK_GT(playback_rate, 0);

  last_playback_rate_ = playback_rate;

  if (number_of_frames == 0) {
    return;
  }

  if (frame_records_.empty() ||
      frame_records_.back().playback_rate != playback_rate) {
    FrameRecord record = {number_of_frames, 0 /* number_of_played_frames */,
                          playback_rate};
    frame_records_.push_back(record);
  } else {
    frame_records_.back().number_of_frames += number_of_frames;
  }
}

void AudioFrameTracker::RecordPlayedFrames(int number_of_frames) {
  while (number_of_frames > 0 && !frame_records_.empty()) {
    FrameRecord& record = frame_records_.front();
    int64_t unplayed_frames =
        record.number_of_frames - record.number_of_played_frames;
    if (unplayed_frames >= number_of_frames) {
      record.number_of_played_frames += number_of_frames;
      number_of_frames = 0;
    } else {
      number_of_frames -= unplayed_frames;
      // Note that since |current_original_frame_head_position_| is an integer,
      // multiplying by a non-integer |playback_rate| can lead to truncation
      // errors. These small errors accumulate every time a record is removed,
      // potentially leading to a slight drift in the original timeline over
      // long periods of playback. Remove record after it's fully played to
      // minimize that drift.
      current_original_frame_head_position_ +=
          record.number_of_frames * record.playback_rate;
      current_adjusted_frame_head_position_ += record.number_of_frames;
      frame_records_.erase(frame_records_.begin());
    }
  }
  overflowed_frames_ += number_of_frames;
}

int64_t AudioFrameTracker::GetOverflowedFrames() const {
  // Overflowed frames calculation uses last playback rate, so any playback rate
  // change after audio end of stream is ignored.
  return overflowed_frames_ * last_playback_rate_;
}

int64_t AudioFrameTracker::GetTotalOriginalFrames() const {
  int64_t ret = current_original_frame_head_position_;
  for (auto& record : frame_records_) {
    ret += record.number_of_frames * record.playback_rate;
  }
  return ret;
}

int64_t AudioFrameTracker::GetFutureFramesPlayedAdjustedToPlaybackRate(
    int number_of_frames,
    double* playback_rate) const {
  SB_DCHECK(playback_rate);

  if (frame_records_.empty()) {
    *playback_rate = last_playback_rate_;
  }

  int64_t frames_played = current_original_frame_head_position_;
  for (auto& record : frame_records_) {
    *playback_rate = record.playback_rate;

    int64_t unplayed_frames =
        record.number_of_frames - record.number_of_played_frames;
    if (unplayed_frames >= number_of_frames) {
      frames_played += (number_of_frames + record.number_of_played_frames) *
                       record.playback_rate;
      number_of_frames = 0;
      break;
    } else {
      frames_played += record.number_of_frames * record.playback_rate;
      number_of_frames -= unplayed_frames;
    }
  }
  return frames_played;
}

int64_t AudioFrameTracker::ConvertOriginalFramePositionToAdjustedFrames(
    int64_t frame_position) const {
  int64_t converted_position = current_adjusted_frame_head_position_;
  int64_t number_of_frames =
      frame_position - current_original_frame_head_position_;
  if (number_of_frames <= 0) {
    // We don't have records for past audio time, which should rarely happen. We
    // use the playback rate of first record or last playback rate to give an
    // estimation.
    if (!frame_records_.empty()) {
      converted_position +=
          number_of_frames / frame_records_.front().playback_rate;
    } else {
      converted_position += number_of_frames / last_playback_rate_;
    }
  } else {
    for (auto& record : frame_records_) {
      if (number_of_frames == 0) {
        break;
      }
      int64_t original_frames_in_record =
          record.number_of_frames * record.playback_rate;
      if (original_frames_in_record > number_of_frames) {
        converted_position += number_of_frames / record.playback_rate;
        number_of_frames = 0;
      } else {
        converted_position += record.number_of_frames;
        number_of_frames -= original_frames_in_record;
      }
    }
  }

  if (number_of_frames > 0) {
    // The original frame position is out of audio record range. The video
    // stream could be longer than audio stream. Use last playback rate to
    // calculate adjusted frame position.
    converted_position += number_of_frames / last_playback_rate_;
  }

  return converted_position;
}

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
void AudioFrameTracker::DumpDebugInfo() const {
  std::stringstream ss;
  ss << "Dump AudioFrameTracker info: "
     << "\n  last_playback_rate: " << last_playback_rate_
     << "\n  overflowed_frames: " << overflowed_frames_
     << "\n  current_original_frame_head_position: "
     << current_original_frame_head_position_
     << "\n  current_adjusted_frame_head_position: "
     << current_adjusted_frame_head_position_;
  for (size_t i = 0; i < frame_records_.size(); ++i) {
    ss << "\n    record[" << i
       << "]: number_of_frames=" << frame_records_[i].number_of_frames
       << ", number_of_played_frames="
       << frame_records_[i].number_of_played_frames
       << ", playback_rate=" << frame_records_[i].playback_rate;
  }
  SB_LOG(INFO) << ss.str();
}
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

}  // namespace starboard
