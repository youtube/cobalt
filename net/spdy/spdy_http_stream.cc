// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"

#include <algorithm>
#include <list>
#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/spdy/spdy_session.h"

namespace {

// Convert a SpdyHeaderBlock into an HttpResponseInfo.
// |headers| input parameter with the SpdyHeaderBlock.
// |info| output parameter for the HttpResponseInfo.
// Returns true if successfully converted.  False if there was a failure
// or if the SpdyHeaderBlock was invalid.
bool SpdyHeadersToHttpResponse(const spdy::SpdyHeaderBlock& headers,
                               net::HttpResponseInfo* response) {
  std::string version;
  std::string status;

  // The "status" and "version" headers are required.
  spdy::SpdyHeaderBlock::const_iterator it;
  it = headers.find("status");
  if (it == headers.end()) {
    LOG(ERROR) << "SpdyHeaderBlock without status header.";
    return false;
  }
  status = it->second;

  // Grab the version.  If not provided by the server,
  it = headers.find("version");
  if (it == headers.end()) {
    LOG(ERROR) << "SpdyHeaderBlock without version header.";
    return false;
  }
  version = it->second;

  response->response_time = base::Time::Now();

  std::string raw_headers(version);
  raw_headers.push_back(' ');
  raw_headers.append(status);
  raw_headers.push_back('\0');
  for (it = headers.begin(); it != headers.end(); ++it) {
    // For each value, if the server sends a NUL-separated
    // list of values, we separate that back out into
    // individual headers for each value in the list.
    // e.g.
    //    Set-Cookie "foo\0bar"
    // becomes
    //    Set-Cookie: foo\0
    //    Set-Cookie: bar\0
    std::string value = it->second;
    size_t start = 0;
    size_t end = 0;
    do {
      end = value.find('\0', start);
      std::string tval;
      if (end != value.npos)
        tval = value.substr(start, (end - start));
      else
        tval = value.substr(start);
      raw_headers.append(it->first);
      raw_headers.push_back(':');
      raw_headers.append(tval);
      raw_headers.push_back('\0');
      start = end + 1;
    } while (end != value.npos);
  }

  response->headers = new net::HttpResponseHeaders(raw_headers);
  response->was_fetched_via_spdy = true;
  return true;
}

// Create a SpdyHeaderBlock for a Spdy SYN_STREAM Frame from
// a HttpRequestInfo block.
void CreateSpdyHeadersFromHttpRequest(
    const net::HttpRequestInfo& info, spdy::SpdyHeaderBlock* headers) {
  // TODO(willchan): It's not really necessary to convert from
  // HttpRequestHeaders to spdy::SpdyHeaderBlock.

  static const char kHttpProtocolVersion[] = "HTTP/1.1";

  net::HttpRequestHeaders::Iterator it(info.extra_headers);

  while (it.GetNext()) {
    std::string name = StringToLowerASCII(it.name());
    if (headers->find(name) == headers->end()) {
      (*headers)[name] = it.value();
    } else {
      std::string new_value = (*headers)[name];
      new_value.append(1, '\0');  // +=() doesn't append 0's
      new_value += it.value();
      (*headers)[name] = new_value;
    }
  }

  // TODO(mbelshe): Add Proxy headers here. (See http_network_transaction.cc)
  // TODO(mbelshe): Add authentication headers here.

  (*headers)["method"] = info.method;
  (*headers)["url"] = info.url.spec();
  (*headers)["version"] = kHttpProtocolVersion;
  if (!info.referrer.is_empty())
    (*headers)["referer"] = info.referrer.spec();

  // Honor load flags that impact proxy caches.
  if (info.load_flags & net::LOAD_BYPASS_CACHE) {
    (*headers)["pragma"] = "no-cache";
    (*headers)["cache-control"] = "no-cache";
  } else if (info.load_flags & net::LOAD_VALIDATE_CACHE) {
    (*headers)["cache-control"] = "max-age=0";
  }
}

}  // anonymous namespace

namespace net {

SpdyHttpStream::SpdyHttpStream()
    : ALLOW_THIS_IN_INITIALIZER_LIST(read_callback_factory_(this)),
      stream_(NULL),
      spdy_session_(NULL),
      response_info_(NULL),
      download_finished_(false),
      user_callback_(NULL),
      user_buffer_len_(0),
      buffered_read_callback_pending_(false),
      more_read_data_pending_(false) { }

SpdyHttpStream::~SpdyHttpStream() {
  if (stream_)
    stream_->DetachDelegate();
}

int SpdyHttpStream::InitializeStream(
    SpdySession* spdy_session,
    const HttpRequestInfo& request_info,
    const BoundNetLog& stream_net_log,
    CompletionCallback* callback) {
  spdy_session_ = spdy_session;
  request_info_ = request_info;
  if (request_info_.method == "GET") {
    int error = spdy_session_->GetPushStream(request_info.url, &stream_,
                                             stream_net_log);
    if (error != OK)
      return error;
  }

  if (stream_.get())
    return OK;
  else
    return spdy_session_->CreateStream(request_info_.url,
                                       request_info_.priority, &stream_,
                                       stream_net_log, callback);
}

void SpdyHttpStream::InitializeRequest(
    base::Time request_time,
    UploadDataStream* upload_data) {
  CHECK(stream_.get());
  stream_->SetDelegate(this);
  linked_ptr<spdy::SpdyHeaderBlock> headers(new spdy::SpdyHeaderBlock);
  CreateSpdyHeadersFromHttpRequest(request_info_, headers.get());
  stream_->set_spdy_headers(headers);

  stream_->SetRequestTime(request_time);
  // This should only get called in the case of a request occuring
  // during server push that has already begun but hasn't finished,
  // so we set the response's request time to be the actual one
  if (response_info_)
    response_info_->request_time = request_time;

  CHECK(!request_body_stream_.get());
  if (upload_data) {
    if (upload_data->size())
      request_body_stream_.reset(upload_data);
    else
      delete upload_data;
  }
}

const HttpResponseInfo* SpdyHttpStream::GetResponseInfo() const {
  return response_info_;
}

uint64 SpdyHttpStream::GetUploadProgress() const {
  if (!request_body_stream_.get())
    return 0;

  return request_body_stream_->position();
}

int SpdyHttpStream::ReadResponseHeaders(CompletionCallback* callback) {
  DCHECK(stream_->is_idle());
  // Note: The SpdyStream may have already received the response headers, so
  //       this call may complete synchronously.
  CHECK(callback);

  if (stream_->response_complete())
    return stream_->response_status();

  int result = stream_->DoReadResponseHeaders();
  if (result == ERR_IO_PENDING) {
    CHECK(!user_callback_);
    user_callback_ = callback;
  }
  return result;
}

int SpdyHttpStream::ReadResponseBody(
    IOBuffer* buf, int buf_len, CompletionCallback* callback) {
  CHECK(buf);
  CHECK(buf_len);
  CHECK(callback);
  DCHECK(stream_->is_idle());

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
  } else if (stream_->response_complete()) {
    return stream_->response_status();
  }

  CHECK(!user_callback_);
  CHECK(!user_buffer_);
  CHECK_EQ(0, user_buffer_len_);

  user_callback_ = callback;
  user_buffer_ = buf;
  user_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

int SpdyHttpStream::SendRequest(HttpResponseInfo* response,
                                CompletionCallback* callback) {
  CHECK(callback);
  CHECK(!stream_->cancelled());
  CHECK(response);

  if (stream_->response_complete()) {
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
  //    pushed.  In this case, the value of |*push_response_info_| is copied
  //    over to the new response object |*response|.  |push_response_info_| is
  //    deleted, and |response_info_| is reset |response|.
  if (push_response_info_.get()) {
    *response = *push_response_info_;
    push_response_info_.reset();
    response_info_ = NULL;
  }

  DCHECK_EQ(static_cast<HttpResponseInfo*>(NULL), response_info_);
  response_info_ = response;

  bool has_upload_data = request_body_stream_.get() != NULL;
  int result = stream_->DoSendRequest(has_upload_data);
  if (result == ERR_IO_PENDING) {
    CHECK(!user_callback_);
    user_callback_ = callback;
  }
  return result;
}

void SpdyHttpStream::Cancel() {
  if (spdy_session_)
    spdy_session_->CancelPendingCreateStreams(&stream_);
  user_callback_ = NULL;
  if (stream_)
    stream_->Cancel();
}

bool SpdyHttpStream::OnSendHeadersComplete(int status) {
  return request_body_stream_.get() == NULL;
}

int SpdyHttpStream::OnSendBody() {
  CHECK(request_body_stream_.get());
  int buf_len = static_cast<int>(request_body_stream_->buf_len());
  if (!buf_len)
    return OK;
  return stream_->WriteStreamData(request_body_stream_->buf(), buf_len,
                                  spdy::DATA_FLAG_FIN);
}

bool SpdyHttpStream::OnSendBodyComplete(int status) {
  CHECK(request_body_stream_.get());
  request_body_stream_->DidConsume(status);
  return request_body_stream_->eof();
}

int SpdyHttpStream::OnResponseReceived(const spdy::SpdyHeaderBlock& response,
                                       base::Time response_time,
                                       int status) {
  if (!response_info_) {
    DCHECK(stream_->pushed());
    push_response_info_.reset(new HttpResponseInfo);
    response_info_ = push_response_info_.get();
  }

  if (!SpdyHeadersToHttpResponse(response, response_info_)) {
    status = ERR_INVALID_RESPONSE;
  } else {
    stream_->GetSSLInfo(&response_info_->ssl_info,
                        &response_info_->was_npn_negotiated);
    response_info_->request_time = stream_->GetRequestTime();
    response_info_->vary_data.Init(request_info_, *response_info_->headers);
    // TODO(ahendrickson): This is recorded after the entire SYN_STREAM control
    // frame has been received and processed.  Move to framer?
    response_info_->response_time = response_time;
  }

  if (user_callback_)
    DoCallback(status);
  return status;
}

void SpdyHttpStream::OnDataReceived(const char* data, int length) {
  // Note that data may be received for a SpdyStream prior to the user calling
  // ReadResponseBody(), therefore user_buffer_ may be NULL.  This may often
  // happen for server initiated streams.
  if (length > 0 && !stream_->response_complete()) {
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
}

void SpdyHttpStream::OnDataSent(int length) {
  // For HTTP streams, no data is sent from the client while in the OPEN state,
  // so it is never called.
  NOTREACHED();
}

void SpdyHttpStream::OnClose(int status) {
  bool invoked_callback = false;
  if (status == net::OK) {
    // We need to complete any pending buffered read now.
    invoked_callback = DoBufferedReadCallback();
  }
  if (!invoked_callback && user_callback_)
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
  MessageLoop::current()->PostDelayedTask(FROM_HERE, read_callback_factory_.
      NewRunnableMethod(&SpdyHttpStream::DoBufferedReadCallback),
      kBufferTimeMs);
}

// Checks to see if we should wait for more buffered data before notifying
// the caller.  Returns true if we should wait, false otherwise.
bool SpdyHttpStream::ShouldWaitForMoreBufferedData() const {
  // If the response is complete, there is no point in waiting.
  if (stream_->response_complete())
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
  read_callback_factory_.RevokeAll();
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
    rv = ReadResponseBody(user_buffer_, user_buffer_len_, user_callback_);
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
  CHECK(user_callback_);

  // Since Run may result in being called back, clear user_callback_ in advance.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

}  // namespace net
