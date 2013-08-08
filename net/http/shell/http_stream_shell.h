// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_SHELL_H_
#define NET_HTTP_HTTP_STREAM_SHELL_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/shell/http_stream_shell_loader.h"

// This is a special HttpStream implementation designed to work with
// HttpStreamShellLoader and different HTTP libraries. Most functions
// have the same name as the same function in the original HttpStream
// and very similar functionality. However this class is NOT a sub class
// of HttpStream.

namespace net {

class ProxyInfo;
// Represents a single HTTP transaction (i.e., a single request/response pair).
// HTTP redirects are not followed and authentication challenges are not
// answered.  Cookies are assumed to be managed by the caller.
class NET_EXPORT_PRIVATE HttpStreamShell {
 public:
  explicit HttpStreamShell(HttpStreamShellLoader* loader);
  virtual ~HttpStreamShell();

  // Initialize stream.  Must be called before calling SendRequest().
  // |request_info| must outlive the HttpStream.
  // Returns a net error code, possibly ERR_IO_PENDING.
  int InitializeStream(const HttpRequestInfo* request_info,
                       const BoundNetLog& net_log,
                       const CompletionCallback& callback);

  // Writes the headers and uploads body data to the underlying loader.
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the result will be passed to the callback
  // when available. Returns OK on success.
  // |response| must outlive the HttpStream.
  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  const CompletionCallback& callback);

  // Reads from the underlying loader until the response headers have been
  // completely received. ERR_IO_PENDING is returned if the operation could
  // not be completed synchronously, in which case the result will be passed
  // to the callback when available. Returns OK on success.  The response
  // headers are available in the HttpResponseInfo returned by GetResponseInfo
  int ReadResponseHeaders(const CompletionCallback& callback);


  // Provides access to HttpResponseInfo.
  const HttpResponseInfo* GetResponseInfo() const;

  // Reads response body data, up to |buf_len| bytes. |buf_len| should be a
  // reasonable size (<2MB). The number of bytes read is returned, or an
  // error is returned upon failure.  0 indicates that the request has been
  // fully satisfied and there is no more data to read.
  // ERR_CONNECTION_CLOSED is returned when the connection has been closed
  // prematurely.  ERR_IO_PENDING is returned if the operation could not be
  // completed synchronously, in which case the result will be passed to the
  // callback when available. If the operation is not completed immediately,
  // the loader acquires a reference to the provided buffer until the callback
  // is invoked or the loader is destroyed.
  int ReadResponseBody(IOBuffer* buf, int buf_len,
                       const CompletionCallback& callback);

  // Closes the stream.
  // |not_reusable| indicates if the stream can be used for further requests.
  // In the case of HTTP, where we re-use the byte-stream (e.g. the connection)
  // this means we need to close the connection.
  void Close(bool not_reusable);

  // Indicates if the response body has been completely read.
  bool IsResponseBodyComplete() const;

  // Set proxy information
  void SetProxy(const ProxyInfo* info);

  // Get underlying stream loader.
  HttpStreamShellLoader* GetStreamLoader() const { return loader_.get(); }

 protected:
  scoped_refptr<HttpStreamShellLoader> loader_;

  std::string request_line_;
  const HttpRequestInfo* request_info_;
  const HttpResponseInfo* response_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_SHELL_H_
