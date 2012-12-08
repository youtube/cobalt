// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <string>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "media/base/media_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kDefaultFramesPerSecond = 30;
static const int kDefaultKeyframesPerSecond = 6;
static const uint8 kDataA = 0x11;
static const uint8 kDataB = 0x33;
static const int kDataSize = 1;
static const gfx::Size kCodedSize(320, 240);

class SourceBufferStreamTest : public testing::Test {
 protected:
  SourceBufferStreamTest() {
    config_.Initialize(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                       VideoFrame::YV12, kCodedSize, gfx::Rect(kCodedSize),
                       kCodedSize, NULL, 0, false, false);
    stream_.reset(new SourceBufferStream(config_, LogCB()));
    SetStreamInfo(kDefaultFramesPerSecond, kDefaultKeyframesPerSecond);
  }

  void SetMemoryLimit(int buffers_of_data) {
    stream_->set_memory_limit(buffers_of_data * kDataSize);
  }

  void SetStreamInfo(int frames_per_second, int keyframes_per_second) {
    frames_per_second_ = frames_per_second;
    keyframes_per_second_ = keyframes_per_second;
    frame_duration_ = ConvertToFrameDuration(frames_per_second);
  }

  void NewSegmentAppend(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), true, &kDataA, kDataSize);
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
                  first_buffer_offset, true, &kDataA, kDataSize);
  }

  void NewSegmentAppend_ExpectFailure(
      int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  base::TimeDelta(), false, &kDataA, kDataSize);
  }

  void AppendBuffers(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), true, &kDataA, kDataSize);
  }

  void AppendBuffers(int starting_position, int number_of_buffers,
                     const uint8* data) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), true, data, kDataSize);
  }

  void NewSegmentAppend(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, false);
  }

  void AppendBuffers(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, false);
  }

  void NewSegmentAppendOneByOne(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, true);
  }

  void AppendBuffersOneByOne(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, true);
  }

  void Seek(int position) {
    stream_->Seek(position * frame_duration_);
  }

  void SeekToTimestamp(base::TimeDelta timestamp) {
    stream_->Seek(timestamp);
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
    EXPECT_EQ(expected, ss.str());
  }

  void CheckExpectedRangesByTimestamp(const std::string& expected) {
    Ranges<base::TimeDelta> r = stream_->GetBufferedTime();

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      int64 start = r.start(i).InMilliseconds();
      int64 end = r.end(i).InMilliseconds();
      ss << "[" << start << "," << end << ") ";
    }
    ss << "}";
    EXPECT_EQ(expected, ss.str());
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
      SourceBufferStream::Status status = stream_->GetNextBuffer(&buffer);

      EXPECT_NE(status, SourceBufferStream::kConfigChange);
      if (status != SourceBufferStream::kSuccess)
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

  void CheckExpectedBuffers(const std::string& expected) {
    std::vector<std::string> timestamps;
    base::SplitString(expected, ' ', &timestamps);
    for (size_t i = 0; i < timestamps.size(); i++) {
      scoped_refptr<StreamParserBuffer> buffer;
      SourceBufferStream::Status status = stream_->GetNextBuffer(&buffer);

      EXPECT_EQ(SourceBufferStream::kSuccess, status);
      if (status != SourceBufferStream::kSuccess)
        break;

      std::stringstream ss;
      ss << buffer->GetDecodeTimestamp().InMilliseconds();
      if (buffer->IsKeyframe())
        ss << "K";
      EXPECT_EQ(timestamps[i], ss.str());
    }
  }

  void CheckNoNextBuffer() {
    scoped_refptr<StreamParserBuffer> buffer;
    EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kNeedBuffer);
  }

  void CheckConfig(const VideoDecoderConfig& config) {
    const VideoDecoderConfig& actual = stream_->GetCurrentVideoDecoderConfig();
    EXPECT_TRUE(actual.Matches(config))
        << "Expected: " << config.AsHumanReadableString()
        << "\nActual: " << actual.AsHumanReadableString();
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

  void AppendBuffers(const std::string& buffers_to_append,
                     bool start_new_segment, bool one_by_one) {
    std::vector<std::string> timestamps;
    base::SplitString(buffers_to_append, ' ', &timestamps);

    CHECK_GT(timestamps.size(), 0u);

    SourceBufferStream::BufferQueue buffers;
    for (size_t i = 0; i < timestamps.size(); i++) {
      bool is_keyframe = false;
      if (EndsWith(timestamps[i], "K", true)) {
        is_keyframe = true;
        // Remove the "K" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }
      int time_in_ms;
      CHECK(base::StringToInt(timestamps[i], &time_in_ms));

      // Create buffer.
      scoped_refptr<StreamParserBuffer> buffer =
          StreamParserBuffer::CopyFrom(&kDataA, kDataSize, is_keyframe);
      base::TimeDelta timestamp =
          base::TimeDelta::FromMilliseconds(time_in_ms);
      buffer->SetDecodeTimestamp(timestamp);

      if (i == 0u && start_new_segment)
        stream_->OnNewMediaSegment(timestamp);

      buffers.push_back(buffer);
    }

    if (!one_by_one) {
      EXPECT_TRUE(stream_->Append(buffers));
      return;
    }

    // Append each buffer one by one.
    for (size_t i = 0; i < buffers.size(); i++) {
      SourceBufferStream::BufferQueue wrapper;
      wrapper.push_back(buffers[i]);
      EXPECT_TRUE(stream_->Append(wrapper));
    }
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

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next keyframe in a separate range.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a|          |A a a a a|
// new  :  B b b b b B
// after: |B b b b b B|                  |A a a a a|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_NoKeyframeAfterNew3) {
  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataA);

  // Append 5 buffers at positions 15 through 19.
  NewSegmentAppend(15, 5, &kDataA);

  // Check expected range.
  CheckExpectedRanges("{ [5,9) [15,19) }");

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 6 buffers at positions 0 through 5.
  NewSegmentAppend(0, 6, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,5) [15,19) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Let's fill in the gap, buffers 6 through 14.
  AppendBuffers(6, 9, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");

  // We should be able to get the next buffer.
  CheckExpectedBuffers(10, 14, &kDataB);

  // We should be able to get the next buffer.
  CheckExpectedBuffers(15, 19, &kDataA);
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

TEST_F(SourceBufferStreamTest, Overlap_OneByOne) {
  // Append 5 buffers starting at 10ms, 30ms apart.
  NewSegmentAppendOneByOne("10K 40 70 100 130");

  // The range ends at 160, accounting for the last buffer's duration.
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Overlap with 10 buffers starting at the beginning, appended one at a
  // time.
  NewSegmentAppend(0, 1, &kDataB);
  for (int i = 1; i < 10; i++)
    AppendBuffers(i, 1, &kDataB);

  // All data should be replaced.
  Seek(0);
  CheckExpectedRanges("{ [0,9) }");
  CheckExpectedBuffers(0, 9, &kDataB);
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne_DeleteGroup) {
  NewSegmentAppendOneByOne("10K 40 70 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 130ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(130));

  // Overlap with a new segment from 0 to 120ms.
  NewSegmentAppendOneByOne("0K 120");

  // Next buffer should still be 130ms.
  CheckExpectedBuffers("130K");

  // Check the final buffers is correct.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers("0K 120 130K");
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne_BetweenMediaSegments) {
  // Append 5 buffers starting at 110ms, 30ms apart.
  NewSegmentAppendOneByOne("110K 140 170 200 230");
  CheckExpectedRangesByTimestamp("{ [110,260) }");

  // Now append 2 media segments from 0ms to 210ms, 30ms apart. Note that the
  // old keyframe 110ms falls in between these two segments.
  NewSegmentAppendOneByOne("0K 30 60 90");
  NewSegmentAppendOneByOne("120K 150 180 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");

  // Check the final buffers is correct; the keyframe at 110ms should be
  // deleted.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers("0K 30 60 90 120K 150 180 210");
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer) {
  NewSegmentAppendOneByOne("10K 40 70 100K 125 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(10));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms.
  NewSegmentAppendOneByOne("0K 30 60 90 120K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Should return frames 70ms and 100ms from the track buffer, then switch
  // to the new data at 120K, then switch back to the old data at 130K. The
  // frame at 125ms that depended on keyframe 100ms should have been deleted.
  CheckExpectedBuffers("70 100K 120K 130K");

  // Check the final result: should not include data from the track buffer.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers("0K 30 60 90 120K 130K");
}

// Overlap the next keyframe after the end of the track buffer with a new
// keyframe.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70   100K
// new  :                     110K    130
// after: 0K   30   60   90  *110K*   130
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer2) {
  NewSegmentAppendOneByOne("10K 40 70 100K 125 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms; 70ms and 100ms go in track
  // buffer.
  NewSegmentAppendOneByOne("0K 30 60 90 120K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now overlap the keyframe at 120ms.
  NewSegmentAppendOneByOne("110K 130");

  // Should expect buffers 70ms and 100ms from the track buffer. Then it should
  // return the keyframe after the track buffer, which is at 110ms.
  CheckExpectedBuffers("70 100K 110K 130");
}

// Overlap the next keyframe after the end of the track buffer without a
// new keyframe.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70   100K
// new  :        50K   80   110          140
// after: 0K   30   50K   80   110   140 * (waiting for keyframe)
// track:               70   100K   120K   130K
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer3) {
  NewSegmentAppendOneByOne("10K 40 70 100K 125 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms; 70ms and 100ms go in track
  // buffer.
  NewSegmentAppendOneByOne("0K 30 60 90 120K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now overlap the keyframe at 120ms. There's no keyframe after 70ms, so 120ms
  // and 130ms go into the track buffer.
  NewSegmentAppendOneByOne("50K 80 110 140");

  // Should have all the buffers from the track buffer, then stall.
  CheckExpectedBuffers("70 100K 120K 130K");
  CheckNoNextBuffer();

  // Appending a keyframe should fulfill the read.
  AppendBuffersOneByOne("150K");
  CheckExpectedBuffers("150K");
  CheckNoNextBuffer();
}

// Overlap the next keyframe after the end of the track buffer with a keyframe
// that comes before the end of the track buffer.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70   100K
// new  :              80K  110          140
// after: 0K   30   60   *80K*  110   140
// track:               70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer4) {
  NewSegmentAppendOneByOne("10K 40 70 100K 125 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms; 70ms and 100ms go in track
  // buffer.
  NewSegmentAppendOneByOne("0K 30 60 90 120K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now append a keyframe at 80ms.
  NewSegmentAppendOneByOne("80K 110 140");

  CheckExpectedBuffers("70 80K 110 140");
  CheckNoNextBuffer();
}

// Overlap the next keyframe after the end of the track buffer with a keyframe
// that comes before the end of the track buffer, when the selected stream was
// waiting for the next keyframe.
// old  :   10K  40  *70*  100K
// new  : 0K   30   60   90   120
// after: 0K   30   60   90   120 * (waiting for keyframe)
// track:             70   100K
// new  :              80K  110          140
// after: 0K   30   60   *80K*  110   140
// track:               70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer5) {
  NewSegmentAppendOneByOne("10K 40 70 100K");
  CheckExpectedRangesByTimestamp("{ [10,130) }");

  // Seek to 70ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(70));
  CheckExpectedBuffers("10K 40");

  // Overlap with a new segment from 0 to 120ms; 70ms and 100ms go in track
  // buffer.
  NewSegmentAppendOneByOne("0K 30 60 90 120");
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Now append a keyframe at 80ms. The buffer at 100ms should be deleted from
  // the track buffer.
  NewSegmentAppendOneByOne("80K 110 140");

  CheckExpectedBuffers("70 80K 110 140");
  CheckNoNextBuffer();
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
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
  EXPECT_EQ(buffer->GetDecodeTimestamp(), 5 * frame_duration() + bump);

  // Check rest of buffers.
  CheckExpectedBuffers(6, 9);

  // Seek to position 15.
  Seek(15);

  // Append 5 buffers at positions (15 + |bump|) through 19, where the media
  // segment begins at 15.
  NewSegmentAppend_OffsetFirstBuffer(15, 5, bump);

  // GetNextBuffer() should return the next buffer at position (15 + |bump|).
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
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
    ASSERT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);

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

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFront) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 1, &kDataA);
  for (int i = 1; i < 20; i++)
    AppendBuffers(i, 1, &kDataA);

  // None of the buffers should trigger garbage collection, so all data should
  // be there as expected.
  CheckExpectedRanges("{ [0,19) }");
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataA);

  // Seek to the middle of the stream.
  Seek(10);

  // Append 5 buffers to the end of the stream.
  AppendBuffers(20, 5, &kDataA);

  // GC should have deleted the first 5 buffers.
  CheckExpectedRanges("{ [5,24) }");
  CheckExpectedBuffers(10, 24, &kDataA);
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFrontGOPsAtATime) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 20, &kDataA);

  // Seek to position 10.
  Seek(10);

  // Add one buffer to put the memory over the cap.
  AppendBuffers(20, 1, &kDataA);

  // GC should have deleted the first 5 buffers so that the range still begins
  // with a keyframe.
  CheckExpectedRanges("{ [5,20) }");
  CheckExpectedBuffers(10, 20, &kDataA);
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteBack) {
  // Set memory limit to 5 buffers.
  SetMemoryLimit(5);

  // Seek to position 0.
  Seek(0);

  // Append 20 buffers at positions 0 through 19.
  NewSegmentAppend(0, 20, &kDataA);

  // Should leave the first 5 buffers from 0 to 4 and the last GOP appended.
  CheckExpectedRanges("{ [0,4) [15,19) }");
  CheckExpectedBuffers(0, 4, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFrontAndBack) {
  // Set memory limit to 3 buffers.
  SetMemoryLimit(3);

  // Seek to position 15.
  Seek(15);

  // Append 40 buffers at positions 0 through 39.
  NewSegmentAppend(0, 40, &kDataA);

  // Should leave the GOP containing the seek position and the last GOP
  // appended.
  CheckExpectedRanges("{ [15,19) [35,39) }");
  CheckExpectedBuffers(15, 19, &kDataA);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteSeveralRanges) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 5);

  // Append 5 buffers at positions 10 through 14.
  NewSegmentAppend(10, 5);

  // Append 5 buffers at positions 20 through 24.
  NewSegmentAppend(20, 5);

  // Append 5 buffers at positions 30 through 34.
  NewSegmentAppend(30, 5);

  CheckExpectedRanges("{ [0,4) [10,14) [20,24) [30,34) }");

  // Seek to position 21.
  Seek(20);
  CheckExpectedBuffers(20, 20);

  // Set memory limit to 1 buffer.
  SetMemoryLimit(1);

  // Append 5 buffers at positions 40 through 44. This will trigger GC.
  NewSegmentAppend(40, 5);

  // Should delete everything except the GOP containing the current buffer and
  // the last GOP appended.
  CheckExpectedRanges("{ [20,24) [40,44) }");
  CheckExpectedBuffers(21, 24);
  CheckNoNextBuffer();

  // Continue appending into the last range to make sure it didn't break.
  AppendBuffers(45, 10);
  // Should only save last GOP appended.
  CheckExpectedRanges("{ [20,24) [50,54) }");

  // Make sure appending before and after the ranges didn't somehow break.
  SetMemoryLimit(100);
  NewSegmentAppend(0, 10);
  CheckExpectedRanges("{ [0,9) [20,24) [50,54) }");
  Seek(0);
  CheckExpectedBuffers(0, 9);

  NewSegmentAppend(90, 10);
  CheckExpectedRanges("{ [0,9) [20,24) [50,54) [90,99) }");
  Seek(50);
  CheckExpectedBuffers(50, 54);
  CheckNoNextBuffer();
  Seek(90);
  CheckExpectedBuffers(90, 99);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GarbageCollection_NoSeek) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 25 buffers at positions 0 through 24.
  NewSegmentAppend(0, 25, &kDataA);

  // GC deletes the first 5 buffers to keep the memory limit within cap.
  CheckExpectedRanges("{ [5,24) }");
  CheckNoNextBuffer();
  Seek(5);
  CheckExpectedBuffers(5, 24, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_PendingSeek) {
  // Append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataA);

  // Append 5 buffers at positions 25 through 29.
  NewSegmentAppend(25, 5, &kDataA);

  // Seek to position 15.
  Seek(15);
  CheckNoNextBuffer();

  CheckExpectedRanges("{ [0,9) [25,29) }");

  // Set memory limit to 5 buffers.
  SetMemoryLimit(5);

  // Append 5 buffers as positions 30 to 34 to trigger GC.
  AppendBuffers(30, 5, &kDataA);

  // The current algorithm will delete from the beginning until the memory is
  // under cap.
  CheckExpectedRanges("{ [30,34) }");

  // Expand memory limit again so that GC won't be triggered.
  SetMemoryLimit(100);

  // Append data to fulfill seek.
  NewSegmentAppend(15, 5, &kDataA);

  // Check to make sure all is well.
  CheckExpectedRanges("{ [15,19) [30,34) }");
  CheckExpectedBuffers(15, 19, &kDataA);
  Seek(30);
  CheckExpectedBuffers(30, 34, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_NeedsMoreData) {
  // Set memory limit to 15 buffers.
  SetMemoryLimit(15);

  // Append 10 buffers at positions 0 through 9.
  NewSegmentAppend(0, 10, &kDataA);

  // Advance next buffer position to 10.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataA);
  CheckNoNextBuffer();

  // Append 20 buffers at positions 15 through 34.
  NewSegmentAppend(15, 20, &kDataA);

  // GC should have saved the keyframe before the current seek position and the
  // data closest to the current seek position. It will also save the last GOP
  // appended.
  CheckExpectedRanges("{ [5,9) [15,19) [30,34) }");

  // Now fulfill the seek at position 10. This will make GC delete the data
  // before position 10 to keep it within cap.
  NewSegmentAppend(10, 5, &kDataA);
  CheckExpectedRanges("{ [10,19) [30,34) }");
  CheckExpectedBuffers(10, 19, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_TrackBuffer) {
  // Set memory limit to 3 buffers.
  SetMemoryLimit(3);

  // Seek to position 15.
  Seek(15);

  // Append 18 buffers at positions 0 through 17.
  NewSegmentAppend(0, 18, &kDataA);

  // Should leave GOP containing seek position.
  CheckExpectedRanges("{ [15,17) }");

  // Seek ahead to position 16.
  CheckExpectedBuffers(15, 15, &kDataA);

  // Completely overlap the existing buffers.
  NewSegmentAppend(0, 20, &kDataB);

  // Because buffers 16 and 17 are not keyframes, they are moved to the track
  // buffer upon overlap. The source buffer (i.e. not the track buffer) is now
  // waiting for the next keyframe.
  CheckExpectedRanges("{ [15,19) }");
  CheckExpectedBuffers(16, 17, &kDataA);
  CheckNoNextBuffer();

  // Now add a keyframe at position 20.
  AppendBuffers(20, 5, &kDataB);

  // Should garbage collect such that there are 5 frames remaining, starting at
  // the keyframe.
  CheckExpectedRanges("{ [20,24) }");
  CheckExpectedBuffers(20, 24, &kDataB);
  CheckNoNextBuffer();
}

// Test saving the last GOP appended when this GOP is the only GOP in its range.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP) {
  // Set memory limit to 3 and make sure the 4-byte GOP is not garbage
  // collected.
  SetMemoryLimit(3);
  NewSegmentAppend("0K 30 60 90");
  CheckExpectedRangesByTimestamp("{ [0,120) }");

  // Make sure you can continue appending data to this GOP; again, GC should not
  // wipe out anything.
  AppendBuffers("120");
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Set memory limit to 100 and append a 2nd range after this without
  // triggering GC.
  SetMemoryLimit(100);
  NewSegmentAppend("200K 230 260 290K 320 350");
  CheckExpectedRangesByTimestamp("{ [0,150) [200,380) }");

  // Seek to 290ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(290));

  // Now set memory limit to 3 and append a GOP in a separate range after the
  // selected range. Because it is after 290ms, this tests that the GOP is saved
  // when deleting from the back.
  SetMemoryLimit(3);
  NewSegmentAppend("500K 530 560 590");

  // Should save GOP with 290ms and last GOP appended.
  CheckExpectedRangesByTimestamp("{ [290,380) [500,620) }");

  // Continue appending to this GOP after GC.
  AppendBuffers("620");
  CheckExpectedRangesByTimestamp("{ [290,380) [500,650) }");
}

// Test saving the last GOP appended when this GOP is in the middle of a
// non-selected range.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Middle) {
  // Append 3 GOPs starting at 0ms, 30ms apart.
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Now set the memory limit to 1 and overlap the middle of the range with a
  // new GOP.
  SetMemoryLimit(1);
  NewSegmentAppend("80K 110 140");

  // This whole GOP should be saved, and should be able to continue appending
  // data to it.
  CheckExpectedRangesByTimestamp("{ [80,170) }");
  AppendBuffers("170");
  CheckExpectedRangesByTimestamp("{ [80,200) }");

  // Set memory limit to 100 and append a 2nd range after this without
  // triggering GC.
  SetMemoryLimit(100);
  NewSegmentAppend("400K 430 460 490K 520 550 580K 610 640");
  CheckExpectedRangesByTimestamp("{ [80,200) [400,670) }");

  // Seek to 80ms to make the first range the selected range.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(80));

  // Now set memory limit to 3 and append a GOP in the middle of the second
  // range. Because it is after the selected range, this tests that the GOP is
  // saved when deleting from the back.
  SetMemoryLimit(3);
  NewSegmentAppend("500K 530 560 590");

  // Should save the GOP containing the seek point and GOP that was last
  // appended.
  CheckExpectedRangesByTimestamp("{ [80,200) [500,620) }");

  // Continue appending to this GOP after GC.
  AppendBuffers("620");
  CheckExpectedRangesByTimestamp("{ [80,200) [500,650) }");
}

// Test saving the last GOP appended when the GOP containing the next buffer is
// adjacent to the last GOP appended.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected1) {
  // Append 3 GOPs at 0ms, 90ms, and 180ms.
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Seek to the GOP at 90ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(90));

  // Set the memory limit to 1, then overlap the GOP at 0.
  SetMemoryLimit(1);
  NewSegmentAppend("0K 30 60");

  // Should save the GOP at 0ms and 90ms.
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Seek to 0 and check all buffers.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));
  CheckExpectedBuffers("0K 30 60 90K 120 150");
  CheckNoNextBuffer();

  // Now seek back to 90ms and append a GOP at 180ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(90));
  NewSegmentAppend("180K 210 240");

  // Should save the GOP at 90ms and the GOP at 180ms.
  CheckExpectedRangesByTimestamp("{ [90,270) }");
  CheckExpectedBuffers("90K 120 150 180K 210 240");
  CheckNoNextBuffer();
}

// Test saving the last GOP appended when it is at the beginning or end of the
// selected range. This tests when the last GOP appended is before or after the
// GOP containing the next buffer, but not directly adjacent to this GOP.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected2) {
  // Append 4 GOPs starting at positions 0ms, 90ms, 180ms, 270ms.
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Seek to the last GOP at 270ms.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(270));

  // Set the memory limit to 1, then overlap the GOP at 90ms.
  SetMemoryLimit(1);
  NewSegmentAppend("90K 120 150");

  // Should save the GOP at 90ms and the GOP at 270ms.
  CheckExpectedRangesByTimestamp("{ [90,180) [270,360) }");

  // Set memory limit to 100 and add 3 GOPs to the end of the selected range
  // at 360ms, 450ms, and 540ms.
  SetMemoryLimit(100);
  NewSegmentAppend("360K 390 420 450K 480 510 540K 570 600");
  CheckExpectedRangesByTimestamp("{ [90,180) [270,630) }");

  // Constrain the memory limit again and overlap the GOP at 450ms to test
  // deleting from the back.
  SetMemoryLimit(1);
  NewSegmentAppend("450K 480 510");

  // Should save GOP at 270ms and the GOP at 450ms.
  CheckExpectedRangesByTimestamp("{ [270,360) [450,540) }");
}

// Test saving the last GOP appended when it is the same as the GOP containing
// the next buffer.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected3) {
  // Seek to start of stream.
  SeekToTimestamp(base::TimeDelta::FromMilliseconds(0));

  // Append 3 GOPs starting at 0ms, 90ms, 180ms.
  NewSegmentAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Set the memory limit to 1 then begin appending the start of a GOP starting
  // at 0ms.
  SetMemoryLimit(1);
  NewSegmentAppend("0K 30");

  // Should save the newly appended GOP, which is also the next GOP that will be
  // returned from the seek request.
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  // Check the buffers in the range.
  CheckExpectedBuffers("0K 30");
  CheckNoNextBuffer();

  // Continue appending to this buffer.
  AppendBuffers("60 90");

  // Should still save the rest of this GOP and should be able to fulfill the
  // read.
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("60 90");
  CheckNoNextBuffer();
}

// Currently disabled because of bug: crbug.com/140875.
TEST_F(SourceBufferStreamTest, DISABLED_GarbageCollection_WaitingForKeyframe) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  NewSegmentAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckExpectedRanges("{ [10,14) }");

  // We are now stalled at position 15.
  CheckNoNextBuffer();

  // Do an end overlap that causes the latter half of the range to be deleted.
  NewSegmentAppend(5, 6, &kDataA);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [5,10) }");

  // Append buffers from position 20 to 29. This should trigger GC.
  NewSegmentAppend(20, 10, &kDataA);

  // GC should keep the keyframe before the seek position 15, and the next 9
  // buffers closest to the seek position.
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [10,10) [20,28) }");

  // Fulfill the seek by appending one buffer at 15.
  NewSegmentAppend(15, 1, &kDataA);
  CheckExpectedBuffers(15, 15, &kDataA);
  CheckExpectedRanges("{ [15,15) [20,28) }");
}

TEST_F(SourceBufferStreamTest, ConfigChange_Basic) {
  gfx::Size kNewCodedSize(kCodedSize.width() * 2, kCodedSize.height() * 2);
  VideoDecoderConfig new_config(
      kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN, VideoFrame::YV12, kNewCodedSize,
      gfx::Rect(kNewCodedSize), kNewCodedSize, NULL, 0, false);
  ASSERT_FALSE(new_config.Matches(config_));

  Seek(0);
  CheckConfig(config_);

  // Append 5 buffers at positions 0 through 4
  NewSegmentAppend(0, 5, &kDataA);

  CheckConfig(config_);

  // Signal a config change.
  stream_->UpdateVideoConfig(new_config);

  // Make sure updating the config doesn't change anything since new_config
  // should not be associated with the buffer GetNextBuffer() will return.
  CheckConfig(config_);

  // Append 5 buffers at positions 5 through 9.
  NewSegmentAppend(5, 5, &kDataB);

  // Consume the buffers associated with the initial config.
  scoped_refptr<StreamParserBuffer> buffer;
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
    CheckConfig(config_);
  }

  // Verify the next attempt to get a buffer will signal that a config change
  // has happened.
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kConfigChange);

  // Verify that the new config is now returned.
  CheckConfig(new_config);

  // Consume the remaining buffers associated with the new config.
  for (int i = 0; i < 5; i++) {
    CheckConfig(new_config);
    EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kSuccess);
  }
}

TEST_F(SourceBufferStreamTest, ConfigChange_Seek) {
  scoped_refptr<StreamParserBuffer> buffer;
  gfx::Size kNewCodedSize(kCodedSize.width() * 2, kCodedSize.height() * 2);
  VideoDecoderConfig new_config(
      kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN, VideoFrame::YV12, kNewCodedSize,
      gfx::Rect(kNewCodedSize), kNewCodedSize, NULL, 0, false);

  Seek(0);
  NewSegmentAppend(0, 5, &kDataA);
  stream_->UpdateVideoConfig(new_config);
  NewSegmentAppend(5, 5, &kDataB);

  // Seek to the start of the buffers with the new config and make sure a
  // config change is signalled.
  CheckConfig(config_);
  Seek(5);
  CheckConfig(config_);
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kConfigChange);
  CheckConfig(new_config);
  CheckExpectedBuffers(5, 9, &kDataB);


  // Seek to the start which has a different config. Don't fetch any buffers and
  // seek back to buffers with the current config. Make sure a config change
  // isn't signalled in this case.
  CheckConfig(new_config);
  Seek(0);
  Seek(7);
  CheckExpectedBuffers(5, 9, &kDataB);


  // Seek to the start and make sure a config change is signalled.
  CheckConfig(new_config);
  Seek(0);
  CheckConfig(new_config);
  EXPECT_EQ(stream_->GetNextBuffer(&buffer), SourceBufferStream::kConfigChange);
  CheckConfig(config_);
  CheckExpectedBuffers(0, 4, &kDataA);
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration) {
  // Append 2 buffers at positions 5 through 6.
  NewSegmentAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewSegmentAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewSegmentAppend(15, 2);

  // Check expected ranges.
  CheckExpectedRanges("{ [5,6) [10,11) [15,16) }");

  // Set duration to be between buffers 6 and 10.
  stream_->OnSetDuration(frame_duration() * 8);

  // Should truncate the data after 6.
  CheckExpectedRanges("{ [5,6) }");

  // Adding data past the previous duration should still work.
  NewSegmentAppend(0, 20);
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_EdgeCase) {
  // Append 10 buffers at positions 10 through 19.
  NewSegmentAppend(10, 10);

  // Append 5 buffers at positions 25 through 29.
  NewSegmentAppend(25, 5);

  // Check expected ranges.
  CheckExpectedRanges("{ [10,19) [25,29) }");

  // Set duration to be right before buffer 25.
  stream_->OnSetDuration(frame_duration() * 25);

  // Should truncate the last range.
  CheckExpectedRanges("{ [10,19) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeletePartialRange) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 5);

  // Append 10 buffers at positions 10 through 19.
  NewSegmentAppend(10, 10);

  // Append 5 buffers at positions 25 through 29.
  NewSegmentAppend(25, 5);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [10,19) [25,29) }");

  // Set duration to be between buffers 13 and 14.
  stream_->OnSetDuration(frame_duration() * 14);

  // Should truncate the data after 13.
  CheckExpectedRanges("{ [0,4) [10,13) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeleteSelectedRange) {
  // Append 2 buffers at positions 5 through 6.
  NewSegmentAppend(5, 2);

  // Append 2 buffers at positions 10 through 11.
  NewSegmentAppend(10, 2);

  // Append 2 buffers at positions 15 through 16.
  NewSegmentAppend(15, 2);

  // Check expected ranges.
  CheckExpectedRanges("{ [5,6) [10,11) [15,16) }");

  // Seek to 10.
  Seek(10);

  // Set duration to be after position 3.
  stream_->OnSetDuration(frame_duration() * 4);

  // Expect everything to be deleted, and should not have next buffer anymore.
  CheckNoNextBuffer();
  CheckExpectedRanges("{ }");

  // Appending data at position 10 should not fulfill the seek.
  // (If the duration is set to be something smaller than the current seek
  // point, then the seek point is reset and the SourceBufferStream waits
  // for a new seek request. Therefore even if the data is re-appended, it
  // should not fulfill the old seek.)
  NewSegmentAppend(0, 15);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [0,14) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeletePartialSelectedRange) {
  // Append 5 buffers at positions 0 through 4.
  NewSegmentAppend(0, 5);

  // Append 20 buffers at positions 10 through 29.
  NewSegmentAppend(10, 20);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [10,29) }");

  // Seek to position 10.
  Seek(10);

  // Set duration to be between buffers 24 and 25.
  stream_->OnSetDuration(frame_duration() * 25);

  // Should truncate the data after 24.
  CheckExpectedRanges("{ [0,4) [10,24) }");

  // The seek position should not be lost.
  CheckExpectedBuffers(10, 10);

  // Now set the duration immediately after buffer 10.
  stream_->OnSetDuration(frame_duration() * 11);

  // Seek position should be reset.
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [0,4) [10,10) }");
}

// TODO(vrk): Add unit tests where keyframes are unaligned between streams.
// (crbug.com/133557)

// TODO(vrk): Add unit tests with end of stream being called at interesting
// times.

}  // namespace media
