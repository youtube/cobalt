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

#include "starboard/shared/starboard/media/decoder_flow_control.h"

#include <algorithm>
#include <deque>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <numeric>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard::shared::starboard::media {
namespace {

constexpr int kMaxDecodingHistory = 20;
constexpr int64_t kDecodingTimeWarningThresholdUs = 1'000'000;
constexpr bool kTryAllocation = false;
constexpr int kMaxFramesToTestAllocation = 800;
constexpr bool kLogMaps = false;
constexpr int k8kDecodedFrameBytes = 49'766'400;

struct MemoryInfo {
  long total_kb;
  long free_kb;
  long available_kb;
};

// Returns the value associated with a key in /proc/meminfo, in kilobytes.
// Returns -1 if the key is not found.
MemoryInfo GetMemoryInfo() {
  MemoryInfo mem_info = {-1, -1, -1};
  std::ifstream meminfo("/proc/meminfo");
  if (!meminfo.is_open()) {
    return mem_info;
  }

  std::string line;
  while (std::getline(meminfo, line)) {
    std::stringstream ss(line);
    std::string currentKey;
    long value;
    std::string unit;

    ss >> currentKey >> value >> unit;
    if (currentKey == "MemTotal:") {
      mem_info.total_kb = value;
    } else if (currentKey == "MemFree:") {
      mem_info.free_kb = value;
    } else if (currentKey == "MemAvailable:") {
      mem_info.available_kb = value;
    }
  }
  return mem_info;
}

std::string FormatSize(size_t size_in_bytes) {
  std::stringstream ss;
  if (size_in_bytes < 1024) {
    ss << size_in_bytes << " B";
  } else if (size_in_bytes < 1024 * 1024) {
    ss << std::fixed << std::setprecision(2) << (size_in_bytes / 1024.0)
       << " KB";
  } else if (size_in_bytes < 1024 * 1024 * 1024) {
    ss << std::fixed << std::setprecision(2)
       << (size_in_bytes / (1024.0 * 1024.0)) << " MB";
  } else {
    ss << std::fixed << std::setprecision(2)
       << (size_in_bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
  }
  return ss.str();
}

constexpr int kNumMemoryToDisplay = 10;

void LogMemoryMapSummary() {
  std::ifstream maps_file("/proc/self/maps");
  if (!maps_file.is_open()) {
    SB_LOG(ERROR) << "Failed to open /proc/self/maps";
    return;
  }

  std::map<std::string, size_t> file_sizes;
  size_t total_size = 0;
  std::string line;
  while (std::getline(maps_file, line)) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string part;
    while (ss >> part) {
      parts.push_back(part);
    }

    if (parts.empty()) {
      continue;
    }

    std::string address_range = parts[0];
    size_t separator = address_range.find('-');
    if (separator == std::string::npos) {
      continue;
    }

    std::string start_str = address_range.substr(0, separator);
    std::string end_str = address_range.substr(separator + 1);

    bool is_valid = std::all_of(start_str.begin(), start_str.end(), isxdigit) &&
                    std::all_of(end_str.begin(), end_str.end(), isxdigit);

    if (is_valid) {
      size_t start = std::stoull(start_str, nullptr, 16);
      size_t end = std::stoull(end_str, nullptr, 16);
      size_t range_size = end - start;
      total_size += range_size;

      std::string file_path = "(unnamed)";
      size_t path_index = std::string::npos;
      for (size_t i = 0; i < parts.size(); ++i) {
        if (!parts[i].empty() && (parts[i][0] == '/' || parts[i][0] == '[')) {
          path_index = i;
          break;
        }
      }

      if (path_index != std::string::npos) {
        file_path = "";
        for (size_t i = path_index; i < parts.size(); ++i) {
          if (i > path_index) {
            file_path += " ";
          }
          file_path += parts[i];
        }
      }

      // Remove BuildId
      size_t build_id_pos = file_path.find(" (BuildId:");
      if (build_id_pos != std::string::npos) {
        file_path = file_path.substr(0, build_id_pos);
      }

      file_sizes[file_path] += range_size;
    }
  }

  // Sort by size in descending order
  std::vector<std::pair<std::string, size_t>> sorted_files(file_sizes.begin(),
                                                           file_sizes.end());
  std::sort(sorted_files.begin(), sorted_files.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  SB_LOG(INFO) << "--- Aggregated Memory Map (Top " << kNumMemoryToDisplay
               << ") ---";
  for (size_t i = 0;
       i < std::min<size_t>(kNumMemoryToDisplay, sorted_files.size()); ++i) {
    const auto& pair = sorted_files[i];
    SB_LOG(INFO) << pair.first << ": " << FormatSize(pair.second);
  }
  SB_LOG(INFO) << "Total memory map size: " << FormatSize(total_size);
  SB_LOG(INFO) << "-----------------------------";
}

int GetMaxAllocatable8kFrames() {
  void* ptrs[kMaxFramesToTestAllocation];
  int num_allocated_frames = 0;
  for (int i = 0; i < kMaxFramesToTestAllocation; ++i) {
    ptrs[i] = malloc(k8kDecodedFrameBytes);
    if (ptrs[i] == nullptr) {
      break;
    }
    num_allocated_frames++;
  }

  for (int i = 0; i < num_allocated_frames; ++i) {
    free(ptrs[i]);
  }
  return num_allocated_frames;
}

class ThrottlingDecoderFlowControl : public DecoderFlowControl {
 public:
  ThrottlingDecoderFlowControl(int max_frames,
                               int64_t log_interval_us,
                               StateChangedCB state_changed_cb);
  ~ThrottlingDecoderFlowControl() override = default;

  bool AddFrame(int64_t presentation_time_us) override;
  bool SetFrameDecoded(int64_t presentation_time_us) override;
  bool ReleaseFrameAt(int64_t release_us) override;

  State GetCurrentState() const override;
  bool CanAcceptMore() override;

 private:
  void UpdateDecodingStat_Locked();
  void UpdateState_Locked();
  void LogStateAndReschedule(int64_t log_interval_us);

  const int max_frames_;
  const StateChangedCB state_changed_cb_;

  mutable std::mutex mutex_;
  State state_;  // GUARDED_BY mutex_;

  std::deque<int64_t> decoding_start_times_us_;
  std::deque<int64_t> previous_decoding_times_us_;

  ::starboard::shared::starboard::player::JobThread task_runner_;

  int entering_frame_id_ = 0;
  int decoded_frame_id_ = 0;
};

ThrottlingDecoderFlowControl::ThrottlingDecoderFlowControl(
    int max_frames,
    int64_t log_interval_us,
    StateChangedCB state_changed_cb)
    : max_frames_(max_frames),
      state_changed_cb_(std::move(state_changed_cb)),
      state_({}),
      task_runner_("frame_tracker") {
  SB_CHECK(state_changed_cb_);
  if (log_interval_us > 0) {
    task_runner_.Schedule(
        [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
        log_interval_us);
  }
  SB_LOG(INFO) << "ThrottlingDecoderFlowControl is created: max_frames="
               << max_frames_
               << ", log_interval(msec)=" << (log_interval_us / 1'000)
               << ", kTryAllocation=" << (kTryAllocation ? "true" : "false");
}

bool ThrottlingDecoderFlowControl::AddFrame(int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  if (state_.total_frames() >= max_frames_) {
    SB_LOG(WARNING) << __func__ << " accepts no more frames: frames="
                    << state_.total_frames() << "/" << max_frames_;
    return false;
  }
  state_.decoding_frames++;
  decoding_start_times_us_.push_back(CurrentMonotonicTime());

  UpdateState_Locked();
  entering_frame_id_++;

  MemoryInfo mem_info = GetMemoryInfo();

  SB_LOG(INFO) << "AddFrame: id=" << entering_frame_id_
               << ", pts(msec)=" << presentation_time_us / 1'000
               << ", decoding=" << state_.decoding_frames
               << ", decoded=" << state_.decoded_frames
               << ", mem(MB)={total=" << mem_info.total_kb / 1'024
               << ", free=" << mem_info.free_kb / 1'024
               << ", available=" << mem_info.available_kb / 1'024 << "}";
  if (kLogMaps) {
    LogMemoryMapSummary();
  }
  if (kTryAllocation) {
    int64_t start_us = CurrentMonotonicTime();
    int max_allocatable_8k_frames = GetMaxAllocatable8kFrames();
    int64_t elapsed_us = CurrentMonotonicTime() - start_us;

    SB_LOG(INFO) << "max_allocatable_8k_frames=" << max_allocatable_8k_frames
                 << ", elapsed_to_get_max_alloc(ms)=" << elapsed_us / 1'000;
  }
  return true;
}

bool ThrottlingDecoderFlowControl::SetFrameDecoded(
    int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  if (state_.decoding_frames == 0) {
    SB_LOG(FATAL) << __func__ << " < no decoding frames"
                  << ", pts(msec)=" << presentation_time_us / 1000;
    return false;
  }
  state_.decoding_frames--;
  state_.decoded_frames++;
  SB_CHECK_GE(state_.decoded_frames, 0);

  SB_CHECK(!decoding_start_times_us_.empty());

  auto start_time = decoding_start_times_us_.front();
  decoding_start_times_us_.pop_front();
  auto decoding_time_us = CurrentMonotonicTime() - start_time;
  decoded_frame_id_++;
  SB_LOG(INFO) << "SetFrameDecoded: id=" << decoded_frame_id_
               << ", pts(msec)=" << presentation_time_us / 1000
               << ", decoding-elapsed time(msec)=" << (decoding_time_us / 1'000)
               << ", decoding=" << state_.decoding_frames
               << ", decoded=" << state_.decoded_frames;
  if (decoding_time_us > kDecodingTimeWarningThresholdUs) {
    SB_LOG(WARNING) << "Decoding time exceeded threshold: "
                    << decoding_time_us / 1'000 << " msec";
  }
  previous_decoding_times_us_.push_back(decoding_time_us);
  if (previous_decoding_times_us_.size() > kMaxDecodingHistory) {
    previous_decoding_times_us_.pop_front();
  }

  UpdateState_Locked();
  UpdateDecodingStat_Locked();
  return true;
}

bool ThrottlingDecoderFlowControl::ReleaseFrameAt(int64_t release_us) {
  std::lock_guard lock(mutex_);

  if (state_.decoded_frames == 0) {
    SB_LOG(FATAL) << __func__ << " < no decoded frames";
    return false;
  }

  int64_t delay_us = std::max<int64_t>(release_us - CurrentMonotonicTime(), 0);
  task_runner_.Schedule(
      [this] {
        state_changed_cb_();

        std::lock_guard lock(mutex_);
        state_.decoded_frames--;
        UpdateState_Locked();
      },
      delay_us);

  return true;
}

DecoderFlowControl::State ThrottlingDecoderFlowControl::GetCurrentState()
    const {
  std::lock_guard lock(mutex_);
  return state_;
}

bool ThrottlingDecoderFlowControl::CanAcceptMore() {
  std::lock_guard lock(mutex_);
  if (state_.total_frames() < max_frames_) {
    return true;
  }

  return false;
}

void ThrottlingDecoderFlowControl::UpdateDecodingStat_Locked() {
  if (previous_decoding_times_us_.empty()) {
    return;
  }
  int64_t min_decoding_time_us = previous_decoding_times_us_[0];
  int64_t max_decoding_time_us = previous_decoding_times_us_[0];
  int64_t sum = 0;
  for (auto decoding_us : previous_decoding_times_us_) {
    min_decoding_time_us = std::min(min_decoding_time_us, decoding_us);
    max_decoding_time_us = std::max(max_decoding_time_us, decoding_us);
    sum += decoding_us;
  }

  state_.min_decoding_time_us = min_decoding_time_us;
  state_.max_decoding_time_us = max_decoding_time_us;
  state_.avg_decoding_time_us = sum / previous_decoding_times_us_.size();
}

void ThrottlingDecoderFlowControl::UpdateState_Locked() {
  // SB_CHECK_LE(state_.total_frames(), max_frames_);
  SB_CHECK_GE(state_.decoding_frames, 0);
  SB_CHECK_GE(state_.decoded_frames, 0);

  state_.decoding_frames_high_water_mark =
      std::max(state_.decoding_frames_high_water_mark, state_.decoding_frames);
  state_.decoded_frames_high_water_mark =
      std::max(state_.decoded_frames_high_water_mark, state_.decoded_frames);
  state_.total_frames_high_water_mark =
      std::max(state_.total_frames_high_water_mark,
               state_.decoding_frames + state_.decoded_frames);
}

void ThrottlingDecoderFlowControl::LogStateAndReschedule(
    int64_t log_interval_us) {
  SB_DCHECK(task_runner_.BelongsToCurrentThread());

  SB_LOG(INFO) << "DecoderFlowControl state: " << GetCurrentState();

  task_runner_.Schedule(
      [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
      log_interval_us);
}

class NoOpDecoderFlowControl : public DecoderFlowControl {
 public:
  NoOpDecoderFlowControl() {
    SB_LOG(INFO) << "NoOpDecoderFlowControl is created.";
  }
  ~NoOpDecoderFlowControl() override = default;

  bool AddFrame(int64_t presentation_time_us) override { return true; }
  bool SetFrameDecoded(int64_t presentation_time_us) override { return true; }
  bool ReleaseFrameAt(int64_t release_us) override { return true; }

  State GetCurrentState() const override { return State(); }
  bool CanAcceptMore() override { return true; }
};

}  // namespace

// static
std::unique_ptr<DecoderFlowControl> DecoderFlowControl::CreateNoOp() {
  return std::make_unique<NoOpDecoderFlowControl>();
}

// static
std::unique_ptr<DecoderFlowControl> DecoderFlowControl::CreateThrottling(
    int max_frames,
    int64_t log_interval_us,
    StateChangedCB state_changed_cb) {
  return std::make_unique<ThrottlingDecoderFlowControl>(
      max_frames, log_interval_us, std::move(state_changed_cb));
}

std::ostream& operator<<(std::ostream& os,
                         const DecoderFlowControl::State& status) {
  os << "{decoding: " << status.decoding_frames
     << ", decoded: " << status.decoded_frames
     << ", decoding (hw): " << status.decoding_frames_high_water_mark
     << ", decoded (hw): " << status.decoded_frames_high_water_mark
     << ", total (hw): " << status.total_frames_high_water_mark
     << ", min decoding(msec): " << status.min_decoding_time_us / 1'000
     << ", max decoding(msec): " << status.max_decoding_time_us / 1'000
     << ", avg decoding(msec): " << status.avg_decoding_time_us / 1'000 << "}";
  return os;
}

}  // namespace starboard::shared::starboard::media
