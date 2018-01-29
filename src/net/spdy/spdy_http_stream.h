// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_HTTP_STREAM_H_
#define NET_SPDY_SPDY_HTTP_STREAM_H_

#include <list>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/http/http_stream.h"
#include "net/spdy/spdy_stream.h"

namespace net {

class DrainableIOBuffer;
struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
class SpdySession;
class UploadDataStream;

// The SpdyHttpStream is a HTTP-specific type of stream known to a SpdySession.
class NET_EXPORT_PRIVATE SpdyHttpStream : public SpdyStream::Delegate,
                                          public HttpStream {
 public:
  SpdyHttpStream(SpdySession* spdy_session, bool direct);
  virtual ~SpdyHttpStream();

  // Initializes this SpdyHttpStream by wrapping an existing SpdyStream.
  void InitializeWithExistingStream(SpdyStream* spdy_stream);

  SpdyStream* stream() { return stream_.get(); }

  // Cancels any callbacks from being invoked and deletes the stream.
  void Cancel();

  // HttpStream implementation.
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) override;
  virtual int SendRequest(const HttpRequestHeaders& headers,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) override;
  virtual UploadProgress GetUploadProgress() const override;
  virtual int ReadResponseHeaders(const CompletionCallback& callback) override;
  virtual const HttpResponseInfo* GetResponseInfo() const override;
  virtual int ReadResponseBody(IOBuffer* buf,
                               int buf_len,
                               const CompletionCallback& callback) override;
  virtual void Close(bool not_reusable) override;
  virtual HttpStream* RenewStreamForAuth() override;
  virtual bool IsResponseBodyComplete() const override;
  virtual bool CanFindEndOfResponse() const override;
  virtual bool IsMoreDataBuffered() const override;
  virtual bool IsConnectionReused() const override;
  virtual void SetConnectionReused() override;
  virtual bool IsConnectionReusable() const override;
  virtual void GetSSLInfo(SSLInfo* ssl_info) override;
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) override;
  virtual bool IsSpdyHttpStream() const override;
  virtual void LogNumRttVsBytesMetrics() const override {}
  virtual void Drain(HttpNetworkSession* session) override;

  // SpdyStream::Delegate implementation.
  virtual bool OnSendHeadersComplete(int status) override;
  virtual int OnSendBody() override;
  virtual int OnSendBodyComplete(int status, bool* eof) override;
  virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) override;
  virtual void OnHeadersSent() override;
  virtual int OnDataReceived(const char* buffer, int bytes) override;
  virtual void OnDataSent(int length) override;
  virtual void OnClose(int status) override;

 private:
  // Reads the data (whether chunked or not) from the request body stream and
  // sends the data by calling WriteStreamData on the underlying SpdyStream.
  int SendData();

  // Call the user callback.
  void DoCallback(int rv);

  int OnRequestBodyReadCompleted(int status);

  void ScheduleBufferedReadCallback();

  // Returns true if the callback is invoked.
  bool DoBufferedReadCallback();
  bool ShouldWaitForMoreBufferedData() const;

  base::WeakPtrFactory<SpdyHttpStream> weak_factory_;
  scoped_refptr<SpdyStream> stream_;
  scoped_refptr<SpdySession> spdy_session_;

  // The request to send.
  const HttpRequestInfo* request_info_;

  bool has_upload_data_;

  // |response_info_| is the HTTP response data object which is filled in
  // when a SYN_REPLY comes in for the stream.
  // It is not owned by this stream object, or point to |push_response_info_|.
  HttpResponseInfo* response_info_;

  scoped_ptr<HttpResponseInfo> push_response_info_;

  bool download_finished_;
  bool response_headers_received_;  // Indicates waiting for more HEADERS.

  // We buffer the response body as it arrives asynchronously from the stream.
  // TODO(mbelshe):  is this infinite buffering?
  std::list<scoped_refptr<IOBufferWithSize> > response_body_;

  CompletionCallback callback_;

  // User provided buffer for the ReadResponseBody() response.
  scoped_refptr<IOBuffer> user_buffer_;
  int user_buffer_len_;

  // Temporary buffer used to read the request body from UploadDataStream.
  scoped_refptr<IOBufferWithSize> raw_request_body_buf_;
  // Wraps raw_request_body_buf_ to read the remaining data progressively.
  scoped_refptr<DrainableIOBuffer> request_body_buf_;

  // Is there a scheduled read callback pending.
  bool buffered_read_callback_pending_;
  // Has more data been received from the network during the wait for the
  // scheduled read callback.
  bool more_read_data_pending_;

  // Is this spdy stream direct to the origin server (or to a proxy).
  bool direct_;

  DISALLOW_COPY_AND_ASSIGN(SpdyHttpStream);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_HTTP_STREAM_H_
