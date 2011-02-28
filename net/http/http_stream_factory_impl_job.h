// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_IMPL_JOB_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_IMPL_JOB_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_alternate_protocols.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream_factory_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"

namespace net {

class ClientSocketHandle;
class HttpAuthController;
class HttpNetworkSession;
class HttpProxySocketParams;
class HttpStream;
class SOCKSSocketParams;
class SSLSocketParams;
class TCPSocketParams;

// An HttpStreamRequestImpl exists for each stream which is in progress of being
// created for the StreamFactory.
class HttpStreamFactoryImpl::Job {
 public:
  Job(HttpStreamFactoryImpl* stream_factory,
      HttpNetworkSession* session);
  ~Job();

  // Start initiates the process of creating a new HttpStream.
  // 3 parameters are passed in by reference.  The caller asserts that the
  // lifecycle of these parameters will remain valid until the stream is
  // created, failed, or destroyed.  In the first two cases, the delegate will
  // be called to notify completion of the request.
  void Start(Request* request,
             const HttpRequestInfo& request_info,
             const SSLConfig& ssl_config,
             const BoundNetLog& net_log);

  int Preconnect(int num_streams,
                 const HttpRequestInfo& request_info,
                 const SSLConfig& ssl_config,
                 const BoundNetLog& net_log);

  int RestartTunnelWithProxyAuth(const string16& username,
                                 const string16& password);
  LoadState GetLoadState() const;

  bool was_alternate_protocol_available() const;
  bool was_npn_negotiated() const;
  bool using_spdy() const;
  const BoundNetLog& net_log() const { return net_log_; }

  const SSLConfig& ssl_config() const;
  const ProxyInfo& proxy_info() const;

  // Indicates whether or not this job is performing a preconnect.
  bool IsPreconnecting() const;

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

  void OnStreamReadyCallback();
  void OnSpdySessionReadyCallback();
  void OnStreamFailedCallback(int result);
  void OnCertificateErrorCallback(int result, const SSLInfo& ssl_info);
  void OnNeedsProxyAuthCallback(const HttpResponseInfo& response_info,
                                HttpAuthController* auth_controller);
  void OnNeedsClientAuthCallback(SSLCertRequestInfo* cert_info);
  void OnHttpsProxyTunnelResponseCallback(const HttpResponseInfo& response_info,
                                          HttpStream* stream);
  void OnPreconnectsComplete();

  void OnIOComplete(int result);
  int RunLoop(int result);
  int DoLoop(int result);
  int StartInternal(const HttpRequestInfo& request_info,
                    const SSLConfig& ssl_config,
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

  // Returns to STATE_INIT_CONNECTION and resets some state.
  void ReturnToStateInitConnection(bool close_connection);

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

  Request* request_;

  HttpRequestInfo request_info_;
  ProxyInfo proxy_info_;
  SSLConfig ssl_config_;

  CompletionCallbackImpl<Job> io_callback_;
  scoped_ptr<ClientSocketHandle> connection_;
  HttpNetworkSession* const session_;
  HttpStreamFactoryImpl* const stream_factory_;
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

  // 0 if we're not preconnecting. Otherwise, the number of streams to
  // preconnect.
  int num_streams_;

  // Initialized when we create a new SpdySession.
  scoped_refptr<SpdySession> new_spdy_session_;

  // Only used if |new_spdy_session_| is non-NULL.
  bool spdy_session_direct_;

  ScopedRunnableMethodFactory<Job> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_IMPL_JOB_H_
