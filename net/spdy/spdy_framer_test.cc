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

void FramerSetEnableCompressionHelper(SpdyFramer* framer, bool compress) {
  framer->set_enable_compression(compress);
}

class TestSpdyVisitor : public SpdyFramerVisitorInterface  {
 public:
  TestSpdyVisitor()
    : error_count_(0),
      syn_frame_count_(0),
      syn_reply_frame_count_(0),
      headers_frame_count_(0),
      data_bytes_(0),
      fin_frame_count_(0),
      fin_flag_count_(0),
      zero_length_data_frame_count_(0) {
  }

  void OnError(SpdyFramer* f) {
    error_count_++;
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
    SpdyHeaderBlock headers;
    bool parsed_headers = false;
    switch (frame->type()) {
      case SYN_STREAM:
        parsed_headers = framer_.ParseHeaderBlock(frame, &headers);
        DCHECK(parsed_headers);
        syn_frame_count_++;
        break;
      case SYN_REPLY:
        parsed_headers = framer_.ParseHeaderBlock(frame, &headers);
        DCHECK(parsed_headers);
        syn_reply_frame_count_++;
        break;
      case RST_STREAM:
        fin_frame_count_++;
        break;
      case HEADERS:
        parsed_headers = framer_.ParseHeaderBlock(frame, &headers);
        DCHECK(parsed_headers);
        headers_frame_count_++;
        break;
      default:
        DCHECK(false);  // Error!
    }
    if (frame->flags() & CONTROL_FLAG_FIN)
      ++fin_flag_count_;
  }

  bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                const char* header_data,
                                size_t len) {
    DCHECK(false);
    return false;
  }

  void OnDataFrameHeader(const SpdyDataFrame* frame) {
    DCHECK(false);
  }

  // Convenience function which runs a framer simulation with particular input.
  void SimulateInFramer(const unsigned char* input, size_t size) {
    framer_.set_enable_compression(false);
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

  SpdyFramer framer_;
  // Counters from the visitor callbacks.
  int error_count_;
  int syn_frame_count_;
  int syn_reply_frame_count_;
  int headers_frame_count_;
  int data_bytes_;
  int fin_frame_count_;  // The count of RST_STREAM type frames received.
  int fin_flag_count_;  // The count of frames with the FIN flag set.
  int zero_length_data_frame_count_;  // The count of zero-length data frames.
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
using spdy::CONTROL_FLAG_NONE;
using spdy::DATA_FLAG_COMPRESSED;
using spdy::DATA_FLAG_FIN;
using spdy::SYN_STREAM;
using spdy::test::CompareCharArraysWithHexError;
using spdy::test::FramerSetEnableCompressionHelper;
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

TEST_F(SpdyFramerTest, OutOfOrderHeaders) {
  SpdyFrameBuilder frame;

  frame.WriteUInt16(kControlFlagMask | 1);
  frame.WriteUInt16(SYN_STREAM);
  frame.WriteUInt32(0);  // Placeholder for the length.
  frame.WriteUInt32(3);  // stream_id
  frame.WriteUInt32(0);  // associated stream id
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
  SpdyFramer framer;
  FramerSetEnableCompressionHelper(&framer, false);
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
  FramerSetEnableCompressionHelper(&framer, false);
  EXPECT_FALSE(framer.ParseHeaderBlock(syn_frame1.get(), &new_headers));
  EXPECT_FALSE(framer.ParseHeaderBlock(syn_frame2.get(), &new_headers));
}

TEST_F(SpdyFramerTest, DuplicateHeader) {
  SpdyFrameBuilder frame;

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
  SpdyFramer framer;
  FramerSetEnableCompressionHelper(&framer, false);
  // This should fail because duplicate headers are verboten by the spec.
  EXPECT_FALSE(framer.ParseHeaderBlock(control_frame.get(), &new_headers));
}

TEST_F(SpdyFramerTest, MultiValueHeader) {
  SpdyFrameBuilder frame;

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
  SpdyFramer framer;
  FramerSetEnableCompressionHelper(&framer, false);
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
  FramerSetEnableCompressionHelper(&framer, true);
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
  FramerSetEnableCompressionHelper(&framer, true);
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
    0x01, 0x00, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x01,
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
}

// Basic compression & decompression
TEST_F(SpdyFramerTest, DataCompression) {
  SpdyFramer send_framer;
  SpdyFramer recv_framer;

  FramerSetEnableCompressionHelper(&send_framer, true);
  FramerSetEnableCompressionHelper(&recv_framer, true);

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

  FramerSetEnableCompressionHelper(&send_framer, false);

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
      send_framer.CreateDataFrame(1,
                                  bytes,
                                  arraysize(bytes),
                                  DATA_FLAG_FIN));
  EXPECT_TRUE(send_frame.get() != NULL);

  // Run the inputs through the framer.
  TestSpdyVisitor visitor;
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

  // We closed the streams, so all compressors should be down.
  EXPECT_EQ(0, visitor.framer_.num_stream_compressors());
  EXPECT_EQ(0, visitor.framer_.num_stream_decompressors());
  EXPECT_EQ(0, send_framer.num_stream_compressors());
  EXPECT_EQ(0, send_framer.num_stream_decompressors());
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
}

TEST_F(SpdyFramerTest, CreateSynStreamUncompressed) {
  SpdyFramer framer;
  FramerSetEnableCompressionHelper(&framer, false);

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
  FramerSetEnableCompressionHelper(&framer, true);

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
  FramerSetEnableCompressionHelper(&framer, false);

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
  FramerSetEnableCompressionHelper(&framer, true);

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
  FramerSetEnableCompressionHelper(&framer, false);

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
  FramerSetEnableCompressionHelper(&framer, true);

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

std::string RandomString(int length) {
  std::string rv;
  for (int index = 0; index < length; index++)
    rv += static_cast<char>('a' + (rand() % 26));
  return rv;
}

// Stress that we can handle a really large header block compression and
// decompression.
TEST_F(SpdyFramerTest, HugeHeaderBlock) {
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
