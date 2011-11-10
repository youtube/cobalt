// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_connection_impl.h"

#include "base/message_loop.h"
#include "base/stl_util.h"
#include "net/base/io_buffer.h"
#include "net/http/http_pipelined_stream.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream_parser.h"
#include "net/socket/client_socket_handle.h"

namespace net {

HttpPipelinedConnectionImpl::HttpPipelinedConnectionImpl(
    ClientSocketHandle* connection,
    HttpPipelinedConnection::Delegate* delegate,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated)
    : delegate_(delegate),
      connection_(connection),
      used_ssl_config_(used_ssl_config),
      used_proxy_info_(used_proxy_info),
      net_log_(net_log),
      was_npn_negotiated_(was_npn_negotiated),
      read_buf_(new GrowableIOBuffer()),
      next_pipeline_id_(1),
      active_(false),
      usable_(true),
      completed_one_request_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      send_next_state_(SEND_STATE_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(send_io_callback_(
          this, &HttpPipelinedConnectionImpl::OnSendIOCallback)),
      send_user_callback_(NULL),
      read_next_state_(READ_STATE_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(read_io_callback_(
          this, &HttpPipelinedConnectionImpl::OnReadIOCallback)),
      read_user_callback_(NULL) {
  CHECK(connection_.get());
}

HttpPipelinedConnectionImpl::~HttpPipelinedConnectionImpl() {
  CHECK_EQ(depth(), 0);
  CHECK(stream_info_map_.empty());
  CHECK(deferred_request_queue_.empty());
  CHECK(request_order_.empty());
  CHECK_EQ(send_next_state_, SEND_STATE_NONE);
  CHECK_EQ(read_next_state_, READ_STATE_NONE);
  CHECK(!send_user_callback_);
  CHECK(!read_user_callback_);
  if (!usable_) {
    connection_->socket()->Disconnect();
  }
  connection_->Reset();
}

HttpPipelinedStream* HttpPipelinedConnectionImpl::CreateNewStream() {
  int pipeline_id = next_pipeline_id_++;
  CHECK(pipeline_id);
  HttpPipelinedStream* stream = new HttpPipelinedStream(this, pipeline_id);
  stream_info_map_.insert(std::make_pair(pipeline_id, StreamInfo()));
  return stream;
}

void HttpPipelinedConnectionImpl::InitializeParser(
    int pipeline_id,
    const HttpRequestInfo* request,
    const BoundNetLog& net_log) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(!stream_info_map_[pipeline_id].parser.get());
  stream_info_map_[pipeline_id].state = STREAM_BOUND;
  stream_info_map_[pipeline_id].parser.reset(new HttpStreamParser(
      connection_.get(), request, read_buf_.get(), net_log));

  // In case our first stream doesn't SendRequest() immediately, we should still
  // allow others to use this pipeline.
  if (pipeline_id == 1) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &HttpPipelinedConnectionImpl::ActivatePipeline));
  }
}

void HttpPipelinedConnectionImpl::ActivatePipeline() {
  if (!active_) {
    active_ = true;
    delegate_->OnPipelineHasCapacity(this);
  }
}

void HttpPipelinedConnectionImpl::OnStreamDeleted(int pipeline_id) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  Close(pipeline_id, false);

  if (stream_info_map_[pipeline_id].state != STREAM_CREATED &&
      stream_info_map_[pipeline_id].state != STREAM_UNUSED) {
    CHECK_EQ(stream_info_map_[pipeline_id].state, STREAM_CLOSED);
    CHECK(stream_info_map_[pipeline_id].parser.get());
    stream_info_map_[pipeline_id].parser.reset();
  }
  CHECK(!stream_info_map_[pipeline_id].parser.get());
  CHECK(!stream_info_map_[pipeline_id].read_headers_callback);
  stream_info_map_.erase(pipeline_id);

  delegate_->OnPipelineHasCapacity(this);
}

int HttpPipelinedConnectionImpl::SendRequest(int pipeline_id,
                                             const std::string& request_line,
                                             const HttpRequestHeaders& headers,
                                             UploadDataStream* request_body,
                                             HttpResponseInfo* response,
                                             OldCompletionCallback* callback) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK_EQ(stream_info_map_[pipeline_id].state, STREAM_BOUND);
  if (!usable_) {
    return ERR_PIPELINE_EVICTION;
  }

  DeferredSendRequest deferred_request;
  deferred_request.pipeline_id = pipeline_id;
  deferred_request.request_line = request_line;
  deferred_request.headers = headers;
  deferred_request.request_body = request_body;
  deferred_request.response = response;
  deferred_request.callback = callback;
  deferred_request_queue_.push(deferred_request);

  int rv;
  if (send_next_state_ == SEND_STATE_NONE) {
    send_next_state_ = SEND_STATE_NEXT_REQUEST;
    rv = DoSendRequestLoop(OK);
  } else {
    rv = ERR_IO_PENDING;
  }
  ActivatePipeline();
  return rv;
}

int HttpPipelinedConnectionImpl::DoSendRequestLoop(int result) {
  int rv = result;
  do {
    SendRequestState state = send_next_state_;
    send_next_state_ = SEND_STATE_NONE;
    switch (state) {
      case SEND_STATE_NEXT_REQUEST:
        rv = DoSendNextRequest(rv);
        break;
      case SEND_STATE_COMPLETE:
        rv = DoSendComplete(rv);
        break;
      case SEND_STATE_UNUSABLE:
        rv = DoEvictPendingSendRequests(rv);
        break;
      default:
        NOTREACHED() << "bad send state: " << state;
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && send_next_state_ != SEND_STATE_NONE);
  return rv;
}

void HttpPipelinedConnectionImpl::OnSendIOCallback(int result) {
  CHECK(send_user_callback_);
  DoSendRequestLoop(result);
}

int HttpPipelinedConnectionImpl::DoSendNextRequest(int result) {
  CHECK(!deferred_request_queue_.empty());
  const DeferredSendRequest& deferred_request = deferred_request_queue_.front();
  CHECK(ContainsKey(stream_info_map_, deferred_request.pipeline_id));
  if (stream_info_map_[deferred_request.pipeline_id].state == STREAM_CLOSED) {
    deferred_request_queue_.pop();
    if (deferred_request_queue_.empty()) {
      send_next_state_ = SEND_STATE_NONE;
    } else {
      send_next_state_ = SEND_STATE_NEXT_REQUEST;
    }
    return OK;
  }
  CHECK(stream_info_map_[deferred_request.pipeline_id].parser.get());
  int rv = stream_info_map_[deferred_request.pipeline_id].parser->SendRequest(
      deferred_request.request_line,
      deferred_request.headers,
      deferred_request.request_body,
      deferred_request.response,
      &send_io_callback_);
  // |result| == ERR_IO_PENDING means this function was *not* called on the same
  // stack as SendRequest(). That means we returned ERR_IO_PENDING to
  // SendRequest() earlier and will need to invoke its callback.
  if (result == ERR_IO_PENDING || rv == ERR_IO_PENDING) {
    send_user_callback_ = deferred_request.callback;
  }
  stream_info_map_[deferred_request.pipeline_id].state = STREAM_SENDING;
  send_next_state_ = SEND_STATE_COMPLETE;
  return rv;
}

int HttpPipelinedConnectionImpl::DoSendComplete(int result) {
  CHECK(!deferred_request_queue_.empty());
  const DeferredSendRequest& deferred_request = deferred_request_queue_.front();
  CHECK_EQ(stream_info_map_[deferred_request.pipeline_id].state,
           STREAM_SENDING);
  request_order_.push(deferred_request.pipeline_id);
  stream_info_map_[deferred_request.pipeline_id].state = STREAM_SENT;
  deferred_request_queue_.pop();
  if (result == ERR_SOCKET_NOT_CONNECTED && completed_one_request_) {
    result = ERR_PIPELINE_EVICTION;
  }
  if (result < OK) {
    send_next_state_ = SEND_STATE_UNUSABLE;
    usable_ = false;
  }
  if (send_user_callback_) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &HttpPipelinedConnectionImpl::FireUserCallback,
            deferred_request.pipeline_id,
            result));
    stream_info_map_[deferred_request.pipeline_id].pending_user_callback =
        send_user_callback_;
    send_user_callback_ = NULL;
  }
  if (result < OK) {
    return result;
  }
  if (deferred_request_queue_.empty()) {
    send_next_state_ = SEND_STATE_NONE;
    return OK;
  }
  send_next_state_ = SEND_STATE_NEXT_REQUEST;
  return OK;
}

int HttpPipelinedConnectionImpl::DoEvictPendingSendRequests(int result) {
  send_next_state_ = SEND_STATE_NONE;
  while (!deferred_request_queue_.empty()) {
    const DeferredSendRequest& evicted_send = deferred_request_queue_.front();
    if (stream_info_map_[evicted_send.pipeline_id].state != STREAM_CLOSED) {
      evicted_send.callback->Run(ERR_PIPELINE_EVICTION);
    }
    deferred_request_queue_.pop();
  }
  return result;
}

int HttpPipelinedConnectionImpl::ReadResponseHeaders(
    int pipeline_id,
    OldCompletionCallback* callback) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK_EQ(stream_info_map_[pipeline_id].state, STREAM_SENT);
  CHECK(!stream_info_map_[pipeline_id].read_headers_callback);
  if (!usable_) {
    return ERR_PIPELINE_EVICTION;
  }
  stream_info_map_[pipeline_id].state = STREAM_READ_PENDING;
  stream_info_map_[pipeline_id].read_headers_callback = callback;
  if (read_next_state_ == READ_STATE_NONE) {
    read_next_state_ = READ_STATE_NEXT_HEADERS;
    return DoReadHeadersLoop(OK);
  } else {
    return ERR_IO_PENDING;
  }
}

int HttpPipelinedConnectionImpl::DoReadHeadersLoop(int result) {
  int rv = result;
  do {
    ReadHeadersState state = read_next_state_;
    read_next_state_ = READ_STATE_NONE;
    switch (state) {
      case READ_STATE_NEXT_HEADERS:
        rv = DoReadNextHeaders(rv);
        break;
      case READ_STATE_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        break;
      case READ_STATE_WAITING_FOR_CLOSE:
        rv = DoReadWaitingForClose(rv);
        return rv;
      case READ_STATE_STREAM_CLOSED:
        rv = DoReadStreamClosed();
        break;
      case READ_STATE_UNUSABLE:
        rv = DoEvictPendingReadHeaders(rv);
        break;
      case READ_STATE_NONE:
        break;
      default:
        NOTREACHED() << "bad read state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && read_next_state_ != READ_STATE_NONE);
  return rv;
}

void HttpPipelinedConnectionImpl::OnReadIOCallback(int result) {
  CHECK(read_user_callback_);
  DoReadHeadersLoop(result);
}

int HttpPipelinedConnectionImpl::DoReadNextHeaders(int result) {
  CHECK(!request_order_.empty());
  int pipeline_id = request_order_.front();
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  if (stream_info_map_[pipeline_id].state == STREAM_CLOSED) {
    // Since nobody will read whatever data is on the pipeline associated with
    // this closed request, we must shut down the rest of the pipeline.
    read_next_state_ = READ_STATE_UNUSABLE;
    return OK;
  }
  if (stream_info_map_[pipeline_id].read_headers_callback == NULL) {
    return ERR_IO_PENDING;
  }
  CHECK(stream_info_map_[pipeline_id].parser.get());

  if (result == ERR_IO_PENDING) {
    CHECK_EQ(stream_info_map_[pipeline_id].state, STREAM_ACTIVE);
  } else {
    CHECK_EQ(stream_info_map_[pipeline_id].state, STREAM_READ_PENDING);
    stream_info_map_[pipeline_id].state = STREAM_ACTIVE;
  }

  int rv = stream_info_map_[pipeline_id].parser->ReadResponseHeaders(
      &read_io_callback_);
  if (rv == ERR_IO_PENDING) {
    read_next_state_ = READ_STATE_COMPLETE;
    read_user_callback_ = stream_info_map_[pipeline_id].read_headers_callback;
  } else if (rv < OK) {
    read_next_state_ = READ_STATE_WAITING_FOR_CLOSE;
    if (rv == ERR_SOCKET_NOT_CONNECTED && completed_one_request_)
      rv = ERR_PIPELINE_EVICTION;
  } else {
    CHECK_LE(OK, rv);
    read_next_state_ = READ_STATE_WAITING_FOR_CLOSE;
  }

  // |result| == ERR_IO_PENDING means this function was *not* called on the same
  // stack as ReadResponseHeaders(). That means we returned ERR_IO_PENDING to
  // ReadResponseHeaders() earlier and now need to invoke its callback.
  if (rv != ERR_IO_PENDING && result == ERR_IO_PENDING) {
    read_next_state_ = READ_STATE_WAITING_FOR_CLOSE;
    read_user_callback_ = stream_info_map_[pipeline_id].read_headers_callback;
    stream_info_map_[pipeline_id].pending_user_callback = read_user_callback_;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &HttpPipelinedConnectionImpl::FireUserCallback,
            pipeline_id,
            rv));
  }
  return rv;
}

int HttpPipelinedConnectionImpl::DoReadHeadersComplete(int result) {
  read_next_state_ = READ_STATE_WAITING_FOR_CLOSE;
  if (read_user_callback_) {
    int pipeline_id = request_order_.front();
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &HttpPipelinedConnectionImpl::FireUserCallback,
            pipeline_id,
            result));
    stream_info_map_[pipeline_id].pending_user_callback = read_user_callback_;
    read_user_callback_ = NULL;
  }
  return result;
}

int HttpPipelinedConnectionImpl::DoReadWaitingForClose(int result) {
  read_next_state_ = READ_STATE_WAITING_FOR_CLOSE;
  return result;
}

int HttpPipelinedConnectionImpl::DoReadStreamClosed() {
  CHECK(!request_order_.empty());
  int pipeline_id = request_order_.front();
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK_EQ(stream_info_map_[pipeline_id].state, STREAM_CLOSED);
  CHECK(stream_info_map_[pipeline_id].read_headers_callback);
  stream_info_map_[pipeline_id].read_headers_callback = NULL;
  request_order_.pop();
  if (!usable_) {
    read_next_state_ = READ_STATE_UNUSABLE;
    return OK;
  } else {
    completed_one_request_ = true;
    if (!request_order_.empty()) {
      int next_pipeline_id = request_order_.front();
      CHECK(ContainsKey(stream_info_map_, next_pipeline_id));
      if (stream_info_map_[next_pipeline_id].read_headers_callback) {
        stream_info_map_[next_pipeline_id].state = STREAM_ACTIVE;
        read_next_state_ = READ_STATE_NEXT_HEADERS;
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &HttpPipelinedConnectionImpl::DoReadHeadersLoop,
                ERR_IO_PENDING));
        return ERR_IO_PENDING;  // Wait for the task to fire.
      }
    }
    read_next_state_ = READ_STATE_NONE;
    return OK;
  }
}

int HttpPipelinedConnectionImpl::DoEvictPendingReadHeaders(int result) {
  while (!request_order_.empty()) {
    int evicted_id = request_order_.front();
    request_order_.pop();
    if (!ContainsKey(stream_info_map_, evicted_id) ||
        (stream_info_map_[evicted_id].read_headers_callback == NULL)) {
      continue;
    }
    if (stream_info_map_[evicted_id].state != STREAM_CLOSED) {
      stream_info_map_[evicted_id].pending_user_callback =
          stream_info_map_[evicted_id].read_headers_callback;
      MessageLoop::current()->PostTask(
          FROM_HERE,
          method_factory_.NewRunnableMethod(
              &HttpPipelinedConnectionImpl::FireUserCallback,
              evicted_id,
              ERR_PIPELINE_EVICTION));
    }
    stream_info_map_[evicted_id].read_headers_callback = NULL;
  }
  read_next_state_ = READ_STATE_NONE;
  return result;
}

void HttpPipelinedConnectionImpl::Close(int pipeline_id,
                                        bool not_reusable) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  switch (stream_info_map_[pipeline_id].state) {
    case STREAM_CREATED:
      stream_info_map_[pipeline_id].state = STREAM_UNUSED;
      break;

    case STREAM_BOUND:
      stream_info_map_[pipeline_id].state = STREAM_CLOSED;
      break;

    case STREAM_SENDING:
      usable_ = false;
      stream_info_map_[pipeline_id].state = STREAM_CLOSED;
      send_user_callback_ = NULL;
      send_next_state_ = SEND_STATE_UNUSABLE;
      DoSendRequestLoop(OK);
      break;

    case STREAM_SENT:
    case STREAM_READ_PENDING:
      usable_ = false;
      stream_info_map_[pipeline_id].state = STREAM_CLOSED;
      stream_info_map_[pipeline_id].read_headers_callback = NULL;
      if (read_next_state_ == READ_STATE_NONE) {
        read_next_state_ = READ_STATE_UNUSABLE;
        DoReadHeadersLoop(OK);
      }
      break;

    case STREAM_ACTIVE:
      stream_info_map_[pipeline_id].state = STREAM_CLOSED;
      if (not_reusable) {
        usable_ = false;
      }
      read_next_state_ = READ_STATE_STREAM_CLOSED;
      read_user_callback_ = NULL;
      DoReadHeadersLoop(OK);
      break;

    case STREAM_CLOSED:
    case STREAM_UNUSED:
      // TODO(simonjam): Why is Close() sometimes called twice?
      break;

    default:
      NOTREACHED();
      break;
  }
}

int HttpPipelinedConnectionImpl::ReadResponseBody(
    int pipeline_id,
    IOBuffer* buf,
    int buf_len,
    OldCompletionCallback* callback) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(!request_order_.empty());
  CHECK_EQ(pipeline_id, request_order_.front());
  CHECK(stream_info_map_[pipeline_id].parser.get());
  return stream_info_map_[pipeline_id].parser->ReadResponseBody(
      buf, buf_len, callback);
}

uint64 HttpPipelinedConnectionImpl::GetUploadProgress(int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_.find(pipeline_id)->second.parser.get());
  return stream_info_map_.find(pipeline_id)->second.parser->GetUploadProgress();
}

HttpResponseInfo* HttpPipelinedConnectionImpl::GetResponseInfo(
    int pipeline_id) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_.find(pipeline_id)->second.parser.get());
  return stream_info_map_.find(pipeline_id)->second.parser->GetResponseInfo();
}

bool HttpPipelinedConnectionImpl::IsResponseBodyComplete(
    int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_.find(pipeline_id)->second.parser.get());
  return stream_info_map_.find(pipeline_id)->second.parser->
      IsResponseBodyComplete();
}

bool HttpPipelinedConnectionImpl::CanFindEndOfResponse(int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_.find(pipeline_id)->second.parser.get());
  return stream_info_map_.find(pipeline_id)->second.parser->
      CanFindEndOfResponse();
}

bool HttpPipelinedConnectionImpl::IsMoreDataBuffered(int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  return read_buf_->offset() != 0;
}

bool HttpPipelinedConnectionImpl::IsConnectionReused(int pipeline_id) const {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  if (pipeline_id > 1) {
    return true;
  }
  ClientSocketHandle::SocketReuseType reuse_type = connection_->reuse_type();
  return connection_->is_reused() ||
         reuse_type == ClientSocketHandle::UNUSED_IDLE;
}

void HttpPipelinedConnectionImpl::SetConnectionReused(int pipeline_id) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  connection_->set_is_reused(true);
}

void HttpPipelinedConnectionImpl::GetSSLInfo(int pipeline_id,
                                             SSLInfo* ssl_info) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_[pipeline_id].parser.get());
  return stream_info_map_[pipeline_id].parser->GetSSLInfo(ssl_info);
}

void HttpPipelinedConnectionImpl::GetSSLCertRequestInfo(
    int pipeline_id,
    SSLCertRequestInfo* cert_request_info) {
  CHECK(ContainsKey(stream_info_map_, pipeline_id));
  CHECK(stream_info_map_[pipeline_id].parser.get());
  return stream_info_map_[pipeline_id].parser->GetSSLCertRequestInfo(
      cert_request_info);
}

void HttpPipelinedConnectionImpl::FireUserCallback(int pipeline_id,
                                                   int result) {
  if (ContainsKey(stream_info_map_, pipeline_id)) {
    stream_info_map_[pipeline_id].pending_user_callback->Run(result);
  }
}

int HttpPipelinedConnectionImpl::depth() const {
  return stream_info_map_.size();
}

bool HttpPipelinedConnectionImpl::usable() const {
  return usable_;
}

bool HttpPipelinedConnectionImpl::active() const {
  return active_;
}

const SSLConfig& HttpPipelinedConnectionImpl::used_ssl_config() const {
  return used_ssl_config_;
}

const ProxyInfo& HttpPipelinedConnectionImpl::used_proxy_info() const {
  return used_proxy_info_;
}

const NetLog::Source& HttpPipelinedConnectionImpl::source() const {
  return net_log_.source();
}

bool HttpPipelinedConnectionImpl::was_npn_negotiated() const {
  return was_npn_negotiated_;
}

HttpPipelinedConnectionImpl::DeferredSendRequest::DeferredSendRequest() {
}

HttpPipelinedConnectionImpl::DeferredSendRequest::~DeferredSendRequest() {
}

HttpPipelinedConnectionImpl::StreamInfo::StreamInfo()
    : read_headers_callback(NULL),
      state(STREAM_CREATED) {
}

HttpPipelinedConnectionImpl::StreamInfo::~StreamInfo() {
}

}  // namespace net
