// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_TRACK_RUN_ITERATOR_H_
#define MEDIA_MP4_TRACK_RUN_ITERATOR_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "media/base/media_export.h"
#include "media/mp4/box_definitions.h"

namespace media {
namespace mp4 {

using base::TimeDelta;
base::TimeDelta MEDIA_EXPORT TimeDeltaFromFrac(int64 numer, int64 denom);

struct SampleInfo;
struct TrackRunInfo;

class MEDIA_EXPORT TrackRunIterator {
 public:
  // Create a new TrackRunIterator. A reference to |moov| will be retained for
  // the lifetime of this object.
  explicit TrackRunIterator(const Movie* moov);
  ~TrackRunIterator();

  void Reset();

  // Sets up the iterator to handle all the runs from the current fragment.
  bool Init(const MovieFragment& moof);

  // Returns true if the properties of the current run or sample are valid.
  bool RunIsValid() const;
  bool SampleIsValid() const;

  // Advance the properties to refer to the next run or sample. Requires that
  // the current sample be valid.
  void AdvanceRun();
  void AdvanceSample();

  // Returns the maximum buffer location at which no data earlier in the stream
  // will be required in order to read the current or any subsequent sample. You
  // may clear all data up to this offset before reading the current sample
  // safely. Result is in the same units as offset() (for Media Source this is
  // in bytes past the the head of the MOOF box).
  int64 GetMaxClearOffset();

  // Returns the minimum timestamp (or kInfiniteDuration if no runs present).
  TimeDelta GetMinDecodeTimestamp();

  // Property of the current run. Only valid if RunIsValid().
  uint32 track_id() const;
  bool is_encrypted() const;
  bool is_audio() const;
  // Only one is valid, based on the value of is_audio().
  const AudioSampleEntry& audio_description() const;
  const VideoSampleEntry& video_description() const;

  // Properties of the current sample. Only valid if SampleIsValid().
  int64 sample_offset() const;
  int sample_size() const;
  TimeDelta dts() const;
  TimeDelta cts() const;
  TimeDelta duration() const;
  bool is_keyframe() const;

 private:
  void ResetRun();

  const Movie* moov_;

  std::vector<TrackRunInfo> runs_;
  std::vector<TrackRunInfo>::const_iterator run_itr_;
  std::vector<SampleInfo>::const_iterator sample_itr_;

  int64 sample_dts_;
  int64 sample_offset_;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_TRACK_RUN_ITERATOR_H_
