// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_bitmasks.h"
#include "net/spdy/spdy_framer.h"
#include "testing/platform_test.h"

namespace {

enum SpdyProtocolTestTypes {
  SPDY2 = 2,
  SPDY3 = 3,
};

} // namespace

namespace net {

class SpdyProtocolTest
    : public ::testing::TestWithParam<SpdyProtocolTestTypes> {
 protected:
  virtual void SetUp() {
    spdy_version_ = GetParam();
  }

  bool IsSpdy2() { return spdy_version_ == SPDY2; }

  // Version of SPDY protocol to be used.
  int spdy_version_;
};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolTests,
                        SpdyProtocolTest,
                        ::testing::Values(SPDY2, SPDY3));

// Test our protocol constants
TEST_P(SpdyProtocolTest, ProtocolConstants) {
  EXPECT_EQ(8u, SpdyFrame::kHeaderSize);
  EXPECT_EQ(8u, SpdyDataFrame::size());
  EXPECT_EQ(8u, SpdyControlFrame::kHeaderSize);
  EXPECT_EQ(18u, SpdySynStreamControlFrame::size());
  EXPECT_EQ(12u, SpdySynReplyControlFrame::size());
  EXPECT_EQ(16u, SpdyRstStreamControlFrame::size());
  EXPECT_EQ(12u, SpdySettingsControlFrame::size());
  EXPECT_EQ(12u, SpdyPingControlFrame::size());
  EXPECT_EQ(16u, SpdyGoAwayControlFrame::size());
  EXPECT_EQ(12u, SpdyHeadersControlFrame::size());
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
TEST_P(SpdyProtocolTest, FrameStructs) {
  SpdyFrame frame(SpdyFrame::kHeaderSize);
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

TEST_P(SpdyProtocolTest, DataFrameStructs) {
  SpdyDataFrame data_frame;
  data_frame.set_stream_id(12345);
  EXPECT_EQ(12345u, data_frame.stream_id());
}

TEST_P(SpdyProtocolTest, ControlFrameStructs) {
  SpdyFramer framer(spdy_version_);
  SpdyHeaderBlock headers;

  const uint8 credential_slot = IsSpdy2() ? 0 : 5;

  scoped_ptr<SpdySynStreamControlFrame> syn_frame(framer.CreateSynStream(
      123, 456, 2, credential_slot, CONTROL_FLAG_FIN, false, &headers));
  EXPECT_EQ(framer.protocol_version(), syn_frame->version());
  EXPECT_TRUE(syn_frame->is_control_frame());
  EXPECT_EQ(SYN_STREAM, syn_frame->type());
  EXPECT_EQ(123u, syn_frame->stream_id());
  EXPECT_EQ(456u, syn_frame->associated_stream_id());
  EXPECT_EQ(2u, syn_frame->priority());
  EXPECT_EQ(credential_slot, syn_frame->credential_slot());
  EXPECT_EQ(IsSpdy2() ? 2 : 4, syn_frame->header_block_len());
  EXPECT_EQ(1u, syn_frame->flags());
  syn_frame->set_associated_stream_id(999u);
  EXPECT_EQ(123u, syn_frame->stream_id());
  EXPECT_EQ(999u, syn_frame->associated_stream_id());

  scoped_ptr<SpdySynReplyControlFrame> syn_reply(
      framer.CreateSynReply(123, CONTROL_FLAG_NONE, false, &headers));
  EXPECT_EQ(framer.protocol_version(), syn_reply->version());
  EXPECT_TRUE(syn_reply->is_control_frame());
  EXPECT_EQ(SYN_REPLY, syn_reply->type());
  EXPECT_EQ(123u, syn_reply->stream_id());
  EXPECT_EQ(IsSpdy2() ? 2 : 4, syn_reply->header_block_len());
  EXPECT_EQ(0, syn_reply->flags());

  scoped_ptr<SpdyRstStreamControlFrame> rst_frame(
      framer.CreateRstStream(123, PROTOCOL_ERROR));
  EXPECT_EQ(framer.protocol_version(), rst_frame->version());
  EXPECT_TRUE(rst_frame->is_control_frame());
  EXPECT_EQ(RST_STREAM, rst_frame->type());
  EXPECT_EQ(123u, rst_frame->stream_id());
  EXPECT_EQ(PROTOCOL_ERROR, rst_frame->status());
  rst_frame->set_status(INVALID_STREAM);
  EXPECT_EQ(INVALID_STREAM, rst_frame->status());
  EXPECT_EQ(0, rst_frame->flags());

  const uint32 kUniqueId = 1234567u;
  const uint32 kUniqueId2 = 31415926u;
  scoped_ptr<SpdyPingControlFrame> ping_frame(
      framer.CreatePingFrame(kUniqueId));
  EXPECT_EQ(framer.protocol_version(), ping_frame->version());
  EXPECT_TRUE(ping_frame->is_control_frame());
  EXPECT_EQ(PING, ping_frame->type());
  EXPECT_EQ(kUniqueId, ping_frame->unique_id());
  ping_frame->set_unique_id(kUniqueId2);
  EXPECT_EQ(kUniqueId2, ping_frame->unique_id());

  scoped_ptr<SpdyGoAwayControlFrame> goaway_frame(
      framer.CreateGoAway(123, GOAWAY_INTERNAL_ERROR));
  EXPECT_EQ(framer.protocol_version(), goaway_frame->version());
  EXPECT_TRUE(goaway_frame->is_control_frame());
  EXPECT_EQ(GOAWAY, goaway_frame->type());
  EXPECT_EQ(123u, goaway_frame->last_accepted_stream_id());
  if (!IsSpdy2()) {
    EXPECT_EQ(GOAWAY_INTERNAL_ERROR, goaway_frame->status());
  }

  scoped_ptr<SpdyHeadersControlFrame> headers_frame(
      framer.CreateHeaders(123, CONTROL_FLAG_NONE, false, &headers));
  EXPECT_EQ(framer.protocol_version(), headers_frame->version());
  EXPECT_TRUE(headers_frame->is_control_frame());
  EXPECT_EQ(HEADERS, headers_frame->type());
  EXPECT_EQ(123u, headers_frame->stream_id());
  EXPECT_EQ(IsSpdy2() ? 2 : 4, headers_frame->header_block_len());
  EXPECT_EQ(0, headers_frame->flags());

  scoped_ptr<SpdyWindowUpdateControlFrame> window_update_frame(
      framer.CreateWindowUpdate(123, 456));
  EXPECT_EQ(framer.protocol_version(), window_update_frame->version());
  EXPECT_TRUE(window_update_frame->is_control_frame());
  EXPECT_EQ(WINDOW_UPDATE, window_update_frame->type());
  EXPECT_EQ(123u, window_update_frame->stream_id());
  EXPECT_EQ(456u, window_update_frame->delta_window_size());
}

TEST_P(SpdyProtocolTest, TestDataFrame) {
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
TEST_P(SpdyProtocolTest, TestSpdySettingsFrame) {
  SpdyFramer framer(spdy_version_);

  // Create a settings frame with no settings.
  SettingsMap settings;
  scoped_ptr<SpdySettingsControlFrame> settings_frame(
      framer.CreateSettings(settings));
  EXPECT_EQ(framer.protocol_version(), settings_frame->version());
  EXPECT_TRUE(settings_frame->is_control_frame());
  EXPECT_EQ(SETTINGS, settings_frame->type());
  EXPECT_EQ(0u, settings_frame->num_entries());

  // We'll add several different ID/Flag combinations and then verify
  // that they encode and decode properly.
  SettingsFlagsAndId ids[] = {
    SettingsFlagsAndId::FromWireFormat(spdy_version_, 0x00000000),
    SettingsFlagsAndId::FromWireFormat(spdy_version_, 0xffffffff),
    SettingsFlagsAndId::FromWireFormat(spdy_version_, 0xff000001),
    SettingsFlagsAndId::FromWireFormat(spdy_version_, 0x01000002),
    SettingsFlagsAndId(6, 9)
  };

  for (size_t index = 0; index < arraysize(ids); ++index) {
    SettingsFlagsAndId flags_and_id = ids[index];
    SpdySettingsIds id = static_cast<SpdySettingsIds>(flags_and_id.id());
    SpdySettingsFlags flags =
        static_cast<SpdySettingsFlags>(flags_and_id.flags());
    settings[id] = SettingsFlagsAndValue(flags, index);
    settings_frame.reset(framer.CreateSettings(settings));
    EXPECT_EQ(framer.protocol_version(), settings_frame->version());
    EXPECT_TRUE(settings_frame->is_control_frame());
    EXPECT_EQ(SETTINGS, settings_frame->type());
    EXPECT_EQ(index + 1, settings_frame->num_entries());

    SettingsMap parsed_settings;
    EXPECT_TRUE(framer.ParseSettings(settings_frame.get(), &parsed_settings));
    EXPECT_EQ(settings.size(), parsed_settings.size());
    for (SettingsMap::const_iterator it = parsed_settings.begin();
         it != parsed_settings.end();
         it++) {
      SettingsMap::const_iterator it2 = settings.find(it->first);
      EXPECT_EQ(it->first, it2->first);
      SettingsFlagsAndValue parsed = it->second;
      SettingsFlagsAndValue created = it2->second;
      EXPECT_EQ(created.first, parsed.first);
      EXPECT_EQ(created.second, parsed.second);
    }
  }
}

TEST_P(SpdyProtocolTest, HasHeaderBlock) {
  SpdyControlFrame frame(SpdyControlFrame::kHeaderSize);
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

class SpdyProtocolDeathTest : public SpdyProtocolTest {};

// All tests are run with two different SPDY versions: SPDY/2 and SPDY/3.
INSTANTIATE_TEST_CASE_P(SpdyProtocolDeathTests,
                        SpdyProtocolDeathTest,
                        ::testing::Values(SPDY2, SPDY3));

// Make sure that overflows both die in debug mode, and do not cause problems
// in opt mode.  Note:  The EXPECT_DEBUG_DEATH call does not work on Win32 yet,
// so we comment it out.
TEST_P(SpdyProtocolDeathTest, TestDataFrame) {
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

TEST_P(SpdyProtocolDeathTest, TestSpdyControlFrameStreamId) {
  SpdyControlFrame frame_store(SpdySynStreamControlFrame::size());
  memset(frame_store.data(), '1', SpdyControlFrame::kHeaderSize);
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

TEST_P(SpdyProtocolDeathTest, TestSpdyControlFrameVersion) {
  const unsigned int kVersionMask = 0x7fff;
  SpdyControlFrame frame(SpdySynStreamControlFrame::size());
  memset(frame.data(), '1', SpdyControlFrame::kHeaderSize);

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

TEST_P(SpdyProtocolDeathTest, TestSpdyControlFrameType) {
  SpdyControlFrame frame(SpdyControlFrame::kHeaderSize);
  memset(frame.data(), 255, SpdyControlFrame::kHeaderSize);

  // type() should be out of bounds.
  EXPECT_FALSE(frame.AppearsToBeAValidControlFrame());

  frame.set_version(spdy_version_);
  uint16 version = frame.version();

  for (int i = SYN_STREAM; i <= WINDOW_UPDATE; ++i) {
    frame.set_type(static_cast<SpdyControlType>(i));
    EXPECT_EQ(i, static_cast<int>(frame.type()));
    if (!IsSpdy2() && i == NOOP) {
      // NOOP frames aren't 'valid'.
      EXPECT_FALSE(frame.AppearsToBeAValidControlFrame());
    } else {
      EXPECT_TRUE(frame.AppearsToBeAValidControlFrame());
    }
    // Make sure setting type does not alter the version block.
    EXPECT_EQ(version, frame.version());
    EXPECT_TRUE(frame.is_control_frame());
  }
}

TEST_P(SpdyProtocolDeathTest, TestRstStreamStatusBounds) {
  SpdyFramer framer(spdy_version_);
  scoped_ptr<SpdyRstStreamControlFrame> rst_frame;

  rst_frame.reset(framer.CreateRstStream(123, PROTOCOL_ERROR));
  EXPECT_EQ(PROTOCOL_ERROR, rst_frame->status());

  rst_frame->set_status(INVALID);
  EXPECT_EQ(INVALID, rst_frame->status());

  rst_frame->set_status(
      static_cast<SpdyStatusCodes>(INVALID - 1));
  EXPECT_EQ(INVALID, rst_frame->status());

  rst_frame->set_status(NUM_STATUS_CODES);
  EXPECT_EQ(INVALID, rst_frame->status());
}

}  // namespace net
