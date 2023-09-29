// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_METRICS_STATS_TRACKER_H_
#define STARBOARD_COMMON_METRICS_STATS_TRACKER_H_

#include <memory>
#include <utility>

#include "starboard/common/log.h"

namespace starboard {

class StatsTracker {
 public:
  virtual ~StatsTracker() = default;

  virtual void FileWriteSuccess() = 0;
  virtual void FileWriteFail() = 0;
  virtual void FileWriteBytesWritten(int bytes_written) = 0;

  virtual void StorageWriteRecordSuccess() = 0;
  virtual void StorageWriteRecordFail() = 0;
  virtual void StorageWriteRecordBytesWritten(int bytes_written) = 0;
};

class StatsTrackerUndefined : public StatsTracker {
 public:
  void FileWriteSuccess() override {}
  void FileWriteFail() override {}
  void FileWriteBytesWritten(int bytes_written) override {}

  void StorageWriteRecordSuccess() override {}
  void StorageWriteRecordFail() override {}
  void StorageWriteRecordBytesWritten(int bytes_written) override {}
};

class StatsTrackerContainer {
 public:
  static StatsTrackerContainer* GetInstance();

  StatsTracker& stats_tracker() {
    if (!stats_tracker_) {
      undefined_logged_ = true;
      return undefined_stats_tracker_;
    }
    return *(stats_tracker_.get());
  }

  void set_stats_tracker(std::unique_ptr<StatsTracker> stats_tracker) {
    stats_tracker_ = std::move(stats_tracker);
  }

 private:
  StatsTrackerUndefined undefined_stats_tracker_;
  std::unique_ptr<StatsTracker> stats_tracker_;
  bool undefined_logged_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_METRICS_STATS_TRACKER_H_
