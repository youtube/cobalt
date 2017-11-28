// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_HTTP_STREAM_H_
#define NET_QUIC_QUIC_HTTP_STREAM_H_

#include <list>

#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"
#include "net/http/http_stream.h"
#include "net/quic/quic_reliable_client_stream.h"

namespace net {

// The QuicHttpStream is a QUIC-specific HttpStream subclass.  It holds a
// non-owning pointer to a QuicReliableClientStream which it uses to
// send and receive data.
class NET_EXPORT_PRIVATE QuicHttpStream :
      public QuicReliableClientStream::Delegate,
      public HttpStream {
 public:
  explicit QuicHttpStream(QuicReliableClientStream* stream);

  virtual ~QuicHttpStream();

  // HttpStream implementation.
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) override;
  virtual int SendRequest(const HttpRequestHeaders& request_headers,
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

  // QuicReliableClientStream::Delegate implementation
  virtual int OnSendData() override;
  virtual int OnSendDataComplete(int status, bool* eof) override;
  virtual int OnDataReceived(const char* data, int length) override;
  virtual void OnClose(QuicErrorCode error) override;

 private:
  enum State {
    STATE_NONE,
    STATE_SEND_HEADERS,
    STATE_SEND_HEADERS_COMPLETE,
    STATE_READ_REQUEST_BODY,
    STATE_READ_REQUEST_BODY_COMPLETE,
    STATE_SEND_BODY,
    STATE_SEND_BODY_COMPLETE,
    STATE_OPEN,
  };

  void OnIOComplete(int rv);
  void DoCallback(int rv);

  int DoLoop(int);
  int DoSendHeaders();
  int DoSendHeadersComplete(int rv);
  int DoReadRequestBody();
  int DoReadRequestBodyComplete(int rv);
  int DoSendBody();
  int DoSendBodyComplete(int rv);
  int DoReadResponseHeaders();
  int DoReadResponseHeadersComplete(int rv);

  int ParseResponseHeaders();

  void BufferResponseBody(const char* data, int length);

  State io_state_;

  QuicReliableClientStream* stream_;  // Non-owning.

  // The following three fields are all owned by the caller and must
  // outlive this object, according to the HttpStream contract.

  // The request to send.
  const HttpRequestInfo* request_info_;
  // The request body to send, if any, owned by the caller.
  UploadDataStream* request_body_stream_;
  // |response_info_| is the HTTP response data object which is filled in
  // when a the response headers are read.  It is not owned by this stream.
  HttpResponseInfo* response_info_;

  bool response_headers_received_;

  // Serialized HTTP request.
  std::string request_;

  // Buffer into which response header data is read.
  scoped_refptr<GrowableIOBuffer> read_buf_;

  // We buffer the response body as it arrives asynchronously from the stream.
  // TODO(rch): This is infinite buffering, which is bad.
  std::list<scoped_refptr<IOBufferWithSize> > response_body_;

  // The caller's callback to be used for asynchronous operations.
  CompletionCallback callback_;

  // Caller provided buffer for the ReadResponseBody() response.
  scoped_refptr<IOBuffer> user_buffer_;
  int user_buffer_len_;

  // Temporary buffer used to read the request body from UploadDataStream.
  scoped_refptr<IOBufferWithSize> raw_request_body_buf_;
  // Wraps raw_request_body_buf_ to read the remaining data progressively.
  scoped_refptr<DrainableIOBuffer> request_body_buf_;

  base::WeakPtrFactory<QuicHttpStream> weak_factory_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_HTTP_STREAM_H_
