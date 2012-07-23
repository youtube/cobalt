// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/mp4/box_definitions.h"
#include "media/mp4/rcheck.h"
#include "media/mp4/track_run_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"

// The sum of the elements in a vector initialized with SumAscending,
// less the value of the last element.
static const int kSumAscending1 = 45;

static const int kAudioScale = 48000;
static const int kVideoScale = 25;

static const uint32 kSampleIsDifferenceSampleFlagMask = 0x10000;

namespace media {
namespace mp4 {

class TrackRunIteratorTest : public testing::Test {
 public:
  TrackRunIteratorTest() {
    CreateMovie();
  }

 protected:
  Movie moov_;
  scoped_ptr<TrackRunIterator> iter_;

  void CreateMovie() {
    moov_.header.timescale = 1000;
    moov_.tracks.resize(3);
    moov_.extends.tracks.resize(2);
    moov_.tracks[0].header.track_id = 1;
    moov_.tracks[0].media.header.timescale = kAudioScale;
    SampleDescription& desc1 =
        moov_.tracks[0].media.information.sample_table.description;
    desc1.type = kAudio;
    desc1.audio_entries.resize(1);
    desc1.audio_entries[0].format = FOURCC_MP4A;
    moov_.extends.tracks[0].track_id = 1;

    moov_.tracks[1].header.track_id = 2;
    moov_.tracks[1].media.header.timescale = kVideoScale;
    SampleDescription& desc2 =
        moov_.tracks[1].media.information.sample_table.description;
    desc2.type = kVideo;
    desc2.video_entries.resize(1);
    desc2.video_entries[0].sinf.info.track_encryption.is_encrypted = false;
    moov_.extends.tracks[1].track_id = 2;

    moov_.tracks[2].header.track_id = 3;
    moov_.tracks[2].media.information.sample_table.description.type = kHint;
  }

  MovieFragment CreateFragment() {
    MovieFragment moof;
    moof.tracks.resize(2);
    moof.tracks[0].decode_time.decode_time = 0;
    moof.tracks[0].header.track_id = 1;
    moof.tracks[0].header.has_default_sample_flags = true;
    moof.tracks[0].header.default_sample_duration = 1024;
    moof.tracks[0].header.default_sample_size = 4;
    moof.tracks[0].runs.resize(2);
    moof.tracks[0].runs[0].sample_count = 10;
    moof.tracks[0].runs[0].data_offset = 100;
    SetAscending(&moof.tracks[0].runs[0].sample_sizes);

    moof.tracks[0].runs[1].sample_count = 10;
    moof.tracks[0].runs[1].data_offset = 10000;

    moof.tracks[1].header.track_id = 2;
    moof.tracks[1].header.has_default_sample_flags = false;
    moof.tracks[1].decode_time.decode_time = 10;
    moof.tracks[1].runs.resize(1);
    moof.tracks[1].runs[0].sample_count = 10;
    moof.tracks[1].runs[0].data_offset = 200;
    SetAscending(&moof.tracks[1].runs[0].sample_sizes);
    SetAscending(&moof.tracks[1].runs[0].sample_durations);
    moof.tracks[1].runs[0].sample_flags.resize(10);
    for (size_t i = 1; i < moof.tracks[1].runs[0].sample_flags.size(); i++) {
      moof.tracks[1].runs[0].sample_flags[i] =
          kSampleIsDifferenceSampleFlagMask;
    }

    return moof;
  }

  void SetAscending(std::vector<uint32>* vec) {
    vec->resize(10);
    for (size_t i = 0; i < vec->size(); i++)
      (*vec)[i] = i+1;
  }
};

TEST_F(TrackRunIteratorTest, NoRunsTest) {
  iter_.reset(new TrackRunIterator(&moov_));
  ASSERT_TRUE(iter_->Init(MovieFragment()));
  EXPECT_FALSE(iter_->RunIsValid());
  EXPECT_FALSE(iter_->SampleIsValid());
}

TEST_F(TrackRunIteratorTest, BasicOperationTest) {
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();

  // Test that runs are sorted correctly, and that properties of the initial
  // sample of the first run are correct
  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_TRUE(iter_->RunIsValid());
  EXPECT_FALSE(iter_->is_encrypted());
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->sample_offset(), 100);
  EXPECT_EQ(iter_->sample_size(), 1);
  EXPECT_EQ(iter_->dts(), TimeDeltaFromFrac(0, kAudioScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromFrac(0, kAudioScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromFrac(1024, kAudioScale));
  EXPECT_TRUE(iter_->is_keyframe());

  // Advance to the last sample in the current run, and test its properties
  for (int i = 0; i < 9; i++) iter_->AdvanceSample();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->sample_offset(), 100 + kSumAscending1);
  EXPECT_EQ(iter_->sample_size(), 10);
  EXPECT_EQ(iter_->dts(), TimeDeltaFromFrac(1024 * 9, kAudioScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromFrac(1024, kAudioScale));
  EXPECT_TRUE(iter_->is_keyframe());

  // Test end-of-run
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->SampleIsValid());

  // Test last sample of next run
  iter_->AdvanceRun();
  EXPECT_TRUE(iter_->is_keyframe());
  for (int i = 0; i < 9; i++) iter_->AdvanceSample();
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_EQ(iter_->sample_offset(), 200 + kSumAscending1);
  EXPECT_EQ(iter_->sample_size(), 10);
  int64 base_dts = kSumAscending1 + moof.tracks[1].decode_time.decode_time;
  EXPECT_EQ(iter_->dts(), TimeDeltaFromFrac(base_dts, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromFrac(10, kVideoScale));
  EXPECT_FALSE(iter_->is_keyframe());

  // Test final run
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->dts(), TimeDeltaFromFrac(1024 * 10, kAudioScale));
  iter_->AdvanceSample();
  EXPECT_EQ(moof.tracks[0].runs[1].data_offset +
            moof.tracks[0].header.default_sample_size,
            iter_->sample_offset());
  iter_->AdvanceRun();
  EXPECT_FALSE(iter_->RunIsValid());
}

TEST_F(TrackRunIteratorTest, TrackExtendsDefaultsTest) {
  moov_.extends.tracks[0].default_sample_duration = 50;
  moov_.extends.tracks[0].default_sample_size = 3;
  moov_.extends.tracks[0].default_sample_flags =
    kSampleIsDifferenceSampleFlagMask;
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();
  moof.tracks[0].header.has_default_sample_flags = false;
  moof.tracks[0].header.default_sample_size = 0;
  moof.tracks[0].header.default_sample_duration = 0;
  moof.tracks[0].runs[0].sample_sizes.clear();
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->is_keyframe());
  EXPECT_EQ(iter_->sample_size(), 3);
  EXPECT_EQ(iter_->sample_offset(), moof.tracks[0].runs[0].data_offset + 3);
  EXPECT_EQ(iter_->duration(), TimeDeltaFromFrac(50, kAudioScale));
  EXPECT_EQ(iter_->dts(), TimeDeltaFromFrac(50, kAudioScale));
}

TEST_F(TrackRunIteratorTest, FirstSampleFlagTest) {
  // Ensure that keyframes are flagged correctly in the face of BMFF boxes which
  // explicitly specify the flags for the first sample in a run and rely on
  // defaults for all subsequent samples
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();
  moof.tracks[1].header.has_default_sample_flags = true;
  moof.tracks[1].header.default_sample_flags =
    kSampleIsDifferenceSampleFlagMask;
  moof.tracks[1].runs[0].sample_flags.resize(1);
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_TRUE(iter_->is_keyframe());
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->is_keyframe());
}

TEST_F(TrackRunIteratorTest, MinDecodeTest) {
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();
  moof.tracks[0].decode_time.decode_time = kAudioScale;
  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_EQ(TimeDeltaFromFrac(moof.tracks[1].decode_time.decode_time,
                              kVideoScale),
            iter_->GetMinDecodeTimestamp());
}

TEST_F(TrackRunIteratorTest, ReorderingTest) {
  iter_.reset(new TrackRunIterator(&moov_));
  MovieFragment moof = CreateFragment();
  std::vector<int32>& cts_offsets =
    moof.tracks[1].runs[0].sample_composition_time_offsets;
  cts_offsets.resize(10);
  cts_offsets[0] = 2;
  cts_offsets[1] = -1;
  moof.tracks[1].decode_time.decode_time = 0;
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->dts(), TimeDeltaFromFrac(0, kVideoScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromFrac(2, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromFrac(1, kVideoScale));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), TimeDeltaFromFrac(1, kVideoScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromFrac(0, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromFrac(2, kVideoScale));
}

}  // namespace mp4
}  // namespace media
