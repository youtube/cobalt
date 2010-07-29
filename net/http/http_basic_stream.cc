// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_stream.h"

namespace net {

HttpBasicStream::HttpBasicStream(ClientSocketHandle* connection)
    : read_buf_(new GrowableIOBuffer()),
      connection_(connection) {
}

int HttpBasicStream::InitializeStream(const HttpRequestInfo* request_info,
                                      const BoundNetLog& net_log,
                                      CompletionCallback* callback) {
  parser_.reset(new HttpStreamParser(connection_, request_info,
                                     read_buf_, net_log));
  connection_ = NULL;
  return OK;
}


int HttpBasicStream::SendRequest(const std::string& headers,
                                 UploadDataStream* request_body,
                                 HttpResponseInfo* response,
                                 CompletionCallback* callback) {
  DCHECK(parser_.get());
  return parser_->SendRequest(headers, request_body, response, callback);
}

HttpBasicStream::~HttpBasicStream() {}

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
