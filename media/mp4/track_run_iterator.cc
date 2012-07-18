// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/track_run_iterator.h"

#include <algorithm>

#include "media/base/stream_parser_buffer.h"
#include "media/mp4/rcheck.h"

namespace media {
namespace mp4 {

struct SampleInfo {
  int size;
  int duration;
  int cts_offset;
  bool is_keyframe;
};

struct TrackRunInfo {
  uint32 track_id;
  std::vector<SampleInfo> samples;
  int64 timescale;
  int64 start_dts;
  int64 sample_start_offset;

  bool is_audio;
  const AudioSampleEntry* audio_description;
  const VideoSampleEntry* video_description;

  TrackRunInfo();
  ~TrackRunInfo();
};

TrackRunInfo::TrackRunInfo()
    : track_id(0),
      timescale(-1),
      start_dts(-1),
      sample_start_offset(-1),
      is_audio(false) {
}
TrackRunInfo::~TrackRunInfo() {}

TimeDelta TimeDeltaFromFrac(int64 numer, int64 denom) {
  DCHECK_LT((numer > 0 ? numer : -numer),
            kint64max / base::Time::kMicrosecondsPerSecond);
  return TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * numer / denom);
}

static const uint32 kSampleIsDifferenceSampleFlagMask = 0x10000;


TrackRunIterator::TrackRunIterator(const Movie* moov)
    : moov_(moov), sample_offset_(0) {
  CHECK(moov);
}
TrackRunIterator::~TrackRunIterator() {}

static void PopulateSampleInfo(const TrackExtends& trex,
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

  if (i < trun.sample_durations.size()) {
    sample_info->duration = trun.sample_durations[i];
  } else if (tfhd.default_sample_duration > 0) {
    sample_info->duration = tfhd.default_sample_duration;
  } else {
    sample_info->duration = trex.default_sample_duration;
  }

  if (i < trun.sample_composition_time_offsets.size()) {
    sample_info->cts_offset = trun.sample_composition_time_offsets[i];
  } else {
    sample_info->cts_offset = 0;
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

class CompareMinTrackRunDataOffset {
 public:
  bool operator()(const TrackRunInfo& a, const TrackRunInfo& b) {
    return a.sample_start_offset < b.sample_start_offset;
  }
};

bool TrackRunIterator::Init(const MovieFragment& moof) {
  runs_.clear();

  for (size_t i = 0; i < moof.tracks.size(); i++) {
    const TrackFragment& traf = moof.tracks[i];

    const Track* trak = NULL;
    for (size_t t = 0; t < moov_->tracks.size(); t++) {
      if (moov_->tracks[t].header.track_id == traf.header.track_id)
        trak = &moov_->tracks[t];
    }
    RCHECK(trak);

    const TrackExtends* trex = NULL;
    for (size_t t = 0; t < moov_->extends.tracks.size(); t++) {
      if (moov_->extends.tracks[t].track_id == traf.header.track_id)
        trex = &moov_->extends.tracks[t];
    }
    RCHECK(trex);

    const SampleDescription& stsd =
        trak->media.information.sample_table.description;
    if (stsd.type != kAudio && stsd.type != kVideo) {
      DVLOG(1) << "Skipping unhandled track type";
      continue;
    }
    int desc_idx = traf.header.sample_description_index;
    if (!desc_idx) desc_idx = trex->default_sample_description_index;
    desc_idx--;  // Descriptions are one-indexed in the file

    int64 run_start_dts = traf.decode_time.decode_time;

    for (size_t j = 0; j < traf.runs.size(); j++) {
      const TrackFragmentRun& trun = traf.runs[j];
      TrackRunInfo tri;
      tri.track_id = traf.header.track_id;
      tri.timescale = trak->media.header.timescale;
      tri.start_dts = run_start_dts;
      tri.sample_start_offset = trun.data_offset;

      tri.is_audio = (stsd.type == kAudio);
      if (tri.is_audio) {
        tri.audio_description = &stsd.audio_entries[desc_idx];
      } else {
        tri.video_description = &stsd.video_entries[desc_idx];
      }

      tri.samples.resize(trun.sample_count);
      for (size_t k = 0; k < trun.sample_count; k++) {
        PopulateSampleInfo(*trex, traf.header, trun, k, &tri.samples[k]);
        run_start_dts += tri.samples[k].duration;
      }
      runs_.push_back(tri);
    }
  }

  std::sort(runs_.begin(), runs_.end(), CompareMinTrackRunDataOffset());
  run_itr_ = runs_.begin();
  ResetRun();
  return true;
}

void TrackRunIterator::AdvanceRun() {
  ++run_itr_;
  ResetRun();
}

void TrackRunIterator::ResetRun() {
  if (!RunIsValid()) return;
  sample_dts_ = run_itr_->start_dts;
  sample_offset_ = run_itr_->sample_start_offset;
  sample_itr_ = run_itr_->samples.begin();
}

void TrackRunIterator::AdvanceSample() {
  DCHECK(SampleIsValid());
  sample_dts_ += sample_itr_->duration;
  sample_offset_ += sample_itr_->size;
  ++sample_itr_;
}

bool TrackRunIterator::RunIsValid() const {
  return run_itr_ != runs_.end();
}

bool TrackRunIterator::SampleIsValid() const {
  return RunIsValid() && (sample_itr_ != run_itr_->samples.end());
}

int64 TrackRunIterator::GetMaxClearOffset() {
  int64 offset = kint64max;

  if (SampleIsValid())
    offset = std::min(offset, sample_offset_);
  if (run_itr_ == runs_.end())
    return offset;
  std::vector<TrackRunInfo>::const_iterator next_run = run_itr_ + 1;
  if (next_run != runs_.end())
    offset = std::min(offset, next_run->sample_start_offset);
  return offset;
}

TimeDelta TrackRunIterator::GetMinDecodeTimestamp() {
  TimeDelta dts = kInfiniteDuration();
  for (size_t i = 0; i < runs_.size(); i++) {
    dts = std::min(dts, TimeDeltaFromFrac(runs_[i].start_dts,
                                          runs_[i].timescale));
  }
  return dts;
}

uint32 TrackRunIterator::track_id() const {
  DCHECK(RunIsValid());
  return run_itr_->track_id;
}

bool TrackRunIterator::is_encrypted() const {
  DCHECK(RunIsValid());
  return false;
}

bool TrackRunIterator::is_audio() const {
  DCHECK(RunIsValid());
  return run_itr_->is_audio;
}

const AudioSampleEntry& TrackRunIterator::audio_description() const {
  DCHECK(is_audio());
  DCHECK(run_itr_->audio_description);
  return *run_itr_->audio_description;
}

const VideoSampleEntry& TrackRunIterator::video_description() const {
  DCHECK(!is_audio());
  DCHECK(run_itr_->video_description);
  return *run_itr_->video_description;
}

int64 TrackRunIterator::sample_offset() const {
  DCHECK(SampleIsValid());
  return sample_offset_;
}

int TrackRunIterator::sample_size() const {
  DCHECK(SampleIsValid());
  return sample_itr_->size;
}

TimeDelta TrackRunIterator::dts() const {
  DCHECK(SampleIsValid());
  return TimeDeltaFromFrac(sample_dts_, run_itr_->timescale);
}

TimeDelta TrackRunIterator::cts() const {
  DCHECK(SampleIsValid());
  return TimeDeltaFromFrac(sample_dts_ + sample_itr_->cts_offset,
                           run_itr_->timescale);
}

TimeDelta TrackRunIterator::duration() const {
  DCHECK(SampleIsValid());
  return TimeDeltaFromFrac(sample_itr_->duration, run_itr_->timescale);
}

bool TrackRunIterator::is_keyframe() const {
  DCHECK(SampleIsValid());
  return sample_itr_->is_keyframe;
}

}  // namespace mp4
}  // namespace media
