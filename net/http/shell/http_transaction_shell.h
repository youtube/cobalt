// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_SHELL_H_
#define NET_HTTP_HTTP_TRANSACTION_SHELL_H_

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "net/http/http_network_session.h"  // net::HttpNetworkSession::Params
#include "net/http/http_transaction.h"
#include "net/http/shell/http_stream_shell.h"
#include "net/proxy/proxy_service.h"

namespace net {

// Represents a single HTTP transaction (i.e., a single request/response pair).
// HTTP redirects are not followed and authentication challenges are not
// answered.  Cookies are assumed to be managed by the caller.
class NET_EXPORT_PRIVATE HttpTransactionShell : public HttpTransaction {
  friend class HttpTransactionShellTest;  // Unittests class needs to access
                                          // member variables.
 public:
  HttpTransactionShell(const net::HttpNetworkSession::Params* params,
                       HttpStreamShell* stream);
  virtual ~HttpTransactionShell() {}

  // HttpTransaction methods:

  // Starts the HTTP transaction (i.e., sends the HTTP request).
  virtual int Start(const HttpRequestInfo* request_info,
                    const CompletionCallback& callback,
                    const BoundNetLog& net_log) override;

  // Restarts the HTTP transaction, ignoring the last error.
  virtual int RestartIgnoringLastError(
      const CompletionCallback& callback) override;

  // Restarts the HTTP transaction with a client certificate.
  virtual int RestartWithCertificate(X509Certificate* client_cert,
      const CompletionCallback& callback) override;

  // Restarts the HTTP transaction with authentication credentials.
  virtual int RestartWithAuth(const AuthCredentials& credentials,
                              const CompletionCallback& callback) override;

  // Returns true if auth is ready to be continued.
  virtual bool IsReadyToRestartForAuth() override;

  // Once response info is available for the transaction, response data may be
  // read by calling this method.
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) override;

  // Stops further caching of this request by the HTTP cache, if there is any.
  virtual void StopCaching() override;

  // Called to tell the transaction that we have successfully reached the end
  // of the stream.
  virtual void DoneReading() override;

  // Returns the response info for this transaction or NULL if the response
  // info is not available.
  virtual const HttpResponseInfo* GetResponseInfo() const override;

  // Returns the load state for this transaction.
  virtual LoadState GetLoadState() const override;

  // Returns the upload progress in bytes.  If there is no upload data,
  // zero will be returned.  This does not include the request headers.
  virtual UploadProgress GetUploadProgress() const override;

  // Access internal member variables
  scoped_refptr<IOBuffer>& GetReadBuffer() { return read_buf_; }

 protected:
  enum State {
    STATE_CREATED,
    STATE_STREAM_INITIALIZED,
    STATE_REQUEST_SENT,
    STATE_RESPONSE_HEADERS_RECEIVED,
    STATE_DONE_READING,
    STATE_FAILED,
  };

  // Callback functions.
  void OnResolveProxyComplete(int result);
  void OnInitStreamComplete(int result);
  void OnInitUploadComplete(int result);
  void OnSendRequestComplete(int result);
  void OnReadResponseHeadersComplete(int result);
  void OnReadResponseBodyComplete(int result);

  int ResolveProxy();
  void BuildRequestHeaders();
  void SetFailed(int result);
  void CloseStream();
  void DoCallback(int result);

  // Stream object
  scoped_ptr<HttpStreamShell> stream_;

  State state_;
  // The result_ variable is used to keep the latest result. The return type
  // of callback functions is void so we can not use return value to pass
  // the result.
  int result_;

  bool pending_callback_;
  CompletionCallback resolve_proxy_callback_;
  CompletionCallback init_stream_callback_;
  CompletionCallback init_upload_callback_;
  CompletionCallback send_request_callback_;
  CompletionCallback read_response_headers_callback_;
  CompletionCallback read_response_body_callback_;
  CompletionCallback callback_;  // callback function from caller
  // Data buffer used to read from stream.
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;

  BoundNetLog net_log_;

  // Http data
  const net::HttpNetworkSession::Params* params_;
  const HttpRequestInfo* request_;
  HttpRequestHeaders request_headers_;
  HttpResponseInfo response_;

  // Proxy info
  ProxyInfo proxy_info_;

  // For checking that we're on the correct thread
  scoped_refptr<base::MessageLoopProxy> current_message_loop_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_SHELL_H_
