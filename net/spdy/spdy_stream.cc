// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "net/spdy/spdy_session.h"

namespace net {

SpdyStream::SpdyStream(
    SpdySession* session, spdy::SpdyStreamId stream_id, bool pushed)
    : stream_id_(stream_id),
      priority_(0),
      pushed_(pushed),
      metrics_(Singleton<BandwidthMetrics>::get()),
      syn_reply_received_(false),
      session_(session),
      delegate_(NULL),
      request_time_(base::Time::Now()),
      response_(new spdy::SpdyHeaderBlock),
      response_complete_(false),
      io_state_(STATE_NONE),
      response_status_(OK),
      cancelled_(false),
      send_bytes_(0),
      recv_bytes_(0),
      histograms_recorded_(false) {}

SpdyStream::~SpdyStream() {
  DLOG(INFO) << "Deleting SpdyStream for stream " << stream_id_;

  // When the stream_id_ is 0, we expect that it is because
  // we've cancelled or closed the stream and set the stream_id to 0.
  if (!stream_id_)
    DCHECK(response_complete_);
}

void SpdyStream::SetDelegate(Delegate* delegate) {
  CHECK(delegate);
  delegate_ = delegate;

  if (!response_->empty()) {
    // The stream already got response.
    delegate_->OnResponseReceived(*response_, response_time_, OK);
  }

  std::vector<scoped_refptr<IOBufferWithSize> > buffers;
  buffers.swap(pending_buffers_);
  for (size_t i = 0; i < buffers.size(); ++i) {
    if (delegate_)
      delegate_->OnDataReceived(buffers[i]->data(), buffers[i]->size());
  }
}

void SpdyStream::DetachDelegate() {
  delegate_ = NULL;
  if (!cancelled())
    Cancel();
}

const linked_ptr<spdy::SpdyHeaderBlock>& SpdyStream::spdy_headers() const {
  return request_;
}

void SpdyStream::set_spdy_headers(
    const linked_ptr<spdy::SpdyHeaderBlock>& headers) {
  request_ = headers;
}

void SpdyStream::UpdateWindowSize(int delta_window_size) {
  DCHECK_GE(delta_window_size, 1);
  int new_window_size = window_size_ + delta_window_size;

  // it's valid for window_size_ to become negative (via an incoming
  // SETTINGS frame which is handled in SpdySession::OnSettings), in which
  // case incoming WINDOW_UPDATEs will eventually make it positive;
  // however, if window_size_ is positive and incoming WINDOW_UPDATE makes
  // it negative, we have an overflow.
  if (window_size_ > 0 && new_window_size < 0) {
    LOG(WARNING) << "Received WINDOW_UPDATE [delta:" << delta_window_size
                 << "] for stream " << stream_id_
                 << " overflows window size [current:" << window_size_ << "]";
    session_->ResetStream(stream_id_, spdy::FLOW_CONTROL_ERROR);
    return;
  }
  window_size_ = new_window_size;
}

base::Time SpdyStream::GetRequestTime() const {
  return request_time_;
}

void SpdyStream::SetRequestTime(base::Time t) {
  request_time_ = t;
}

int SpdyStream::OnResponseReceived(const spdy::SpdyHeaderBlock& response) {
  int rv = OK;
  LOG(INFO) << "OnResponseReceived";
  DCHECK_NE(io_state_, STATE_OPEN);

  metrics_.StartStream();

  DCHECK(response_->empty());
  *response_ = response;  // TODO(ukai): avoid copy.

  recv_first_byte_time_ = base::TimeTicks::Now();
  response_time_ = base::Time::Now();

  if (io_state_ == STATE_NONE) {
    CHECK(pushed_);
    io_state_ = STATE_READ_HEADERS;
  } else if (io_state_ == STATE_READ_HEADERS_COMPLETE) {
    // This SpdyStream could be in this state in both true and false pushed_
    // conditions.
    // The false pushed_ condition (client request) will always go through
    // this state.
    // The true pushed_condition (server push) can be in this state when the
    // client requests an X-Associated-Content piece of content prior
    // to when the server push happens.
  } else {
    // We're not expecting a response while in this state.  Error!
    rv = ERR_SPDY_PROTOCOL_ERROR;
  }

  rv = DoLoop(rv);
  if (delegate_)
    rv = delegate_->OnResponseReceived(*response_, response_time_, rv);
  // if delegate_ is not yet attached, we'll return response when delegate
  // gets attached to the stream.

  return rv;
}

void SpdyStream::OnDataReceived(const char* data, int length) {
  DCHECK_GE(length, 0);
  LOG(INFO) << "SpdyStream: Data (" << length << " bytes) received for "
            << stream_id_;

  CHECK(!response_complete_);

  // If we don't have a response, then the SYN_REPLY did not come through.
  // We cannot pass data up to the caller unless the reply headers have been
  // received.
  if (response_->empty()) {
    session_->CloseStream(stream_id_, ERR_SYN_REPLY_NOT_RECEIVED);
    return;
  }

  // A zero-length read means that the stream is being closed.
  if (!length) {
    metrics_.StopStream();
    scoped_refptr<SpdyStream> self(this);
    session_->CloseStream(stream_id_, net::OK);
    UpdateHistograms();
    return;
  }

  // Track our bandwidth.
  metrics_.RecordBytes(length);
  recv_bytes_ += length;
  recv_last_byte_time_ = base::TimeTicks::Now();

  if (!delegate_) {
    // It should be valid for this to happen in the server push case.
    // We'll return received data when delegate gets attached to the stream.
    IOBufferWithSize* buf = new IOBufferWithSize(length);
    memcpy(buf->data(), data, length);
    pending_buffers_.push_back(buf);
    return;
  }

  delegate_->OnDataReceived(data, length);
}

void SpdyStream::OnWriteComplete(int status) {
  // TODO(mbelshe): Check for cancellation here.  If we're cancelled, we
  // should discontinue the DoLoop.

  // It is possible that this stream was closed while we had a write pending.
  if (response_complete_)
    return;

  if (status > 0)
    send_bytes_ += status;

  DoLoop(status);
}

void SpdyStream::OnClose(int status) {
  response_complete_ = true;
  response_status_ = status;
  stream_id_ = 0;
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  if (delegate)
    delegate->OnClose(status);
}

void SpdyStream::Cancel() {
  cancelled_ = true;
  session_->CloseStream(stream_id_, ERR_ABORTED);
}

int SpdyStream::DoSendRequest(bool has_upload_data) {
  CHECK(!cancelled_);

  if (!pushed_) {
    spdy::SpdyControlFlags flags = spdy::CONTROL_FLAG_NONE;
    if (!has_upload_data)
      flags = spdy::CONTROL_FLAG_FIN;

    CHECK(request_.get());
    int result = session_->WriteSynStream(
        stream_id_, static_cast<RequestPriority>(priority_), flags,
        request_);
    if (result != ERR_IO_PENDING)
      return result;
  }

  send_time_ = base::TimeTicks::Now();

  int result = OK;
  if (!pushed_) {
    DCHECK_EQ(io_state_, STATE_NONE);
    io_state_ = STATE_SEND_HEADERS;
  } else {
    DCHECK(!has_upload_data);
    if (!response_->empty()) {
      // We already have response headers, so we don't need to read the header.
      // Pushed stream should not have upload data.
      // We don't need to call DoLoop() in this state.
      DCHECK_EQ(io_state_, STATE_OPEN);
      return OK;
    } else {
      io_state_ = STATE_READ_HEADERS;
    }
  }
  return DoLoop(result);
}

int SpdyStream::DoReadResponseHeaders() {
  CHECK(!cancelled_);

  // The SYN_REPLY has already been received.
  if (!response_->empty()) {
    CHECK_EQ(STATE_OPEN, io_state_);
    return OK;
  } else {
    CHECK_EQ(STATE_NONE, io_state_);
  }


  io_state_ = STATE_READ_HEADERS;
  // Q: do we need to run DoLoop here?
  return ERR_IO_PENDING;
}

int SpdyStream::WriteStreamData(IOBuffer* data, int length,
                                spdy::SpdyDataFlags flags) {
  return session_->WriteStreamData(stream_id_, data, length, flags);
}

bool SpdyStream::GetSSLInfo(SSLInfo* ssl_info, bool* was_npn_negotiated) {
  return session_->GetSSLInfo(ssl_info, was_npn_negotiated);
}

int SpdyStream::DoLoop(int result) {
  do {
    State state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      // State machine 1: Send headers and wait for response headers.
      case STATE_SEND_HEADERS:
        CHECK_EQ(OK, result);
        net_log_.BeginEvent(NetLog::TYPE_SPDY_STREAM_SEND_HEADERS, NULL);
        result = DoSendHeaders();
        break;
      case STATE_SEND_HEADERS_COMPLETE:
        net_log_.EndEvent(NetLog::TYPE_SPDY_STREAM_SEND_HEADERS, NULL);
        result = DoSendHeadersComplete(result);
        break;
      case STATE_SEND_BODY:
        CHECK_EQ(OK, result);
        net_log_.BeginEvent(NetLog::TYPE_SPDY_STREAM_SEND_BODY, NULL);
        result = DoSendBody();
        break;
      case STATE_SEND_BODY_COMPLETE:
        net_log_.EndEvent(NetLog::TYPE_SPDY_STREAM_SEND_BODY, NULL);
        result = DoSendBodyComplete(result);
        break;
      case STATE_READ_HEADERS:
        CHECK_EQ(OK, result);
        net_log_.BeginEvent(NetLog::TYPE_SPDY_STREAM_READ_HEADERS, NULL);
        result = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        net_log_.EndEvent(NetLog::TYPE_SPDY_STREAM_READ_HEADERS, NULL);
        result = DoReadHeadersComplete(result);
        break;

      // State machine 2: connection is established.
      // In STATE_OPEN, OnResponseReceived has already been called.
      // OnDataReceived, OnClose and OnWriteCompelte can be called.
      // Only OnWriteCompletee calls DoLoop(().
      //
      // For HTTP streams, no data is sent from the client while in the OPEN
      // state, so OnWriteComplete is never called here.  The HTTP body is
      // handled in the OnDataReceived callback, which does not call into
      // DoLoop.
      //
      // For WebSocket streams, which are bi-directional, we'll send and
      // receive data once the connection is established.  Received data is
      // handled in OnDataReceived.  Sent data is handled in OnWriteComplete,
      // which calls DoOpen().
      case STATE_OPEN:
        result = DoOpen(result);
        break;

      case STATE_DONE:
        DCHECK(result != ERR_IO_PENDING);
        break;
      default:
        NOTREACHED() << io_state_;
        break;
    }
  } while (result != ERR_IO_PENDING && io_state_ != STATE_NONE &&
           io_state_ != STATE_OPEN);

  return result;
}

int SpdyStream::DoSendHeaders() {
  // The SpdySession will always call us back when the send is complete.
  // TODO(willchan): This code makes the assumption that for the non-push stream
  // case, the client code calls SendRequest() after creating the stream and
  // before yielding back to the MessageLoop.  This is true in the current code,
  // but is not obvious from the headers.  We should make the code handle
  // SendRequest() being called after the SYN_REPLY has been received.
  io_state_ = STATE_SEND_HEADERS_COMPLETE;
  return ERR_IO_PENDING;
}

int SpdyStream::DoSendHeadersComplete(int result) {
  if (result < 0)
    return result;

  CHECK_GT(result, 0);

  if (!delegate_)
    return ERR_UNEXPECTED;

  // There is no body, skip that state.
  if (delegate_->OnSendHeadersComplete(result)) {
    io_state_ = STATE_READ_HEADERS;
    return OK;
  }

  io_state_ = STATE_SEND_BODY;
  return OK;
}

// DoSendBody is called to send the optional body for the request.  This call
// will also be called as each write of a chunk of the body completes.
int SpdyStream::DoSendBody() {
  // If we're already in the STATE_SENDING_BODY state, then we've already
  // sent a portion of the body.  In that case, we need to first consume
  // the bytes written in the body stream.  Note that the bytes written is
  // the number of bytes in the frame that were written, only consume the
  // data portion, of course.
  io_state_ = STATE_SEND_BODY_COMPLETE;
  if (!delegate_)
    return ERR_UNEXPECTED;
  return delegate_->OnSendBody();
}

int SpdyStream::DoSendBodyComplete(int result) {
  if (result < 0)
    return result;

  CHECK_NE(result, 0);

  if (!delegate_)
    return ERR_UNEXPECTED;

  if (!delegate_->OnSendBodyComplete(result))
    io_state_ = STATE_SEND_BODY;
  else
    io_state_ = STATE_READ_HEADERS;

  return OK;
}

int SpdyStream::DoReadHeaders() {
  io_state_ = STATE_READ_HEADERS_COMPLETE;
  return !response_->empty() ? OK : ERR_IO_PENDING;
}

int SpdyStream::DoReadHeadersComplete(int result) {
  io_state_ = STATE_OPEN;
  return result;
}

int SpdyStream::DoOpen(int result) {
  if (delegate_)
    delegate_->OnDataSent(result);
  io_state_ = STATE_OPEN;
  return result;
}

void SpdyStream::UpdateHistograms() {
  if (histograms_recorded_)
    return;

  histograms_recorded_ = true;

  // We need all timers to be filled in, otherwise metrics can be bogus.
  if (send_time_.is_null() || recv_first_byte_time_.is_null() ||
      recv_last_byte_time_.is_null())
    return;

  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTimeToFirstByte",
      recv_first_byte_time_ - send_time_);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamDownloadTime",
      recv_last_byte_time_ - recv_first_byte_time_);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTime",
      recv_last_byte_time_ - send_time_);

  UMA_HISTOGRAM_COUNTS("Net.SpdySendBytes", send_bytes_);
  UMA_HISTOGRAM_COUNTS("Net.SpdyRecvBytes", recv_bytes_);
}

}  // namespace net
