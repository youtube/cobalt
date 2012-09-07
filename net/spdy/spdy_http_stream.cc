// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"

#include <algorithm>
#include <list>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"

namespace net {

SpdyHttpStream::SpdyHttpStream(SpdySession* spdy_session,
                               bool direct)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      stream_(NULL),
      spdy_session_(spdy_session),
      response_info_(NULL),
      download_finished_(false),
      response_headers_received_(false),
      user_buffer_len_(0),
      buffered_read_callback_pending_(false),
      more_read_data_pending_(false),
      direct_(direct),
      waiting_for_chunk_(false) { }

void SpdyHttpStream::InitializeWithExistingStream(SpdyStream* spdy_stream) {
  stream_ = spdy_stream;
  stream_->SetDelegate(this);
  response_headers_received_ = true;
}

SpdyHttpStream::~SpdyHttpStream() {
  if (request_body_stream_ != NULL)
    request_body_stream_->set_chunk_callback(NULL);
  if (stream_)
    stream_->DetachDelegate();
}

int SpdyHttpStream::InitializeStream(const HttpRequestInfo* request_info,
                                     const BoundNetLog& stream_net_log,
                                     const CompletionCallback& callback) {
  DCHECK(!stream_.get());
  if (spdy_session_->IsClosed())
   return ERR_CONNECTION_CLOSED;

  request_info_ = request_info;
  if (request_info_->method == "GET") {
    int error = spdy_session_->GetPushStream(request_info_->url, &stream_,
                                             stream_net_log);
    if (error != OK)
      return error;
  }

  if (stream_.get())
    return OK;

  return spdy_session_->CreateStream(request_info_->url,
                                     request_info_->priority, &stream_,
                                     stream_net_log, callback);
}

const HttpResponseInfo* SpdyHttpStream::GetResponseInfo() const {
  return response_info_;
}

UploadProgress SpdyHttpStream::GetUploadProgress() const {
  if (!request_body_stream_.get())
    return UploadProgress();

  return UploadProgress(request_body_stream_->position(),
                        request_body_stream_->size());
}

int SpdyHttpStream::ReadResponseHeaders(const CompletionCallback& callback) {
  CHECK(!callback.is_null());
  CHECK(!stream_->cancelled());

  if (stream_->closed())
    return stream_->response_status();

  // Check if we already have the response headers. If so, return synchronously.
  if(stream_->response_received()) {
    CHECK(stream_->is_idle());
    return OK;
  }

  // Still waiting for the response, return IO_PENDING.
  CHECK(callback_.is_null());
  callback_ = callback;
  return ERR_IO_PENDING;
}

int SpdyHttpStream::ReadResponseBody(
    IOBuffer* buf, int buf_len, const CompletionCallback& callback) {
  CHECK(stream_->is_idle());
  CHECK(buf);
  CHECK(buf_len);
  CHECK(!callback.is_null());

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
        response_body_.push_front(make_scoped_refptr(new_buffer));
      }
      bytes_read += bytes_to_copy;
    }
    stream_->IncreaseRecvWindowSize(bytes_read);
    return bytes_read;
  } else if (stream_->closed()) {
    return stream_->response_status();
  }

  CHECK(callback_.is_null());
  CHECK(!user_buffer_);
  CHECK_EQ(0, user_buffer_len_);

  callback_ = callback;
  user_buffer_ = buf;
  user_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

void SpdyHttpStream::Close(bool not_reusable) {
  // Note: the not_reusable flag has no meaning for SPDY streams.

  Cancel();
}

HttpStream* SpdyHttpStream::RenewStreamForAuth() {
  return NULL;
}

bool SpdyHttpStream::IsResponseBodyComplete() const {
  if (!stream_)
    return false;
  return stream_->closed();
}

bool SpdyHttpStream::CanFindEndOfResponse() const {
  return true;
}

bool SpdyHttpStream::IsMoreDataBuffered() const {
  return false;
}

bool SpdyHttpStream::IsConnectionReused() const {
  return spdy_session_->IsReused();
}

void SpdyHttpStream::SetConnectionReused() {
  // SPDY doesn't need an indicator here.
}

bool SpdyHttpStream::IsConnectionReusable() const {
  // SPDY streams aren't considered reusable.
  return false;
}

int SpdyHttpStream::SendRequest(const HttpRequestHeaders& request_headers,
                                scoped_ptr<UploadDataStream> request_body,
                                HttpResponseInfo* response,
                                const CompletionCallback& callback) {
  base::Time request_time = base::Time::Now();
  CHECK(stream_.get());

  stream_->SetDelegate(this);

  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
  CreateSpdyHeadersFromHttpRequest(*request_info_, request_headers,
                                   headers.get(), stream_->GetProtocolVersion(),
                                   direct_);
  stream_->net_log().AddEvent(
      NetLog::TYPE_HTTP_TRANSACTION_SPDY_SEND_REQUEST_HEADERS,
      base::Bind(&SpdyHeaderBlockNetLogCallback, headers.get()));
  stream_->set_spdy_headers(headers.Pass());

  stream_->SetRequestTime(request_time);
  // This should only get called in the case of a request occurring
  // during server push that has already begun but hasn't finished,
  // so we set the response's request time to be the actual one
  if (response_info_)
    response_info_->request_time = request_time;

  CHECK(!request_body_stream_.get());
  if (request_body != NULL) {
    if (request_body->size() || request_body->is_chunked()) {
      request_body_stream_.reset(request_body.release());
      request_body_stream_->set_chunk_callback(this);
      // Use kMaxSpdyFrameChunkSize as the buffer size, since the request
      // body data is written with this size at a time.
      raw_request_body_buf_ = new IOBufferWithSize(kMaxSpdyFrameChunkSize);
      // The request body buffer is empty at first.
      request_body_buf_ = new DrainableIOBuffer(raw_request_body_buf_, 0);
    }
  }

  CHECK(!callback.is_null());
  CHECK(!stream_->cancelled());
  CHECK(response);

  if (!stream_->pushed() && stream_->closed()) {
    if (stream_->response_status() == OK)
      return ERR_FAILED;
    else
      return stream_->response_status();
  }

  // SendRequest can be called in two cases.
  //
  // a) A client initiated request. In this case, |response_info_| should be
  //    NULL to start with.
  // b) A client request which matches a response that the server has already
  //    pushed.
  if (push_response_info_.get()) {
    *response = *(push_response_info_.get());
    push_response_info_.reset();
  } else {
    DCHECK_EQ(static_cast<HttpResponseInfo*>(NULL), response_info_);
  }

  response_info_ = response;

  // Put the peer's IP address and port into the response.
  IPEndPoint address;
  int result = stream_->GetPeerAddress(&address);
  if (result != OK)
    return result;
  response_info_->socket_address = HostPortPair::FromIPEndPoint(address);

  bool has_upload_data = request_body_stream_.get() != NULL;
  result = stream_->SendRequest(has_upload_data);
  if (result == ERR_IO_PENDING) {
    CHECK(callback_.is_null());
    callback_ = callback;
  }
  return result;
}

void SpdyHttpStream::Cancel() {
  if (spdy_session_)
    spdy_session_->CancelPendingCreateStreams(&stream_);
  callback_.Reset();
  if (stream_)
    stream_->Cancel();
}

int SpdyHttpStream::SendData() {
  CHECK(request_body_stream_.get());
  CHECK_EQ(0, request_body_buf_->BytesRemaining());

  // Read the data from the request body stream.
  const int bytes_read = request_body_stream_->Read(
      raw_request_body_buf_, raw_request_body_buf_->size());
  DCHECK(!waiting_for_chunk_ || bytes_read != ERR_IO_PENDING);

  if (request_body_stream_->is_chunked() && bytes_read == ERR_IO_PENDING) {
    waiting_for_chunk_ = true;
    return ERR_IO_PENDING;
  }

  waiting_for_chunk_ = false;

  // ERR_IO_PENDING with chunked encoding is the only possible error.
  DCHECK_GE(bytes_read, 0);

  request_body_buf_ = new DrainableIOBuffer(raw_request_body_buf_,
                                            bytes_read);

  const bool eof = request_body_stream_->IsEOF();
  return stream_->WriteStreamData(
      request_body_buf_,
      request_body_buf_->BytesRemaining(),
      eof ? DATA_FLAG_FIN : DATA_FLAG_NONE);
}

bool SpdyHttpStream::OnSendHeadersComplete(int status) {
  if (!callback_.is_null())
    DoCallback(status);
  return request_body_stream_.get() == NULL;
}

int SpdyHttpStream::OnSendBody() {
  CHECK(request_body_stream_.get());
  const bool eof = request_body_stream_->IsEOF();
  if (request_body_buf_->BytesRemaining() > 0) {
    return stream_->WriteStreamData(
        request_body_buf_,
        request_body_buf_->BytesRemaining(),
        eof ? DATA_FLAG_FIN : DATA_FLAG_NONE);
  }

  // The entire body data has been sent.
  if (eof)
    return OK;

  return SendData();
}

int SpdyHttpStream::OnSendBodyComplete(int status, bool* eof) {
  // |status| is the number of bytes written to the SPDY stream.
  CHECK(request_body_stream_.get());
  *eof = false;

  if (status > 0) {
    request_body_buf_->DidConsume(status);
    if (request_body_buf_->BytesRemaining()) {
      // Go back to OnSendBody() to send the remaining data.
      return OK;
    }
  }

  // Check if the entire body data has been sent.
  *eof = (request_body_stream_->IsEOF() &&
          !request_body_buf_->BytesRemaining());
  return OK;
}

int SpdyHttpStream::OnResponseReceived(const SpdyHeaderBlock& response,
                                       base::Time response_time,
                                       int status) {
  if (!response_info_) {
    DCHECK(stream_->pushed());
    push_response_info_.reset(new HttpResponseInfo);
    response_info_ = push_response_info_.get();
  }

  // If the response is already received, these headers are too late.
  if (response_headers_received_) {
    LOG(WARNING) << "SpdyHttpStream headers received after response started.";
    return OK;
  }

  // TODO(mbelshe): This is the time of all headers received, not just time
  // to first byte.
  response_info_->response_time = base::Time::Now();

  if (!SpdyHeadersToHttpResponse(response, stream_->GetProtocolVersion(),
                                 response_info_)) {
    // We might not have complete headers yet.
    return ERR_INCOMPLETE_SPDY_HEADERS;
  }

  response_headers_received_ = true;
  // Don't store the SSLInfo in the response here, HttpNetworkTransaction
  // will take care of that part.
  SSLInfo ssl_info;
  NextProto protocol_negotiated = kProtoUnknown;
  stream_->GetSSLInfo(&ssl_info,
                      &response_info_->was_npn_negotiated,
                      &protocol_negotiated);
  response_info_->npn_negotiated_protocol =
      SSLClientSocket::NextProtoToString(protocol_negotiated);
  response_info_->request_time = stream_->GetRequestTime();
  response_info_->vary_data.Init(*request_info_, *response_info_->headers);
  // TODO(ahendrickson): This is recorded after the entire SYN_STREAM control
  // frame has been received and processed.  Move to framer?
  response_info_->response_time = response_time;

  if (!callback_.is_null())
    DoCallback(status);

  return status;
}

void SpdyHttpStream::OnHeadersSent() {
  // For HTTP streams, no HEADERS frame is sent from the client.
  NOTREACHED();
}

int SpdyHttpStream::OnDataReceived(const char* data, int length) {
  // SpdyStream won't call us with data if the header block didn't contain a
  // valid set of headers.  So we don't expect to not have headers received
  // here.
  if (!response_headers_received_)
    return ERR_INCOMPLETE_SPDY_HEADERS;

  // Note that data may be received for a SpdyStream prior to the user calling
  // ReadResponseBody(), therefore user_buffer_ may be NULL.  This may often
  // happen for server initiated streams.
  DCHECK(!stream_->closed() || stream_->pushed());
  if (length > 0) {
    // Save the received data.
    IOBufferWithSize* io_buffer = new IOBufferWithSize(length);
    memcpy(io_buffer->data(), data, length);
    response_body_.push_back(make_scoped_refptr(io_buffer));

    if (user_buffer_) {
      // Handing small chunks of data to the caller creates measurable overhead.
      // We buffer data in short time-spans and send a single read notification.
      ScheduleBufferedReadCallback();
    }
  }
  return OK;
}

void SpdyHttpStream::OnDataSent(int length) {
  // For HTTP streams, no data is sent from the client while in the OPEN state,
  // so it is never called.
  NOTREACHED();
}

void SpdyHttpStream::OnClose(int status) {
  bool invoked_callback = false;
  if (request_body_stream_ != NULL)
    request_body_stream_->set_chunk_callback(NULL);
  if (status == net::OK) {
    // We need to complete any pending buffered read now.
    invoked_callback = DoBufferedReadCallback();
  }
  if (!invoked_callback && !callback_.is_null())
    DoCallback(status);
}

void SpdyHttpStream::OnChunkAvailable() {
  if (!waiting_for_chunk_)
    return;
  DCHECK(request_body_stream_->is_chunked());
  SendData();
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
  const base::TimeDelta kBufferTime = base::TimeDelta::FromMilliseconds(1);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&SpdyHttpStream::DoBufferedReadCallback),
                 weak_factory_.GetWeakPtr()),
      kBufferTime);
}

// Checks to see if we should wait for more buffered data before notifying
// the caller.  Returns true if we should wait, false otherwise.
bool SpdyHttpStream::ShouldWaitForMoreBufferedData() const {
  // If the response is complete, there is no point in waiting.
  if (stream_->closed())
    return false;

  int bytes_buffered = 0;
  std::list<scoped_refptr<IOBufferWithSize> >::const_iterator it;
  for (it = response_body_.begin();
       it != response_body_.end() && bytes_buffered < user_buffer_len_;
       ++it)
    bytes_buffered += (*it)->size();

  return bytes_buffered < user_buffer_len_;
}

bool SpdyHttpStream::DoBufferedReadCallback() {
  weak_factory_.InvalidateWeakPtrs();
  buffered_read_callback_pending_ = false;

  // If the transaction is cancelled or errored out, we don't need to complete
  // the read.
  if (!stream_ || stream_->response_status() != OK || stream_->cancelled())
    return false;

  // When more_read_data_pending_ is true, it means that more data has
  // arrived since we started waiting.  Wait a little longer and continue
  // to buffer.
  if (more_read_data_pending_ && ShouldWaitForMoreBufferedData()) {
    ScheduleBufferedReadCallback();
    return false;
  }

  int rv = 0;
  if (user_buffer_) {
    rv = ReadResponseBody(user_buffer_, user_buffer_len_, callback_);
    CHECK_NE(rv, ERR_IO_PENDING);
    user_buffer_ = NULL;
    user_buffer_len_ = 0;
    DoCallback(rv);
    return true;
  }
  return false;
}

void SpdyHttpStream::DoCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(!callback_.is_null());

  // Since Run may result in being called back, clear user_callback_ in advance.
  CompletionCallback c = callback_;
  callback_.Reset();
  c.Run(rv);
}

void SpdyHttpStream::GetSSLInfo(SSLInfo* ssl_info) {
  DCHECK(stream_);
  bool using_npn;
  NextProto protocol_negotiated = kProtoUnknown;
  stream_->GetSSLInfo(ssl_info, &using_npn, &protocol_negotiated);
}

void SpdyHttpStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  DCHECK(stream_);
  stream_->GetSSLCertRequestInfo(cert_request_info);
}

bool SpdyHttpStream::IsSpdyHttpStream() const {
  return true;
}

void SpdyHttpStream::Drain(HttpNetworkSession* session) {
  Close(false);
  delete this;
}

}  // namespace net
