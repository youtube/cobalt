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

#ifndef MEDIA_BASE_SHELL_MEDIA_STATISTICS_H_
#define MEDIA_BASE_SHELL_MEDIA_STATISTICS_H_

#include "base/synchronization/lock.h"
#include "base/time.h"

namespace media {

// This class collects events and their durations in the media stack.
// Note that it is not thread safe but as its purpose is just to collect
// performance data it is ok to call it from different threads.
class ShellMediaStatistics {
 public:
  enum StatType {
    STAT_TYPE_AUDIO_CODEC,
    STAT_TYPE_AUDIO_CHANNELS,
    STAT_TYPE_AUDIO_SAMPLE_PER_SECOND,
    STAT_TYPE_AUDIO_UNDERFLOW,
    STAT_TYPE_VIDEO_CODEC,
    STAT_TYPE_VIDEO_WIDTH,
    STAT_TYPE_VIDEO_HEIGHT,
    // Decode a video frame
    STAT_TYPE_VIDEO_FRAME_DECODE,
    // Drop a video frame that we don't have enough time to render
    STAT_TYPE_VIDEO_FRAME_DROP,
    // The frame arrives too late to the renderer, usually indicates that it
    // takes too many time in the decoder.
    STAT_TYPE_VIDEO_FRAME_LATE,
    // How many frames we cached in the video renderer. If this value drops it
    // is more likely that we will have jitter.
    STAT_TYPE_VIDEO_RENDERER_BACKLOG,
    // Time spend in decrypting a buffer
    STAT_TYPE_DECRYPT,
    // The stat types after the following are global stats. i.e. their values
    // will be preserved between playng back of different videos.
    STAT_TYPE_START_OF_GLOBAL_STAT,
    // The size of the shell buffer block just allocated.
    STAT_TYPE_ALLOCATED_SHELL_BUFFER_SIZE,
    STAT_TYPE_MAX
  };

  // The structure is used to track all statistics. int64 is used here so we
  // can track a TimeDelta. Note that some of the fields may not be meaningful
  // to all Stat types. For example, `total` is not meanful to video resolution
  // as we don't care about the sum of video resolultion over time. But for
  // simplicity we just keep all the information and it is the responsibility
  // of the user of this class to use the individual statistics correctly.
  struct Stat {
    int64 times_;  // How many times the stat has been updated.
    int64 current_;
    int64 total_;
    int64 min_;
    int64 max_;
  };

  ShellMediaStatistics();

  void OnPlaybackBegin();
  void record(StatType type, int64 value);
  void record(StatType type, const base::TimeDelta& duration);

  // Returns the time elapsed since last reset in the unit of second.
  double GetElapsedTime() const;
  int64 GetTimes(StatType type) const;

  int64 GetTotal(StatType type) const;
  int64 GetCurrent(StatType type) const;
  int64 GetAverage(StatType type) const;
  int64 GetMin(StatType type) const;
  int64 GetMax(StatType type) const;

  // The following access functions are just provided for easy of use. They are
  // not applicable to all stats. it is the responsibility of the user of these
  // functions to ensure that the call is valid.
  // The unit of time is second.
  double GetTotalDuration(StatType type) const;
  double GetCurrentDuration(StatType type) const;
  double GetAverageDuration(StatType type) const;
  double GetMinDuration(StatType type) const;
  double GetMaxDuration(StatType type) const;

  static ShellMediaStatistics& Instance();

 private:
  void Reset(bool include_global_stats);

  base::Time start_;
  Stat stats_[STAT_TYPE_MAX];
};

class ShellScopedMediaStat {
 public:
  ShellScopedMediaStat(ShellMediaStatistics::StatType type);
  ~ShellScopedMediaStat();

 private:
  ShellMediaStatistics::StatType type_;
  base::Time start_;
};

}  // namespace media

#if defined(__LB_SHELL__FOR_RELEASE__)
#define UPDATE_MEDIA_STATISTICS(type, value) \
  do {                                       \
  } while (false)

// This macro reports a media stat with its duration
#define SCOPED_MEDIA_STATISTICS(type) \
  do {                                \
  } while (false)
#else  // defined(__LB_SHELL__FOR_RELEASE__)
// This macro reports a media stat with its new value
#define UPDATE_MEDIA_STATISTICS(type, value)      \
  media::ShellMediaStatistics::Instance().record( \
      media::ShellMediaStatistics::type, value)

// This macro reports a media stat with its duration
#define SCOPED_MEDIA_STATISTICS(type)           \
  media::ShellScopedMediaStat statistics_event( \
      media::ShellMediaStatistics::type)
#endif  // defined(__LB_SHELL__FOR_RELEASE__)

#endif  // MEDIA_BASE_SHELL_MEDIA_STATISTICS_H_
