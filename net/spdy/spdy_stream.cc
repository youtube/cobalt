// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/spdy/spdy_session.h"

namespace net {

SpdyStream::SpdyStream(
    SpdySession* session, spdy::SpdyStreamId stream_id, bool pushed)
    : stream_id_(stream_id),
      priority_(0),
      pushed_(pushed),
      metrics_(Singleton<BandwidthMetrics>::get()),
      session_(session),
      request_time_(base::Time::Now()),
      response_(NULL),
      response_complete_(false),
      io_state_(STATE_NONE),
      response_status_(OK),
      cancelled_(false),
      send_bytes_(0),
      recv_bytes_(0),
      histograms_recorded_(false) {}

SpdyStream::~SpdyStream() {
  DLOG(INFO) << "Deleting SpdyStream for stream " << stream_id_;

  // TODO(willchan): We're still calling CancelStream() too many times, because
  // inactive pending/pushed streams will still have stream_id_ set.
  if (stream_id_) {
    session_->CancelStream(stream_id_);
  } else {
    // When the stream_id_ is 0, we expect that it is because
    // we've cancelled or closed the stream and set the stream_id to 0.
    DCHECK(response_complete_);
  }
}

const HttpResponseInfo* SpdyStream::GetResponseInfo() const {
  return response_;
}

void SpdyStream::SetPushResponse(HttpResponseInfo* response_info) {
  DCHECK(!response_);
  DCHECK(!push_response_.get());
  push_response_.reset(response_info);
  response_ = response_info;
}

const HttpRequestInfo* SpdyStream::GetRequestInfo() const {
  return &request_;
}

void SpdyStream::SetRequestInfo(const HttpRequestInfo& request) {
  request_ = request;
}

base::Time SpdyStream::GetRequestTime() const {
  return request_time_;
}

void SpdyStream::SetRequestTime(base::Time t) {
  request_time_ = t;

  // This should only get called in the case of a request occuring
  // during server push that has already begun but hasn't finished,
  // so we set the response's request time to be the actual one
  if (response_)
    response_->request_time = request_time_;
}

int SpdyStream::DoOnResponseReceived(const HttpResponseInfo& response) {
  int rv = OK;

  metrics_.StartStream();

  CHECK(!response_->headers);

  *response_ = response;  // TODO(mbelshe): avoid copy.
  DCHECK(response_->headers);

  recv_first_byte_time_ = base::TimeTicks::Now();

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

  return rv;
}

bool SpdyStream::DoOnDataReceived(const char* data, int length) {
  DCHECK_GE(length, 0);
  LOG(INFO) << "SpdyStream: Data (" << length << " bytes) received for "
            << stream_id_;

  CHECK(!response_complete_);

  // If we don't have a response, then the SYN_REPLY did not come through.
  // We cannot pass data up to the caller unless the reply headers have been
  // received.
  if (!response_->headers) {
    OnClose(ERR_SYN_REPLY_NOT_RECEIVED);
    return false;
  }

  // A zero-length read means that the stream is being closed.
  if (!length) {
    metrics_.StopStream();
    OnClose(net::OK);
    UpdateHistograms();
    return true;
  }

  // Track our bandwidth.
  metrics_.RecordBytes(length);
  recv_bytes_ += length;
  recv_last_byte_time_ = base::TimeTicks::Now();

  return true;
}

void SpdyStream::DoOnWriteComplete(int status) {
  // TODO(mbelshe): Check for cancellation here.  If we're cancelled, we
  // should discontinue the DoLoop.

  // It is possible that this stream was closed while we had a write pending.
  if (response_complete_)
    return;

  if (status > 0)
    send_bytes_ += status;

  DoLoop(status);
}

void SpdyStream::DoOnClose(int status) {
  response_complete_ = true;
  response_status_ = status;
  stream_id_ = 0;
}

void SpdyStream::DoCancel() {
  cancelled_ = true;
  session_->CancelStream(stream_id_);
}

int SpdyStream::DoSendRequest(UploadDataStream* upload_data,
                              HttpResponseInfo* response) {
  CHECK(!cancelled_);
  CHECK(response);

  // SendRequest can be called in two cases.
  //
  // a) A client initiated request. In this case, response_ should be NULL
  //    to start with.
  // b) A client request which matches a response that the server has already
  //    pushed. In this case, the value of |*push_response_| is copied over to
  //    the new response object |*response|. |push_response_| is cleared
  //    and |*push_response_| is deleted, and |response_| is reset to
  //    |response|.
  if (push_response_.get()) {
    *response = *push_response_;
    push_response_.reset(NULL);
    response_ = NULL;
  }

  DCHECK_EQ(static_cast<HttpResponseInfo*>(NULL), response_);
  response_ = response;

  if (upload_data) {
    if (upload_data->size())
      request_body_stream_.reset(upload_data);
    else
      delete upload_data;
  }

  send_time_ = base::TimeTicks::Now();

  DCHECK_EQ(io_state_, STATE_NONE);
  if (!pushed_)
    io_state_ = STATE_SEND_HEADERS;
  else {
    if (response_->headers) {
      io_state_ = STATE_READ_BODY;
    } else {
      io_state_ = STATE_READ_HEADERS;
    }
  }
  return DoLoop(OK);
}

int SpdyStream::DoReadResponseHeaders() {
  CHECK_EQ(STATE_NONE, io_state_);
  CHECK(!cancelled_);

  // The SYN_REPLY has already been received.
  if (response_->headers)
    return OK;

  io_state_ = STATE_READ_HEADERS;
  // Q: do we need to run DoLoop here?
  return ERR_IO_PENDING;
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

      // State machine 2: Read body.
      // NOTE(willchan): Currently unused.  Currently we handle this stuff in
      // the OnDataReceived()/OnClose()/ReadResponseHeaders()/etc.  Only reason
      // to do this is for consistency with the Http code.
      case STATE_READ_BODY:
        net_log_.BeginEvent(NetLog::TYPE_SPDY_STREAM_READ_BODY, NULL);
        result = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        net_log_.EndEvent(NetLog::TYPE_SPDY_STREAM_READ_BODY, NULL);
        result = DoReadBodyComplete(result);
        break;
      case STATE_DONE:
        DCHECK(result != ERR_IO_PENDING);
        break;
      default:
        NOTREACHED();
        break;
    }
  } while (result != ERR_IO_PENDING && io_state_ != STATE_NONE);

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

  // There is no body, skip that state.
  if (!request_body_stream_.get()) {
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
  int buf_len = static_cast<int>(request_body_stream_->buf_len());
  if (!buf_len)
    return OK;
  return session_->WriteStreamData(stream_id_,
                                   request_body_stream_->buf(),
                                   buf_len);
}

int SpdyStream::DoSendBodyComplete(int result) {
  if (result < 0)
    return result;

  CHECK_NE(result, 0);

  request_body_stream_->DidConsume(result);

  if (!request_body_stream_->eof())
    io_state_ = STATE_SEND_BODY;
  else
    io_state_ = STATE_READ_HEADERS;

  return OK;
}

int SpdyStream::DoReadHeaders() {
  io_state_ = STATE_READ_HEADERS_COMPLETE;
  return response_->headers ? OK : ERR_IO_PENDING;
}

int SpdyStream::DoReadHeadersComplete(int result) {
  return result;
}

int SpdyStream::DoReadBody() {
  // TODO(mbelshe): merge SpdyStreamParser with SpdyStream and then this
  // makes sense.
  if (response_complete_) {
    io_state_ = STATE_READ_BODY_COMPLETE;
    return OK;
  }
  return ERR_IO_PENDING;
}

int SpdyStream::DoReadBodyComplete(int result) {
  // TODO(mbelshe): merge SpdyStreamParser with SpdyStream and then this
  // makes sense.
  return OK;
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
