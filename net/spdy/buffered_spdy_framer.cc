// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/buffered_spdy_framer.h"

#include "base/logging.h"

namespace {

bool g_enable_compression_default = true;

}  // namespace

namespace net {

BufferedSpdyFramer::BufferedSpdyFramer(int version)
    : spdy_framer_(version),
      visitor_(NULL),
      header_buffer_used_(0),
      header_buffer_valid_(false),
      header_stream_id_(SpdyFramer::kInvalidStream),
      frames_received_(0) {
  spdy_framer_.set_enable_compression(g_enable_compression_default);
  memset(header_buffer_, 0, sizeof(header_buffer_));
}

BufferedSpdyFramer::~BufferedSpdyFramer() {
}

void BufferedSpdyFramer::set_visitor(
    BufferedSpdyFramerVisitorInterface* visitor) {
  visitor_ = visitor;
  spdy_framer_.set_visitor(this);
}

void BufferedSpdyFramer::OnError(SpdyFramer* spdy_framer) {
  DCHECK(spdy_framer);
  visitor_->OnError(spdy_framer->error_code());
}

void BufferedSpdyFramer::OnControl(const SpdyControlFrame* frame) {
  frames_received_++;
  switch (frame->type()) {
    case SYN_STREAM:
    case SYN_REPLY:
    case HEADERS:
      InitHeaderStreaming(frame);
      break;
    case GOAWAY:
      visitor_->OnGoAway(
          *reinterpret_cast<const SpdyGoAwayControlFrame*>(frame));
      break;
    case PING:
      visitor_->OnPing(
          *reinterpret_cast<const SpdyPingControlFrame*>(frame));
      break;
    case SETTINGS:
      break;
    case RST_STREAM:
      visitor_->OnRstStream(
          *reinterpret_cast<const SpdyRstStreamControlFrame*>(frame));
      break;
    case WINDOW_UPDATE:
      visitor_->OnWindowUpdate(
          *reinterpret_cast<const SpdyWindowUpdateControlFrame*>(frame));
      break;
    default:
      NOTREACHED();  // Error!
  }
}

bool BufferedSpdyFramer::OnCredentialFrameData(const char* frame_data,
                                               size_t len) {
  DCHECK(false);
  return false;
}

bool BufferedSpdyFramer::OnControlFrameHeaderData(SpdyStreamId stream_id,
                                                  const char* header_data,
                                                  size_t len) {
  CHECK_EQ(header_stream_id_, stream_id);

  if (len == 0) {
    // Indicates end-of-header-block.
    CHECK(header_buffer_valid_);

    const linked_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
    bool parsed_headers = spdy_framer_.ParseHeaderBlockInBuffer(
        header_buffer_, header_buffer_used_, headers.get());
    if (!parsed_headers) {
      visitor_->OnStreamError(
          stream_id, "Could not parse Spdy Control Frame Header.");
      return false;
    }
    SpdyControlFrame* control_frame =
        reinterpret_cast<SpdyControlFrame*>(control_frame_.get());
    switch (control_frame->type()) {
      case SYN_STREAM:
        visitor_->OnSynStream(
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
    visitor_->OnStreamError(
        stream_id, "Received more data than the allocated size.");
    return false;
  }
  memcpy(header_buffer_ + header_buffer_used_, header_data, len);
  header_buffer_used_ += len;
  return true;
}

void BufferedSpdyFramer::OnDataFrameHeader(const SpdyDataFrame* frame) {
  frames_received_++;
  header_stream_id_ = frame->stream_id();
}

void BufferedSpdyFramer::OnStreamFrameData(SpdyStreamId stream_id,
                                           const char* data,
                                           size_t len) {
  visitor_->OnStreamFrameData(stream_id, data, len);
}

void BufferedSpdyFramer::OnSetting(SpdySettingsIds id,
                                   uint8 flags,
                                   uint32 value) {
  visitor_->OnSetting(id, flags, value);
}

void BufferedSpdyFramer::OnControlFrameCompressed(
    const SpdyControlFrame& uncompressed_frame,
    const SpdyControlFrame& compressed_frame) {
  visitor_->OnControlFrameCompressed(uncompressed_frame, compressed_frame);
}


int BufferedSpdyFramer::protocol_version() {
  return spdy_framer_.protocol_version();
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

SpdySynStreamControlFrame* BufferedSpdyFramer::CreateSynStream(
    SpdyStreamId stream_id,
    SpdyStreamId associated_stream_id,
    SpdyPriority priority,
    uint8 credential_slot,
    SpdyControlFlags flags,
    bool compressed,
    const SpdyHeaderBlock* headers) {
  return spdy_framer_.CreateSynStream(stream_id, associated_stream_id, priority,
                                      credential_slot, flags, compressed,
                                      headers);
}

SpdySynReplyControlFrame* BufferedSpdyFramer::CreateSynReply(
    SpdyStreamId stream_id,
    SpdyControlFlags flags,
    bool compressed,
    const SpdyHeaderBlock* headers) {
  return spdy_framer_.CreateSynReply(stream_id, flags, compressed, headers);
}

SpdyRstStreamControlFrame* BufferedSpdyFramer::CreateRstStream(
    SpdyStreamId stream_id,
    SpdyStatusCodes status) const {
  return spdy_framer_.CreateRstStream(stream_id, status);
}

SpdySettingsControlFrame* BufferedSpdyFramer::CreateSettings(
    const SettingsMap& values) const {
  return spdy_framer_.CreateSettings(values);
}

SpdyPingControlFrame* BufferedSpdyFramer::CreatePingFrame(
    uint32 unique_id) const {
  return spdy_framer_.CreatePingFrame(unique_id);
}

SpdyGoAwayControlFrame* BufferedSpdyFramer::CreateGoAway(
    SpdyStreamId last_accepted_stream_id,
    SpdyGoAwayStatus status) const {
  return spdy_framer_.CreateGoAway(last_accepted_stream_id, status);
}

SpdyHeadersControlFrame* BufferedSpdyFramer::CreateHeaders(
    SpdyStreamId stream_id,
    SpdyControlFlags flags,
    bool compressed,
    const SpdyHeaderBlock* headers) {
  return spdy_framer_.CreateHeaders(stream_id, flags, compressed, headers);
}

SpdyWindowUpdateControlFrame* BufferedSpdyFramer::CreateWindowUpdate(
    SpdyStreamId stream_id,
    uint32 delta_window_size) const {
  return spdy_framer_.CreateWindowUpdate(stream_id, delta_window_size);
}

SpdyCredentialControlFrame* BufferedSpdyFramer::CreateCredentialFrame(
    const SpdyCredential& credential) const {
  return spdy_framer_.CreateCredentialFrame(credential);
}

SpdyDataFrame* BufferedSpdyFramer::CreateDataFrame(SpdyStreamId stream_id,
                                                   const char* data,
                                                   uint32 len,
                                                   SpdyDataFlags flags) {
  return spdy_framer_.CreateDataFrame(stream_id, data, len, flags);
}

SpdyPriority BufferedSpdyFramer::GetHighestPriority() const {
  return spdy_framer_.GetHighestPriority();
}

bool BufferedSpdyFramer::IsCompressible(const SpdyFrame& frame) const {
  return spdy_framer_.IsCompressible(frame);
}

SpdyControlFrame* BufferedSpdyFramer::CompressControlFrame(
    const SpdyControlFrame& frame) {
  return spdy_framer_.CompressControlFrame(frame);
}

// static
void BufferedSpdyFramer::set_enable_compression_default(bool value) {
  g_enable_compression_default = value;
}

void BufferedSpdyFramer::InitHeaderStreaming(const SpdyControlFrame* frame) {
  memset(header_buffer_, 0, kHeaderBufferSize);
  header_buffer_used_ = 0;
  header_buffer_valid_ = true;
  header_stream_id_ = SpdyFramer::GetControlFrameStreamId(frame);
  DCHECK_NE(header_stream_id_, SpdyFramer::kInvalidStream);

  int32 frame_size_without_header_block = SpdyFrame::kHeaderSize;
  switch (frame->type()) {
    case SYN_STREAM:
      frame_size_without_header_block = SpdySynStreamControlFrame::size();
      break;
    case SYN_REPLY:
      frame_size_without_header_block = SpdySynReplyControlFrame::size();
      break;
    case HEADERS:
      frame_size_without_header_block = SpdyHeadersControlFrame::size();
      break;
    default:
      DCHECK(false);  // Error!
      break;
  }
  control_frame_.reset(new SpdyFrame(frame_size_without_header_block));
  memcpy(control_frame_.get()->data(), frame->data(),
         frame_size_without_header_block);
}

}  // namespace net
