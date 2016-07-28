/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/base/shell_media_statistics.h"

#include <limits>

#include "base/basictypes.h"

namespace media {

ShellMediaStatistics::ShellMediaStatistics() {
  Reset(true);  // reset all stats, include global stats.
}

void ShellMediaStatistics::OnPlaybackBegin() {
  Reset(false);  // reset non-global stats.
}

void ShellMediaStatistics::record(StatType type, int64 value) {
  if (type == STAT_TYPE_VIDEO_WIDTH)
    type = STAT_TYPE_VIDEO_WIDTH;
  ++stats_[type].times_;
  stats_[type].total_ += value;
  if (value > stats_[type].max_)
    stats_[type].max_ = value;
  if (value < stats_[type].min_)
    stats_[type].min_ = value;
  stats_[type].current_ = value;
}

void ShellMediaStatistics::record(StatType type,
                                  const base::TimeDelta& duration) {
  record(type, duration.ToInternalValue());
}

double ShellMediaStatistics::GetElapsedTime() const {
  return (base::Time::Now() - start_).InSecondsF();
}

int64 ShellMediaStatistics::GetTimes(StatType type) const {
  return stats_[type].times_;
}

int64 ShellMediaStatistics::GetTotal(StatType type) const {
  return stats_[type].total_;
}

int64 ShellMediaStatistics::GetCurrent(StatType type) const {
  return stats_[type].current_;
}

int64 ShellMediaStatistics::GetAverage(StatType type) const {
  if (stats_[type].times_ == 0)
    return 0;
  return stats_[type].total_ / stats_[type].times_;
}

int64 ShellMediaStatistics::GetMin(StatType type) const {
  return stats_[type].min_;
}

int64 ShellMediaStatistics::GetMax(StatType type) const {
  return stats_[type].max_;
}

double ShellMediaStatistics::GetTotalDuration(StatType type) const {
  return base::TimeDelta::FromInternalValue(GetTotal(type)).InSecondsF();
}

double ShellMediaStatistics::GetCurrentDuration(StatType type) const {
  return base::TimeDelta::FromInternalValue(GetCurrent(type)).InSecondsF();
}

double ShellMediaStatistics::GetAverageDuration(StatType type) const {
  return base::TimeDelta::FromInternalValue(GetAverage(type)).InSecondsF();
}

double ShellMediaStatistics::GetMinDuration(StatType type) const {
  return base::TimeDelta::FromInternalValue(GetMin(type)).InSecondsF();
}

double ShellMediaStatistics::GetMaxDuration(StatType type) const {
  return base::TimeDelta::FromInternalValue(GetMax(type)).InSecondsF();
}

// static
ShellMediaStatistics& ShellMediaStatistics::Instance() {
  static ShellMediaStatistics media_statistics;
  return media_statistics;
}

void ShellMediaStatistics::Reset(bool include_global_stats) {
  start_ = base::Time::Now();
  int items_to_reset =
      include_global_stats ? arraysize(stats_) : STAT_TYPE_START_OF_GLOBAL_STAT;
  for (int i = 0; i < items_to_reset; ++i) {
    // We deliberately not reset current_ so its value can be kept after reset.
    stats_[i].times_ = 0;
    stats_[i].total_ = 0;
    stats_[i].min_ = std::numeric_limits<int64>::max();
    stats_[i].max_ = std::numeric_limits<int64>::min();
  }
}

ShellScopedMediaStat::ShellScopedMediaStat(ShellMediaStatistics::StatType type)
    : type_(type), start_(base::Time::Now()) {}

ShellScopedMediaStat::~ShellScopedMediaStat() {
  ShellMediaStatistics::Instance().record(type_, base::Time::Now() - start_);
}

}  // namespace media
