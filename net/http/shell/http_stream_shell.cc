// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/shell/http_stream_shell.h"

#include "base/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/http/shell/http_stream_shell_loader.h"

namespace net {

HttpStreamShell::HttpStreamShell(HttpStreamShellLoader* loader)
    : loader_(loader),
      request_info_(NULL),
      response_(NULL) {
}

HttpStreamShell::~HttpStreamShell() {
  // make sure close is called.
  Close(true);
}

int HttpStreamShell::InitializeStream(const HttpRequestInfo* request_info,
                                      const BoundNetLog& net_log,
                                      const CompletionCallback& callback) {
  // Loader object is received in constructor, not other steps left for
  // this function.
  DCHECK(loader_);
  loader_->Open(request_info, net_log);
  request_info_ = request_info;
  return OK;
}

int HttpStreamShell::SendRequest(const HttpRequestHeaders& request_headers,
                                 HttpResponseInfo* response,
                                 const CompletionCallback& callback) {
  DCHECK(loader_);
  DCHECK(request_info_);
  const std::string path = HttpUtil::PathForRequest(request_info_->url);
  // Force http 1.1
  request_line_ = base::StringPrintf("%s %s HTTP/1.1\r\n",
                                     request_info_->method.c_str(),
                                     path.c_str());
  return loader_->SendRequest(request_line_, request_headers,
                              response, callback);
}

int HttpStreamShell::ReadResponseHeaders(const CompletionCallback& callback) {
  return loader_->ReadResponseHeaders(callback);
}

const HttpResponseInfo* HttpStreamShell::GetResponseInfo() const {
  return response_;
}

int HttpStreamShell::ReadResponseBody(IOBuffer* buf, int buf_len,
                                      const CompletionCallback& callback) {
  return loader_->ReadResponseBody(buf, buf_len, callback);
}

void HttpStreamShell::Close(bool not_reusable) {
  loader_->Close(not_reusable);
}

bool HttpStreamShell::IsResponseBodyComplete() const {
  return loader_->IsResponseBodyComplete();
}

// Set proxy information
void HttpStreamShell::SetProxy(const ProxyInfo* info) {
  loader_->SetProxy(info);
}

}  // namespace net
