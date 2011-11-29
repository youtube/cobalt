// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>

#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_frame_builder.h"
#include "testing/platform_test.h"

namespace spdy {

namespace test {

std::string HexDumpWithMarks(const unsigned char* data, int length,
                             const bool* marks, int mark_length) {
  static const char kHexChars[] = "0123456789ABCDEF";
  static const int kColumns = 4;

  const int kSizeLimit = 1024;
  if (length > kSizeLimit || mark_length > kSizeLimit) {
    LOG(ERROR) << "Only dumping first " << kSizeLimit << " bytes.";
    length = std::min(length, kSizeLimit);
    mark_length = std::min(mark_length, kSizeLimit);
  }

  std::string hex;
  for (const unsigned char* row = data; length > 0;
       row += kColumns, length -= kColumns) {
    for (const unsigned char *p = row; p < row + 4; ++p) {
      if (p < row + length) {
        const bool mark =
            (marks && (p - data) < mark_length && marks[p - data]);
        hex += mark ? '*' : ' ';
        hex += kHexChars[(*p & 0xf0) >> 4];
        hex += kHexChars[*p & 0x0f];
        hex += mark ? '*' : ' ';
      } else {
        hex += "    ";
      }
    }
    hex = hex + "  ";

    for (const unsigned char *p = row; p < row + 4 && p < row + length; ++p)
      hex += (*p >= 0x20 && *p <= 0x7f) ? (*p) : '.';

    hex = hex + '\n';
  }
  return hex;
}

void CompareCharArraysWithHexError(
    const std::string& description,
    const unsigned char* actual,
    const int actual_len,
    const unsigned char* expected,
    const int expected_len) {
  const int min_len = actual_len > expected_len ? expected_len : actual_len;
  const int max_len = actual_len > expected_len ? actual_len : expected_len;
  scoped_array<bool> marks(new bool[max_len]);
  bool identical = (actual_len == expected_len);
  for (int i = 0; i < min_len; ++i) {
    if (actual[i] != expected[i]) {
      marks[i] = true;
      identical = false;
    } else {
      marks[i] = false;
    }
  }
  for (int i = min_len; i < max_len; ++i) {
    marks[i] = true;
  }
  if (identical) return;
  ADD_FAILURE()
      << "Description:\n"
      << description
      << "\n\nExpected:\n"
      << HexDumpWithMarks(expected, expected_len, marks.get(), max_len)
      << "\nActual:\n"
      << HexDumpWithMarks(actual, actual_len, marks.get(), max_len);
}

class TestSpdyVisitor : public SpdyFramerVisitorInterface  {
 public:
  static const size_t kDefaultHeaderBufferSize = 16 * 1024;

  TestSpdyVisitor()
    : use_compression_(false),
      error_count_(0),
      syn_frame_count_(0),
      syn_reply_frame_count_(0),
      headers_frame_count_(0),
      goaway_count_(0),
      data_bytes_(0),
      fin_frame_count_(0),
      fin_flag_count_(0),
      zero_length_data_frame_count_(0),
      header_blocks_count_(0),
      control_frame_header_data_count_(0),
      zero_length_control_frame_header_data_count_(0),
      data_frame_count_(0),
      header_buffer_(new char[kDefaultHeaderBufferSize]),
      header_buffer_length_(0),
      header_buffer_size_(kDefaultHeaderBufferSize),
      header_stream_id_(-1),
      header_control_type_(NUM_CONTROL_FRAME_TYPES),
      header_buffer_valid_(false) {
  }

  void OnError(SpdyFramer* f) {
    LOG(INFO) << "SpdyFramer Error: "
              << SpdyFramer::ErrorCodeToString(f->error_code());
    error_count_++;
  }

  void OnDataFrameHeader(const SpdyDataFrame* frame) {
    data_frame_count_++;
    header_stream_id_ = frame->stream_id();
  }

  void OnStreamFrameData(SpdyStreamId stream_id,
                         const char* data,
                         size_t len) {
    if (len == 0)
      ++zero_length_data_frame_count_;

    data_bytes_ += len;
    std::cerr << "OnStreamFrameData(" << stream_id << ", \"";
    if (len > 0) {
      for (size_t i = 0 ; i < len; ++i) {
        std::cerr << std::hex << (0xFF & (unsigned int)data[i]) << std::dec;
      }
    }
    std::cerr << "\", " << len << ")\n";
  }

  void OnControl(const SpdyControlFrame* frame) {
    switch (frame->type()) {
      case SYN_STREAM:
        syn_frame_count_++;
        InitHeaderStreaming(frame);
        break;
      case SYN_REPLY:
        syn_reply_frame_count_++;
        InitHeaderStreaming(frame);
        break;
      case RST_STREAM:
        fin_frame_count_++;
        break;
      case HEADERS:
        headers_frame_count_++;
        InitHeaderStreaming(frame);
        break;
      case GOAWAY:
        goaway_count_++;
        break;
      default:
        DLOG(FATAL);  // Error!
    }
    if (frame->flags() & CONTROL_FLAG_FIN)
      ++fin_flag_count_;
  }

  bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                const char* header_data,
                                size_t len) {
    ++control_frame_header_data_count_;
    CHECK_EQ(header_stream_id_, stream_id);
    if (len == 0) {
      ++zero_length_control_frame_header_data_count_;
      // Indicates end-of-header-block.
      CHECK(header_buffer_valid_);
      bool parsed_headers = SpdyFramer::ParseHeaderBlockInBuffer(
          header_buffer_.get(), header_buffer_length_, &headers_);
      DCHECK(parsed_headers);
      return true;
    }
    const size_t available = header_buffer_size_ - header_buffer_length_;
    if (len > available) {
      header_buffer_valid_ = false;
      return false;
    }
    memcpy(header_buffer_.get() + header_buffer_length_, header_data, len);
    header_buffer_length_ += len;
    return true;
  }

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const unsigned char* input, size_t size) {
    framer_.set_enable_compression(use_compression_);
    framer_.set_visitor(this);
    size_t input_remaining = size;
    const char* input_ptr = reinterpret_cast<const char*>(input);
    while (input_remaining > 0 &&
           framer_.error_code() == SpdyFramer::SPDY_NO_ERROR) {
      // To make the tests more interesting, we feed random (amd small) chunks
      // into the framer.  This simulates getting strange-sized reads from
      // the socket.
      const size_t kMaxReadSize = 32;
      size_t bytes_read =
          (rand() % std::min(input_remaining, kMaxReadSize)) + 1;
      size_t bytes_processed = framer_.ProcessInput(input_ptr, bytes_read);
      input_remaining -= bytes_processed;
      input_ptr += bytes_processed;
      if (framer_.state() == SpdyFramer::SPDY_DONE)
        framer_.Reset();
    }
  }

  void InitHeaderStreaming(const SpdyControlFrame* frame) {
    memset(header_buffer_.get(), 0, header_buffer_size_);
    header_buffer_length_ = 0;
    header_stream_id_ = SpdyFramer::GetControlFrameStreamId(frame);
    header_control_type_ = frame->type();
    header_buffer_valid_ = true;
    DCHECK_NE(header_stream_id_, SpdyFramer::kInvalidStream);
  }

  // Override the default buffer size (16K). Call before using the framer!
  void set_header_buffer_size(size_t header_buffer_size) {
    header_buffer_size_ = header_buffer_size;
    header_buffer_.reset(new char[header_buffer_size]);
  }

  static size_t control_frame_buffer_max_size() {
    return SpdyFramer::kControlFrameBufferMaxSize;
  }

  static size_t header_data_chunk_max_size() {
    return SpdyFramer::kHeaderDataChunkMaxSize;
  }

  SpdyFramer framer_;
  bool use_compression_;

  // Counters from the visitor callbacks.
  int error_count_;
  int syn_frame_count_;
  int syn_reply_frame_count_;
  int headers_frame_count_;
  int goaway_count_;
  int data_bytes_;
  int fin_frame_count_;  // The count of RST_STREAM type frames received.
  int fin_flag_count_;  // The count of frames with the FIN flag set.
  int zero_length_data_frame_count_;  // The count of zero-length data frames.
  int header_blocks_count_;
  int control_frame_header_data_count_;  // The count of chunks received.
  // The count of zero-length control frame header data chunks received.
  int zero_length_control_frame_header_data_count_;
  int data_frame_count_;

  // Header block streaming state:
  scoped_array<char> header_buffer_;
  size_t header_buffer_length_;
  size_t header_buffer_size_;
  SpdyStreamId header_stream_id_;
  SpdyControlType header_control_type_;
  bool header_buffer_valid_;
  SpdyHeaderBlock headers_;
};

}  // namespace test

}  // namespace spdy

using spdy::SpdyControlFlags;
using spdy::SpdyControlFrame;
using spdy::SpdyDataFrame;
using spdy::SpdyFrame;
using spdy::SpdyFrameBuilder;
using spdy::SpdyFramer;
using spdy::SpdyHeaderBlock;
using spdy::SpdySynStreamControlFrame;
using spdy::kControlFlagMask;
using spdy::kLengthMask;
using spdy::CONTROL_FLAG_NONE;
using spdy::DATA_FLAG_COMPRESSED;
using spdy::DATA_FLAG_FIN;
using spdy::SYN_STREAM;
using spdy::test::CompareCharArraysWithHexError;
using spdy::test::TestSpdyVisitor;

namespace spdy {

class SpdyFramerTest : public PlatformTest {
 public:
  virtual void TearDown() {}

 protected:
  void CompareFrame(const std::string& description,
                    const SpdyFrame& actual_frame,
                    const unsigned char* expected,
                    const int expected_len) {
    const unsigned char* actual =
        reinterpret_cast<const unsigned char*>(actual_frame.data());
    int actual_len = actual_frame.length() + SpdyFrame::size();
    CompareCharArraysWithHexError(
        description, actual, actual_len, expected, expected_len);
  }
};


TEST(SpdyFrameBuilderTest, WriteLimits) {
  SpdyFrameBuilder builder(kLengthMask + 4);
  // length field should fail.
  EXPECT_FALSE(builder.WriteBytes(reinterpret_cast<const void*>(0x1),
                                  kLengthMask + 1));
  EXPECT_EQ(0, builder.length());

  // Writing a block of the maximum allowed size should succeed.
  const std::string kLargeData(kLengthMask, 'A');
  builder.WriteUInt32(kLengthMask);
  EXPECT_EQ(4, builder.length());
  EXPECT_TRUE(builder.WriteBytes(kLargeData.data(), kLengthMask));
  EXPECT_EQ(4 + kLengthMask, static_cast<unsigned>(builder.length()));
}

// Test that we can encode and decode a SpdyHeaderBlock.
TEST_F(SpdyFramerTest, HeaderBlock) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "charlie";
  SpdyFramer framer;

  // Encode the header block into a SynStream frame.
  scoped_ptr<SpdySynStreamControlFrame> frame(
      framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE, true, &headers));
  EXPECT_TRUE(frame.get() != NULL);

  SpdyHeaderBlock new_headers;
  EXPECT_TRUE(framer.ParseHeaderBlock(frame.get(), &new_headers));

  EXPECT_EQ(headers.size(), new_headers.size());
  EXPECT_EQ(headers["alpha"], new_headers["alpha"]);
  EXPECT_EQ(headers["gamma"], new_headers["gamma"]);
}

// Test that we can encode and decode a SpdyHeaderBlock in serialized form.
TEST_F(SpdyFramerTest, HeaderBlockInBuffer) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "charlie";
  SpdyFramer framer;

  // Encode the header block into a SynStream frame.
  scoped_ptr<SpdySynStreamControlFrame> frame(
      framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE, false, &headers));
  EXPECT_TRUE(frame.get() != NULL);
  std::string serialized_headers(frame->header_block(),
                                 frame->header_block_len());
  SpdyHeaderBlock new_headers;
  EXPECT_TRUE(framer.ParseHeaderBlockInBuffer(serialized_headers.c_str(),
                                              serialized_headers.size(),
                                              &new_headers));

  EXPECT_EQ(headers.size(), new_headers.size());
  EXPECT_EQ(headers["alpha"], new_headers["alpha"]);
  EXPECT_EQ(headers["gamma"], new_headers["gamma"]);
}

// Test that if there's not a full frame, we fail to parse it.
TEST_F(SpdyFramerTest, UndersizedHeaderBlockInBuffer) {
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  headers["gamma"] = "charlie";
  SpdyFramer framer;

  // Encode the header block into a SynStream frame.
  scoped_ptr<SpdySynStreamControlFrame> frame(
      framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE, false, &headers));
  EXPECT_TRUE(frame.get() != NULL);

  std::string serialized_headers(frame->header_block(),
                                 frame->header_block_len());
  SpdyHeaderBlock new_headers;
  EXPECT_FALSE(framer.ParseHeaderBlockInBuffer(serialized_headers.c_str(),
                                               serialized_headers.size() - 2,
                                               &new_headers));
}

TEST_F(SpdyFramerTest, OutOfOrderHeaders) {
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024);

  frame.WriteUInt16(kControlFlagMask | 1);
  frame.WriteUInt16(SYN_STREAM);
  frame.WriteUInt32(0);  // Placeholder for the length.
  frame.WriteUInt32(3);  // stream_id
  frame.WriteUInt32(0);  // Associated stream id
  frame.WriteUInt16(0);  // Priority.

  frame.WriteUInt16(2);  // Number of headers.
  SpdyHeaderBlock::iterator it;
  frame.WriteString("gamma");
  frame.WriteString("gamma");
  frame.WriteString("alpha");
  frame.WriteString("alpha");
  // write the length
  frame.WriteUInt32ToOffset(4, frame.length() - SpdyFrame::size());

  SpdyHeaderBlock new_headers;
  scoped_ptr<SpdyFrame> control_frame(frame.take());
  SpdySynStreamControlFrame syn_frame(control_frame->data(), false);
  std::string serialized_headers(syn_frame.header_block(),
                                 syn_frame.header_block_len());
  SpdyFramer framer;
  framer.set_enable_compression(false);
  EXPECT_TRUE(framer.ParseHeaderBlock(control_frame.get(), &new_headers));
}

TEST_F(SpdyFramerTest, WrongNumberOfHeaders) {
  SpdyFrameBuilder frame1;
  SpdyFrameBuilder frame2;

  // a frame with smaller number of actual headers
  frame1.WriteUInt16(kControlFlagMask | 1);
  frame1.WriteUInt16(SYN_STREAM);
  frame1.WriteUInt32(0);  // Placeholder for the length.
  frame1.WriteUInt32(3);  // stream_id
  frame1.WriteUInt16(0);  // Priority.

  frame1.WriteUInt16(1);  // Wrong number of headers (underflow)
  frame1.WriteString("gamma");
  frame1.WriteString("gamma");
  frame1.WriteString("alpha");
  frame1.WriteString("alpha");
  // write the length
  frame1.WriteUInt32ToOffset(4, frame1.length() - SpdyFrame::size());

  // a frame with larger number of actual headers
  frame2.WriteUInt16(kControlFlagMask | 1);
  frame2.WriteUInt16(SYN_STREAM);
  frame2.WriteUInt32(0);  // Placeholder for the length.
  frame2.WriteUInt32(3);  // stream_id
  frame2.WriteUInt16(0);  // Priority.

  frame2.WriteUInt16(100);  // Wrong number of headers (overflow)
  frame2.WriteString("gamma");
  frame2.WriteString("gamma");
  frame2.WriteString("alpha");
  frame2.WriteString("alpha");
  // write the length
  frame2.WriteUInt32ToOffset(4, frame2.length() - SpdyFrame::size());

  SpdyHeaderBlock new_headers;
  scoped_ptr<SpdyFrame> syn_frame1(frame1.take());
  scoped_ptr<SpdyFrame> syn_frame2(frame2.take());
  SpdyFramer framer;
  framer.set_enable_compression(false);
  EXPECT_FALSE(framer.ParseHeaderBlock(syn_frame1.get(), &new_headers));
  EXPECT_FALSE(framer.ParseHeaderBlock(syn_frame2.get(), &new_headers));
}

TEST_F(SpdyFramerTest, DuplicateHeader) {
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024);

  frame.WriteUInt16(kControlFlagMask | 1);
  frame.WriteUInt16(SYN_STREAM);
  frame.WriteUInt32(0);  // Placeholder for the length.
  frame.WriteUInt32(3);  // stream_id
  frame.WriteUInt32(0);  // associated stream id
  frame.WriteUInt16(0);  // Priority.

  frame.WriteUInt16(2);  // Number of headers.
  SpdyHeaderBlock::iterator it;
  frame.WriteString("name");
  frame.WriteString("value1");
  frame.WriteString("name");
  frame.WriteString("value2");
  // write the length
  frame.WriteUInt32ToOffset(4, frame.length() - SpdyFrame::size());

  SpdyHeaderBlock new_headers;
  scoped_ptr<SpdyFrame> control_frame(frame.take());
  SpdySynStreamControlFrame syn_frame(control_frame->data(), false);
  std::string serialized_headers(syn_frame.header_block(),
                                 syn_frame.header_block_len());
  SpdyFramer framer;
  framer.set_enable_compression(false);
  // This should fail because duplicate headers are verboten by the spec.
  EXPECT_FALSE(framer.ParseHeaderBlock(control_frame.get(), &new_headers));
}

TEST_F(SpdyFramerTest, MultiValueHeader) {
  // Frame builder with plentiful buffer size.
  SpdyFrameBuilder frame(1024);

  frame.WriteUInt16(kControlFlagMask | 1);
  frame.WriteUInt16(SYN_STREAM);
  frame.WriteUInt32(0);  // Placeholder for the length.
  frame.WriteUInt32(3);  // stream_id
  frame.WriteUInt32(0);  // associated stream id
  frame.WriteUInt16(0);  // Priority.

  frame.WriteUInt16(1);  // Number of headers.
  SpdyHeaderBlock::iterator it;
  frame.WriteString("name");
  std::string value("value1\0value2");
  frame.WriteString(value);
  // write the length
  frame.WriteUInt32ToOffset(4, frame.length() - SpdyFrame::size());

  SpdyHeaderBlock new_headers;
  scoped_ptr<SpdyFrame> control_frame(frame.take());
  SpdySynStreamControlFrame syn_frame(control_frame->data(), false);
  std::string serialized_headers(syn_frame.header_block(),
                                 syn_frame.header_block_len());
  SpdyFramer framer;
  framer.set_enable_compression(false);
  EXPECT_TRUE(framer.ParseHeaderBlock(control_frame.get(), &new_headers));
  EXPECT_TRUE(new_headers.find("name") != new_headers.end());
  EXPECT_EQ(value, new_headers.find("name")->second);
}

TEST_F(SpdyFramerTest, ZeroLengthHeader) {
  SpdyHeaderBlock header1;
  SpdyHeaderBlock header2;
  SpdyHeaderBlock header3;

  header1[""] = "value2";
  header2["name3"] = "";
  header3[""] = "";

  SpdyFramer framer;
  SpdyHeaderBlock parsed_headers;

  scoped_ptr<SpdySynStreamControlFrame> frame1(
      framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE, true, &header1));
  EXPECT_TRUE(frame1.get() != NULL);
  EXPECT_FALSE(framer.ParseHeaderBlock(frame1.get(), &parsed_headers));

  scoped_ptr<SpdySynStreamControlFrame> frame2(
      framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE, true, &header2));
  EXPECT_TRUE(frame2.get() != NULL);
  EXPECT_FALSE(framer.ParseHeaderBlock(frame2.get(), &parsed_headers));

  scoped_ptr<SpdySynStreamControlFrame> frame3(
      framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE, true, &header3));
  EXPECT_TRUE(frame3.get() != NULL);
  EXPECT_FALSE(framer.ParseHeaderBlock(frame3.get(), &parsed_headers));
}

TEST_F(SpdyFramerTest, BasicCompression) {
  SpdyHeaderBlock headers;
  headers["server"] = "SpdyServer 1.0";
  headers["date"] = "Mon 12 Jan 2009 12:12:12 PST";
  headers["status"] = "200";
  headers["version"] = "HTTP/1.1";
  headers["content-type"] = "text/html";
  headers["content-length"] = "12";

  SpdyFramer framer;
  framer.set_enable_compression(true);
  scoped_ptr<SpdySynStreamControlFrame>
      frame1(framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE, true,
                                    &headers));
  scoped_ptr<SpdySynStreamControlFrame>
      frame2(framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE, true,
                                    &headers));

  // Expect the second frame to be more compact than the first.
  EXPECT_LE(frame2->length(), frame1->length());

  // Decompress the first frame
  scoped_ptr<SpdyFrame> frame3(framer.DecompressFrame(*frame1.get()));

  // Decompress the second frame
  scoped_ptr<SpdyFrame> frame4(framer.DecompressFrame(*frame2.get()));

  // Expect frames 3 & 4 to be the same.
  EXPECT_EQ(0,
      memcmp(frame3->data(), frame4->data(),
      SpdyFrame::size() + frame3->length()));

  // Expect frames 3 to be the same as a uncompressed frame created
  // from scratch.
  scoped_ptr<SpdySynStreamControlFrame>
      uncompressed_frame(framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE,
                                                false, &headers));
  EXPECT_EQ(frame3->length(), uncompressed_frame->length());
  EXPECT_EQ(0,
      memcmp(frame3->data(), uncompressed_frame->data(),
      SpdyFrame::size() + uncompressed_frame->length()));
}

TEST_F(SpdyFramerTest, DecompressUncompressedFrame) {
  SpdyHeaderBlock headers;
  headers["server"] = "SpdyServer 1.0";
  headers["date"] = "Mon 12 Jan 2009 12:12:12 PST";
  headers["status"] = "200";
  headers["version"] = "HTTP/1.1";
  headers["content-type"] = "text/html";
  headers["content-length"] = "12";

  SpdyFramer framer;
  framer.set_enable_compression(true);
  scoped_ptr<SpdySynStreamControlFrame>
      frame1(framer.CreateSynStream(1, 0, 1, CONTROL_FLAG_NONE, false,
                                    &headers));

  // Decompress the frame
  scoped_ptr<SpdyFrame> frame2(framer.DecompressFrame(*frame1.get()));

  EXPECT_EQ(NULL, frame2.get());
}

TEST_F(SpdyFramerTest, Basic) {
  const unsigned char input[] = {
    0x80, 0x02, 0x00, 0x01,   // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, 0x02, 0x00, 0x08,   // HEADERS on Stream #1
    0x00, 0x00, 0x00, 0x18,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x02, 'h', '2',
    0x00, 0x02, 'v', '2',
    0x00, 0x02, 'h', '3',
    0x00, 0x02, 'v', '3',

    0x00, 0x00, 0x00, 0x01,   // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,

    0x80, 0x02, 0x00, 0x01,   // SYN Stream #3
    0x00, 0x00, 0x00, 0x0c,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,   // DATA on Stream #3
    0x00, 0x00, 0x00, 0x08,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,   // DATA on Stream #1
    0x00, 0x00, 0x00, 0x04,
      0xde, 0xad, 0xbe, 0xef,

    0x80, 0x02, 0x00, 0x03,   // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,   // DATA on Stream #3
    0x00, 0x00, 0x00, 0x00,

    0x80, 0x02, 0x00, 0x03,   // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
  };

  TestSpdyVisitor visitor;
  visitor.SimulateInFramer(input, sizeof(input));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(1, visitor.headers_frame_count_);
  EXPECT_EQ(24, visitor.data_bytes_);
  EXPECT_EQ(2, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
  // EXPECT_EQ(4, visitor.data_frame_count_);
}

// Test that the FIN flag on a data frame signifies EOF.
TEST_F(SpdyFramerTest, FinOnDataFrame) {
  const unsigned char input[] = {
    0x80, 0x02, 0x00, 0x01,   // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, 0x02, 0x00, 0x02,   // SYN REPLY Stream #1
    0x00, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',

    0x00, 0x00, 0x00, 0x01,   // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,
    0xde, 0xad, 0xbe, 0xef,

    0x00, 0x00, 0x00, 0x01,   // DATA on Stream #1, with EOF
    0x01, 0x00, 0x00, 0x04,
    0xde, 0xad, 0xbe, 0xef,
  };

  TestSpdyVisitor visitor;
  visitor.SimulateInFramer(input, sizeof(input));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(1, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(16, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  // EXPECT_EQ(2, visitor.data_frame_count_);
}

// Test that the FIN flag on a SYN reply frame signifies EOF.
TEST_F(SpdyFramerTest, FinOnSynReplyFrame) {
  const unsigned char input[] = {
    0x80, 0x02, 0x00, 0x01,   // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, 0x02, 0x00, 0x02,   // SYN REPLY Stream #1
    0x01, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'a', 'a',
    0x00, 0x02, 'b', 'b',
  };

  TestSpdyVisitor visitor;
  visitor.SimulateInFramer(input, sizeof(input));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(1, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(0, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(1, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  // EXPECT_EQ(0, visitor.data_frame_count_);
}

// Basic compression & decompression
TEST_F(SpdyFramerTest, DataCompression) {
  SpdyFramer send_framer;
  SpdyFramer recv_framer;

  send_framer.set_enable_compression(true);
  recv_framer.set_enable_compression(true);

  // Mix up some SYNs and DATA frames since they use different compressors.
  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kHeader3[] = "header3";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";
  const char kValue3[] = "value3";

  // SYN_STREAM #1
  SpdyHeaderBlock block;
  block[kHeader1] = kValue1;
  block[kHeader2] = kValue2;
  SpdyControlFlags flags(CONTROL_FLAG_NONE);
  scoped_ptr<spdy::SpdyFrame> syn_frame_1(
      send_framer.CreateSynStream(1, 0, 0, flags, true, &block));
  EXPECT_TRUE(syn_frame_1.get() != NULL);

  // DATA #1
  const char bytes[] = "this is a test test test test test!";
  scoped_ptr<SpdyFrame> data_frame_1(
      send_framer.CreateDataFrame(1, bytes, arraysize(bytes),
                                  DATA_FLAG_COMPRESSED));
  EXPECT_TRUE(data_frame_1.get() != NULL);

  // SYN_STREAM #2
  block[kHeader3] = kValue3;
  scoped_ptr<SpdyFrame> syn_frame_2(
      send_framer.CreateSynStream(3, 0, 0, flags, true, &block));
  EXPECT_TRUE(syn_frame_2.get() != NULL);

  // DATA #2
  scoped_ptr<SpdyFrame> data_frame_2(
      send_framer.CreateDataFrame(3, bytes, arraysize(bytes),
                                  DATA_FLAG_COMPRESSED));
  EXPECT_TRUE(data_frame_2.get() != NULL);

  // Now start decompressing
  scoped_ptr<SpdyFrame> decompressed;
  SpdyControlFrame* control_frame;
  SpdyDataFrame* data_frame;
  SpdyHeaderBlock decompressed_headers;

  decompressed.reset(recv_framer.DuplicateFrame(*syn_frame_1.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  EXPECT_TRUE(decompressed->is_control_frame());
  control_frame = reinterpret_cast<SpdyControlFrame*>(decompressed.get());
  EXPECT_EQ(SYN_STREAM, control_frame->type());
  EXPECT_TRUE(recv_framer.ParseHeaderBlock(
      control_frame, &decompressed_headers));
  EXPECT_EQ(2u, decompressed_headers.size());
  EXPECT_EQ(SYN_STREAM, control_frame->type());
  EXPECT_EQ(kValue1, decompressed_headers[kHeader1]);
  EXPECT_EQ(kValue2, decompressed_headers[kHeader2]);

  decompressed.reset(recv_framer.DecompressFrame(*data_frame_1.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  EXPECT_FALSE(decompressed->is_control_frame());
  data_frame = reinterpret_cast<SpdyDataFrame*>(decompressed.get());
  EXPECT_EQ(arraysize(bytes), data_frame->length());
  EXPECT_EQ(0, memcmp(data_frame->payload(), bytes, data_frame->length()));

  decompressed.reset(recv_framer.DuplicateFrame(*syn_frame_2.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  EXPECT_TRUE(decompressed->is_control_frame());
  control_frame = reinterpret_cast<SpdyControlFrame*>(decompressed.get());
  EXPECT_EQ(control_frame->type(), SYN_STREAM);
  decompressed_headers.clear();
  EXPECT_TRUE(recv_framer.ParseHeaderBlock(
      control_frame, &decompressed_headers));
  EXPECT_EQ(3u, decompressed_headers.size());
  EXPECT_EQ(SYN_STREAM, control_frame->type());
  EXPECT_EQ(kValue1, decompressed_headers[kHeader1]);
  EXPECT_EQ(kValue2, decompressed_headers[kHeader2]);
  EXPECT_EQ(kValue3, decompressed_headers[kHeader3]);

  decompressed.reset(recv_framer.DecompressFrame(*data_frame_2.get()));
  EXPECT_TRUE(decompressed.get() != NULL);
  EXPECT_FALSE(decompressed->is_control_frame());
  data_frame = reinterpret_cast<SpdyDataFrame*>(decompressed.get());
  EXPECT_EQ(arraysize(bytes), data_frame->length());
  EXPECT_EQ(0, memcmp(data_frame->payload(), bytes, data_frame->length()));

  // We didn't close these streams, so the compressors should be active.
  EXPECT_EQ(2, send_framer.num_stream_compressors());
  EXPECT_EQ(0, send_framer.num_stream_decompressors());
  EXPECT_EQ(0, recv_framer.num_stream_compressors());
  EXPECT_EQ(2, recv_framer.num_stream_decompressors());
}

// Verify we don't leak when we leave streams unclosed
TEST_F(SpdyFramerTest, UnclosedStreamDataCompressors) {
  SpdyFramer send_framer;

  send_framer.set_enable_compression(true);

  const char kHeader1[] = "header1";
  const char kHeader2[] = "header2";
  const char kValue1[] = "value1";
  const char kValue2[] = "value2";

  SpdyHeaderBlock block;
  block[kHeader1] = kValue1;
  block[kHeader2] = kValue2;
  SpdyControlFlags flags(CONTROL_FLAG_NONE);
  scoped_ptr<spdy::SpdyFrame> syn_frame(
      send_framer.CreateSynStream(1, 0, 0, flags, true, &block));
  EXPECT_TRUE(syn_frame.get() != NULL);

  const char bytes[] = "this is a test test test test test!";
  scoped_ptr<SpdyFrame> send_frame(
      send_framer.CreateDataFrame(
          1, bytes, arraysize(bytes),
          DATA_FLAG_FIN));
  EXPECT_TRUE(send_frame.get() != NULL);

  // Run the inputs through the framer.
  TestSpdyVisitor visitor;
  visitor.use_compression_ = true;
  const unsigned char* data;
  data = reinterpret_cast<const unsigned char*>(syn_frame->data());
  visitor.SimulateInFramer(data, syn_frame->length() + SpdyFrame::size());
  data = reinterpret_cast<const unsigned char*>(send_frame->data());
  visitor.SimulateInFramer(data, send_frame->length() + SpdyFrame::size());

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(1, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(0, visitor.headers_frame_count_);
  EXPECT_EQ(arraysize(bytes), static_cast<unsigned>(visitor.data_bytes_));
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
  // EXPECT_EQ(1, visitor.data_frame_count_);

  // We closed the streams, so all compressors should be down.
  EXPECT_EQ(0, visitor.framer_.num_stream_compressors());
  EXPECT_EQ(0, visitor.framer_.num_stream_decompressors());
  EXPECT_EQ(0, send_framer.num_stream_compressors());
  EXPECT_EQ(0, send_framer.num_stream_decompressors());
}

TEST_F(SpdyFramerTest, WindowUpdateFrame) {
  scoped_ptr<SpdyWindowUpdateControlFrame> window_update_frame(
      SpdyFramer::CreateWindowUpdate(1, 0x12345678));

  const unsigned char expected_data_frame[] = {
      0x80, 0x02, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x12, 0x34, 0x56, 0x78
  };

  EXPECT_EQ(16u, window_update_frame->size());
  EXPECT_EQ(0,
            memcmp(window_update_frame->data(), expected_data_frame, 16));
}

TEST_F(SpdyFramerTest, CreateDataFrame) {
  SpdyFramer framer;

  {
    const char kDescription[] = "'hello' data frame, no FIN";
    const unsigned char kFrameData[] = {
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x05,
      'h', 'e', 'l', 'l',
      'o'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, "hello", 5, DATA_FLAG_NONE));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "Data frame with negative data byte, no FIN";
    const unsigned char kFrameData[] = {
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
      0xff
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, "\xff", 1, DATA_FLAG_NONE));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "'hello' data frame, with FIN";
    const unsigned char kFrameData[] = {
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x05,
      'h', 'e', 'l', 'l',
      'o'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, "hello", 5, DATA_FLAG_FIN));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "Empty data frame";
    const unsigned char kFrameData[] = {
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, "", 0, DATA_FLAG_NONE));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "Data frame with max stream ID";
    const unsigned char kFrameData[] = {
      0x7f, 0xff, 0xff, 0xff,
      0x01, 0x00, 0x00, 0x05,
      'h', 'e', 'l', 'l',
      'o'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        0x7fffffff, "hello", 5, DATA_FLAG_FIN));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "Large data frame";
    const int kDataSize = 4 * 1024 * 1024;  // 4 MB
    const std::string kData(kDataSize, 'A');
    const unsigned char kFrameHeader[] = {
      0x00, 0x00, 0x00, 0x01,
      0x01, 0x40, 0x00, 0x00,
    };

    const int kFrameSize = arraysize(kFrameHeader) + kDataSize;
    scoped_array<unsigned char> expected_frame_data(
        new unsigned char[kFrameSize]);
    memcpy(expected_frame_data.get(), kFrameHeader, arraysize(kFrameHeader));
    memset(expected_frame_data.get() + arraysize(kFrameHeader), 'A', kDataSize);

    scoped_ptr<SpdyFrame> frame(framer.CreateDataFrame(
        1, kData.data(), kData.size(), DATA_FLAG_FIN));
    CompareFrame(kDescription, *frame, expected_frame_data.get(), kFrameSize);
  }
}

TEST_F(SpdyFramerTest, CreateSynStreamUncompressed) {
  SpdyFramer framer;
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "SYN_STREAM frame, lowest pri, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x20,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0xC0, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynStream(
        1, 0, SPDY_PRIORITY_LOWEST, CONTROL_FLAG_NONE,
        false, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
    EXPECT_EQ(1u, SpdyFramer::GetControlFrameStreamId(
        reinterpret_cast<const SpdyControlFrame*>(frame.get())));
  }

  {
    const char kDescription[] =
        "SYN_STREAM frame with a 0-length header name, highest pri, FIN, "
        "max stream ID";

    SpdyHeaderBlock headers;
    headers[""] = "foo";
    headers["foo"] = "bar";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x1D,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynStream(
        0x7fffffff, 0x7fffffff, SPDY_PRIORITY_HIGHEST, CONTROL_FLAG_FIN,
        false, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] =
        "SYN_STREAM frame with a 0-length header val, highest pri, FIN, "
        "max stream ID";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x1D,
      0x7f, 0xff, 0xff, 0xff,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynStream(
        0x7fffffff, 0x7fffffff, SPDY_PRIORITY_HIGHEST, CONTROL_FLAG_FIN,
        false, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, CreateSynStreamCompressed) {
  SpdyFramer framer;
  framer.set_enable_compression(true);

  {
    const char kDescription[] =
        "SYN_STREAM frame, lowest pri, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x25,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00,
      0xC0, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x4e, 0xcb, 0xcf,
      0x87, 0x12, 0x40, 0x2e,
      0x00, 0x00, 0x00, 0xff,
      0xff
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynStream(
        1, 0, SPDY_PRIORITY_LOWEST, CONTROL_FLAG_NONE,
        true, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, CreateSynReplyUncompressed) {
  SpdyFramer framer;
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "SYN_REPLY frame, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x1C,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynReply(
        1, CONTROL_FLAG_NONE, false, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] =
        "SYN_REPLY frame with a 0-length header name, FIN, max stream ID";

    SpdyHeaderBlock headers;
    headers[""] = "foo";
    headers["foo"] = "bar";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynReply(
        0x7fffffff, CONTROL_FLAG_FIN, false, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] =
        "SYN_REPLY frame with a 0-length header val, FIN, max stream ID";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynReply(
        0x7fffffff, CONTROL_FLAG_FIN, false, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, CreateSynReplyCompressed) {
  SpdyFramer framer;
  framer.set_enable_compression(true);

  {
    const char kDescription[] = "SYN_REPLY frame, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x21,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x4e, 0xcb, 0xcf,
      0x87, 0x12, 0x40, 0x2e,
      0x00, 0x00, 0x00, 0xff,
      0xff
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSynReply(
        1, CONTROL_FLAG_NONE, true, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, CreateRstStream) {
  SpdyFramer framer;

  {
    const char kDescription[] = "RST_STREAM frame";
    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateRstStream(1, PROTOCOL_ERROR));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
    EXPECT_EQ(1u, SpdyFramer::GetControlFrameStreamId(
        reinterpret_cast<const SpdyControlFrame*>(frame.get())));
  }

  {
    const char kDescription[] = "RST_STREAM frame with max stream ID";
    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateRstStream(0x7FFFFFFF,
                                                       PROTOCOL_ERROR));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "RST_STREAM frame with max status code";
    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x03,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x06,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateRstStream(0x7FFFFFFF,
                                                       INTERNAL_ERROR));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, CreateSettings) {
  SpdyFramer framer;

  {
    const char kDescription[] = "Basic SETTINGS frame";

    SpdySettings settings;
    settings.push_back(SpdySetting(0x00000000, 0x00000000));
    settings.push_back(SpdySetting(0xffffffff, 0x00000001));
    settings.push_back(SpdySetting(0xff000001, 0x00000002));

    // Duplicates allowed
    settings.push_back(SpdySetting(0x01000002, 0x00000003));
    settings.push_back(SpdySetting(0x01000002, 0x00000003));

    settings.push_back(SpdySetting(0x01000003, 0x000000ff));
    settings.push_back(SpdySetting(0x01000004, 0xff000001));
    settings.push_back(SpdySetting(0x01000004, 0xffffffff));

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x44,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
      0xff, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x01, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      0x01, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      0x01, 0x00, 0x00, 0x03,
      0x00, 0x00, 0x00, 0xff,
      0x01, 0x00, 0x00, 0x04,
      0xff, 0x00, 0x00, 0x01,
      0x01, 0x00, 0x00, 0x04,
      0xff, 0xff, 0xff, 0xff,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSettings(settings));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
    EXPECT_EQ(SpdyFramer::kInvalidStream,
              SpdyFramer::GetControlFrameStreamId(
                  reinterpret_cast<const SpdyControlFrame*>(frame.get())));
  }

  {
    const char kDescription[] = "Empty SETTINGS frame";

    SpdySettings settings;

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateSettings(settings));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, CreateNopFrame) {
  SpdyFramer framer;

  {
    const char kDescription[] = "NOOP frame";
    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x05,
      0x00, 0x00, 0x00, 0x00,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateNopFrame());
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
    EXPECT_EQ(SpdyFramer::kInvalidStream,
              SpdyFramer::GetControlFrameStreamId(
                  reinterpret_cast<const SpdyControlFrame*>(frame.get())));
  }
}

TEST_F(SpdyFramerTest, CreatePingFrame) {
  SpdyFramer framer;

  {
    const char kDescription[] = "PING frame";
    const unsigned char kFrameData[] = {
        0x80, 0x02, 0x00, 0x06,
        0x00, 0x00, 0x00, 0x04,
        0x12, 0x34, 0x56, 0x78,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreatePingFrame(0x12345678u));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
    EXPECT_EQ(SpdyFramer::kInvalidStream,
              SpdyFramer::GetControlFrameStreamId(
                  reinterpret_cast<const SpdyControlFrame*>(frame.get())));
  }
}

TEST_F(SpdyFramerTest, CreateGoAway) {
  SpdyFramer framer;

  {
    const char kDescription[] = "GOAWAY frame";
    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateGoAway(0));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
    EXPECT_EQ(SpdyFramer::kInvalidStream,
              SpdyFramer::GetControlFrameStreamId(
                  reinterpret_cast<const SpdyControlFrame*>(frame.get())));
  }

  {
    const char kDescription[] = "GOAWAY frame with max stream ID";
    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x04,
      0x7f, 0xff, 0xff, 0xff,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateGoAway(0x7FFFFFFF));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, CreateHeadersUncompressed) {
  SpdyFramer framer;
  framer.set_enable_compression(false);

  {
    const char kDescription[] = "HEADERS frame, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x1C,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'b',  'a',  'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateHeaders(
        1, CONTROL_FLAG_NONE, false, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header name, FIN, max stream ID";

    SpdyHeaderBlock headers;
    headers[""] = "foo";
    headers["foo"] = "bar";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x03, 'f',  'o',  'o',
      0x00, 0x03, 'b',  'a',
      'r'
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateHeaders(
        0x7fffffff, CONTROL_FLAG_FIN, false, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] =
        "HEADERS frame with a 0-length header val, FIN, max stream ID";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x08,
      0x01, 0x00, 0x00, 0x19,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x02,
      0x00, 0x03, 'b',  'a',
      'r',  0x00, 0x03, 'f',
      'o',  'o',  0x00, 0x03,
      'f',  'o',  'o',  0x00,
      0x00
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateHeaders(
        0x7fffffff, CONTROL_FLAG_FIN, false, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, CreateHeadersCompressed) {
  SpdyFramer framer;
  framer.set_enable_compression(true);

  {
    const char kDescription[] = "HEADERS frame, no FIN";

    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "bar";

    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x21,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x38, 0xea,
      0xdf, 0xa2, 0x51, 0xb2,
      0x62, 0x60, 0x62, 0x60,
      0x4e, 0x4a, 0x2c, 0x62,
      0x60, 0x4e, 0xcb, 0xcf,
      0x87, 0x12, 0x40, 0x2e,
      0x00, 0x00, 0x00, 0xff,
      0xff
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateHeaders(
        1, CONTROL_FLAG_NONE, true, &headers));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, CreateWindowUpdate) {
  SpdyFramer framer;

  {
    const char kDescription[] = "WINDOW_UPDATE frame";
    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateWindowUpdate(1, 1));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
    EXPECT_EQ(1u, SpdyFramer::GetControlFrameStreamId(
        reinterpret_cast<const SpdyControlFrame*>(frame.get())));
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max stream ID";
    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x7f, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateWindowUpdate(0x7FFFFFFF, 1));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }

  {
    const char kDescription[] = "WINDOW_UPDATE frame with max window delta";
    const unsigned char kFrameData[] = {
      0x80, 0x02, 0x00, 0x09,
      0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x01,
      0x7f, 0xff, 0xff, 0xff,
    };
    scoped_ptr<SpdyFrame> frame(framer.CreateWindowUpdate(1, 0x7FFFFFFF));
    CompareFrame(kDescription, *frame, kFrameData, arraysize(kFrameData));
  }
}

TEST_F(SpdyFramerTest, DuplicateFrame) {
  SpdyFramer framer;

  {
    const char kDescription[] = "PING frame";
    const unsigned char kFrameData[] = {
        0x80, 0x02, 0x00, 0x06,
        0x00, 0x00, 0x00, 0x04,
        0x12, 0x34, 0x56, 0x78,
    };
    scoped_ptr<SpdyFrame> frame1(framer.CreatePingFrame(0x12345678u));
    CompareFrame(kDescription, *frame1, kFrameData, arraysize(kFrameData));

    scoped_ptr<SpdyFrame> frame2(framer.DuplicateFrame(*frame1));
    CompareFrame(kDescription, *frame2, kFrameData, arraysize(kFrameData));
  }
}

// This test case reproduces conditions that caused ExpandControlFrameBuffer to
// fail to expand the buffer control frame buffer when it should have, allowing
// the framer to overrun the buffer, and smash other heap contents. This test
// relies on the debug version of the heap manager, which checks for buffer
// overrun errors during delete processing. Regression test for b/2974814.
TEST_F(SpdyFramerTest, ExpandBuffer_HeapSmash) {
  // Sweep through the area of problematic values, to make sure we always cover
  // the danger zone, even if it moves around at bit due to SPDY changes.
  for (uint16 val2_len = SpdyFramer::kControlFrameBufferInitialSize - 50;
       val2_len < SpdyFramer::kControlFrameBufferInitialSize;
       val2_len++) {
    std::string val2 = std::string(val2_len, 'a');
    SpdyHeaderBlock headers;
    headers["bar"] = "foo";
    headers["foo"] = "baz";
    headers["grue"] = val2.c_str();
    SpdyFramer framer;
    scoped_ptr<SpdySynStreamControlFrame> template_frame(
        framer.CreateSynStream(1,                      // stream_id
                               0,                      // associated_stream_id
                               1,                      // priority
                               CONTROL_FLAG_NONE,
                               false,                  // compress
                               &headers));
    EXPECT_TRUE(template_frame.get() != NULL);
    TestSpdyVisitor visitor;
    visitor.SimulateInFramer(
        reinterpret_cast<unsigned char*>(template_frame.get()->data()),
         template_frame.get()->length() + SpdyControlFrame::size());
    EXPECT_EQ(1, visitor.syn_frame_count_);
  }
}

TEST_F(SpdyFramerTest, ReadGarbage) {
  SpdyFramer framer;
  unsigned char garbage_frame[256];
  memset(garbage_frame, ~0, sizeof(garbage_frame));
  TestSpdyVisitor visitor;
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(garbage_frame, sizeof(garbage_frame));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST_F(SpdyFramerTest, ReadGarbageWithValidVersion) {
  SpdyFramer framer;
  char garbage_frame[256];
  memset(garbage_frame, ~0, sizeof(garbage_frame));
  SpdyControlFrame control_frame(&garbage_frame[0], false);
  control_frame.set_version(kSpdyProtocolVersion);
  TestSpdyVisitor visitor;
  visitor.use_compression_ = false;
  visitor.SimulateInFramer(
      reinterpret_cast<unsigned char*>(control_frame.data()),
      sizeof(garbage_frame));
  EXPECT_EQ(1, visitor.error_count_);
}

TEST(SpdyFramer, StateToStringTest) {
  EXPECT_STREQ("ERROR",
               SpdyFramer::StateToString(SpdyFramer::SPDY_ERROR));
  EXPECT_STREQ("DONE",
               SpdyFramer::StateToString(SpdyFramer::SPDY_DONE));
  EXPECT_STREQ("AUTO_RESET",
               SpdyFramer::StateToString(SpdyFramer::SPDY_AUTO_RESET));
  EXPECT_STREQ("RESET",
               SpdyFramer::StateToString(SpdyFramer::SPDY_RESET));
  EXPECT_STREQ("READING_COMMON_HEADER",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_READING_COMMON_HEADER));
  EXPECT_STREQ("INTERPRET_CONTROL_FRAME_COMMON_HEADER",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_INTERPRET_CONTROL_FRAME_COMMON_HEADER));
  EXPECT_STREQ("CONTROL_FRAME_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_PAYLOAD));
  EXPECT_STREQ("IGNORE_REMAINING_PAYLOAD",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_IGNORE_REMAINING_PAYLOAD));
  EXPECT_STREQ("FORWARD_STREAM_FRAME",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_FORWARD_STREAM_FRAME));
  EXPECT_STREQ("SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK));
  EXPECT_STREQ("SPDY_CONTROL_FRAME_HEADER_BLOCK",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_HEADER_BLOCK));
  EXPECT_STREQ("UNKNOWN_STATE",
               SpdyFramer::StateToString(
                   SpdyFramer::SPDY_CONTROL_FRAME_HEADER_BLOCK + 1));
}

TEST(SpdyFramer, ErrorCodeToStringTest) {
  EXPECT_STREQ("NO_ERROR",
               SpdyFramer::ErrorCodeToString(SpdyFramer::SPDY_NO_ERROR));
  EXPECT_STREQ("INVALID_CONTROL_FRAME",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_INVALID_CONTROL_FRAME));
  EXPECT_STREQ("CONTROL_PAYLOAD_TOO_LARGE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_CONTROL_PAYLOAD_TOO_LARGE));
  EXPECT_STREQ("ZLIB_INIT_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_ZLIB_INIT_FAILURE));
  EXPECT_STREQ("UNSUPPORTED_VERSION",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_UNSUPPORTED_VERSION));
  EXPECT_STREQ("DECOMPRESS_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_DECOMPRESS_FAILURE));
  EXPECT_STREQ("COMPRESS_FAILURE",
               SpdyFramer::ErrorCodeToString(
                   SpdyFramer::SPDY_COMPRESS_FAILURE));
  EXPECT_STREQ("UNKNOWN_ERROR",
               SpdyFramer::ErrorCodeToString(SpdyFramer::LAST_ERROR));
}

TEST(SpdyFramer, StatusCodeToStringTest) {
  EXPECT_STREQ("INVALID",
               SpdyFramer::StatusCodeToString(INVALID));
  EXPECT_STREQ("PROTOCOL_ERROR",
               SpdyFramer::StatusCodeToString(PROTOCOL_ERROR));
  EXPECT_STREQ("INVALID_STREAM",
               SpdyFramer::StatusCodeToString(INVALID_STREAM));
  EXPECT_STREQ("REFUSED_STREAM",
               SpdyFramer::StatusCodeToString(REFUSED_STREAM));
  EXPECT_STREQ("UNSUPPORTED_VERSION",
               SpdyFramer::StatusCodeToString(UNSUPPORTED_VERSION));
  EXPECT_STREQ("CANCEL",
               SpdyFramer::StatusCodeToString(CANCEL));
  EXPECT_STREQ("INTERNAL_ERROR",
               SpdyFramer::StatusCodeToString(INTERNAL_ERROR));
  EXPECT_STREQ("FLOW_CONTROL_ERROR",
               SpdyFramer::StatusCodeToString(FLOW_CONTROL_ERROR));
  EXPECT_STREQ("UNKNOWN_STATUS",
               SpdyFramer::StatusCodeToString(NUM_STATUS_CODES));
}

TEST(SpdyFramer, ControlTypeToStringTest) {
  EXPECT_STREQ("SYN_STREAM",
               SpdyFramer::ControlTypeToString(SYN_STREAM));
  EXPECT_STREQ("SYN_REPLY",
               SpdyFramer::ControlTypeToString(SYN_REPLY));
  EXPECT_STREQ("RST_STREAM",
               SpdyFramer::ControlTypeToString(RST_STREAM));
  EXPECT_STREQ("SETTINGS",
               SpdyFramer::ControlTypeToString(SETTINGS));
  EXPECT_STREQ("NOOP",
               SpdyFramer::ControlTypeToString(NOOP));
  EXPECT_STREQ("PING",
               SpdyFramer::ControlTypeToString(PING));
  EXPECT_STREQ("GOAWAY",
               SpdyFramer::ControlTypeToString(GOAWAY));
  EXPECT_STREQ("HEADERS",
               SpdyFramer::ControlTypeToString(HEADERS));
  EXPECT_STREQ("WINDOW_UPDATE",
               SpdyFramer::ControlTypeToString(WINDOW_UPDATE));
  EXPECT_STREQ("UNKNOWN_CONTROL_TYPE",
               SpdyFramer::ControlTypeToString(NUM_CONTROL_FRAME_TYPES));
}

TEST(SpdyFramer, GetMinimumControlFrameSizeTest) {
  EXPECT_EQ(SpdySynStreamControlFrame::size(),
            SpdyFramer::GetMinimumControlFrameSize(SYN_STREAM));
  EXPECT_EQ(SpdySynReplyControlFrame::size(),
            SpdyFramer::GetMinimumControlFrameSize(SYN_REPLY));
  EXPECT_EQ(SpdyRstStreamControlFrame::size(),
            SpdyFramer::GetMinimumControlFrameSize(RST_STREAM));
  EXPECT_EQ(SpdySettingsControlFrame::size(),
            SpdyFramer::GetMinimumControlFrameSize(SETTINGS));
  EXPECT_EQ(SpdyNoOpControlFrame::size(),
            SpdyFramer::GetMinimumControlFrameSize(NOOP));
  EXPECT_EQ(SpdyPingControlFrame::size(),
            SpdyFramer::GetMinimumControlFrameSize(PING));
  EXPECT_EQ(SpdyGoAwayControlFrame::size(),
            SpdyFramer::GetMinimumControlFrameSize(GOAWAY));
  EXPECT_EQ(SpdyHeadersControlFrame::size(),
            SpdyFramer::GetMinimumControlFrameSize(HEADERS));
  EXPECT_EQ(SpdyWindowUpdateControlFrame::size(),
            SpdyFramer::GetMinimumControlFrameSize(WINDOW_UPDATE));
  EXPECT_EQ(static_cast<size_t>(0x7FFFFFFF),
            SpdyFramer::GetMinimumControlFrameSize(NUM_CONTROL_FRAME_TYPES));
}

std::string RandomString(int length) {
  std::string rv;
  for (int index = 0; index < length; index++)
    rv += static_cast<char>('a' + (rand() % 26));
  return rv;
}

// Stress that we can handle a really large header block compression and
// decompression.
TEST_F(SpdyFramerTest, DISABLED_HugeHeaderBlock) {
  // Loop targetting various sizes which will potentially jam up the
  // frame compressor/decompressor.
  SpdyFramer compress_framer;
  SpdyFramer decompress_framer;
  for (size_t target_size = 1024;
       target_size < SpdyFramer::kControlFrameBufferInitialSize;
       target_size += 1024) {
    SpdyHeaderBlock headers;
    for (size_t index = 0; index < target_size; ++index) {
      std::string name = RandomString(4);
      std::string value = RandomString(8);
      headers[name] = value;
    }

    // Encode the header block into a SynStream frame.
    scoped_ptr<SpdySynStreamControlFrame> frame(
        compress_framer.CreateSynStream(1,
                                        0,
                                        1,
                                        CONTROL_FLAG_NONE,
                                        true,
                                        &headers));
    // The point of this test is to exercise the limits.  So, it is ok if the
    // frame was too large to encode, or if the decompress fails.  We just want
    // to make sure we don't crash.
    if (frame.get() != NULL) {
      // Now that same header block should decompress just fine.
      SpdyHeaderBlock new_headers;
      decompress_framer.ParseHeaderBlock(frame.get(), &new_headers);
    }
  }
}

}  // namespace
