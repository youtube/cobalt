// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"

#include <list>

#include "base/logging.h"
#include "base/message_loop.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/spdy/spdy_session.h"

namespace net {

SpdyHttpStream::SpdyHttpStream(
    SpdySession* session, spdy::SpdyStreamId stream_id, bool pushed)
    : SpdyStream(session, stream_id, pushed),
      download_finished_(false),
      user_callback_(NULL),
      user_buffer_len_(0),
      buffered_read_callback_pending_(false),
      more_read_data_pending_(false) {}

SpdyHttpStream::~SpdyHttpStream() {
  DLOG(INFO) << "Deleting SpdyHttpStream for stream " << stream_id();
}

uint64 SpdyHttpStream::GetUploadProgress() const {
  if (!request_body_stream())
    return 0;

  return request_body_stream()->position();
}

int SpdyHttpStream::ReadResponseHeaders(CompletionCallback* callback) {
  DCHECK(is_idle());
  // Note: The SpdyStream may have already received the response headers, so
  //       this call may complete synchronously.
  CHECK(callback);

  int result = DoReadResponseHeaders();
  if (result == ERR_IO_PENDING) {
    CHECK(!user_callback_);
    user_callback_ = callback;
  }
  return result;
}

int SpdyHttpStream::ReadResponseBody(
    IOBuffer* buf, int buf_len, CompletionCallback* callback) {
  DCHECK(is_idle());
  CHECK(buf);
  CHECK(buf_len);
  CHECK(callback);
  CHECK(!cancelled());

  // If we have data buffered, complete the IO immediately.
  if (!response_body_.empty()) {
    int bytes_read = 0;
    while (!response_body_.empty() && buf_len > 0) {
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
    return bytes_read;
  } else if (response_complete()) {
    return response_status();
  }

  CHECK(!user_callback_);
  CHECK(!user_buffer_);
  CHECK_EQ(0, user_buffer_len_);

  user_callback_ = callback;
  user_buffer_ = buf;
  user_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

int SpdyHttpStream::SendRequest(UploadDataStream* upload_data,
                                HttpResponseInfo* response,
                                CompletionCallback* callback) {
  CHECK(callback);
  CHECK(!cancelled());
  CHECK(response);

  int result = DoSendRequest(upload_data, response);
  if (result == ERR_IO_PENDING) {
    CHECK(!user_callback_);
    user_callback_ = callback;
  }
  return result;
}

void SpdyHttpStream::Cancel() {
  user_callback_ = NULL;
  DoCancel();
}

int SpdyHttpStream::OnResponseReceived(const HttpResponseInfo& response) {
  int rv = DoOnResponseReceived(response);
  if (user_callback_)
    DoCallback(rv);
  return rv;
}

bool SpdyHttpStream::OnDataReceived(const char* data, int length) {
  bool result = DoOnDataReceived(data, length);
  // Note that data may be received for a SpdyStream prior to the user calling
  // ReadResponseBody(), therefore user_buffer_ may be NULL.  This may often
  // happen for server initiated streams.
  if (result && !response_complete()) {
    // Save the received data.
    IOBufferWithSize* io_buffer = new IOBufferWithSize(length);
    memcpy(io_buffer->data(), data, length);
    response_body_.push_back(io_buffer);

    if (user_buffer_) {
      // Handing small chunks of data to the caller creates measurable overhead.
      // We buffer data in short time-spans and send a single read notification.
      ScheduleBufferedReadCallback();
    }
  }
  return result;
}

void SpdyHttpStream::OnWriteComplete(int status) {
  DoOnWriteComplete(status);
}

void SpdyHttpStream::OnClose(int status) {
  if (status == net::OK) {
    download_finished_ = true;
    set_response_complete(true);

    // We need to complete any pending buffered read now.
    DoBufferedReadCallback();
  }
  DoOnClose(status);
  if (user_callback_)
    DoCallback(status);
}

void SpdyHttpStream::ScheduleBufferedReadCallback() {
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
    this, &SpdyHttpStream::DoBufferedReadCallback), kBufferTimeMs);
}

// Checks to see if we should wait for more buffered data before notifying
// the caller.  Returns true if we should wait, false otherwise.
bool SpdyHttpStream::ShouldWaitForMoreBufferedData() const {
  // If the response is complete, there is no point in waiting.
  if (response_complete())
    return false;

  int bytes_buffered = 0;
  std::list<scoped_refptr<IOBufferWithSize> >::const_iterator it;
  for (it = response_body_.begin();
       it != response_body_.end() && bytes_buffered < user_buffer_len_;
       ++it)
    bytes_buffered += (*it)->size();

  return bytes_buffered < user_buffer_len_;
}

void SpdyHttpStream::DoBufferedReadCallback() {
  buffered_read_callback_pending_ = false;

  // If the transaction is cancelled or errored out, we don't need to complete
  // the read.
  if (response_status() != OK || cancelled())
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

void SpdyHttpStream::DoCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(user_callback_);

  // Since Run may result in being called back, clear user_callback_ in advance.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

}  // namespace net
