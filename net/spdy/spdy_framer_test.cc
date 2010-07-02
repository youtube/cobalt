// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>

#include "base/scoped_ptr.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_frame_builder.h"
#include "testing/platform_test.h"

namespace spdy {

namespace test {

void FramerSetEnableCompressionHelper(SpdyFramer* framer, bool compress) {
  framer->set_enable_compression(compress);
}

class TestSpdyVisitor : public SpdyFramerVisitorInterface  {
 public:
  TestSpdyVisitor()
    : error_count_(0),
      syn_frame_count_(0),
      syn_reply_frame_count_(0),
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
      default:
        DCHECK(false);  // Error!
    }
    if (frame->flags() & CONTROL_FLAG_FIN)
      ++fin_flag_count_;
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
using spdy::test::FramerSetEnableCompressionHelper;
using spdy::test::TestSpdyVisitor;

namespace spdy {

class SpdyFramerTest : public PlatformTest {
 public:
  virtual void TearDown() {}
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
  frame1.WriteUInt32(0);  // associated stream id
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
  frame2.WriteUInt32(0);  // associated stream id
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
    0x80, 0x01, 0x00, 0x01,   // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x00, 0x00, 0x00, 0x01,   // DATA on Stream #1
    0x00, 0x00, 0x00, 0x0c,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,
      0xde, 0xad, 0xbe, 0xef,

    0x80, 0x01, 0x00, 0x01,   // SYN Stream #3
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

    0x80, 0x01, 0x00, 0x03,   // RST_STREAM on Stream #1
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x03,   // DATA on Stream #3
    0x00, 0x00, 0x00, 0x00,

    0x80, 0x01, 0x00, 0x03,   // RST_STREAM on Stream #3
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x00,
  };

  TestSpdyVisitor visitor;
  visitor.SimulateInFramer(input, sizeof(input));

  EXPECT_EQ(0, visitor.error_count_);
  EXPECT_EQ(2, visitor.syn_frame_count_);
  EXPECT_EQ(0, visitor.syn_reply_frame_count_);
  EXPECT_EQ(24, visitor.data_bytes_);
  EXPECT_EQ(2, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(0, visitor.zero_length_data_frame_count_);
}

// Test that the FIN flag on a data frame signifies EOF.
TEST_F(SpdyFramerTest, FinOnDataFrame) {
  const unsigned char input[] = {
    0x80, 0x01, 0x00, 0x01,   // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, 0x01, 0x00, 0x02,   // SYN REPLY Stream #1
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
  EXPECT_EQ(16, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(0, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
}

// Test that the FIN flag on a SYN reply frame signifies EOF.
TEST_F(SpdyFramerTest, FinOnSynReplyFrame) {
  const unsigned char input[] = {
    0x80, 0x01, 0x00, 0x01,   // SYN Stream #1
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x02, 'h', 'h',
    0x00, 0x02, 'v', 'v',

    0x80, 0x01, 0x00, 0x02,   // SYN REPLY Stream #1
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
      send_framer.CreateDataFrame(1, bytes, arraysize(bytes),
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

}  // namespace

