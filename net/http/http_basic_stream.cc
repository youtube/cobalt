// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_stream.h"

namespace net {

HttpBasicStream::HttpBasicStream(ClientSocketHandle* handle)
    : read_buf_(new GrowableIOBuffer()),
      parser_(new HttpStreamParser(handle, read_buf_)) {
}

int HttpBasicStream::SendRequest(const HttpRequestInfo* request,
                                 const std::string& headers,
                                 UploadDataStream* request_body,
                                 CompletionCallback* callback) {
  return parser_->SendRequest(request, headers, request_body, callback);
}

uint64 HttpBasicStream::GetUploadProgress() const {
  return parser_->GetUploadProgress();
}

int HttpBasicStream::ReadResponseHeaders(CompletionCallback* callback) {
  return parser_->ReadResponseHeaders(callback);
}

HttpResponseInfo* HttpBasicStream::GetResponseInfo() const {
  return parser_->GetResponseInfo();
}

int HttpBasicStream::ReadResponseBody(IOBuffer* buf, int buf_len,
                                      CompletionCallback* callback) {
  return parser_->ReadResponseBody(buf, buf_len, callback);
}

bool HttpBasicStream::IsResponseBodyComplete() const {
  return parser_->IsResponseBodyComplete();
}

bool HttpBasicStream::CanFindEndOfResponse() const {
  return parser_->CanFindEndOfResponse();
}

bool HttpBasicStream::IsMoreDataBuffered() const {
  return parser_->IsMoreDataBuffered();
}

}  // namespace net
