// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_BUFFERED_SPDY_FRAMER_H_
#define NET_SPDY_BUFFERED_SPDY_FRAMER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE BufferedSpdyFramerVisitorInterface {
 public:
  BufferedSpdyFramerVisitorInterface() {}

  // Called if an error is detected in the SpdyFrame protocol.
  virtual void OnError(SpdyFramer::SpdyError error_code) = 0;

  // Called if an error is detected in a SPDY stream.
  virtual void OnStreamError(SpdyStreamId stream_id,
                             const std::string& description) = 0;

  // Called after all the header data for SYN_STREAM control frame is received.
  virtual void OnSynStream(const SpdySynStreamControlFrame& frame,
                           const linked_ptr<SpdyHeaderBlock>& headers) = 0;

  // Called after all the header data for SYN_REPLY control frame is received.
  virtual void OnSynReply(const SpdySynReplyControlFrame& frame,
                          const linked_ptr<SpdyHeaderBlock>& headers) = 0;

  // Called after all the header data for HEADERS control frame is received.
  virtual void OnHeaders(const SpdyHeadersControlFrame& frame,
                         const linked_ptr<SpdyHeaderBlock>& headers) = 0;

  // Called after a RST_STREAM frame is received.
  virtual void OnRstStream(const SpdyRstStreamControlFrame& frame) = 0;

  // Called after a GOAWAY frame is received.
  virtual void OnGoAway(const SpdyGoAwayControlFrame& frame) = 0;

  // Called after a PING frame is received.
  virtual void OnPing(const SpdyPingControlFrame& frame) = 0;

  // Called after a WINDOW_UPDATE frame is received.
  virtual void OnWindowUpdate(
      const SpdyWindowUpdateControlFrame& frame) = 0;

  // Called when data is received.
  // |stream_id| The stream receiving data.
  // |data| A buffer containing the data received.
  // |len| The length of the data buffer.
  // When the other side has finished sending data on this stream,
  // this method will be called with a zero-length buffer.
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len) = 0;

  // Called when an individual setting within a SETTINGS frame has been parsed
  // and validated.
  virtual void OnSetting(SpdySettingsIds id, uint8 flags, uint32 value) = 0;

  // Called after a control frame has been compressed to allow the visitor
  // to record compression statistics.
  virtual void OnControlFrameCompressed(
      const SpdyControlFrame& uncompressed_frame,
      const SpdyControlFrame& compressed_frame) = 0;

 protected:
  virtual ~BufferedSpdyFramerVisitorInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedSpdyFramerVisitorInterface);
};

class NET_EXPORT_PRIVATE BufferedSpdyFramer
    : public SpdyFramerVisitorInterface {
 public:
  explicit BufferedSpdyFramer(int version);
  virtual ~BufferedSpdyFramer();

  // Sets callbacks to be called from the buffered spdy framer.  A visitor must
  // be set, or else the framer will likely crash.  It is acceptable for the
  // visitor to do nothing.  If this is called multiple times, only the last
  // visitor will be used.
  void set_visitor(BufferedSpdyFramerVisitorInterface* visitor);

  // SpdyFramerVisitorInterface
  virtual void OnError(SpdyFramer* spdy_framer) OVERRIDE;
  virtual void OnControl(const SpdyControlFrame* frame) OVERRIDE;
  virtual bool OnCredentialFrameData(const char* frame_data,
                                     size_t len) OVERRIDE;
  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE;
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len) OVERRIDE;
  virtual void OnSetting(
      SpdySettingsIds id, uint8 flags, uint32 value) OVERRIDE;
  virtual void OnDataFrameHeader(const SpdyDataFrame* frame) OVERRIDE;

  // Called after a control frame has been compressed to allow the visitor
  // to record compression statistics.
  virtual void OnControlFrameCompressed(
      const SpdyControlFrame& uncompressed_frame,
      const SpdyControlFrame& compressed_frame) OVERRIDE;

  // SpdyFramer methods.
  size_t ProcessInput(const char* data, size_t len);
  int protocol_version();
  void Reset();
  SpdyFramer::SpdyError error_code() const;
  SpdyFramer::SpdyState state() const;
  bool MessageFullyRead();
  bool HasError();
  SpdySynStreamControlFrame* CreateSynStream(SpdyStreamId stream_id,
                                             SpdyStreamId associated_stream_id,
                                             SpdyPriority priority,
                                             uint8 credential_slot,
                                             SpdyControlFlags flags,
                                             bool compressed,
                                             const SpdyHeaderBlock* headers);
  SpdySynReplyControlFrame* CreateSynReply(SpdyStreamId stream_id,
                                           SpdyControlFlags flags,
                                           bool compressed,
                                           const SpdyHeaderBlock* headers);
  SpdyRstStreamControlFrame* CreateRstStream(SpdyStreamId stream_id,
                                             SpdyStatusCodes status) const;
  SpdySettingsControlFrame* CreateSettings(const SettingsMap& values) const;
  SpdyPingControlFrame* CreatePingFrame(uint32 unique_id) const;
  SpdyGoAwayControlFrame* CreateGoAway(
      SpdyStreamId last_accepted_stream_id,
      SpdyGoAwayStatus status) const;
  SpdyHeadersControlFrame* CreateHeaders(SpdyStreamId stream_id,
                                         SpdyControlFlags flags,
                                         bool compressed,
                                         const SpdyHeaderBlock* headers);
  SpdyWindowUpdateControlFrame* CreateWindowUpdate(
      SpdyStreamId stream_id,
      uint32 delta_window_size) const;
  SpdyCredentialControlFrame* CreateCredentialFrame(
      const SpdyCredential& credential) const;
  SpdyDataFrame* CreateDataFrame(SpdyStreamId stream_id,
                                 const char* data,
                                 uint32 len,
                                 SpdyDataFlags flags);
  SpdyPriority GetHighestPriority() const;
  bool IsCompressible(const SpdyFrame& frame) const;
  SpdyControlFrame* CompressControlFrame(const SpdyControlFrame& frame);
  // Specify if newly created SpdySessions should have compression enabled.
  static void set_enable_compression_default(bool value);

  int frames_received() const { return frames_received_; }

 private:
  // The size of the header_buffer_.
  enum { kHeaderBufferSize = 32 * 1024 };

  void InitHeaderStreaming(const SpdyControlFrame* frame);

  SpdyFramer spdy_framer_;
  BufferedSpdyFramerVisitorInterface* visitor_;

  // Header block streaming state:
  char header_buffer_[kHeaderBufferSize];
  size_t header_buffer_used_;
  bool header_buffer_valid_;
  SpdyStreamId header_stream_id_;
  scoped_ptr<SpdyFrame> control_frame_;
  int frames_received_;

  DISALLOW_COPY_AND_ASSIGN(BufferedSpdyFramer);
};

}  // namespace net

#endif  // NET_SPDY_BUFFERED_SPDY_FRAMER_H_
