// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_TRANSACTION_H_
#define NET_HTTP_HTTP_NETWORK_TRANSACTION_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_alternate_protocols.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_pool.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

class ClientSocketFactory;
class ClientSocketHandle;
class HttpNetworkSession;
class HttpRequestHeaders;
class HttpStream;
class SpdyHttpStream;

class HttpNetworkTransaction : public HttpTransaction {
 public:
  explicit HttpNetworkTransaction(HttpNetworkSession* session);

  virtual ~HttpNetworkTransaction();

  static void SetHostMappingRules(const std::string& rules);

  // Controls whether or not we use the Alternate-Protocol header.
  static void SetUseAlternateProtocols(bool value);

  // Sets the next protocol negotiation value used during the SSL handshake.
  static void SetNextProtos(const std::string& next_protos);

  // Sets the HttpNetworkTransaction into a mode where it can ignore
  // certificate errors.  This is for testing.
  static void IgnoreCertificateErrors(bool enabled);

  // HttpTransaction methods:
  virtual int Start(const HttpRequestInfo* request_info,
                    CompletionCallback* callback,
                    const BoundNetLog& net_log);
  virtual int RestartIgnoringLastError(CompletionCallback* callback);
  virtual int RestartWithCertificate(X509Certificate* client_cert,
                                     CompletionCallback* callback);
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual bool IsReadyToRestartForAuth() {
    return pending_auth_target_ != HttpAuth::AUTH_NONE &&
        HaveAuth(pending_auth_target_);
  }

  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual void StopCaching() {}
  virtual const HttpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

 private:
  FRIEND_TEST(HttpNetworkTransactionTest, ResetStateForRestart);

  enum State {
    STATE_RESOLVE_PROXY,
    STATE_RESOLVE_PROXY_COMPLETE,
    STATE_INIT_CONNECTION,
    STATE_INIT_CONNECTION_COMPLETE,
    STATE_GENERATE_PROXY_AUTH_TOKEN,
    STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE,
    STATE_GENERATE_SERVER_AUTH_TOKEN,
    STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_READ_BODY,
    STATE_READ_BODY_COMPLETE,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE,
    STATE_SPDY_GET_STREAM,
    STATE_SPDY_GET_STREAM_COMPLETE,
    STATE_SPDY_SEND_REQUEST,
    STATE_SPDY_SEND_REQUEST_COMPLETE,
    STATE_SPDY_READ_HEADERS,
    STATE_SPDY_READ_HEADERS_COMPLETE,
    STATE_SPDY_READ_BODY,
    STATE_SPDY_READ_BODY_COMPLETE,
    STATE_NONE
  };

  enum AlternateProtocolMode {
    kUnspecified,  // Unspecified, check HttpAlternateProtocols
    kUsingAlternateProtocol,  // Using an alternate protocol
    kDoNotUseAlternateProtocol,  // Failed to connect once, do not try again.
  };

  void DoCallback(int result);
  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoResolveProxy();
  int DoResolveProxyComplete(int result);
  int DoInitConnection();
  int DoInitConnectionComplete(int result);
  int DoGenerateProxyAuthToken();
  int DoGenerateProxyAuthTokenComplete(int result);
  int DoGenerateServerAuthToken();
  int DoGenerateServerAuthTokenComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoReadBody();
  int DoReadBodyComplete(int result);
  int DoDrainBodyForAuthRestart();
  int DoDrainBodyForAuthRestartComplete(int result);
  int DoSpdyGetStream();
  int DoSpdyGetStreamComplete(int result);
  int DoSpdySendRequest();
  int DoSpdySendRequestComplete(int result);
  int DoSpdyReadHeaders();
  int DoSpdyReadHeadersComplete(int result);
  int DoSpdyReadBody();
  int DoSpdyReadBodyComplete(int result);

  // Record histograms of latency until Connect() completes.
  static void LogHttpConnectedMetrics(const ClientSocketHandle& handle);

  // Record histogram of time until first byte of header is received.
  void LogTransactionConnectedMetrics();

  // Record histogram of latency (durations until last byte received).
  void LogTransactionMetrics() const;

  // Writes a log message to help debugging in the field when we block a proxy
  // response to a CONNECT request.
  void LogBlockedTunnelResponse(int response_code) const;

  static void LogIOErrorMetrics(const ClientSocketHandle& handle);

  // Called to handle a certificate error.  Returns OK if the error should be
  // ignored.  Otherwise, stores the certificate in response_.ssl_info and
  // returns the same error code.
  int HandleCertificateError(int error);

  // Called to handle a client certificate request.
  int HandleCertificateRequest(int error);

  // Called to possibly recover from an SSL handshake error.  Sets next_state_
  // and returns OK if recovering from the error.  Otherwise, the same error
  // code is returned.
  int HandleSSLHandshakeError(int error);

  // Called to possibly recover from the given error.  Sets next_state_ and
  // returns OK if recovering from the error.  Otherwise, the same error code
  // is returned.
  int HandleIOError(int error);

  // Gets the response headers from the HttpStream.
  HttpResponseHeaders* GetResponseHeaders() const;

  // Called when we reached EOF or got an error.  Returns true if we should
  // resend the request.  |error| is OK when we reached EOF.
  bool ShouldResendRequest(int error) const;

  // Resets the connection and the request headers for resend.  Called when
  // ShouldResendRequest() is true.
  void ResetConnectionAndRequestForResend();

  // Called when we encounter a network error that could be resolved by trying
  // a new proxy configuration.  If there is another proxy configuration to try
  // then this method sets next_state_ appropriately and returns either OK or
  // ERR_IO_PENDING depending on whether or not the new proxy configuration is
  // available synchronously or asynchronously.  Otherwise, the given error
  // code is simply returned.
  int ReconsiderProxyAfterError(int error);

  // Decides the policy when the connection is closed before the end of headers
  // has been read. This only applies to reading responses, and not writing
  // requests.
  int HandleConnectionClosedBeforeEndOfHeaders();

  // Sets up the state machine to restart the transaction with auth.
  void PrepareForAuthRestart(HttpAuth::Target target);

  // Called when we don't need to drain the response body or have drained it.
  // Resets |connection_| unless |keep_alive| is true, then calls
  // ResetStateForRestart.  Sets |next_state_| appropriately.
  void DidDrainBodyForAuthRestart(bool keep_alive);

  // Resets the members of the transaction so it can be restarted.
  void ResetStateForRestart();

  // Returns true if we should try to add a Proxy-Authorization header
  bool ShouldApplyProxyAuth() const;

  // Returns true if we should try to add an Authorization header.
  bool ShouldApplyServerAuth() const;

  // Handles HTTP status code 401 or 407.
  // HandleAuthChallenge() returns a network error code, or OK on success.
  // May update |pending_auth_target_| or |response_.auth_challenge|.
  int HandleAuthChallenge();

  bool HaveAuth(HttpAuth::Target target) const {
    return auth_controllers_[target].get() &&
         auth_controllers_[target]->HaveAuth();
  }

  // Get the {scheme, host, path, port} for the authentication target
  GURL AuthURL(HttpAuth::Target target) const;

  void MarkBrokenAlternateProtocolAndFallback();

  // Debug helper.
  static std::string DescribeState(State state);

  static bool g_ignore_certificate_errors;

  scoped_refptr<HttpAuthController>
      auth_controllers_[HttpAuth::AUTH_NUM_TARGETS];

  // Whether this transaction is waiting for proxy auth, server auth, or is
  // not waiting for any auth at all. |pending_auth_target_| is read and
  // cleared by RestartWithAuth().
  HttpAuth::Target pending_auth_target_;

  CompletionCallbackImpl<HttpNetworkTransaction> io_callback_;
  CompletionCallback* user_callback_;

  scoped_refptr<HttpNetworkSession> session_;

  BoundNetLog net_log_;
  const HttpRequestInfo* request_;
  HttpResponseInfo response_;

  ProxyService::PacRequest* pac_request_;
  ProxyInfo proxy_info_;

  scoped_ptr<ClientSocketHandle> connection_;
  scoped_ptr<HttpStream> http_stream_;
  scoped_ptr<SpdyHttpStream> spdy_http_stream_;
  bool reused_socket_;

  // True if we've validated the headers that the stream parser has returned.
  bool headers_valid_;

  // True if we've logged the time of the first response byte.  Used to
  // prevent logging across authentication activity where we see multiple
  // responses.
  bool logged_response_time_;

  bool using_ssl_;     // True if handling a HTTPS request

  // True if this network transaction is using SPDY instead of HTTP.
  bool using_spdy_;

  // The certificate error while using SPDY over SSL for insecure URLs.
  int spdy_certificate_error_;

  AlternateProtocolMode alternate_protocol_mode_;

  // Only valid if |alternate_protocol_mode_| == kUsingAlternateProtocol.
  HttpAlternateProtocols::Protocol alternate_protocol_;

  SSLConfig ssl_config_;

  std::string request_headers_;

  // The size in bytes of the buffer we use to drain the response body that
  // we want to throw away.  The response body is typically a small error
  // page just a few hundred bytes long.
  enum { kDrainBodyBufferSize = 1024 };

  // User buffer and length passed to the Read method.
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;

  // The time the Start method was called.
  base::Time start_time_;

  // The next state in the state machine.
  State next_state_;

  // The hostname and port of the endpoint.  This is not necessarily the one
  // specified by the URL, due to Alternate-Protocol or fixed testing ports.
  HostPortPair endpoint_;

  // True when the tunnel is in the process of being established - we can't
  // read from the socket until the tunnel is done.
  bool establishing_tunnel_;

  DISALLOW_COPY_AND_ASSIGN(HttpNetworkTransaction);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_TRANSACTION_H_
