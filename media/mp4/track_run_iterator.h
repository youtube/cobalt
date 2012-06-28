// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_TRACK_RUN_ITERATOR_H_
#define MEDIA_MP4_TRACK_RUN_ITERATOR_H_

#include <vector>

#include "base/time.h"
#include "media/mp4/box_definitions.h"
#include "media/mp4/cenc.h"

namespace media {
namespace mp4 {

using base::TimeDelta;

base::TimeDelta TimeDeltaFromFrac(int64 numer, int64 denom);

struct SampleInfo {
  int size;
  TimeDelta duration;
  TimeDelta cts_offset;
  bool is_keyframe;
};

struct TrackRunInfo {
  uint32 track_id;
  std::vector<SampleInfo> samples;
  TimeDelta start_dts;
  int64 sample_start_offset;

  bool is_encrypted;
  int64 cenc_start_offset;
  int cenc_total_size;
  uint8 default_cenc_size;
  // Only valid if default_cenc_size == 0. (In this case, infer the sample count
  // from the 'samples' parameter; they shall be the same.)
  std::vector<uint8> cenc_sizes;

  TrackRunInfo();
  ~TrackRunInfo();
};

class TrackRunIterator {
 public:
  TrackRunIterator();
  ~TrackRunIterator();

  void Reset();

  // Sets up the iterator to handle all the runs from the current fragment.
  bool Init(const Movie& moov, const MovieFragment& moof);

  // Returns true if the properties of the current run or sample are valid.
  bool RunValid() const;
  bool SampleValid() const;

  // Advance the properties to refer to the next run or sample. Requires that
  // the current sample be valid.
  void AdvanceRun();
  void AdvanceSample();

  // Returns true iff the track is encrypted and the common encryption sample
  // auxiliary information has not yet been cached for the current track.
  bool NeedsCENC();

  // Cache the CENC data from the given buffer.
  bool CacheCENC(const uint8* buf, int size);

  // Returns the maximum buffer location at which no data earlier in the stream
  // will be required in order to read the current or any subsequent sample. You
  // may clear all data up to this offset before reading the current sample
  // safely. Result is in the same units as offset() (for Media Source this is
  // in bytes past the the head of the MOOF box).
  int64 GetMaxClearOffset();

  // Returns the minimum timestamp (or kInfiniteDuration if no runs present).
  TimeDelta GetMinDecodeTimestamp();

  // Property of the current run. Reqiures RunValid().
  uint32 track_id() const;
  bool is_encrypted() const;
  // Only valid if is_encrypted() is true.
  int64 cenc_offset() const;
  int cenc_size() const;

  // Properties of the current sample. Requires SampleValid().
  int64 offset() const;
  int size() const;
  TimeDelta dts() const;
  TimeDelta cts() const;
  TimeDelta duration() const;
  bool is_keyframe() const;
  // Only valid if is_encrypted() is true and NeedsCENC() is false.
  const FrameCENCInfo& frame_cenc_info();

 private:
  void ResetRun();
  std::vector<TrackRunInfo> runs_;
  std::vector<TrackRunInfo>::const_iterator run_itr_;

  std::vector<SampleInfo>::const_iterator sample_itr_;
  std::vector<uint8> cenc_cache_;

  std::vector<int64> min_clear_offsets_;
  std::vector<int64>::const_iterator min_clear_offset_itr_;

  TimeDelta sample_dts_;
  int64 sample_offset_;
  FrameCENCInfo frame_cenc_info_;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_TRACK_RUN_ITERATOR_H_
