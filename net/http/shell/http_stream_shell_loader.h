// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STRAM_SHELL_LOADER_H_
#define NET_HTTP_HTTP_STRAM_SHELL_LOADER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_info.h"

namespace net {

class HttpStreamShellLoader;
class ProxyInfo;
// Here are the declaration of three functions that need to be called
// from external classes. These function needs to be implemented on
// different platforms to create the right type of loader (derived from
// HttpStreamShellLoader) used in application.
NET_EXPORT_PRIVATE void HttpStreamShellLoaderGlobalInit();
NET_EXPORT_PRIVATE void HttpStreamShellLoaderGlobalDeinit();
NET_EXPORT_PRIVATE HttpStreamShellLoader* CreateHttpStreamShellLoader();

// Represents a single HTTP transaction (i.e., a single request/response pair).
// HTTP redirects are not followed and authentication challenges are not
// answered.  Cookies are assumed to be managed by the caller.
class NET_EXPORT_PRIVATE HttpStreamShellLoader
    : public base::RefCountedThreadSafe<HttpStreamShellLoader> {
 public:
  static const size_t kChunkHeaderFooterSize = 12;

  HttpStreamShellLoader() {}
  virtual ~HttpStreamShellLoader() {}

  virtual int Open(const HttpRequestInfo* info, const BoundNetLog& net_log) = 0;

  virtual int SendRequest(const std::string& request_line,
                          const HttpRequestHeaders& headers,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) = 0;

  virtual int ReadResponseHeaders(const CompletionCallback& callback) = 0;

  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) = 0;

  virtual void Close(bool not_reusable) = 0;

  virtual bool IsResponseBodyComplete() const = 0;

  // Set proxy information
  virtual void SetProxy(const ProxyInfo* info) = 0;

  static std::string GetResponseHeaderLines(
      const net::HttpResponseHeaders& headers);

 protected:
  // Helper functions used by sub classes.
  // Return true if |headers| contain multiple |field_name| fields with
  // different values.
  static bool HeadersContainMultipleCopiesOfField(
      const net::HttpResponseHeaders& headers,
      const std::string& field_name);
  // Parser response headers.
  int ParseResponseHeaders(const char* buf, int buf_len,
      const HttpRequestInfo*, HttpResponseInfo* response);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STRAM_SHELL_LOADER_H_
