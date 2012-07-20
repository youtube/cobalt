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
    stream_.reset(new SourceBufferStream(config_));
    SetStreamInfo(kDefaultFramesPerSecond, kDefaultKeyframesPerSecond);

    // Set start time to the beginning of the stream.
    stream_->SetStartTime(base::TimeDelta());
  }

  void SetStreamInfo(int frames_per_second, int keyframes_per_second) {
    frames_per_second_ = frames_per_second;
    keyframes_per_second_ = keyframes_per_second;
    frame_duration_ = ConvertToFrameDuration(frames_per_second);
  }

  void NewSegmentAppend(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), true, NULL, 0);
  }

  void NewSegmentAppend(int starting_position, int number_of_buffers,
                        const uint8* data) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), true, data, kDataSize);
  }

  void NewSegmentAppend_OffsetFirstBuffer(
      int starting_position, int number_of_buffers,
      base::TimeDelta first_buffer_offset) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  first_buffer_offset, true, NULL, 0);
  }

  void NewSegmentAppend_ExpectFailure(
      int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), false, NULL, 0);
  }

  void AppendBuffers(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), true, NULL, 0);
  }

  void AppendBuffers(int starting_position, int number_of_buffers,
                     const uint8* data) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), true, data, kDataSize);
  }

  void Seek(int position) {
    stream_->Seek(position * frame_duration_);
  }

  void CheckExpectedRanges(const std::string& expected) {
    Ranges<base::TimeDelta> r = stream_->GetBufferedTime();

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
      if (!stream_->GetNextBuffer(&buffer))
        break;

      if (expect_keyframe && current_position == starting_position)
        EXPECT_TRUE(buffer->IsKeyframe());

      if (expected_data) {
        const uint8* actual_data = buffer->GetData();
        const int actual_size = buffer->GetDataSize();
        EXPECT_EQ(expected_size, actual_size);
        for (int i = 0; i < std::min(actual_size, expected_size); i++) {
          EXPECT_EQ(expected_data[i], actual_data[i]);
        }
      }

      EXPECT_EQ(buffer->GetDecodeTimestamp() / frame_duration_,
                current_position);
    }

    EXPECT_EQ(ending_position + 1, current_position);
  }

  void CheckNoNextBuffer() {
    scoped_refptr<StreamParserBuffer> buffer;
    EXPECT_FALSE(stream_->GetNextBuffer(&buffer));
  }

  base::TimeDelta frame_duration() const { return frame_duration_; }

  scoped_ptr<SourceBufferStream> stream_;
  VideoDecoderConfig config_;

 private:
  base::TimeDelta ConvertToFrameDuration(int frames_per_second) {
    return base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond / frames_per_second);
  }

  void AppendBuffers(int starting_position,
                     int number_of_buffers,
                     bool begin_media_segment,
                     base::TimeDelta first_buffer_offset,
                     bool expect_success,
                     const uint8* data,
                     int size) {
    if (begin_media_segment)
      stream_->OnNewMediaSegment(starting_position * frame_duration_);

    int keyframe_interval = frames_per_second_ / keyframes_per_second_;

    SourceBufferStream::BufferQueue queue;
    for (int i = 0; i < number_of_buffers; i++) {
      int position = starting_position + i;
      bool is_keyframe = position % keyframe_interval == 0;
      scoped_refptr<StreamParserBuffer> buffer =
          StreamParserBuffer::CopyFrom(data, size, is_keyframe);
      base::TimeDelta timestamp = frame_duration_ * position;

      if (i == 0)
        timestamp += first_buffer_offset;
      buffer->SetDecodeTimestamp(timestamp);

      // Simulate an IBB...BBP pattern in which all B-frames reference both
      // the I- and P-frames. For a GOP with playback order 12345, this would
      // result in a decode timestamp order of 15234.
      base::TimeDelta presentation_timestamp;
      if (is_keyframe) {
        presentation_timestamp = timestamp;
      } else if ((position - 1) % keyframe_interval == 0) {
        // This is the P-frame (first frame following the I-frame)
        presentation_timestamp =
            (timestamp + frame_duration_ * (keyframe_interval - 2));
      } else {
        presentation_timestamp = timestamp - frame_duration_;
      }
      buffer->SetTimestamp(presentation_timestamp);

      queue.push_back(buffer);
    }
    if (!queue.empty())
      EXPECT_EQ(expect_success, stream_->Append(queue));
  }

  int frames_per_second_;
  int keyframes_per_second_;
  base::TimeDelta frame_duration_;
  DISALLOW_COPY_AND_ASSIGN(SourceBufferStreamTest);
};

TEST_F(SourceBufferStreamTest, Append_SingleRange) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Append_SingleRange_OneBufferAtATime) {
  // Append 15 buffers starting at position 0, one buffer at a time.
  NewSegmentAppend(0, 1);
  for (int i = 1; i < 15; i++)
    AppendBuffers(i, 1);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Append_DisjointRanges) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 5);

  // Append 10 buffers at positions 15 through 24.
  NewSegmentAppend(15, 10);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [15,24) }");
  // Check buffers in ranges.
  Seek(0);
  CheckExpectedBuffers(0, 4);
  Seek(15);
  CheckExpectedBuffers(15, 24);
}

TEST_F(SourceBufferStreamTest, Append_AdjacentRanges) {
  // Append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10);

  // Append 11 buffers at positions 15 through 25.
  NewSegmentAppend(15, 11);

  // Append 5 buffers at positions 10 through 14 to bridge the gap.
  NewSegmentAppend(10, 5);

  // Check expected range.
  CheckExpectedRanges("{ [0,25) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 25);
}

TEST_F(SourceBufferStreamTest, Append_DoesNotBeginWithKeyframe) {
  // Append fails because the range doesn't begin with a keyframe.
  NewSegmentAppend_ExpectFailure(3, 2);

  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10);

  // Check expected range.
  CheckExpectedRanges("{ [5,14) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 14);

  // Append fails because the range doesn't begin with a keyframe.
  NewSegmentAppend_ExpectFailure(17, 3);

  CheckExpectedRanges("{ [5,14) }");
  Seek(5);
  CheckExpectedBuffers(5, 14);
}

TEST_F(SourceBufferStreamTest, Append_DoesNotBeginWithKeyframe_Adjacent) {
  // Append 8 buffers at positions 0 through 7.
  NewSegmentAppend(0, 8);

  // Now start a new media segment at position 8. Append should fail because
  // the media segment does not begin with a keyframe.
  NewSegmentAppend_ExpectFailure(8, 2);

  // Check expected range.
  CheckExpectedRanges("{ [0,7) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 7);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap) {
  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5);

  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15);

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
  NewSegmentAppend(6, 6);

  // Append 8 buffers at positions 5 through 12.
  NewSegmentAppend(5, 8);

  // Check expected range.
  CheckExpectedRanges("{ [5,12) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 12);
}

TEST_F(SourceBufferStreamTest, Start_Overlap) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 5);

  // Append 6 buffers at positions 10 through 15.
  NewSegmentAppend(10, 6);

  // Check expected range.
  CheckExpectedRanges("{ [5,15) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 15);
}

TEST_F(SourceBufferStreamTest, End_Overlap) {
  // Append 10 buffers at positions 10 through 19.
  NewSegmentAppend(10, 10);

  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10);

  // Check expected range.
  CheckExpectedRanges("{ [5,19) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 19);
}

TEST_F(SourceBufferStreamTest, End_Overlap_Several) {
  // Append 10 buffers at positions 10 through 19.
  NewSegmentAppend(10, 10);

  // Append 8 buffers at positions 5 through 12.
  NewSegmentAppend(5, 8);

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
  NewSegmentAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewSegmentAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewSegmentAppend(15, 2);

  // Check expected ranges.
  CheckExpectedRanges("{ [5,6) [10,11) [15,16) }");

  // Append buffers at positions 0 through 19.
  NewSegmentAppend(0, 20);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 19);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Several_Then_Merge) {
  // Append 2 buffers at positions 5 through 6.
  NewSegmentAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewSegmentAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewSegmentAppend(15, 2);

  // Append 2 buffers at positions 20 through 21.
  NewSegmentAppend(20, 2);

  // Append buffers at positions 0 through 19.
  NewSegmentAppend(0, 20);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,21) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 21);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to buffer at position 5.
  Seek(5);

  // Replace old data with new data.
  NewSegmentAppend(5, 10, &kDataB);

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
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 20, &kDataB);

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
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewSegmentAppend(5, 10, &kDataB);

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
  NewSegmentAppend(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewSegmentAppend(5, 5, &kDataB);

  // Then replace it again with different data.
  NewSegmentAppend(5, 5, &kDataC);

  // Now append 5 new buffers at positions 10 through 14.
  NewSegmentAppend(10, 5, &kDataC);

  // Now replace all the data entirely.
  NewSegmentAppend(5, 10, &kDataD);

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
  NewSegmentAppend(0, 10, &kDataA);

  // Seek to position 5, then add buffers to overlap data at that position.
  Seek(5);
  NewSegmentAppend(5, 10, &kDataB);

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
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to 10 and get buffer.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 10 buffers of new data at positions 10 through 19.
  NewSegmentAppend(10, 10, &kDataB);

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
  NewSegmentAppend(5, 10, &kDataA);

  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now replace the last 5 buffers with new data.
  NewSegmentAppend(10, 5, &kDataB);

  // The next 4 buffers should be the origial data, held in the track buffer.
  CheckExpectedBuffers(11, 14, &kDataA);

  // The next buffer is at position 15, so we should fail to fulfill the
  // request.
  CheckNoNextBuffer();

  // Now append data at 15 through 19 and check to make sure it's correct.
  NewSegmentAppend(15, 5, &kDataB);
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
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataB);

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
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataB);

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
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 8 buffers at positions 0 through 7.
  NewSegmentAppend(0, 8, &kDataB);

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
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 8 buffers at positions 0 through 7.
  NewSegmentAppend(0, 8, &kDataB);

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
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataB);

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
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 7 buffers at positions 0 through 6.
  NewSegmentAppend(0, 7, &kDataB);

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
  NewSegmentAppend(5, 15, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 13 buffers at positions 0 through 12.
  NewSegmentAppend(0, 13, &kDataB);

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
  NewSegmentAppend(5, 5, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 6 buffers at positions 0 through 5.
  NewSegmentAppend(0, 6, &kDataB);

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

// This test covers the case where new buffers end-overlap an existing, selected
// range, and there is no keyframe after the end of the new buffers, then the
// range gets split.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :                     |A a a a a A*a*|
// new  :            B b b b b B b b b b B
// after:           |B b b b b B b b b b B|
// new  :  A a a a a A
// after: |A a a a a A|       |B b b b b B|
// track:                                 |a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_NoKeyframeAfterNew2) {
  // Append 7 buffers at positions 10 through 16.
  NewSegmentAppend(10, 7, &kDataA);

  // Seek to position 15, then move to position 16.
  Seek(15);
  CheckExpectedBuffers(15, 15, &kDataA);

  // Now append 11 buffers at positions 5 through 15.
  NewSegmentAppend(5, 11, &kDataB);
  CheckExpectedRanges("{ [5,15) }");

  // Now do another end-overlap to split the range into two parts, where the
  // 2nd range should have the next buffer position.
  NewSegmentAppend(0, 6, &kDataA);
  CheckExpectedRanges("{ [0,5) [10,15) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(16, 16, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Add data to the 2nd range, should not be able to fulfill the next read
  // until we've added a keyframe.
  NewSegmentAppend(15, 1, &kDataB);
  CheckNoNextBuffer();
  for (int i = 16; i <= 19; i++) {
    AppendBuffers(i, 1, &kDataB);
    CheckNoNextBuffer();
  }

  // Now append a keyframe.
  AppendBuffers(20, 1, &kDataB);

  // We should be able to get the next buffer.
  CheckExpectedBuffers(20, 20, &kDataB, true);
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
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataB);

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
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to 10 then move to position 11.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataB);

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
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to beginning then move to position 2.
  Seek(0);
  CheckExpectedBuffers(0, 1, &kDataA);

  // Now append 3 buffers at positions 5 through 7.
  NewSegmentAppend(5, 3, &kDataB);

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
  NewSegmentAppend(0, 15, &kDataA);

  // Seek to 5 then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 3 buffers at positions 5 through 7.
  NewSegmentAppend(5, 3, &kDataB);

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
  NewSegmentAppend(0, 6);

  // Seek to beginning.
  Seek(0);
  CheckExpectedBuffers(0, 5, true);
}

TEST_F(SourceBufferStreamTest, Seek_NonKeyframe) {
  // Append 15 buffers at positions 0 through 14.
  NewSegmentAppend(0, 15);

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
  NewSegmentAppend(0, 2);
  Seek(0);
  CheckExpectedBuffers(0, 1);

  // Try to get buffer out of range.
  Seek(2);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Seek_InBetweenTimestamps) {
  // Append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10);

  base::TimeDelta bump = frame_duration() / 4;
  CHECK(bump > base::TimeDelta());

  // Seek to buffer a little after position 5.
  stream_->Seek(5 * frame_duration() + bump);
  CheckExpectedBuffers(5, 5, true);

  // Seek to buffer a little before position 5.
  stream_->Seek(5 * frame_duration() - bump);
  CheckExpectedBuffers(0, 0, true);
}

// This test will do a complete overlap of an existing range in order to add
// buffers to the track buffers. Then the test does a seek to another part of
// the stream. The SourceBufferStream should clear its internal track buffer in
// response to the Seek().
TEST_F(SourceBufferStreamTest, Seek_After_TrackBuffer_Filled) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 20, &kDataB);

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
  Seek(5);
  NewSegmentAppend_OffsetFirstBuffer(5, 5, bump);
  scoped_refptr<StreamParserBuffer> buffer;

  // GetNextBuffer() should return the next buffer at position (5 + |bump|).
  EXPECT_TRUE(stream_->GetNextBuffer(&buffer));
  EXPECT_EQ(buffer->GetDecodeTimestamp(), 5 * frame_duration() + bump);

  // Check rest of buffers.
  CheckExpectedBuffers(6, 9);

  // Seek to position 15.
  Seek(15);

  // Append 5 buffers at positions (15 + |bump|) through 19, where the media
  // segment begins at 15.
  NewSegmentAppend_OffsetFirstBuffer(15, 5, bump);

  // GetNextBuffer() should return the next buffer at position (15 + |bump|).
  EXPECT_TRUE(stream_->GetNextBuffer(&buffer));
  EXPECT_EQ(buffer->GetDecodeTimestamp(), 15 * frame_duration() + bump);

  // Check rest of buffers.
  CheckExpectedBuffers(16, 19);
}

TEST_F(SourceBufferStreamTest, Seek_BeforeStartOfSegment) {
  // Append 10 buffers at positions 5 through 14.
  NewSegmentAppend(5, 10);

  // Seek to a time before the first buffer in the range.
  Seek(0);

  // Should return buffers from the beginning of the range.
  CheckExpectedBuffers(5, 14);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_CompleteOverlap) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 4);

  // Append 5 buffers at positions 10 through 14, and seek to the beginning of
  // this range.
  NewSegmentAppend(10, 5);
  Seek(10);

  // Now seek to the beginning of the first range.
  Seek(0);

  // Completely overlap the old seek point.
  NewSegmentAppend(5, 15);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_CompleteOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 5 buffers at positions 15 through 19 and seek to beginning of the
  // range.
  NewSegmentAppend(15, 5);
  Seek(15);

  // Now seek position 5.
  Seek(5);

  // Completely overlap the old seek point.
  NewSegmentAppend(10, 15);

  // The seek at position 5 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_MiddleOverlap) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 5 through 19 and seek to position 15.
  NewSegmentAppend(5, 15);
  Seek(15);

  // Now seek to the beginning of the stream.
  Seek(0);

  // Overlap the middle of the range such that there are now three ranges.
  NewSegmentAppend(10, 3);
  CheckExpectedRanges("{ [0,1) [5,12) [15,19) }");

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_MiddleOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 10 through 24 and seek to position 20.
  NewSegmentAppend(10, 15);
  Seek(20);

  // Now seek to position 5.
  Seek(5);

  // Overlap the middle of the range such that it is now split into two ranges.
  NewSegmentAppend(15, 3);
  CheckExpectedRanges("{ [0,1) [10,17) [20,24) }");

  // The seek at position 5 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_StartOverlap) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 5 through 19 and seek to position 15.
  NewSegmentAppend(5, 15);
  Seek(15);

  // Now seek to the beginning of the stream.
  Seek(0);

  // Start overlap the old seek point.
  NewSegmentAppend(10, 10);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_StartOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 10 through 24 and seek to position 20.
  NewSegmentAppend(10, 15);
  Seek(20);

  // Now seek to position 5.
  Seek(5);

  // Start overlap the old seek point.
  NewSegmentAppend(15, 10);

  // The seek at time 0 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_EndOverlap) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 4);

  // Append 15 buffers at positions 10 through 24 and seek to start of range.
  NewSegmentAppend(10, 15);
  Seek(10);

  // Now seek to the beginning of the stream.
  Seek(0);

  // End overlap the old seek point.
  NewSegmentAppend(5, 10);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_EndOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewSegmentAppend(0, 2);

  // Append 15 buffers at positions 15 through 29 and seek to start of range.
  NewSegmentAppend(15, 15);
  Seek(15);

  // Now seek to position 5
  Seek(5);

  // End overlap the old seek point.
  NewSegmentAppend(10, 10);

  // The seek at time 0 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_AfterMerges) {
  // Append 5 buffers at positions 10 through 14.
  NewSegmentAppend(10, 5);

  // Seek to buffer at position 12.
  Seek(12);

  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5);

  // Make sure ranges are merged.
  CheckExpectedRanges("{ [5,14) }");

  // Make sure the next buffer is correct.
  CheckExpectedBuffers(10, 10);

  // Append 5 buffers at positions 15 through 19.
  NewSegmentAppend(15, 5);
  CheckExpectedRanges("{ [5,19) }");

  // Make sure the remaining next buffers are correct.
  CheckExpectedBuffers(11, 14);
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenAppend) {
  // Append 4 buffers at positions 0 through 3.
  NewSegmentAppend(0, 4);

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
  NewSegmentAppend(0, 10, &kDataA);
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataA);

  // Next buffer is at position 10, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Append 6 buffers at positons 5 through 10. This is to test that doing a
  // start-overlap successfully fulfills the read at position 10, even though
  // position 10 was unbuffered.
  NewSegmentAppend(5, 6, &kDataB);
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
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenCompleteOverlap) {
  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  NewSegmentAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do a complete overlap and test that this successfully fulfills the read
  // at position 15.
  NewSegmentAppend(5, 11, &kDataB);
  CheckExpectedBuffers(15, 15, &kDataB);

  // Then add 5 buffers from positions 16 though 20.
  AppendBuffers(16, 5, &kDataB);

  // Check the next 4 buffers are correct, which also effectively seeks to
  // position 20.
  CheckExpectedBuffers(16, 19, &kDataB);

  // Do a complete overlap and replace the buffer at position 20.
  NewSegmentAppend(0, 21, &kDataA);
  CheckExpectedBuffers(20, 20, &kDataA);
}

// This test covers the case where a range is stalled waiting for its next
// buffer, then an end-overlap causes the end of the range to be deleted.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenEndOverlap) {
  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  NewSegmentAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckExpectedRanges("{ [10,14) }");

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do an end overlap that causes the latter half of the range to be deleted.
  NewSegmentAppend(5, 6, &kDataB);
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
  NewSegmentAppend(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewSegmentAppend(5, 5, &kDataB);

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Next buffer is at position 10, so should not be able to fulfill the
  // request.
  CheckNoNextBuffer();

  // Now add 5 new buffers at positions 10 through 14.
  AppendBuffers(10, 5, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataB);
}

TEST_F(SourceBufferStreamTest, PresentationTimestampIndependence) {
  // Append 20 buffers at position 0.
  NewSegmentAppend(0, 20);
  Seek(0);

  int last_keyframe_idx = -1;
  base::TimeDelta last_keyframe_presentation_timestamp;
  base::TimeDelta last_p_frame_presentation_timestamp;

  // Check for IBB...BBP pattern.
  for (int i = 0; i < 20; i++) {
    scoped_refptr<StreamParserBuffer> buffer;
    ASSERT_TRUE(stream_->GetNextBuffer(&buffer));

    if (buffer->IsKeyframe()) {
      EXPECT_EQ(buffer->GetTimestamp(), buffer->GetDecodeTimestamp());
      last_keyframe_idx = i;
      last_keyframe_presentation_timestamp = buffer->GetTimestamp();
    } else if (i == last_keyframe_idx + 1) {
      ASSERT_NE(last_keyframe_idx, -1);
      last_p_frame_presentation_timestamp = buffer->GetTimestamp();
      EXPECT_LT(last_keyframe_presentation_timestamp,
                last_p_frame_presentation_timestamp);
    } else {
      EXPECT_GT(buffer->GetTimestamp(), last_keyframe_presentation_timestamp);
      EXPECT_LT(buffer->GetTimestamp(), last_p_frame_presentation_timestamp);
      EXPECT_LT(buffer->GetTimestamp(), buffer->GetDecodeTimestamp());
    }
  }
}

// TODO(vrk): Add unit tests where keyframes are unaligned between streams.
// (crbug.com/133557)

// TODO(vrk): Add unit tests with end of stream being called at interesting
// times.

}  // namespace media
