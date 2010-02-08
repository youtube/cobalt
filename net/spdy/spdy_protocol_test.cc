// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

#include "base/scoped_ptr.h"
#include "net/spdy/spdy_bitmasks.h"
#include "net/spdy/spdy_framer.h"
#include "testing/platform_test.h"

using spdy::SpdyDataFrame;
using spdy::SpdyFrame;
using spdy::SpdyControlFrame;
using spdy::SpdySynStreamControlFrame;
using spdy::SpdySynReplyControlFrame;
using spdy::SpdyFinStreamControlFrame;
using spdy::SpdyFramer;
using spdy::SpdyHeaderBlock;
using spdy::FlagsAndLength;
using spdy::kLengthMask;
using spdy::kStreamIdMask;
using spdy::kSpdyProtocolVersion;
using spdy::SYN_STREAM;
using spdy::SYN_REPLY;
using spdy::FIN_STREAM;
using spdy::CONTROL_FLAG_FIN;
using spdy::CONTROL_FLAG_NONE;

namespace {

// Test our protocol constants
TEST(SpdyProtocolTest, ProtocolConstants) {
  EXPECT_EQ(8u, SpdyFrame::size());
  EXPECT_EQ(8u, SpdyDataFrame::size());
  EXPECT_EQ(12u, SpdyControlFrame::size());
  EXPECT_EQ(14u, SpdySynStreamControlFrame::size());
  EXPECT_EQ(14u, SpdySynReplyControlFrame::size());
  EXPECT_EQ(16u, SpdyFinStreamControlFrame::size());
  EXPECT_EQ(4u, sizeof(FlagsAndLength));
  EXPECT_EQ(1, SYN_STREAM);
  EXPECT_EQ(2, SYN_REPLY);
  EXPECT_EQ(3, FIN_STREAM);
}

// Test some of the protocol helper functions
TEST(SpdyProtocolTest, FrameStructs) {
  SpdyFrame frame(SpdyFrame::size());
  frame.set_length(12345);
  frame.set_flags(10);
  EXPECT_EQ(12345u, frame.length());
  EXPECT_EQ(10u, frame.flags());
  EXPECT_EQ(false, frame.is_control_frame());

  frame.set_length(0);
  frame.set_flags(10);
  EXPECT_EQ(0u, frame.length());
  EXPECT_EQ(10u, frame.flags());
  EXPECT_EQ(false, frame.is_control_frame());
}

TEST(SpdyProtocolTest, DataFrameStructs) {
  SpdyDataFrame data_frame;
  data_frame.set_stream_id(12345);
  EXPECT_EQ(12345u, data_frame.stream_id());
}

TEST(SpdyProtocolTest, ControlFrameStructs) {
  SpdyFramer framer;
  SpdyHeaderBlock headers;

  scoped_ptr<SpdySynStreamControlFrame> syn_frame(
      framer.CreateSynStream(123, 2, CONTROL_FLAG_FIN, false, &headers));
  EXPECT_EQ(kSpdyProtocolVersion, syn_frame->version());
  EXPECT_EQ(true, syn_frame->is_control_frame());
  EXPECT_EQ(SYN_STREAM, syn_frame->type());
  EXPECT_EQ(123u, syn_frame->stream_id());
  EXPECT_EQ(2u, syn_frame->priority());
  EXPECT_EQ(2, syn_frame->header_block_len());
  EXPECT_EQ(1u, syn_frame->flags());

  scoped_ptr<SpdySynReplyControlFrame> syn_reply(
      framer.CreateSynReply(123, CONTROL_FLAG_NONE, false, &headers));
  EXPECT_EQ(kSpdyProtocolVersion, syn_reply->version());
  EXPECT_EQ(true, syn_reply->is_control_frame());
  EXPECT_EQ(SYN_REPLY, syn_reply->type());
  EXPECT_EQ(123u, syn_reply->stream_id());
  EXPECT_EQ(2, syn_reply->header_block_len());
  EXPECT_EQ(0, syn_reply->flags());

  scoped_ptr<SpdyFinStreamControlFrame> fin_frame(
      framer.CreateFinStream(123, 444));
  EXPECT_EQ(kSpdyProtocolVersion, fin_frame->version());
  EXPECT_EQ(true, fin_frame->is_control_frame());
  EXPECT_EQ(FIN_STREAM, fin_frame->type());
  EXPECT_EQ(123u, fin_frame->stream_id());
  EXPECT_EQ(444u, fin_frame->status());
  fin_frame->set_status(555);
  EXPECT_EQ(555u, fin_frame->status());
  EXPECT_EQ(0, fin_frame->flags());
}

TEST(SpdyProtocolTest, TestDataFrame) {
  SpdyDataFrame frame;

  // Set the stream ID to various values.
  frame.set_stream_id(0);
  EXPECT_EQ(0u, frame.stream_id());
  EXPECT_FALSE(frame.is_control_frame());
  frame.set_stream_id(~0 & kStreamIdMask);
  EXPECT_EQ(~0 & kStreamIdMask, frame.stream_id());
  EXPECT_FALSE(frame.is_control_frame());

  // Set length to various values.  Make sure that when you set_length(x),
  // length() == x.  Also make sure the flags are unaltered.
  memset(frame.data(), '1', SpdyDataFrame::size());
  int8 flags = frame.flags();
  frame.set_length(0);
  EXPECT_EQ(0u, frame.length());
  EXPECT_EQ(flags, frame.flags());
  frame.set_length(kLengthMask);
  EXPECT_EQ(kLengthMask, frame.length());
  EXPECT_EQ(flags, frame.flags());
  frame.set_length(5u);
  EXPECT_EQ(5u, frame.length());
  EXPECT_EQ(flags, frame.flags());

  // Set flags to various values.  Make sure that when you set_flags(x),
  // flags() == x.  Also make sure the length is unaltered.
  memset(frame.data(), '1', SpdyDataFrame::size());
  uint32 length = frame.length();
  frame.set_flags(0);
  EXPECT_EQ(0u, frame.flags());
  EXPECT_EQ(length, frame.length());
  int8 all_flags = ~0;
  frame.set_flags(all_flags);
  flags = frame.flags();
  EXPECT_EQ(all_flags, flags);
  EXPECT_EQ(length, frame.length());
  frame.set_flags(5u);
  EXPECT_EQ(5u, frame.flags());
  EXPECT_EQ(length, frame.length());
}

// Make sure that overflows both die in debug mode, and do not cause problems
// in opt mode.  Note:  Chrome doesn't die on DCHECK failures, so the
// EXPECT_DEBUG_DEATH doesn't work.
TEST(SpdyProtocolDeathTest, TestDataFrame) {
  SpdyDataFrame frame;

  frame.set_stream_id(0);
#ifndef WIN32
  EXPECT_DEBUG_DEATH(frame.set_stream_id(~0), "");
#endif
  EXPECT_FALSE(frame.is_control_frame());

  frame.set_flags(0);
#ifndef WIN32
  EXPECT_DEBUG_DEATH(frame.set_length(~0), "");
#endif
  EXPECT_EQ(0, frame.flags());
}

TEST(SpdyProtocolDeathTest, TestSpdyControlFrame) {
  SpdyControlFrame frame(SpdyControlFrame::size());
  memset(frame.data(), '1', SpdyControlFrame::size());

  // Set the stream ID to various values.
  frame.set_stream_id(0);
  EXPECT_EQ(0u, frame.stream_id());
  EXPECT_FALSE(frame.is_control_frame());
  frame.set_stream_id(~0 & kStreamIdMask);
  EXPECT_EQ(~0 & kStreamIdMask, frame.stream_id());
  EXPECT_FALSE(frame.is_control_frame());
}

}  // namespace

