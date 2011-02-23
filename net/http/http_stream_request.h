// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_REQUEST_H_
#define NET_HTTP_HTTP_STREAM_REQUEST_H_

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "net/base/completion_callback.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_alternate_protocols.h"
#include "net/http/stream_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"

namespace net {

class ClientSocketHandle;
class HttpAuthController;
class HttpNetworkSession;
class HttpProxySocketParams;
class SOCKSSocketParams;
class SSLSocketParams;
class StreamRequestDelegate;
class TCPSocketParams;
struct HttpRequestInfo;

// An HttpStreamRequest exists for each stream which is in progress of being
// created for the StreamFactory.
class HttpStreamRequest : public StreamRequest {
 public:
  class PreconnectDelegate {
   public:
    virtual ~PreconnectDelegate() {}

    virtual void OnPreconnectsComplete(HttpStreamRequest* request,
                                       int result) = 0;
  };

  HttpStreamRequest(StreamFactory* factory,
                    HttpNetworkSession* session);
  virtual ~HttpStreamRequest();

  // Start initiates the process of creating a new HttpStream.
  // 3 parameters are passed in by reference.  The caller asserts that the
  // lifecycle of these parameters will remain valid until the stream is
  // created, failed, or destroyed.  In the first two cases, the delegate will
  // be called to notify completion of the request.
  void Start(const HttpRequestInfo* request_info,
             SSLConfig* ssl_config,
             ProxyInfo* proxy_info,
             Delegate* delegate,
             const BoundNetLog& net_log);

  int Preconnect(int num_streams,
                 const HttpRequestInfo* request_info,
                 SSLConfig* ssl_config,
                 ProxyInfo* proxy_info,
                 PreconnectDelegate* delegate,
                 const BoundNetLog& net_log);

  // StreamRequest interface
  virtual int RestartWithCertificate(X509Certificate* client_cert);
  virtual int RestartTunnelWithProxyAuth(const string16& username,
                                         const string16& password);
  virtual LoadState GetLoadState() const;

  virtual bool was_alternate_protocol_available() const;
  virtual bool was_npn_negotiated() const;
  virtual bool using_spdy() const;

 private:
  enum AlternateProtocolMode {
    kUnspecified,  // Unspecified, check HttpAlternateProtocols
    kUsingAlternateProtocol,  // Using an alternate protocol
    kDoNotUseAlternateProtocol,  // Failed to connect once, do not try again.
  };

  enum State {
    STATE_RESOLVE_PROXY,
    STATE_RESOLVE_PROXY_COMPLETE,
    STATE_INIT_CONNECTION,
    STATE_INIT_CONNECTION_COMPLETE,
    STATE_WAITING_USER_ACTION,
    STATE_RESTART_TUNNEL_AUTH,
    STATE_RESTART_TUNNEL_AUTH_COMPLETE,
    STATE_CREATE_STREAM,
    STATE_CREATE_STREAM_COMPLETE,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE,
    STATE_DONE,
    STATE_NONE
  };

  const HttpRequestInfo& request_info() const;
  ProxyInfo* proxy_info() const;
  SSLConfig* ssl_config() const;

  // Callbacks to the delegate.
  void OnStreamReadyCallback();
  void OnStreamFailedCallback(int result);
  void OnCertificateErrorCallback(int result, const SSLInfo& ssl_info);
  void OnNeedsProxyAuthCallback(const HttpResponseInfo& response_info,
                                HttpAuthController* auth_controller);
  void OnNeedsClientAuthCallback(SSLCertRequestInfo* cert_info);
  void OnHttpsProxyTunnelResponseCallback(const HttpResponseInfo& response_info,
                                          HttpStream* stream);
  void OnPreconnectsComplete(int result);

  void OnIOComplete(int result);
  int RunLoop(int result);
  int DoLoop(int result);
  int StartInternal(const HttpRequestInfo* request_info,
                    SSLConfig* ssl_config,
                    ProxyInfo* proxy_info,
                    const BoundNetLog& net_log);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoResolveProxy();
  int DoResolveProxyComplete(int result);
  int DoInitConnection();
  int DoInitConnectionComplete(int result);
  int DoWaitingUserAction(int result);
  int DoCreateStream();
  int DoCreateStreamComplete(int result);
  int DoRestartTunnelAuth();
  int DoRestartTunnelAuthComplete(int result);

  // Set the motivation for this request onto the underlying socket.
  void SetSocketMotivation();

  bool IsHttpsProxyAndHttpUrl();

  // Returns a newly create SSLSocketParams, and sets several
  // fields of ssl_config_.
  scoped_refptr<SSLSocketParams> GenerateSSLParams(
      scoped_refptr<TCPSocketParams> tcp_params,
      scoped_refptr<HttpProxySocketParams> http_proxy_params,
      scoped_refptr<SOCKSSocketParams> socks_params,
      ProxyServer::Scheme proxy_scheme,
      const HostPortPair& host_and_port,
      bool want_spdy_over_npn);

  // AlternateProtocol API
  void MarkBrokenAlternateProtocolAndFallback();

  // Retrieve SSLInfo from our SSL Socket.
  // This must only be called when we are using an SSLSocket.
  // After calling, the caller can use ssl_info_.
  void GetSSLInfo();

  // Called when we encounter a network error that could be resolved by trying
  // a new proxy configuration.  If there is another proxy configuration to try
  // then this method sets next_state_ appropriately and returns either OK or
  // ERR_IO_PENDING depending on whether or not the new proxy configuration is
  // available synchronously or asynchronously.  Otherwise, the given error
  // code is simply returned.
  int ReconsiderProxyAfterError(int error);

  // Called to handle a certificate error.  Stores the certificate in the
  // allowed_bad_certs list, and checks if the error can be ignored.  Returns
  // OK if it can be ignored, or the error code otherwise.
  int HandleCertificateError(int error);

  // Called to handle a client certificate request.
  int HandleCertificateRequest(int error);

  // Moves this stream request into SPDY mode.
  void SwitchToSpdyMode();

  // Should we force SPDY to run over SSL for this stream request.
  bool ShouldForceSpdySSL();

  // Should we force SPDY to run without SSL for this stream request.
  bool ShouldForceSpdyWithoutSSL();

  // Record histograms of latency until Connect() completes.
  static void LogHttpConnectedMetrics(const ClientSocketHandle& handle);

  const HttpRequestInfo* request_info_;  // Use request_info().
  ProxyInfo* proxy_info_;  // Use proxy_info().
  SSLConfig* ssl_config_;  // Use ssl_config().

  scoped_refptr<HttpNetworkSession> session_;
  CompletionCallbackImpl<HttpStreamRequest> io_callback_;
  scoped_ptr<ClientSocketHandle> connection_;
  StreamFactory* const factory_;
  Delegate* delegate_;
  BoundNetLog net_log_;
  State next_state_;
  ProxyService::PacRequest* pac_request_;
  SSLInfo ssl_info_;
  // The hostname and port of the endpoint.  This is not necessarily the one
  // specified by the URL, due to Alternate-Protocol or fixed testing ports.
  HostPortPair endpoint_;

  // True if handling a HTTPS request, or using SPDY with SSL
  bool using_ssl_;

  // True if this network transaction is using SPDY instead of HTTP.
  bool using_spdy_;

  // Force spdy for all connections.
  bool force_spdy_always_;

  // Force spdy only for SSL connections.
  bool force_spdy_over_ssl_;

  // The certificate error while using SPDY over SSL for insecure URLs.
  int spdy_certificate_error_;

  scoped_refptr<HttpAuthController>
    auth_controllers_[HttpAuth::AUTH_NUM_TARGETS];

  AlternateProtocolMode alternate_protocol_mode_;

  // Only valid if |alternate_protocol_mode_| == kUsingAlternateProtocol.
  HttpAlternateProtocols::Protocol alternate_protocol_;

  // True when the tunnel is in the process of being established - we can't
  // read from the socket until the tunnel is done.
  bool establishing_tunnel_;

  scoped_ptr<HttpStream> stream_;

  // True if finding the connection for this request found an alternate
  // protocol was available.
  bool was_alternate_protocol_available_;

  // True if we negotiated NPN.
  bool was_npn_negotiated_;

  PreconnectDelegate* preconnect_delegate_;

  // Only used if |preconnect_delegate_| is non-NULL.
  int num_streams_;

  ScopedRunnableMethodFactory<HttpStreamRequest> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamRequest);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_REQUEST_H_
