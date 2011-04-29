// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpStream is an interface for reading and writing data to an HttpStream that
// keeps the client agnostic of the actual underlying transport layer.  This
// provides an abstraction for both a basic http stream as well as http
// pipelining implementations.  The HttpStream subtype is expected to manage the
// underlying transport appropriately.  For example, a non-pipelined HttpStream
// would return the transport socket to the pool for reuse.  SPDY streams on the
// other hand leave the transport socket management to the SpdySession.

#ifndef NET_HTTP_HTTP_STREAM_H_
#define NET_HTTP_HTTP_STREAM_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "net/base/completion_callback.h"

namespace net {

class BoundNetLog;
class HttpRequestHeaders;
struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
class SSLCertRequestInfo;
class SSLInfo;
class UploadDataStream;

class HttpStream {
 public:
  HttpStream() {}
  virtual ~HttpStream() {}

  // Initialize stream.  Must be called before calling SendRequest().
  // Returns a net error code, possibly ERR_IO_PENDING.
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               const BoundNetLog& net_log,
                               CompletionCallback* callback) = 0;

  // Writes the headers and uploads body data to the underlying socket.
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the result will be passed to the callback
  // when available. Returns OK on success. The HttpStream takes ownership
  // of the request_body.
  virtual int SendRequest(const HttpRequestHeaders& request_headers,
                          UploadDataStream* request_body,
                          HttpResponseInfo* response,
                          CompletionCallback* callback) = 0;

  // Queries the UploadDataStream for its progress (bytes sent).
  virtual uint64 GetUploadProgress() const = 0;

  // Reads from the underlying socket until the response headers have been
  // completely received. ERR_IO_PENDING is returned if the operation could
  // not be completed synchronously, in which case the result will be passed
  // to the callback when available. Returns OK on success.  The response
  // headers are available in the HttpResponseInfo returned by GetResponseInfo
  virtual int ReadResponseHeaders(CompletionCallback* callback) = 0;

  // Provides access to HttpResponseInfo (owned by HttpStream).
  virtual const HttpResponseInfo* GetResponseInfo() const = 0;

  // Reads response body data, up to |buf_len| bytes. |buf_len| should be a
  // reasonable size (<2MB). The number of bytes read is returned, or an
  // error is returned upon failure.  0 indicates that the request has been
  // fully satisfied and there is no more data to read.
  // ERR_CONNECTION_CLOSED is returned when the connection has been closed
  // prematurely.  ERR_IO_PENDING is returned if the operation could not be
  // completed synchronously, in which case the result will be passed to the
  // callback when available. If the operation is not completed immediately,
  // the socket acquires a reference to the provided buffer until the callback
  // is invoked or the socket is destroyed.
  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               CompletionCallback* callback) = 0;

  // Closes the stream.
  // |not_reusable| indicates if the stream can be used for further requests.
  // In the case of HTTP, where we re-use the byte-stream (e.g. the connection)
  // this means we need to close the connection; in the case of SPDY, where the
  // underlying stream is never reused, it has no effect.
  // TODO(mbelshe): We should figure out how to fold the not_reusable flag
  //                into the stream implementation itself so that the caller
  //                does not need to pass it at all.  We might also be able to
  //                eliminate the SetConnectionReused() below.
  virtual void Close(bool not_reusable) = 0;

  // Returns a new (not initialized) stream using the same underlying
  // connection and invalidates the old stream - no further methods should be
  // called on the old stream.  The caller should ensure that the response body
  // from the previous request is drained before calling this method.  If the
  // subclass does not support renewing the stream, NULL is returned.
  virtual HttpStream* RenewStreamForAuth() = 0;

  // Indicates if the response body has been completely read.
  virtual bool IsResponseBodyComplete() const = 0;

  // Indicates that the end of the response is detectable. This means that
  // the response headers indicate either chunked encoding or content length.
  // If neither is sent, the server must close the connection for us to detect
  // the end of the response.
  virtual bool CanFindEndOfResponse() const = 0;

  // After the response headers have been read and after the response body
  // is complete, this function indicates if more data (either erroneous or
  // as part of the next pipelined response) has been read from the socket.
  virtual bool IsMoreDataBuffered() const = 0;

  // A stream exists on top of a connection.  If the connection has been used
  // to successfully exchange data in the past, error handling for the
  // stream is done differently.  This method returns true if the underlying
  // connection is reused or has been connected and idle for some time.
  virtual bool IsConnectionReused() const = 0;
  virtual void SetConnectionReused() = 0;

  // Get the SSLInfo associated with this stream's connection.  This should
  // only be called for streams over SSL sockets, otherwise the behavior is
  // undefined.
  virtual void GetSSLInfo(SSLInfo* ssl_info) = 0;

  // Get the SSLCertRequestInfo associated with this stream's connection.
  // This should only be called for streams over SSL sockets, otherwise the
  // behavior is undefined.
  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) = 0;

  // HACK(willchan): Really, we should move the HttpResponseDrainer logic into
  // the HttpStream implementation. This is just a quick hack.
  virtual bool IsSpdyHttpStream() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_H_
