// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_websocket_stream.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/io_buffer.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"

namespace net {

SpdyWebSocketStream::SpdyWebSocketStream(
    SpdySession* spdy_session, Delegate* delegate)
    : stream_(NULL),
      spdy_session_(spdy_session),
      delegate_(delegate) {
  DCHECK(spdy_session_);
  DCHECK(delegate_);
}

SpdyWebSocketStream::~SpdyWebSocketStream() {
  if (stream_) {
    // If Close() has not already been called, DetachDelegate() will send a
    // SPDY RST_STREAM. Deleting SpdyWebSocketStream is good enough to initiate
    // graceful shutdown, so we call Close() to avoid sending a RST_STREAM.
    // For safe, we should eliminate |delegate_| for OnClose() calback.
    delegate_ = NULL;
    stream_->Close();
  }
}

int SpdyWebSocketStream::InitializeStream(const GURL& url,
                                          RequestPriority request_priority,
                                          const BoundNetLog& net_log) {
  if (spdy_session_->IsClosed())
    return ERR_SOCKET_NOT_CONNECTED;

  int result = spdy_session_->CreateStream(
      url, request_priority, &stream_, net_log,
      base::Bind(&SpdyWebSocketStream::OnSpdyStreamCreated,
                 base::Unretained(this)));

  if (result == OK) {
    DCHECK(stream_);
    stream_->SetDelegate(this);
  }
  return result;
}

int SpdyWebSocketStream::SendRequest(
    const linked_ptr<SpdyHeaderBlock>& headers) {
  if (!stream_) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  stream_->set_spdy_headers(headers);
  int result = stream_->SendRequest(true);
  if (result < OK && result != ERR_IO_PENDING)
    Close();
  return result;
}

int SpdyWebSocketStream::SendData(const char* data, int length) {
  if (!stream_) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  scoped_refptr<IOBuffer> buf(new IOBuffer(length));
  memcpy(buf->data(), data, length);
  return stream_->WriteStreamData(buf.get(), length, DATA_FLAG_NONE);
}

void SpdyWebSocketStream::Close() {
  if (spdy_session_)
    spdy_session_->CancelPendingCreateStreams(&stream_);
  if (stream_)
    stream_->Close();
}

bool SpdyWebSocketStream::OnSendHeadersComplete(int status) {
  DCHECK(delegate_);
  delegate_->OnSentSpdyHeaders(status);
  return true;
}

int SpdyWebSocketStream::OnSendBody() {
  NOTREACHED();
  return ERR_UNEXPECTED;
}

int SpdyWebSocketStream::OnSendBodyComplete(int status, bool* eof) {
  NOTREACHED();
  *eof = true;
  return ERR_UNEXPECTED;
}

int SpdyWebSocketStream::OnResponseReceived(
    const SpdyHeaderBlock& response,
    base::Time response_time, int status) {
  DCHECK(delegate_);
  return delegate_->OnReceivedSpdyResponseHeader(response, status);
}

void SpdyWebSocketStream::OnDataReceived(const char* data, int length) {
  DCHECK(delegate_);
  delegate_->OnReceivedSpdyData(data, length);
}

void SpdyWebSocketStream::OnDataSent(int length) {
  DCHECK(delegate_);
  delegate_->OnSentSpdyData(length);
}

void SpdyWebSocketStream::OnClose(int status) {
  stream_ = NULL;

  // Destruction without Close() call OnClose() with delegate_ being NULL.
  if (!delegate_)
    return;
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  delegate->OnCloseSpdyStream();
}

void SpdyWebSocketStream::set_chunk_callback(ChunkCallback* callback) {
  // Do nothing. SpdyWebSocketStream doesn't send any chunked data.
}

void SpdyWebSocketStream::OnSpdyStreamCreated(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  if (result == OK) {
    DCHECK(stream_);
    stream_->SetDelegate(this);
  }
  DCHECK(delegate_);
  delegate_->OnCreatedSpdyStream(result);
}

}  // namespace net
