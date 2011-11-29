// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_bitmasks.h"
#include "net/spdy/spdy_framer.h"
#include "testing/platform_test.h"

using spdy::CONTROL_FLAG_FIN;
using spdy::CONTROL_FLAG_NONE;
using spdy::GOAWAY;
using spdy::HEADERS;
using spdy::NOOP;
using spdy::NUM_CONTROL_FRAME_TYPES;
using spdy::PING;
using spdy::RST_STREAM;
using spdy::SETTINGS;
using spdy::SYN_REPLY;
using spdy::SYN_STREAM;
using spdy::WINDOW_UPDATE;
using spdy::FlagsAndLength;
using spdy::SpdyControlFrame;
using spdy::SpdyControlType;
using spdy::SpdyDataFrame;
using spdy::SpdyFrame;
using spdy::SpdyFramer;
using spdy::SpdyHeaderBlock;
using spdy::SpdyHeadersControlFrame;
using spdy::SpdyGoAwayControlFrame;
using spdy::SpdyNoOpControlFrame;
using spdy::SpdyPingControlFrame;
using spdy::SpdyRstStreamControlFrame;
using spdy::SpdySettings;
using spdy::SpdySettingsControlFrame;
using spdy::SpdyStatusCodes;
using spdy::SpdySynReplyControlFrame;
using spdy::SpdySynStreamControlFrame;
using spdy::SpdyWindowUpdateControlFrame;
using spdy::SettingsFlagsAndId;
using spdy::kLengthMask;
using spdy::kSpdyProtocolVersion;
using spdy::kStreamIdMask;

namespace {

// Test our protocol constants
TEST(SpdyProtocolTest, ProtocolConstants) {
  EXPECT_EQ(8u, SpdyFrame::size());
  EXPECT_EQ(8u, SpdyDataFrame::size());
  EXPECT_EQ(8u, SpdyControlFrame::size());
  EXPECT_EQ(18u, SpdySynStreamControlFrame::size());
  EXPECT_EQ(14u, SpdySynReplyControlFrame::size());
  EXPECT_EQ(16u, SpdyRstStreamControlFrame::size());
  EXPECT_EQ(12u, SpdySettingsControlFrame::size());
  EXPECT_EQ(8u, SpdyNoOpControlFrame::size());
  EXPECT_EQ(12u, SpdyPingControlFrame::size());
  EXPECT_EQ(12u, SpdyGoAwayControlFrame::size());
  EXPECT_EQ(14u, SpdyHeadersControlFrame::size());
  EXPECT_EQ(16u, SpdyWindowUpdateControlFrame::size());
  EXPECT_EQ(4u, sizeof(FlagsAndLength));
  EXPECT_EQ(1, SYN_STREAM);
  EXPECT_EQ(2, SYN_REPLY);
  EXPECT_EQ(3, RST_STREAM);
  EXPECT_EQ(4, SETTINGS);
  EXPECT_EQ(5, NOOP);
  EXPECT_EQ(6, PING);
  EXPECT_EQ(7, GOAWAY);
  EXPECT_EQ(8, HEADERS);
  EXPECT_EQ(9, WINDOW_UPDATE);
}

// Test some of the protocol helper functions
TEST(SpdyProtocolTest, FrameStructs) {
  SpdyFrame frame(SpdyFrame::size());
  frame.set_length(12345);
  frame.set_flags(10);
  EXPECT_EQ(12345u, frame.length());
  EXPECT_EQ(10u, frame.flags());
  EXPECT_FALSE(frame.is_control_frame());

  frame.set_length(0);
  frame.set_flags(10);
  EXPECT_EQ(0u, frame.length());
  EXPECT_EQ(10u, frame.flags());
  EXPECT_FALSE(frame.is_control_frame());
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
      framer.CreateSynStream(123, 456, 2, CONTROL_FLAG_FIN, false, &headers));
  EXPECT_EQ(kSpdyProtocolVersion, syn_frame->version());
  EXPECT_TRUE(syn_frame->is_control_frame());
  EXPECT_EQ(SYN_STREAM, syn_frame->type());
  EXPECT_EQ(123u, syn_frame->stream_id());
  EXPECT_EQ(456u, syn_frame->associated_stream_id());
  EXPECT_EQ(2u, syn_frame->priority());
  EXPECT_EQ(2, syn_frame->header_block_len());
  EXPECT_EQ(1u, syn_frame->flags());
  syn_frame->set_associated_stream_id(999u);
  EXPECT_EQ(123u, syn_frame->stream_id());
  EXPECT_EQ(999u, syn_frame->associated_stream_id());

  scoped_ptr<SpdySynReplyControlFrame> syn_reply(
      framer.CreateSynReply(123, CONTROL_FLAG_NONE, false, &headers));
  EXPECT_EQ(kSpdyProtocolVersion, syn_reply->version());
  EXPECT_TRUE(syn_reply->is_control_frame());
  EXPECT_EQ(SYN_REPLY, syn_reply->type());
  EXPECT_EQ(123u, syn_reply->stream_id());
  EXPECT_EQ(2, syn_reply->header_block_len());
  EXPECT_EQ(0, syn_reply->flags());

  scoped_ptr<SpdyRstStreamControlFrame> rst_frame(
      framer.CreateRstStream(123, spdy::PROTOCOL_ERROR));
  EXPECT_EQ(kSpdyProtocolVersion, rst_frame->version());
  EXPECT_TRUE(rst_frame->is_control_frame());
  EXPECT_EQ(RST_STREAM, rst_frame->type());
  EXPECT_EQ(123u, rst_frame->stream_id());
  EXPECT_EQ(spdy::PROTOCOL_ERROR, rst_frame->status());
  rst_frame->set_status(spdy::INVALID_STREAM);
  EXPECT_EQ(spdy::INVALID_STREAM, rst_frame->status());
  EXPECT_EQ(0, rst_frame->flags());

  scoped_ptr<SpdyNoOpControlFrame> noop_frame(
      framer.CreateNopFrame());
  EXPECT_EQ(kSpdyProtocolVersion, noop_frame->version());
  EXPECT_TRUE(noop_frame->is_control_frame());
  EXPECT_EQ(NOOP, noop_frame->type());
  EXPECT_EQ(0, noop_frame->flags());

  const uint32 kUniqueId = 1234567u;
  const uint32 kUniqueId2 = 31415926u;
  scoped_ptr<SpdyPingControlFrame> ping_frame(
      framer.CreatePingFrame(kUniqueId));
  EXPECT_EQ(kSpdyProtocolVersion, ping_frame->version());
  EXPECT_TRUE(ping_frame->is_control_frame());
  EXPECT_EQ(PING, ping_frame->type());
  EXPECT_EQ(kUniqueId, ping_frame->unique_id());
  ping_frame->set_unique_id(kUniqueId2);
  EXPECT_EQ(kUniqueId2, ping_frame->unique_id());

  scoped_ptr<SpdyGoAwayControlFrame> goaway_frame(
      framer.CreateGoAway(123));
  EXPECT_EQ(kSpdyProtocolVersion, goaway_frame->version());
  EXPECT_TRUE(goaway_frame->is_control_frame());
  EXPECT_EQ(GOAWAY, goaway_frame->type());
  EXPECT_EQ(123u, goaway_frame->last_accepted_stream_id());

  scoped_ptr<SpdyHeadersControlFrame> headers_frame(
      framer.CreateHeaders(123, CONTROL_FLAG_NONE, false, &headers));
  EXPECT_EQ(kSpdyProtocolVersion, headers_frame->version());
  EXPECT_TRUE(headers_frame->is_control_frame());
  EXPECT_EQ(HEADERS, headers_frame->type());
  EXPECT_EQ(123u, headers_frame->stream_id());
  EXPECT_EQ(2, headers_frame->header_block_len());
  EXPECT_EQ(0, headers_frame->flags());

  scoped_ptr<SpdyWindowUpdateControlFrame> window_update_frame(
      framer.CreateWindowUpdate(123, 456));
  EXPECT_EQ(kSpdyProtocolVersion, window_update_frame->version());
  EXPECT_TRUE(window_update_frame->is_control_frame());
  EXPECT_EQ(WINDOW_UPDATE, window_update_frame->type());
  EXPECT_EQ(123u, window_update_frame->stream_id());
  EXPECT_EQ(456u, window_update_frame->delta_window_size());
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
  frame.set_flags(0u);
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

// Test various types of SETTINGS frames.
TEST(SpdyProtocolTest, TestSpdySettingsFrame) {
  SpdyFramer framer;

  // Create a settings frame with no settings.
  SpdySettings settings;
  scoped_ptr<SpdySettingsControlFrame> settings_frame(
      framer.CreateSettings(settings));
  EXPECT_EQ(kSpdyProtocolVersion, settings_frame->version());
  EXPECT_TRUE(settings_frame->is_control_frame());
  EXPECT_EQ(SETTINGS, settings_frame->type());
  EXPECT_EQ(0u, settings_frame->num_entries());

  // We'll add several different ID/Flag combinations and then verify
  // that they encode and decode properly.
  SettingsFlagsAndId ids[] = {
    0x00000000,
    0xffffffff,
    0xff000001,
    0x01000002,
  };

  for (size_t index = 0; index < arraysize(ids); ++index) {
    settings.insert(settings.end(), std::make_pair(ids[index], index));
    settings_frame.reset(framer.CreateSettings(settings));
    EXPECT_EQ(kSpdyProtocolVersion, settings_frame->version());
    EXPECT_TRUE(settings_frame->is_control_frame());
    EXPECT_EQ(SETTINGS, settings_frame->type());
    EXPECT_EQ(index + 1, settings_frame->num_entries());

    SpdySettings parsed_settings;
    EXPECT_TRUE(framer.ParseSettings(settings_frame.get(), &parsed_settings));
    EXPECT_EQ(parsed_settings.size(), settings.size());
    SpdySettings::const_iterator it = parsed_settings.begin();
    int pos = 0;
    while (it != parsed_settings.end()) {
      SettingsFlagsAndId parsed = it->first;
      uint32 value = it->second;
      EXPECT_EQ(parsed.flags(), ids[pos].flags());
      EXPECT_EQ(parsed.id(), ids[pos].id());
      EXPECT_EQ(value, static_cast<uint32>(pos));
      ++it;
      ++pos;
    }
  }
}

TEST(SpdyProtocolTest, HasHeaderBlock) {
  SpdyControlFrame frame(SpdyControlFrame::size());
  for (SpdyControlType type = SYN_STREAM;
      type < NUM_CONTROL_FRAME_TYPES;
      type = static_cast<SpdyControlType>(type + 1)) {
    frame.set_type(type);
    if (type == SYN_STREAM || type == SYN_REPLY || type == HEADERS) {
      EXPECT_TRUE(frame.has_header_block());
    } else {
      EXPECT_FALSE(frame.has_header_block());
    }
  }
}

// Make sure that overflows both die in debug mode, and do not cause problems
// in opt mode.  Note:  The EXPECT_DEBUG_DEATH call does not work on Win32 yet,
// so we comment it out.
TEST(SpdyProtocolDeathTest, TestDataFrame) {
  SpdyDataFrame frame;

  frame.set_stream_id(0);
  // TODO(mbelshe):  implement EXPECT_DEBUG_DEATH on windows.
#if !defined(WIN32) && defined(GTEST_HAS_DEATH_TEST)
#if !defined(DCHECK_ALWAYS_ON)
  EXPECT_DEBUG_DEATH(frame.set_stream_id(~0), "");
#else
  EXPECT_DEATH(frame.set_stream_id(~0), "");
#endif
#endif
  EXPECT_FALSE(frame.is_control_frame());

  frame.set_flags(0);
#if !defined(WIN32) && defined(GTEST_HAS_DEATH_TEST)
#if !defined(DCHECK_ALWAYS_ON)
  EXPECT_DEBUG_DEATH(frame.set_length(~0), "");
#else
  EXPECT_DEATH(frame.set_length(~0), "");
#endif
#endif
  EXPECT_EQ(0, frame.flags());
}

TEST(SpdyProtocolDeathTest, TestSpdyControlFrameStreamId) {
  SpdyControlFrame frame_store(SpdySynStreamControlFrame::size());
  memset(frame_store.data(), '1', SpdyControlFrame::size());
  SpdySynStreamControlFrame* frame =
      reinterpret_cast<SpdySynStreamControlFrame*>(&frame_store);

  // Set the stream ID to various values.
  frame->set_stream_id(0);
  EXPECT_EQ(0u, frame->stream_id());
  EXPECT_FALSE(frame->is_control_frame());
  frame->set_stream_id(kStreamIdMask);
  EXPECT_EQ(kStreamIdMask, frame->stream_id());
  EXPECT_FALSE(frame->is_control_frame());
}

TEST(SpdyProtocolDeathTest, TestSpdyControlFrameVersion) {
  const unsigned int kVersionMask = 0x7fff;
  SpdyControlFrame frame(SpdySynStreamControlFrame::size());
  memset(frame.data(), '1', SpdyControlFrame::size());

  // Set the version to various values, and make sure it does not affect the
  // type.
  frame.set_type(SYN_STREAM);
  frame.set_version(0);
  EXPECT_EQ(0, frame.version());
  EXPECT_TRUE(frame.is_control_frame());
  EXPECT_EQ(SYN_STREAM, frame.type());

  SpdySynStreamControlFrame* syn_stream =
      reinterpret_cast<SpdySynStreamControlFrame*>(&frame);
  syn_stream->set_stream_id(~0 & kVersionMask);
  EXPECT_EQ(~0 & kVersionMask, syn_stream->stream_id());
  EXPECT_TRUE(frame.is_control_frame());
  EXPECT_EQ(SYN_STREAM, frame.type());
}

TEST(SpdyProtocolDeathTest, TestSpdyControlFrameType) {
  SpdyControlFrame frame(SpdyControlFrame::size());
  memset(frame.data(), 255, SpdyControlFrame::size());

  // type() should be out of bounds.
  EXPECT_FALSE(frame.AppearsToBeAValidControlFrame());

  uint16 version = frame.version();

  for (int i = SYN_STREAM; i <= spdy::NOOP; ++i) {
    frame.set_type(static_cast<SpdyControlType>(i));
    EXPECT_EQ(i, static_cast<int>(frame.type()));
    EXPECT_TRUE(frame.AppearsToBeAValidControlFrame());
    // Make sure setting type does not alter the version block.
    EXPECT_EQ(version, frame.version());
    EXPECT_TRUE(frame.is_control_frame());
  }
}

TEST(SpdyProtocolDeathTest, TestRstStreamStatusBounds) {
  SpdyFramer framer;
  scoped_ptr<SpdyRstStreamControlFrame> rst_frame;

  rst_frame.reset(framer.CreateRstStream(123, spdy::PROTOCOL_ERROR));
  EXPECT_EQ(spdy::PROTOCOL_ERROR, rst_frame->status());

  rst_frame->set_status(spdy::INVALID);
  EXPECT_EQ(spdy::INVALID, rst_frame->status());

  rst_frame->set_status(
      static_cast<spdy::SpdyStatusCodes>(spdy::INVALID - 1));
  EXPECT_EQ(spdy::INVALID, rst_frame->status());

  rst_frame->set_status(spdy::NUM_STATUS_CODES);
  EXPECT_EQ(spdy::INVALID, rst_frame->status());
}

}  // namespace
