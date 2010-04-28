// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/spdy/spdy_session.h"

namespace net {

SpdyStream::SpdyStream(SpdySession* session, spdy::SpdyStreamId stream_id,
                       bool pushed, const BoundNetLog& log)
    : stream_id_(stream_id),
      priority_(0),
      pushed_(pushed),
      download_finished_(false),
      metrics_(Singleton<BandwidthMetrics>::get()),
      session_(session),
      request_time_(base::Time::Now()),
      response_(NULL),
      request_body_stream_(NULL),
      response_complete_(false),
      io_state_(STATE_NONE),
      response_status_(OK),
      user_callback_(NULL),
      user_buffer_(NULL),
      user_buffer_len_(0),
      cancelled_(false),
      net_log_(log),
      send_bytes_(0),
      recv_bytes_(0),
      histograms_recorded_(false),
      buffered_read_callback_pending_(false),
      more_read_data_pending_(false) {}

SpdyStream::~SpdyStream() {
  DLOG(INFO) << "Deleting SpdyStream for stream " << stream_id_;

  // TODO(willchan): We're still calling CancelStream() too many times, because
  // inactive pending/pushed streams will still have stream_id_ set.
  if (stream_id_) {
    session_->CancelStream(stream_id_);
  } else if (!response_complete_) {
    NOTREACHED();
  }
}

uint64 SpdyStream::GetUploadProgress() const {
  if (!request_body_stream_.get())
    return 0;

  return request_body_stream_->position();
}

const HttpResponseInfo* SpdyStream::GetResponseInfo() const {
  return response_;
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

int SpdyStream::ReadResponseHeaders(CompletionCallback* callback) {
  // Note: The SpdyStream may have already received the response headers, so
  //       this call may complete synchronously.
  CHECK(callback);
  CHECK_EQ(STATE_NONE, io_state_);
  CHECK(!cancelled_);

  // The SYN_REPLY has already been received.
  if (response_->headers)
    return OK;

  io_state_ = STATE_READ_HEADERS;
  CHECK(!user_callback_);
  user_callback_ = callback;
  return ERR_IO_PENDING;
}

int SpdyStream::ReadResponseBody(
    IOBuffer* buf, int buf_len, CompletionCallback* callback) {
  DCHECK_EQ(io_state_, STATE_NONE);
  CHECK(buf);
  CHECK(buf_len);
  CHECK(callback);
  CHECK(!cancelled_);

  // If we have data buffered, complete the IO immediately.
  if (response_body_.size()) {
    int bytes_read = 0;
    while (response_body_.size() && buf_len > 0) {
      scoped_refptr<IOBufferWithSize> data = response_body_.front();
      const int bytes_to_copy = std::min(buf_len, data->size());
      memcpy(&(buf->data()[bytes_read]), data->data(), bytes_to_copy);
      buf_len -= bytes_to_copy;
      if (bytes_to_copy == data->size()) {
        response_body_.pop_front();
      } else {
        const int bytes_remaining = data->size() - bytes_to_copy;
        IOBufferWithSize* new_buffer = new IOBufferWithSize(bytes_remaining);
        memcpy(new_buffer->data(), &(data->data()[bytes_to_copy]),
               bytes_remaining);
        response_body_.pop_front();
        response_body_.push_front(new_buffer);
      }
      bytes_read += bytes_to_copy;
    }
    if (bytes_read > 0)
      recv_bytes_ += bytes_read;
    return bytes_read;
  } else if (response_complete_) {
    return response_status_;
  }

  CHECK(!user_callback_);
  CHECK(!user_buffer_);
  CHECK_EQ(0, user_buffer_len_);

  user_callback_ = callback;
  user_buffer_ = buf;
  user_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

int SpdyStream::SendRequest(UploadDataStream* upload_data,
                            HttpResponseInfo* response,
                            CompletionCallback* callback) {
  CHECK(callback);
  CHECK(!cancelled_);
  CHECK(response);

  if (response_) {
    *response = *response_;
    delete response_;
  }
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
  int result = DoLoop(OK);
  if (result == ERR_IO_PENDING) {
    CHECK(!user_callback_);
    user_callback_ = callback;
  }
  return result;
}

void SpdyStream::Cancel() {
  cancelled_ = true;
  user_callback_ = NULL;

  session_->CancelStream(stream_id_);
}

void SpdyStream::OnResponseReceived(const HttpResponseInfo& response) {
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
    NOTREACHED();
  }

  int rv = DoLoop(OK);

  if (user_callback_)
    DoCallback(rv);
}

bool SpdyStream::OnDataReceived(const char* data, int length) {
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
    download_finished_ = true;
    response_complete_ = true;

    // We need to complete any pending buffered read now.
    DoBufferedReadCallback();

    OnClose(net::OK);
    return true;
  }

  // Track our bandwidth.
  metrics_.RecordBytes(length);
  recv_bytes_ += length;
  recv_last_byte_time_ = base::TimeTicks::Now();

  // Save the received data.
  IOBufferWithSize* io_buffer = new IOBufferWithSize(length);
  memcpy(io_buffer->data(), data, length);
  response_body_.push_back(io_buffer);

  // Note that data may be received for a SpdyStream prior to the user calling
  // ReadResponseBody(), therefore user_buffer_ may be NULL.  This may often
  // happen for server initiated streams.
  if (user_buffer_) {
    // Handing small chunks of data to the caller creates measurable overhead.
    // We buffer data in short time-spans and send a single read notification.
    ScheduleBufferedReadCallback();
  }

  return true;
}

void SpdyStream::OnWriteComplete(int status) {
  // TODO(mbelshe): Check for cancellation here.  If we're cancelled, we
  // should discontinue the DoLoop.

  if (status > 0)
    send_bytes_ += status;

  DoLoop(status);
}

void SpdyStream::OnClose(int status) {
  response_complete_ = true;
  response_status_ = status;
  stream_id_ = 0;

  if (user_callback_)
    DoCallback(status);

  UpdateHistograms();
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

void SpdyStream::ScheduleBufferedReadCallback() {
  // If there is already a scheduled DoBufferedReadCallback, don't issue
  // another one.  Mark that we have received more data and return.
  if (buffered_read_callback_pending_) {
    more_read_data_pending_ = true;
    return;
  }

  more_read_data_pending_ = false;
  buffered_read_callback_pending_ = true;
  const int kBufferTimeMs = 1;
  MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(
    this, &SpdyStream::DoBufferedReadCallback), kBufferTimeMs);
}

// Checks to see if we should wait for more buffered data before notifying
// the caller.  Returns true if we should wait, false otherwise.
bool SpdyStream::ShouldWaitForMoreBufferedData() const {
  // If the response is complete, there is no point in waiting.
  if (response_complete_)
    return false;

  int bytes_buffered = 0;
  std::list<scoped_refptr<IOBufferWithSize> >::const_iterator it;
  for (it = response_body_.begin();
       it != response_body_.end() && bytes_buffered < user_buffer_len_;
       ++it)
    bytes_buffered += (*it)->size();

  return bytes_buffered < user_buffer_len_;
}

void SpdyStream::DoBufferedReadCallback() {
  buffered_read_callback_pending_ = false;

  // If the transaction is cancelled or errored out, we don't need to complete
  // the read.
  if (response_status_ != OK || cancelled_)
    return;

  // When more_read_data_pending_ is true, it means that more data has
  // arrived since we started waiting.  Wait a little longer and continue
  // to buffer.
  if (more_read_data_pending_ && ShouldWaitForMoreBufferedData()) {
    ScheduleBufferedReadCallback();
    return;
  }

  int rv = 0;
  if (user_buffer_) {
    rv = ReadResponseBody(user_buffer_, user_buffer_len_, user_callback_);
    CHECK_NE(rv, ERR_IO_PENDING);
    user_buffer_ = NULL;
    user_buffer_len_ = 0;
    DoCallback(rv);
  }
}

void SpdyStream::DoCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(user_callback_);

  // Since Run may result in being called back, clear user_callback_ in advance.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
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
  return ERR_IO_PENDING;
}

int SpdyStream::DoReadBodyComplete(int result) {
  // TODO(mbelshe): merge SpdyStreamParser with SpdyStream and then this
  // makes sense.
  return ERR_IO_PENDING;
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
