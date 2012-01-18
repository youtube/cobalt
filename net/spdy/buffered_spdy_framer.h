// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_BUFFERED_SPDY_FRAMER_H_
#define NET_SPDY_BUFFERED_SPDY_FRAMER_H_
#pragma once

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

namespace spdy {

class NET_EXPORT_PRIVATE BufferedSpdyFramerVisitorInterface
    : public SpdyFramerVisitorInterface  {
 public:
  BufferedSpdyFramerVisitorInterface() {}
  virtual ~BufferedSpdyFramerVisitorInterface() {}

  // Called after all the header data for SYN_STREAM control frame is received.
  virtual void OnSyn(const SpdySynStreamControlFrame& frame,
                     const linked_ptr<SpdyHeaderBlock>& headers) = 0;

  // Called after all the header data for SYN_REPLY control frame is received.
  virtual void OnSynReply(const SpdySynReplyControlFrame& frame,
                          const linked_ptr<SpdyHeaderBlock>& headers) = 0;

  // Called after all the header data for HEADERS control frame is received.
  virtual void OnHeaders(const SpdyHeadersControlFrame& frame,
                         const linked_ptr<SpdyHeaderBlock>& headers) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferedSpdyFramerVisitorInterface);
};

class NET_EXPORT_PRIVATE BufferedSpdyFramer {
 public:
  BufferedSpdyFramer();
  virtual ~BufferedSpdyFramer();

  // Sets callbacks to be called from the buffered spdy framer.  A visitor must
  // be set, or else the framer will likely crash.  It is acceptable for the
  // visitor to do nothing.  If this is called multiple times, only the last
  // visitor will be used.
  void set_visitor(BufferedSpdyFramerVisitorInterface* visitor);

  void OnControl(const SpdyControlFrame* frame);

  bool OnControlFrameHeaderData(const SpdyControlFrame* control_frame,
                                const char* header_data,
                                size_t len);

  void OnDataFrameHeader(const SpdyDataFrame* frame);

  // spdy_framer_ methods.
  size_t ProcessInput(const char* data, size_t len);
  void Reset();
  SpdyFramer::SpdyError error_code() const;
  SpdyFramer::SpdyState state() const;
  bool MessageFullyRead();
  bool HasError();
  bool ParseHeaderBlock(const SpdyFrame* frame, SpdyHeaderBlock* block);
  SpdySynStreamControlFrame* CreateSynStream(SpdyStreamId stream_id,
                                             SpdyStreamId associated_stream_id,
                                             int priority,
                                             SpdyControlFlags flags,
                                             bool compressed,
                                             const SpdyHeaderBlock* headers);
  SpdySynReplyControlFrame* CreateSynReply(SpdyStreamId stream_id,
                                           SpdyControlFlags flags,
                                           bool compressed,
                                           const SpdyHeaderBlock* headers);
  SpdyHeadersControlFrame* CreateHeaders(SpdyStreamId stream_id,
                                         SpdyControlFlags flags,
                                         bool compressed,
                                         const SpdyHeaderBlock* headers);
  SpdyDataFrame* CreateDataFrame(SpdyStreamId stream_id,
                                 const char* data,
                                 uint32 len,
                                 SpdyDataFlags flags);
  SpdyFrame* CompressFrame(const SpdyFrame& frame);
  bool IsCompressible(const SpdyFrame& frame) const;

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

  DISALLOW_COPY_AND_ASSIGN(BufferedSpdyFramer);
};

}  // namespace spdy

#endif  // NET_SPDY_BUFFERED_SPDY_FRAMER_H_
