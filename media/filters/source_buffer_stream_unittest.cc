// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include "base/logging.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kDefaultFramesPerSecond = 30;
static const int kDefaultKeyframesPerSecond = 6;
static const uint8 kDataA = 0x11;
static const uint8 kDataB = 0x33;
static const int kDataSize = 1;

class SourceBufferStreamTest : public testing::Test {
 protected:
  SourceBufferStreamTest() {
    SetStreamInfo(kDefaultFramesPerSecond, kDefaultKeyframesPerSecond);
  }

  void SetStreamInfo(int frames_per_second, int keyframes_per_second) {
    frames_per_second_ = frames_per_second;
    keyframes_per_second_ = keyframes_per_second;
    frame_duration_ = ConvertToFrameDuration(frames_per_second);
  }

  void AppendBuffers(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers,
                  base::TimeDelta(), true, NULL, 0);
  }

  void AppendBuffers(int starting_position, int number_of_buffers,
                     const uint8* data) {
    AppendBuffers(starting_position, number_of_buffers,
                  base::TimeDelta(), true, data, kDataSize);
  }

  void AppendBuffers(int starting_position, int number_of_buffers,
                     base::TimeDelta first_buffer_offset) {
    AppendBuffers(starting_position, number_of_buffers,
                  first_buffer_offset, true, NULL, 0);
  }

  void AppendBuffers_ExpectFailure(
      int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers,
                  base::TimeDelta(), false, NULL, 0);
  }

  void Seek(int position) {
    stream_.Seek(position * frame_duration_);
  }

  void CheckExpectedRanges(const std::string& expected) {
    Ranges<base::TimeDelta> r = stream_.GetBufferedTime();

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      int64 start = (r.start(i) / frame_duration_);
      int64 end = (r.end(i) / frame_duration_) - 1;
      ss << "[" << start << "," << end << ") ";
    }
    ss << "}";
    EXPECT_EQ(ss.str(), expected);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position) {
    CheckExpectedBuffers(starting_position, ending_position, false, NULL, 0);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position, bool expect_keyframe) {
    CheckExpectedBuffers(starting_position, ending_position, expect_keyframe,
                         NULL, 0);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position, const uint8* data) {
    CheckExpectedBuffers(starting_position, ending_position, false, data,
                         kDataSize);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position, const uint8* data,
      bool expect_keyframe) {
    CheckExpectedBuffers(starting_position, ending_position, expect_keyframe,
                         data, kDataSize);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position, bool expect_keyframe,
      const uint8* expected_data, int expected_size) {
    int current_position = starting_position;
    for (; current_position <= ending_position; current_position++) {
      scoped_refptr<StreamParserBuffer> buffer;
      if (!stream_.GetNextBuffer(&buffer))
        break;

      if (expect_keyframe && current_position == starting_position)
        EXPECT_TRUE(buffer->IsKeyframe());

      if (expected_data) {
        const uint8* actual_data = buffer->GetData();
        const int  actual_size = buffer->GetDataSize();
        EXPECT_EQ(expected_size, actual_size);
        for (int i = 0; i < std::min(actual_size, expected_size); i++) {
          EXPECT_EQ(expected_data[i], actual_data[i]);
        }
      }

      EXPECT_EQ(buffer->GetTimestamp() / frame_duration_, current_position);
    }

    EXPECT_EQ(ending_position + 1, current_position);
  }

  void CheckNoNextBuffer() {
    scoped_refptr<StreamParserBuffer> buffer;
    EXPECT_FALSE(stream_.GetNextBuffer(&buffer));
  }

  base::TimeDelta frame_duration() const { return frame_duration_; }

  SourceBufferStream stream_;

 private:
  base::TimeDelta ConvertToFrameDuration(int frames_per_second) {
    return base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond / frames_per_second);
  }

  void AppendBuffers(int starting_position, int number_of_buffers,
                     base::TimeDelta first_buffer_offset,
                     bool expect_success, const uint8* data, int size) {
    int keyframe_interval = frames_per_second_ / keyframes_per_second_;
    base::TimeDelta media_segment_start_time =
        starting_position * frame_duration_;

    SourceBufferStream::BufferQueue queue;
    for (int i = 0; i < number_of_buffers; i++) {
      int position = starting_position + i;
      bool is_keyframe = position % keyframe_interval == 0;
      scoped_refptr<StreamParserBuffer> buffer =
          StreamParserBuffer::CopyFrom(data, size, is_keyframe);
      base::TimeDelta duration = frame_duration_;
      base::TimeDelta timestamp = frame_duration_ * position;
      if (i == 0) {
        duration -= first_buffer_offset;
        timestamp += first_buffer_offset;
      }
      buffer->SetDuration(duration);
      buffer->SetTimestamp(timestamp);
      queue.push_back(buffer);
    }
    EXPECT_EQ(stream_.Append(queue, media_segment_start_time), expect_success);
  }

  int frames_per_second_;
  int keyframes_per_second_;
  base::TimeDelta frame_duration_;
  DISALLOW_COPY_AND_ASSIGN(SourceBufferStreamTest);
};

TEST_F(SourceBufferStreamTest, Append_SingleRange) {
  // Append 15 buffers at positions 0 through 14.
  AppendBuffers(0, 15);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Append_SingleRange_OneBufferAtATime) {
  // Append 15 buffers starting at position 0, one buffer at a time.
  for (int i = 0; i < 15; i++)
    AppendBuffers(i, 1);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Append_DisjointRanges) {
  // Append 5 buffers at positions 0 through 4.
  AppendBuffers(0, 5);

  // Append 10 buffers at positions 15 through 24.
  AppendBuffers(15, 10);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [15,24) }");
  // Check buffers in ranges.
  Seek(0);
  CheckExpectedBuffers(0, 4);
  Seek(15);
  CheckExpectedBuffers(15, 24);
}

TEST_F(SourceBufferStreamTest, Append_AdjacentRanges) {
  // Append 12 buffers at positions 0 through 11.
  AppendBuffers(0, 12);

  // Append 11 buffers at positions 15 through 25.
  AppendBuffers(15, 11);

  // Append 3 buffers at positions 12 through 14 to bridge the gap.
  AppendBuffers(12, 3);

  // Check expected range.
  CheckExpectedRanges("{ [0,25) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 25);
}

TEST_F(SourceBufferStreamTest, Append_DoesNotBeginWithKeyframe) {
  // Append fails because the range doesn't begin with a keyframe.
  AppendBuffers_ExpectFailure(3, 5);

  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10);

  // Check expected range.
  CheckExpectedRanges("{ [5,14) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 14);

  // Append fails because the range doesn't begin with a keyframe.
  AppendBuffers_ExpectFailure(17, 10);
  CheckExpectedRanges("{ [5,14) }");
  Seek(5);
  CheckExpectedBuffers(5, 14);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap) {
  // Append 5 buffers at positions 5 through 9.
  AppendBuffers(5, 5);

  // Append 15 buffers at positions 0 through 14.
  AppendBuffers(0, 15);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_EdgeCase) {
  // Make each frame a keyframe so that it's okay to overlap frames at any point
  // (instead of needing to respect keyframe boundaries).
  SetStreamInfo(30, 30);

  // Append 6 buffers at positions 6 through 11.
  AppendBuffers(6, 6);

  // Append 8 buffers at positions 5 through 12.
  AppendBuffers(5, 8);

  // Check expected range.
  CheckExpectedRanges("{ [5,12) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 12);
}

TEST_F(SourceBufferStreamTest, Start_Overlap) {
  // Append 5 buffers at positions 5 through 9.
  AppendBuffers(5, 5);

  // Append 6 buffers at positions 8 through 13.
  AppendBuffers(8, 6);

  // Check expected range.
  CheckExpectedRanges("{ [5,13) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 13);
}

TEST_F(SourceBufferStreamTest, End_Overlap) {
  // Append 10 buffers at positions 10 through 19.
  AppendBuffers(10, 10);

  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10);

  // Check expected range.
  CheckExpectedRanges("{ [5,19) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 19);
}

TEST_F(SourceBufferStreamTest, End_Overlap_Several) {
  // Append 10 buffers at positions 10 through 19.
  AppendBuffers(10, 10);

  // Append 8 buffers at positions 5 through 12.
  AppendBuffers(5, 8);

  // Check expected ranges: stream should not have kept buffers 13 and 14
  // because the keyframe on which they depended was overwritten.
  CheckExpectedRanges("{ [5,12) [15,19) }");

  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 12);
  CheckNoNextBuffer();

  Seek(19);
  CheckExpectedBuffers(15, 19);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Several) {
  // Append 2 buffers at positions 5 through 6.
  AppendBuffers(5, 2);

  // Append 2 buffers at positions 10 through 11.
  AppendBuffers(10, 2);

  // Append 2 buffers at positions 15 through 16.
  AppendBuffers(15, 2);

  // Check expected ranges.
  CheckExpectedRanges("{ [5,6) [10,11) [15,16) }");

  // Append buffers at positions 0 through 19.
  AppendBuffers(0, 20);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 19);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Several_Then_Merge) {
  // Append 2 buffers at positions 5 through 6.
  AppendBuffers(5, 2);

  // Append 2 buffers at positions 10 through 11.
  AppendBuffers(10, 2);

  // Append 2 buffers at positions 15 through 16.
  AppendBuffers(15, 2);

  // Append 2 buffers at positions 20 through 21.
  AppendBuffers(20, 2);

  // Append buffers at positions 0 through 19.
  AppendBuffers(0, 20);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,21) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 21);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to buffer at position 5.
  Seek(5);

  // Replace old data with new data.
  AppendBuffers(5, 10, &kDataB);

  // Check ranges are correct.
  CheckExpectedRanges("{ [5,14) }");

  // Check that data has been replaced with new data.
  CheckExpectedBuffers(5, 14, &kDataB);
}

// This test is testing that a client can append data to SourceBufferStream that
// overlaps the range from which the client is currently grabbing buffers. We
// would expect that the SourceBufferStream would return old data until it hits
// the keyframe of the new data, after which it will return the new data.
TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected_TrackBuffer) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  AppendBuffers(0, 20, &kDataB);

  // Check range is correct.
  CheckExpectedRanges("{ [0,19) }");

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);
  CheckExpectedBuffers(10, 10, &kDataB, true);

  // Expect rest of data to be new.
  CheckExpectedBuffers(11, 19, &kDataB);

  // Seek back to beginning; all data should be new.
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected_EdgeCase) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  AppendBuffers(5, 10, &kDataB);

  // Check ranges are correct.
  CheckExpectedRanges("{ [5,14) }");

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);
  CheckExpectedBuffers(10, 10, &kDataB, true);

  // Expect rest of data to be new.
  CheckExpectedBuffers(11, 14, &kDataB);

  // Seek back to beginning; all data should be new.
  Seek(5);
  CheckExpectedBuffers(5, 14, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [5,14) }");
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected_Multiple) {
  static const uint8 kDataC = 0x55;
  static const uint8 kDataD = 0x77;

  // Append 5 buffers at positions 5 through 9.
  AppendBuffers(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  AppendBuffers(5, 5, &kDataB);

  // Then replace it again with different data.
  AppendBuffers(5, 5, &kDataC);

  // Now append 5 new buffers at positions 10 through 14.
  AppendBuffers(10, 5, &kDataC);

  // Now replace all the data entirely.
  AppendBuffers(5, 10, &kDataD);

  // Expect buffers 6 through 9 to be DataA, and the remaining
  // buffers to be kDataD.
  CheckExpectedBuffers(6, 9, &kDataA);
  CheckExpectedBuffers(10, 14, &kDataD);

  // At this point we cannot fulfill request.
  CheckNoNextBuffer();

  // Seek back to beginning; all data should be new.
  Seek(5);
  CheckExpectedBuffers(5, 14, &kDataD);
}

TEST_F(SourceBufferStreamTest, Start_Overlap_Selected) {
  // Append 10 buffers at positions 0 through 9.
  AppendBuffers(0, 10, &kDataA);

  // Seek to position 5, then add buffers to overlap data at that position.
  Seek(5);
  AppendBuffers(5, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Because we seeked to a keyframe, the next buffers should all be new data.
  CheckExpectedBuffers(5, 14, &kDataB);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 14, &kDataB);
}

TEST_F(SourceBufferStreamTest, Start_Overlap_Selected_TrackBuffer) {
  // Append 15 buffers at positions 0 through 14.
  AppendBuffers(0, 15, &kDataA);

  // Seek to 10 and get buffer.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 10 buffers of new data at positions 10 through 19.
  AppendBuffers(10, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");

  // The next 4 buffers should be a from the old buffer, followed by a keyframe
  // from the new data.
  CheckExpectedBuffers(11, 14, &kDataA);
  CheckExpectedBuffers(15, 15, &kDataB, true);

  // The rest of the buffers should be new data.
  CheckExpectedBuffers(16, 19, &kDataB);

  // Now seek to the beginning; positions 0 through 9 should be the original
  // data, positions 10 through 19 should be the new data.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataA);
  CheckExpectedBuffers(10, 19, &kDataB);

  // Make sure range is still correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Start_Overlap_Selected_EdgeCase) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now replace the last 5 buffers with new data.
  AppendBuffers(10, 5, &kDataB);

  // The next 4 buffers should be the origial data, held in the track buffer.
  CheckExpectedBuffers(11, 14, &kDataA);

  // The next buffer is at position 15, so we should fail to fulfill the
  // request.
  CheckNoNextBuffer();

  // Now append data at 15 through 19 and check to make sure it's correct.
  AppendBuffers(15, 5, &kDataB);
  CheckExpectedBuffers(15, 19, &kDataB);

  // Seek to beginning of buffered range and check buffers.
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
  CheckExpectedBuffers(10, 19, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [5,19) }");
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer is a keyframe that's being overlapped by new
// buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           *A*a a a a A a a a a
// new  :  B b b b b B b b b b
// after:  B b b b b*B*b b b b A a a a a
TEST_F(SourceBufferStreamTest, End_Overlap_Selected) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 10 buffers at positions 0 through 9.
  AppendBuffers(0, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Because we seeked to a keyframe, the next buffers should be new.
  CheckExpectedBuffers(5, 9, &kDataB);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is after the newly appended buffers.
// In this particular case, the end overlap does not require a split.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a a a A a a*a*a|
// new  :  B b b b b B b b b b
// after: |B b b b b B b b b b A a a*a*a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_AfterEndOfNew_1) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  AppendBuffers(0, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Make sure rest of data is as expected.
  CheckExpectedBuffers(13, 14, &kDataA);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is after the newly appended buffers.
// In this particular case, the end overlap requires a split, and the next
// buffer is in the split range.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a a a A a a*a*a|
// new  :  B b b b b B b b
// after: |B b b b b B b b|   |A a a*a*a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_AfterEndOfNew_2) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 8 buffers at positions 0 through 7.
  AppendBuffers(0, 8, &kDataB);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,7) [10,14) }");

  // Make sure rest of data is as expected.
  CheckExpectedBuffers(13, 14, &kDataA);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 7, &kDataB);
  CheckNoNextBuffer();

  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is after the newly appended buffers.
// In this particular case, the end overlap requires a split, and the next
// buffer was in between the end of the new data and the split range.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a*a*a A a a a a|
// new  :  B b b b b B b b
// after: |B b b b b B b b|   |A a a a a|
// track:                 |a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_AfterEndOfNew_3) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 8 buffers at positions 0 through 7.
  AppendBuffers(0, 8, &kDataB);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,7) [10,14) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(8, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 7, &kDataB);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is overlapped by the new buffers.
// In this particular case, the end overlap does not require a split.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a*a*a A a a a a|
// new  :  B b b b b B b b b b
// after: |B b b b b B b b b b A a a a a|
// track:                 |a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_OverlappedByNew_1) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  AppendBuffers(0, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(8, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is overlapped by the new buffers.
// In this particular case, the end overlap requires a split, and the next
// keyframe after the track buffer is in the split range.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a A a a a a|
// new  :  B b b b b B b
// after: |B b b b b B b|     |A a a a a|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_OverlappedByNew_2) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 7 buffers at positions 0 through 6.
  AppendBuffers(0, 7, &kDataB);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,6) [10,14) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 6, &kDataB);
  CheckNoNextBuffer();

  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is overlapped by the new buffers.
// In this particular case, the end overlap requires a split, and the next
// keyframe after the track buffer is in the range with the new buffers.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a A a a a a A a a a a|
// new  :  B b b b b B b b b b B b b
// after: |B b b b b B b b b b B b b|   |A a a a a|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_OverlappedByNew_3) {
  // Append 15 buffers at positions 5 through 19.
  AppendBuffers(5, 15, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 13 buffers at positions 0 through 12.
  AppendBuffers(0, 13, &kDataB);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,12) [15,19) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe
  // from the new data.
  CheckExpectedBuffers(10, 10, &kDataB, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 12, &kDataB);
  CheckNoNextBuffer();

  Seek(15);
  CheckExpectedBuffers(15, 19, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and there is no keyframe after the end of the new buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a|
// new  :  B b b b b B
// after: |B b b b b B|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_NoKeyframeAfterNew) {
  // Append 5 buffers at positions 5 through 9.
  AppendBuffers(5, 5, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 6 buffers at positions 0 through 5.
  AppendBuffers(0, 6, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,5) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Let's fill in the gap, buffers 6 through 10.
  AppendBuffers(6, 5, &kDataB);

  // We should be able to get the next buffer.
  CheckExpectedBuffers(10, 10, &kDataB);
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is no split and the next buffer is a
// keyframe.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a a a a*A*a a a a A a a a a
// new  :            B b b b b
// after:  A a a a a*B*b b b b A a a a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_1) {
  // Append 15 buffers at positions 0 through 14.
  AppendBuffers(0, 15, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 5 buffers at positions 5 through 9.
  AppendBuffers(5, 5, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Check for next data; should be new data.
  CheckExpectedBuffers(5, 9, &kDataB);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is no split and the next buffer is
// after the new buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a a a a A a a a a A*a*a a a
// new  :            B b b b b
// after:  A a a a a B b b b b A*a*a a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_2) {
  // Append 15 buffers at positions 0 through 14.
  AppendBuffers(0, 15, &kDataA);

  // Seek to 10 then move to position 11.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 5 buffers at positions 5 through 9.
  AppendBuffers(5, 5, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Make sure data is correct.
  CheckExpectedBuffers(11, 14, &kDataA);
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is a split and the next buffer is
// before the new buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a*a*a a A a a a a A a a a a
// new  :            B b b
// after:  A a*a*a a B b b|   |A a a a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_3) {
  // Append 15 buffers at positions 0 through 14.
  AppendBuffers(0, 15, &kDataA);

  // Seek to beginning then move to position 2.
  Seek(0);
  CheckExpectedBuffers(0, 1, &kDataA);

  // Now append 3 buffers at positions 5 through 7.
  AppendBuffers(5, 3, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,7) [10,14) }");

  // Make sure data is correct.
  CheckExpectedBuffers(2, 4, &kDataA);
  CheckExpectedBuffers(5, 7, &kDataB);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is a split and the next buffer is after
// the new buffers but before the split range.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a a a a A a a*a*a A a a a a
// new  :            B b b
// after: |A a a a a B b b|   |A a a a a|
// track:                 |a a|
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_4) {
  // Append 15 buffers at positions 0 through 14.
  AppendBuffers(0, 15, &kDataA);

  // Seek to 5 then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 3 buffers at positions 5 through 7.
  AppendBuffers(5, 3, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,7) [10,14) }");

  // Buffers 8 and 9 should be in the track buffer.
  CheckExpectedBuffers(8, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 7, &kDataB);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
}

TEST_F(SourceBufferStreamTest, Seek_Keyframe) {
  // Append 6 buffers at positions 0 through 5.
  AppendBuffers(0, 6);

  // Seek to beginning.
  Seek(0);
  CheckExpectedBuffers(0, 5, true);
}

TEST_F(SourceBufferStreamTest, Seek_NonKeyframe) {
  // Append 15 buffers at positions 0 through 14.
  AppendBuffers(0, 15);

  // Seek to buffer at position 13.
  Seek(13);

  // Expect seeking back to the nearest keyframe.
  CheckExpectedBuffers(10, 14, true);

  // Seek to buffer at position 3.
  Seek(3);

  // Expect seeking back to the nearest keyframe.
  CheckExpectedBuffers(0, 3, true);
}

TEST_F(SourceBufferStreamTest, Seek_NotBuffered) {
  // Seek to beginning.
  Seek(0);

  // Try to get buffer; nothing's appended.
  CheckNoNextBuffer();

  // Append 2 buffers at positions 0.
  AppendBuffers(0, 2);
  Seek(0);
  CheckExpectedBuffers(0, 1);

  // Try to get buffer out of range.
  Seek(2);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Seek_InBetweenTimestamps) {
  // Append 10 buffers at positions 0 through 9.
  AppendBuffers(0, 10);

  base::TimeDelta bump = frame_duration() / 4;
  CHECK(bump > base::TimeDelta());

  // Seek to buffer a little after position 5.
  stream_.Seek(5 * frame_duration() + bump);
  CheckExpectedBuffers(5, 5, true);

  // Seek to buffer a little before position 5.
  stream_.Seek(5 * frame_duration() - bump);
  CheckExpectedBuffers(0, 0, true);
}

// This test will do a complete overlap of an existing range in order to add
// buffers to the track buffers. Then the test does a seek to another part of
// the stream. The SourceBufferStream should clear its internal track buffer in
// response to the Seek().
TEST_F(SourceBufferStreamTest, Seek_After_TrackBuffer_Filled) {
  // Append 10 buffers at positions 5 through 14.
  AppendBuffers(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  AppendBuffers(0, 20, &kDataB);

  // Check range is correct.
  CheckExpectedRanges("{ [0,19) }");

  // Seek to beginning; all data should be new.
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Seek_StartOfSegment) {
  base::TimeDelta bump = frame_duration() / 4;
  CHECK(bump > base::TimeDelta());

  // Append 5 buffers at position (5 + |bump|) through 9, where the media
  // segment begins at position 5.
  AppendBuffers(5, 5, bump);
  scoped_refptr<StreamParserBuffer> buffer;

  // GetNextBuffer() should return the next buffer at position (5 + |bump|).
  EXPECT_TRUE(stream_.GetNextBuffer(&buffer));
  EXPECT_EQ(buffer->GetTimestamp(), 5 * frame_duration() + bump);

  // Check rest of buffers.
  CheckExpectedBuffers(6, 9);

  // Seek to position 15.
  Seek(15);

  // Append 5 buffers at positions (15 + |bump|) through 19, where the media
  // segment begins at 15.
  AppendBuffers(15, 5, bump);

  // GetNextBuffer() should return the next buffer at position (15 + |bump|).
  EXPECT_TRUE(stream_.GetNextBuffer(&buffer));
  EXPECT_EQ(buffer->GetTimestamp(), 15 * frame_duration() + bump);

  // Check rest of buffers.
  CheckExpectedBuffers(16, 19);
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_AfterMerges) {
  // Append 5 buffers at positions 10 through 14.
  AppendBuffers(10, 5);

  // Seek to buffer at position 12.
  Seek(12);

  // Append 5 buffers at positions 5 through 9.
  AppendBuffers(5, 5);

  // Make sure ranges are merged.
  CheckExpectedRanges("{ [5,14) }");

  // Make sure the next buffer is correct.
  CheckExpectedBuffers(10, 10);

  // Append 5 buffers at positions 15 through 19.
  AppendBuffers(15, 5);
  CheckExpectedRanges("{ [5,19) }");

  // Make sure the remaining next buffers are correct.
  CheckExpectedBuffers(11, 14);
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenAppend) {
  // Append 4 buffers at positions 0 through 3.
  AppendBuffers(0, 4);

  // Seek to buffer at position 0 and get all buffers.
  Seek(0);
  CheckExpectedBuffers(0, 3);

  // Next buffer is at position 4, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Append 2 buffers at positions 4 through 5.
  AppendBuffers(4, 2);
  CheckExpectedBuffers(4, 5);
}

// This test covers the case where new buffers start-overlap a range whose next
// buffer is not buffered.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenStartOverlap) {
  // Append 10 buffers at positions 0 through 9 and exhaust the buffers.
  AppendBuffers(0, 10, &kDataA);
  CheckExpectedBuffers(0, 9, &kDataA);

  // Next buffer is at position 10, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Append 6 buffers at positons 5 through 10. This is to test that doing a
  // start-overlap successfully fulfills the read at position 10, even though
  // position 10 was unbuffered.
  AppendBuffers(5, 6, &kDataB);
  CheckExpectedBuffers(10, 10, &kDataB);

  // Then add 5 buffers from positions 11 though 15.
  AppendBuffers(11, 5, &kDataB);

  // Check the next 4 buffers are correct, which also effectively seeks to
  // position 15.
  CheckExpectedBuffers(11, 14, &kDataB);

  // Replace the next buffer at position 15 with another start overlap.
  AppendBuffers(15, 2, &kDataA);
  CheckExpectedBuffers(15, 16, &kDataA);
}

// This test covers the case where new buffers completely overlap a range
// whose next buffer is not buffered.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenCompeteOverlap) {
  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  AppendBuffers(10, 5, &kDataA);
  CheckExpectedBuffers(10, 14, &kDataA);

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do a complete overlap and test that this successfully fulfills the read
  // at position 15.
  AppendBuffers(5, 11, &kDataB);
  CheckExpectedBuffers(15, 15, &kDataB);

  // Then add 5 buffers from positions 16 though 20.
  AppendBuffers(16, 5, &kDataB);

  // Check the next 4 buffers are correct, which also effectively seeks to
  // position 20.
  CheckExpectedBuffers(16, 19, &kDataB);

  // Do a complete overlap and replace the buffer at position 20.
  AppendBuffers(0, 21, &kDataA);
  CheckExpectedBuffers(20, 20, &kDataA);
}

// This test covers the case where a range is stalled waiting for its next
// buffer, then an end-overlap causes the end of the range to be deleted.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenEndOverlap) {
  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  AppendBuffers(10, 5, &kDataA);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckExpectedRanges("{ [10,14) }");

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do an end overlap that causes the latter half of the range to be deleted.
  AppendBuffers(5, 6, &kDataB);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [5,10) }");

  // Fill in the gap. Getting the next buffer should still stall at position 15.
  for (int i = 11; i <= 14; i++) {
    AppendBuffers(i, 1, &kDataB);
    CheckNoNextBuffer();
  }

  // Append the buffer at position 15 and check to make sure all is correct.
  AppendBuffers(15, 1);
  CheckExpectedBuffers(15, 15);
  CheckExpectedRanges("{ [5,15) }");
}

// This test is testing the "next buffer" logic after a complete overlap. In
// this scenario, when the track buffer is exhausted, there is no buffered data
// to fulfill the request. The SourceBufferStream should be able to fulfill the
// request when the data is later appended, and should not lose track of the
// "next buffer" position.
TEST_F(SourceBufferStreamTest, GetNextBuffer_Overlap_Selected_Complete) {
  // Append 5 buffers at positions 5 through 9.
  AppendBuffers(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  AppendBuffers(5, 5, &kDataB);

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Next buffer is at position 10, so should not be able to fulfill the
  // request.
  CheckNoNextBuffer();

  // Now add 5 new buffers at positions 10 through 14.
  AppendBuffers(10, 5, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataB);
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_NoSeek) {
  // Append 5 buffers at position 5.
  AppendBuffers(5, 5);

  // Should receive buffers from the start without needing to seek.
  CheckExpectedBuffers(5, 5);
}

// TODO(vrk): Add unit tests where keyframes are unaligned between streams.
// (crbug.com/133557)

}  // namespace media
