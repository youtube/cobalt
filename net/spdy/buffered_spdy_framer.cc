// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/buffered_spdy_framer.h"

#include "base/logging.h"

namespace spdy {

BufferedSpdyFramer::BufferedSpdyFramer()
    : visitor_(NULL),
      header_buffer_used_(0),
      header_buffer_valid_(false),
      header_stream_id_(SpdyFramer::kInvalidStream) {
}

BufferedSpdyFramer::~BufferedSpdyFramer() {
}

void BufferedSpdyFramer::set_visitor(
    BufferedSpdyFramerVisitorInterface* visitor) {
  visitor_ = visitor;
  spdy_framer_.set_visitor(visitor);
}

void BufferedSpdyFramer::OnControl(const SpdyControlFrame* frame) {
  switch (frame->type()) {
    case SYN_STREAM:
    case SYN_REPLY:
    case HEADERS:
      InitHeaderStreaming(frame);
      break;
    default:
      DCHECK(false);  // Error!
      break;
  }
}

bool BufferedSpdyFramer::OnControlFrameHeaderData(
    const SpdyControlFrame* control_frame,
    const char* header_data,
    size_t len) {
  SpdyStreamId stream_id =
      SpdyFramer::GetControlFrameStreamId(control_frame);
  CHECK_EQ(header_stream_id_, stream_id);

  if (len == 0) {
    // Indicates end-of-header-block.
    CHECK(header_buffer_valid_);

    const linked_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
    bool parsed_headers = SpdyFramer::ParseHeaderBlockInBuffer(
        header_buffer_, header_buffer_used_, headers.get());
    if (!parsed_headers) {
      LOG(WARNING) << "Could not parse Spdy Control Frame Header.";
      return false;
    }
    switch (control_frame->type()) {
      case SYN_STREAM:
        visitor_->OnSyn(
            *reinterpret_cast<const SpdySynStreamControlFrame*>(
                control_frame), headers);
        break;
      case SYN_REPLY:
        visitor_->OnSynReply(
            *reinterpret_cast<const SpdySynReplyControlFrame*>(
                control_frame), headers);
        break;
      case HEADERS:
        visitor_->OnHeaders(
            *reinterpret_cast<const SpdyHeadersControlFrame*>(
                control_frame), headers);
        break;
      default:
        DCHECK(false);  // Error!
        break;
    }
    return true;
  }

  const size_t available = kHeaderBufferSize - header_buffer_used_;
  if (len > available) {
    header_buffer_valid_ = false;
    return false;
  }
  memcpy(header_buffer_ + header_buffer_used_, header_data, len);
  header_buffer_used_ += len;
  return true;
}

void BufferedSpdyFramer::OnDataFrameHeader(const SpdyDataFrame* frame) {
  header_stream_id_ = frame->stream_id();
}

size_t BufferedSpdyFramer::ProcessInput(const char* data, size_t len) {
  return spdy_framer_.ProcessInput(data, len);
}

void BufferedSpdyFramer::Reset() {
  spdy_framer_.Reset();
}

SpdyFramer::SpdyError BufferedSpdyFramer::error_code() const {
  return spdy_framer_.error_code();
}

SpdyFramer::SpdyState BufferedSpdyFramer::state() const {
  return spdy_framer_.state();
}

bool BufferedSpdyFramer::MessageFullyRead() {
  return spdy_framer_.MessageFullyRead();
}

bool BufferedSpdyFramer::HasError() {
  return spdy_framer_.HasError();
}

bool BufferedSpdyFramer::ParseHeaderBlock(const SpdyFrame* frame,
                                          SpdyHeaderBlock* block) {
  return spdy_framer_.ParseHeaderBlock(frame, block);
}

SpdySynStreamControlFrame* BufferedSpdyFramer::CreateSynStream(
    SpdyStreamId stream_id,
    SpdyStreamId associated_stream_id,
    int priority,
    SpdyControlFlags flags,
    bool compressed,
    const SpdyHeaderBlock* headers) {
  return spdy_framer_.CreateSynStream(
      stream_id, associated_stream_id, priority, flags, compressed, headers);
}

SpdySynReplyControlFrame* BufferedSpdyFramer::CreateSynReply(
    SpdyStreamId stream_id,
    SpdyControlFlags flags,
    bool compressed,
    const SpdyHeaderBlock* headers) {
  return spdy_framer_.CreateSynReply(stream_id, flags, compressed, headers);
}

SpdyHeadersControlFrame* BufferedSpdyFramer::CreateHeaders(
    SpdyStreamId stream_id,
    SpdyControlFlags flags,
    bool compressed,
    const SpdyHeaderBlock* headers) {
  return spdy_framer_.CreateHeaders(stream_id, flags, compressed, headers);
}

SpdyDataFrame* BufferedSpdyFramer::CreateDataFrame(SpdyStreamId stream_id,
                                                   const char* data,
                                                   uint32 len,
                                                   SpdyDataFlags flags) {
  return spdy_framer_.CreateDataFrame(stream_id, data, len, flags);
}

SpdyFrame* BufferedSpdyFramer::CompressFrame(const SpdyFrame& frame) {
  return spdy_framer_.CompressFrame(frame);
}

bool BufferedSpdyFramer::IsCompressible(const SpdyFrame& frame) const {
  return spdy_framer_.IsCompressible(frame);
}

void BufferedSpdyFramer::InitHeaderStreaming(const SpdyControlFrame* frame) {
  memset(header_buffer_, 0, kHeaderBufferSize);
  header_buffer_used_ = 0;
  header_buffer_valid_ = true;
  header_stream_id_ = SpdyFramer::GetControlFrameStreamId(frame);
  DCHECK_NE(header_stream_id_, SpdyFramer::kInvalidStream);
}

}  // namespace spdy
