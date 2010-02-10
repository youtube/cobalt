// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

using spdy::SpdyFrame;
using spdy::SpdyFrameBuilder;
using spdy::SpdyFramer;
using spdy::SpdyHeaderBlock;
using spdy::SpdySynStreamControlFrame;
using spdy::kControlFlagMask;
using spdy::CONTROL_FLAG_NONE;
using spdy::SYN_STREAM;
using spdy::test::FramerSetEnableCompressionHelper;
using spdy::test::TestSpdyVisitor;

namespace {

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
  framer.ParseHeaderBlock(frame.get(), &new_headers);

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

  frame.WriteUInt16(2);  // Number of headers.
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
  scoped_ptr<SpdyFrame> frame3(framer.DecompressFrame(frame1.get()));

  // Decompress the second frame
  scoped_ptr<SpdyFrame> frame4(framer.DecompressFrame(frame2.get()));

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
  scoped_ptr<SpdyFrame> frame2(framer.DecompressFrame(frame1.get()));

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
  EXPECT_EQ(0, visitor.data_bytes_);
  EXPECT_EQ(0, visitor.fin_frame_count_);
  EXPECT_EQ(1, visitor.fin_flag_count_);
  EXPECT_EQ(1, visitor.zero_length_data_frame_count_);
}

}  // namespace

