// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_MP4_TRACK_RUN_ITERATOR_H_
#define COBALT_MEDIA_FORMATS_MP4_TRACK_RUN_ITERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/base/stream_parser_buffer.h"
#include "cobalt/media/formats/mp4/box_definitions.h"

namespace media {

class DecryptConfig;

namespace mp4 {

base::TimeDelta MEDIA_EXPORT TimeDeltaFromRational(int64_t numer,
                                                   int64_t denom);
DecodeTimestamp MEDIA_EXPORT DecodeTimestampFromRational(int64_t numer,
                                                         int64_t denom);

struct SampleInfo;
struct TrackRunInfo;

class MEDIA_EXPORT TrackRunIterator {
 public:
  // Create a new TrackRunIterator. A reference to |moov| will be retained for
  // the lifetime of this object.
  TrackRunIterator(const Movie* moov, const scoped_refptr<MediaLog>& media_log);
  ~TrackRunIterator();

  // Sets up the iterator to handle all the runs from the current fragment.
  bool Init(const MovieFragment& moof);

  // Returns true if the properties of the current run or sample are valid.
  bool IsRunValid() const;
  bool IsSampleValid() const;

  // Advance the properties to refer to the next run or sample. Requires that
  // the current sample be valid.
  void AdvanceRun();
  void AdvanceSample();

  // Returns true if this track run has auxiliary information and has not yet
  // been cached. Only valid if IsRunValid().
  bool AuxInfoNeedsToBeCached();

  // Caches the CENC data from the given buffer. |buf| must be a buffer starting
  // at the offset given by cenc_offset(), with a |size| of at least
  // cenc_size(). Returns true on success, false on error.
  bool CacheAuxInfo(const uint8_t* buf, int size);

  // Returns the maximum buffer location at which no data earlier in the stream
  // will be required in order to read the current or any subsequent sample. You
  // may clear all data up to this offset before reading the current sample
  // safely. Result is in the same units as offset() (for Media Source this is
  // in bytes past the the head of the MOOF box).
  int64_t GetMaxClearOffset();

  // Property of the current run. Only valid if IsRunValid().
  uint32_t track_id() const;
  int64_t aux_info_offset() const;
  int aux_info_size() const;
  bool is_encrypted() const;
  bool is_audio() const;
  // Only one is valid, based on the value of is_audio().
  const AudioSampleEntry& audio_description() const;
  const VideoSampleEntry& video_description() const;

  // Properties of the current sample. Only valid if IsSampleValid().
  int64_t sample_offset() const;
  int sample_size() const;
  DecodeTimestamp dts() const;
  base::TimeDelta cts() const;
  base::TimeDelta duration() const;
  bool is_keyframe() const;

  // Only call when is_encrypted() is true and AuxInfoNeedsToBeCached() is
  // false. Result is owned by caller.
  scoped_ptr<DecryptConfig> GetDecryptConfig();

 private:
  void ResetRun();
  const TrackEncryption& track_encryption() const;

  uint32_t GetGroupDescriptionIndex(uint32_t sample_index) const;

  // Sample encryption information.
  bool IsSampleEncrypted(size_t sample_index) const;
  uint8_t GetIvSize(size_t sample_index) const;
  const std::vector<uint8_t>& GetKeyId(size_t sample_index) const;

  const Movie* moov_;
  scoped_refptr<MediaLog> media_log_;

  std::vector<TrackRunInfo> runs_;
  std::vector<TrackRunInfo>::const_iterator run_itr_;
  std::vector<SampleInfo>::const_iterator sample_itr_;

  int64_t sample_dts_;
  int64_t sample_offset_;

  DISALLOW_COPY_AND_ASSIGN(TrackRunIterator);
};

}  // namespace mp4
}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_MP4_TRACK_RUN_ITERATOR_H_
