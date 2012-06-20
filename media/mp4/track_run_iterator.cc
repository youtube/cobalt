// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/track_run_iterator.h"

#include <algorithm>

#include "media/base/stream_parser_buffer.h"
#include "media/mp4/rcheck.h"

namespace media {
namespace mp4 {

base::TimeDelta TimeDeltaFromFrac(int64 numer, uint64 denom) {
  DCHECK_LT((numer > 0 ? numer : -numer),
            kint64max / base::Time::kMicrosecondsPerSecond);
  return base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * numer / denom);
}

static const uint32 kSampleIsDifferenceSampleFlagMask = 0x10000;

TrackRunInfo::TrackRunInfo()
    : track_id(0),
      sample_start_offset(-1),
      is_encrypted(false),
      cenc_start_offset(-1),
      cenc_total_size(-1),
      default_cenc_size(0) {}

TrackRunInfo::~TrackRunInfo() {}

TrackRunIterator::TrackRunIterator() : sample_offset_(0) {}
TrackRunIterator::~TrackRunIterator() {}

static void PopulateSampleInfo(const Track& trak,
                               const TrackExtends& trex,
                               const TrackFragmentHeader& tfhd,
                               const TrackFragmentRun& trun,
                               const uint32 i,
                               SampleInfo* sample_info) {
  if (i < trun.sample_sizes.size()) {
    sample_info->size = trun.sample_sizes[i];
  } else if (tfhd.default_sample_size > 0) {
    sample_info->size = tfhd.default_sample_size;
  } else {
    sample_info->size = trex.default_sample_size;
  }

  const uint64 timescale = trak.media.header.timescale;
  uint64 duration;
  if (i < trun.sample_durations.size()) {
    duration = trun.sample_durations[i];
  } else if (tfhd.default_sample_duration > 0) {
    duration = tfhd.default_sample_duration;
  } else {
    duration = trex.default_sample_duration;
  }
  sample_info->duration = TimeDeltaFromFrac(duration, timescale);

  if (i < trun.sample_composition_time_offsets.size()) {
    sample_info->cts_offset =
        TimeDeltaFromFrac(trun.sample_composition_time_offsets[i], timescale);
  } else {
    sample_info->cts_offset = TimeDelta::FromMicroseconds(0);
  }

  uint32 flags;
  if (i < trun.sample_flags.size()) {
    flags = trun.sample_flags[i];
  } else if (tfhd.has_default_sample_flags) {
    flags = tfhd.default_sample_flags;
  } else {
    flags = trex.default_sample_flags;
  }
  sample_info->is_keyframe = !(flags & kSampleIsDifferenceSampleFlagMask);
}

class CompareOffset {
 public:
  bool operator()(TrackRunInfo a, TrackRunInfo b) {
    int64 a_min = a.sample_start_offset;
    if (a.is_encrypted && a.cenc_start_offset < a_min)
      a_min = a.cenc_start_offset;
    int64 b_min = b.sample_start_offset;
    if (b.is_encrypted && b.cenc_start_offset < b_min)
      b_min = b.cenc_start_offset;
    return a_min < b_min;
  }
};

bool TrackRunIterator::Init(const Movie& moov, const MovieFragment& moof) {
  runs_.clear();

  for (size_t i = 0; i < moof.tracks.size(); i++) {
    const TrackFragment& traf = moof.tracks[i];

    const Track* trak = NULL;
    for (size_t t = 0; t < moov.tracks.size(); t++) {
      if (moov.tracks[t].header.track_id == traf.header.track_id)
        trak = &moov.tracks[t];
    }

    const TrackExtends* trex = NULL;
    for (size_t t = 0; t < moov.extends.tracks.size(); t++) {
      if (moov.extends.tracks[t].track_id == traf.header.track_id)
        trex = &moov.extends.tracks[t];
    }
    RCHECK(trak && trex);

    const ProtectionSchemeInfo* sinf = NULL;
    const SampleDescription& stsd =
        trak->media.information.sample_table.description;
    if (stsd.type == kAudio) {
      sinf = &stsd.audio_entries[0].sinf;
    } else if (stsd.type == kVideo) {
      sinf = &stsd.video_entries[0].sinf;
    } else {
      DVLOG(1) << "Skipping unhandled track type";
      continue;
    }

    if (sinf->info.track_encryption.is_encrypted) {
      // TODO(strobe): CENC recovery and testing (http://crbug.com/132351)
      DVLOG(1) << "Encrypted tracks not handled";
      continue;
    }

    for (size_t j = 0; j < traf.runs.size(); j++) {
      const TrackFragmentRun& trun = traf.runs[j];
      TrackRunInfo tri;
      tri.track_id = traf.header.track_id;
      tri.start_dts = TimeDeltaFromFrac(traf.decode_time.decode_time,
                                        trak->media.header.timescale);
      tri.sample_start_offset = trun.data_offset;

      tri.is_encrypted = false;
      tri.cenc_start_offset = 0;
      tri.cenc_total_size = 0;
      tri.default_cenc_size = 0;

      tri.samples.resize(trun.sample_count);

      for (size_t k = 0; k < trun.sample_count; k++) {
        PopulateSampleInfo(*trak, *trex, traf.header, trun, k, &tri.samples[k]);
      }
      runs_.push_back(tri);
    }
  }

  std::sort(runs_.begin(), runs_.end(), CompareOffset());
  run_itr_ = runs_.begin();
  min_clear_offset_itr_ = min_clear_offsets_.begin();
  ResetRun();
  return true;
}

void TrackRunIterator::AdvanceRun() {
  ++run_itr_;
  if (min_clear_offset_itr_ != min_clear_offsets_.end())
    ++min_clear_offset_itr_;
  ResetRun();
}

void TrackRunIterator::ResetRun() {
  if (!RunValid()) return;
  sample_dts_ = run_itr_->start_dts;
  sample_offset_ = run_itr_->sample_start_offset;
  sample_itr_ = run_itr_->samples.begin();
}

void TrackRunIterator::AdvanceSample() {
  DCHECK(SampleValid());
  sample_dts_ += sample_itr_->duration;
  sample_offset_ += sample_itr_->size;
  ++sample_itr_;
}

bool TrackRunIterator::NeedsCENC() {
  CHECK(!is_encrypted()) << "TODO(strobe): Implement CENC.";
  return is_encrypted();
}

bool TrackRunIterator::CacheCENC(const uint8* buf, int size) {
  LOG(FATAL) << "Not implemented";
  return false;
}

bool TrackRunIterator::RunValid() const {
  return run_itr_ != runs_.end();
}

bool TrackRunIterator::SampleValid() const {
  return RunValid() && (sample_itr_ != run_itr_->samples.end());
}

int64 TrackRunIterator::GetMaxClearOffset() {
  int64 offset = kint64max;

  if (SampleValid()) {
    offset = std::min(offset, sample_offset_);
    if (NeedsCENC()) {
      offset = std::min(offset, cenc_offset());
    }
  }
  if (min_clear_offset_itr_ != min_clear_offsets_.end()) {
    offset = std::min(offset, *min_clear_offset_itr_);
  }
  if (offset == kint64max) return 0;
  return offset;
}

TimeDelta TrackRunIterator::GetMinDecodeTimestamp() {
  TimeDelta dts = kInfiniteDuration();
  for (size_t i = 0; i < runs_.size(); i++) {
    if (runs_[i].start_dts < dts)
      dts = runs_[i].start_dts;
  }
  return dts;
}

uint32 TrackRunIterator::track_id() const {
  DCHECK(RunValid());
  return run_itr_->track_id;
}

bool TrackRunIterator::is_encrypted() const {
  DCHECK(RunValid());
  return run_itr_->is_encrypted;
}

int64 TrackRunIterator::cenc_offset() const {
  DCHECK(is_encrypted());
  return run_itr_->cenc_start_offset;
}

int TrackRunIterator::cenc_size() const {
  DCHECK(is_encrypted());
  return run_itr_->cenc_total_size;
}

int64 TrackRunIterator::offset() const {
  DCHECK(SampleValid());
  return sample_offset_;
}

int TrackRunIterator::size() const {
  DCHECK(SampleValid());
  return sample_itr_->size;
}

TimeDelta TrackRunIterator::dts() const {
  DCHECK(SampleValid());
  return sample_dts_;
}

TimeDelta TrackRunIterator::cts() const {
  DCHECK(SampleValid());
  return sample_dts_ + sample_itr_->cts_offset;
}

TimeDelta TrackRunIterator::duration() const {
  DCHECK(SampleValid());
  return sample_itr_->duration;
}

bool TrackRunIterator::is_keyframe() const {
  DCHECK(SampleValid());
  return sample_itr_->is_keyframe;
}

const FrameCENCInfo& TrackRunIterator::frame_cenc_info() {
  DCHECK(is_encrypted());
  return frame_cenc_info_;
}

}  // namespace mp4
}  // namespace media
