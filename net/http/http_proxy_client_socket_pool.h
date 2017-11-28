// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_POOL_H_
#define NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_POOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "net/http/http_response_info.h"
#include "net/http/proxy_client_socket.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class HostResolver;
class HttpAuthCache;
class HttpAuthHandlerFactory;
class SSLClientSocketPool;
class SSLSocketParams;
class SpdySessionPool;
class SpdyStream;
class TransportClientSocketPool;
class TransportSocketParams;

// HttpProxySocketParams only needs the socket params for one of the proxy
// types.  The other param must be NULL.  When using an HTTP Proxy,
// |transport_params| must be set.  When using an HTTPS Proxy, |ssl_params|
// must be set.
class NET_EXPORT_PRIVATE HttpProxySocketParams
    : public base::RefCounted<HttpProxySocketParams> {
 public:
  HttpProxySocketParams(
      const scoped_refptr<TransportSocketParams>& transport_params,
      const scoped_refptr<SSLSocketParams>& ssl_params,
      const GURL& request_url,
      const std::string& user_agent,
      const HostPortPair& endpoint,
      HttpAuthCache* http_auth_cache,
      HttpAuthHandlerFactory* http_auth_handler_factory,
      SpdySessionPool* spdy_session_pool,
      bool tunnel);

  const scoped_refptr<TransportSocketParams>& transport_params() const {
    return transport_params_;
  }
  const scoped_refptr<SSLSocketParams>& ssl_params() const {
    return ssl_params_;
  }
  const GURL& request_url() const { return request_url_; }
  const std::string& user_agent() const { return user_agent_; }
  const HostPortPair& endpoint() const { return endpoint_; }
  HttpAuthCache* http_auth_cache() const { return http_auth_cache_; }
  HttpAuthHandlerFactory* http_auth_handler_factory() const {
    return http_auth_handler_factory_;
  }
  SpdySessionPool* spdy_session_pool() {
    return spdy_session_pool_;
  }
  const HostResolver::RequestInfo& destination() const;
  bool tunnel() const { return tunnel_; }
  bool ignore_limits() const { return ignore_limits_; }

 private:
  friend class base::RefCounted<HttpProxySocketParams>;
  ~HttpProxySocketParams();

  const scoped_refptr<TransportSocketParams> transport_params_;
  const scoped_refptr<SSLSocketParams> ssl_params_;
  SpdySessionPool* spdy_session_pool_;
  const GURL request_url_;
  const std::string user_agent_;
  const HostPortPair endpoint_;
  HttpAuthCache* const http_auth_cache_;
  HttpAuthHandlerFactory* const http_auth_handler_factory_;
  const bool tunnel_;
  bool ignore_limits_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxySocketParams);
};

// HttpProxyConnectJob optionally establishes a tunnel through the proxy
// server after connecting the underlying transport socket.
class HttpProxyConnectJob : public ConnectJob {
 public:
  HttpProxyConnectJob(const std::string& group_name,
                      const scoped_refptr<HttpProxySocketParams>& params,
                      const base::TimeDelta& timeout_duration,
                      TransportClientSocketPool* transport_pool,
                      SSLClientSocketPool* ssl_pool,
                      HostResolver* host_resolver,
                      Delegate* delegate,
                      NetLog* net_log);
  virtual ~HttpProxyConnectJob();

  // ConnectJob methods.
  virtual LoadState GetLoadState() const override;

  virtual void GetAdditionalErrorState(ClientSocketHandle* handle) override;

 private:
  enum State {
    STATE_TCP_CONNECT,
    STATE_TCP_CONNECT_COMPLETE,
    STATE_SSL_CONNECT,
    STATE_SSL_CONNECT_COMPLETE,
    STATE_HTTP_PROXY_CONNECT,
    STATE_HTTP_PROXY_CONNECT_COMPLETE,
    STATE_SPDY_PROXY_CREATE_STREAM,
    STATE_SPDY_PROXY_CREATE_STREAM_COMPLETE,
    STATE_SPDY_PROXY_CONNECT_COMPLETE,
    STATE_NONE,
  };

  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  // Connecting to HTTP Proxy
  int DoTransportConnect();
  int DoTransportConnectComplete(int result);
  // Connecting to HTTPS Proxy
  int DoSSLConnect();
  int DoSSLConnectComplete(int result);

  int DoHttpProxyConnect();
  int DoHttpProxyConnectComplete(int result);

  int DoSpdyProxyCreateStream();
  int DoSpdyProxyCreateStreamComplete(int result);

  // Begins the tcp connection and the optional Http proxy tunnel.  If the
  // request is not immediately servicable (likely), the request will return
  // ERR_IO_PENDING. An OK return from this function or the callback means
  // that the connection is established; ERR_PROXY_AUTH_REQUESTED means
  // that the tunnel needs authentication credentials, the socket will be
  // returned in this case, and must be release back to the pool; or
  // a standard net error code will be returned.
  virtual int ConnectInternal() override;

  scoped_refptr<HttpProxySocketParams> params_;
  TransportClientSocketPool* const transport_pool_;
  SSLClientSocketPool* const ssl_pool_;
  HostResolver* const resolver_;

  State next_state_;
  CompletionCallback callback_;
  scoped_ptr<ClientSocketHandle> transport_socket_handle_;
  scoped_ptr<ProxyClientSocket> transport_socket_;
  bool using_spdy_;
  // Protocol negotiated with the server.
  NextProto protocol_negotiated_;

  HttpResponseInfo error_response_info_;

  scoped_refptr<SpdyStream> spdy_stream_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxyConnectJob);
};

class NET_EXPORT_PRIVATE HttpProxyClientSocketPool
    : public ClientSocketPool,
      public LayeredPool {
 public:
  HttpProxyClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      ClientSocketPoolHistograms* histograms,
      HostResolver* host_resolver,
      TransportClientSocketPool* transport_pool,
      SSLClientSocketPool* ssl_pool,
      NetLog* net_log);

  virtual ~HttpProxyClientSocketPool();

  // ClientSocketPool implementation.
  virtual int RequestSocket(const std::string& group_name,
                            const void* connect_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            const CompletionCallback& callback,
                            const BoundNetLog& net_log) override;

  virtual void RequestSockets(const std::string& group_name,
                              const void* params,
                              int num_sockets,
                              const BoundNetLog& net_log) override;

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) override;

  virtual void ReleaseSocket(const std::string& group_name,
                             StreamSocket* socket,
                             int id) override;

  virtual void FlushWithError(int error) override;

  virtual bool IsStalled() const override;

  virtual void CloseIdleSockets() override;

  virtual int IdleSocketCount() const override;

  virtual int IdleSocketCountInGroup(
      const std::string& group_name) const override;

  virtual LoadState GetLoadState(
      const std::string& group_name,
      const ClientSocketHandle* handle) const override;

  virtual void AddLayeredPool(LayeredPool* layered_pool) override;

  virtual void RemoveLayeredPool(LayeredPool* layered_pool) override;

  virtual base::DictionaryValue* GetInfoAsValue(
      const std::string& name,
      const std::string& type,
      bool include_nested_pools) const override;

  virtual base::TimeDelta ConnectionTimeout() const override;

  virtual ClientSocketPoolHistograms* histograms() const override;

  // LayeredPool implementation.
  virtual bool CloseOneIdleConnection() override;

 private:
  typedef ClientSocketPoolBase<HttpProxySocketParams> PoolBase;

  class HttpProxyConnectJobFactory : public PoolBase::ConnectJobFactory {
   public:
    HttpProxyConnectJobFactory(
        TransportClientSocketPool* transport_pool,
        SSLClientSocketPool* ssl_pool,
        HostResolver* host_resolver,
        NetLog* net_log);

    // ClientSocketPoolBase::ConnectJobFactory methods.
    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const override;

    virtual base::TimeDelta ConnectionTimeout() const override;

   private:
    TransportClientSocketPool* const transport_pool_;
    SSLClientSocketPool* const ssl_pool_;
    HostResolver* const host_resolver_;
    NetLog* net_log_;
    base::TimeDelta timeout_;

    DISALLOW_COPY_AND_ASSIGN(HttpProxyConnectJobFactory);
  };

  TransportClientSocketPool* const transport_pool_;
  SSLClientSocketPool* const ssl_pool_;
  PoolBase base_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxyClientSocketPool);
};

REGISTER_SOCKET_PARAMS_FOR_POOL(HttpProxyClientSocketPool,
                                HttpProxySocketParams);

}  // namespace net

#endif  // NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_POOL_H_
