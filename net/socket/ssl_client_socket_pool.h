// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_POOL_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_response_info.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/client_socket_pool.h"

namespace net {

class ClientSocketFactory;
class ConnectJobFactory;
class HttpProxyClientSocketPool;
class HttpProxySocketParams;
class SOCKSClientSocketPool;
class SOCKSSocketParams;
class SSLClientSocket;
class TCPClientSocketPool;
class TCPSocketParams;
struct RRResponse;

// SSLSocketParams only needs the socket params for the transport socket
// that will be used (denoted by |proxy|).
class SSLSocketParams : public base::RefCounted<SSLSocketParams>,
                        public DNSSECProvider {
 public:
  SSLSocketParams(const scoped_refptr<TCPSocketParams>& tcp_params,
                  const scoped_refptr<SOCKSSocketParams>& socks_params,
                  const scoped_refptr<HttpProxySocketParams>& http_proxy_params,
                  ProxyServer::Scheme proxy,
                  const std::string& hostname,
                  const SSLConfig& ssl_config,
                  int load_flags,
                  bool force_spdy_over_ssl,
                  bool want_spdy_over_npn);

  const scoped_refptr<TCPSocketParams>& tcp_params() { return tcp_params_; }
  const scoped_refptr<HttpProxySocketParams>& http_proxy_params() {
    return http_proxy_params_;
  }
  const scoped_refptr<SOCKSSocketParams>& socks_params() {
    return socks_params_;
  }
  ProxyServer::Scheme proxy() const { return proxy_; }
  const std::string& hostname() const { return hostname_; }
  const SSLConfig& ssl_config() const { return ssl_config_; }
  int load_flags() const { return load_flags_; }
  bool force_spdy_over_ssl() const { return force_spdy_over_ssl_; }
  bool want_spdy_over_npn() const { return want_spdy_over_npn_; }
  // Start to resolve DNSSEC records for the given hostname.
  void StartDNSSECResolution();

  // DNSSECProvider implementation.
  virtual int GetDNSSECRecords(RRResponse** out, CompletionCallback* callback);

 private:
  friend class base::RefCounted<SSLSocketParams>;
  ~SSLSocketParams();
  void DNSSECResolutionComplete(int rv);

  const scoped_refptr<TCPSocketParams> tcp_params_;
  const scoped_refptr<HttpProxySocketParams> http_proxy_params_;
  const scoped_refptr<SOCKSSocketParams> socks_params_;
  const ProxyServer::Scheme proxy_;
  const std::string hostname_;
  const SSLConfig ssl_config_;
  const int load_flags_;
  const bool force_spdy_over_ssl_;
  const bool want_spdy_over_npn_;

  // This is true if we have started a DNSSEC resolution.
  bool dnssec_resolution_attempted_;
  // This is true if |dnssec_response_| is valid.
  bool dnssec_resolution_complete_;
  scoped_ptr<RRResponse> dnssec_response_;
  CompletionCallback* dnssec_resolution_callback_;

  DISALLOW_COPY_AND_ASSIGN(SSLSocketParams);
};

// SSLConnectJob handles the SSL handshake after setting up the underlying
// connection as specified in the params.
class SSLConnectJob : public ConnectJob {
 public:
  SSLConnectJob(
      const std::string& group_name,
      const scoped_refptr<SSLSocketParams>& params,
      const base::TimeDelta& timeout_duration,
      TCPClientSocketPool* tcp_pool,
      SOCKSClientSocketPool* socks_pool,
      HttpProxyClientSocketPool* http_proxy_pool,
      ClientSocketFactory* client_socket_factory,
      const scoped_refptr<HostResolver>& host_resolver,
      Delegate* delegate,
      NetLog* net_log);
  virtual ~SSLConnectJob();

  // ConnectJob methods.
  virtual LoadState GetLoadState() const;

  virtual void GetAdditionalErrorState(ClientSocketHandle * handle);

 private:
  enum State {
    STATE_TCP_CONNECT,
    STATE_TCP_CONNECT_COMPLETE,
    STATE_SOCKS_CONNECT,
    STATE_SOCKS_CONNECT_COMPLETE,
    STATE_TUNNEL_CONNECT,
    STATE_TUNNEL_CONNECT_COMPLETE,
    STATE_SSL_CONNECT,
    STATE_SSL_CONNECT_COMPLETE,
    STATE_NONE,
  };

  // Starts the SSL connection process.  Returns OK on success and
  // ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  virtual int ConnectInternal();

  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  int DoTCPConnect();
  int DoTCPConnectComplete(int result);
  int DoSOCKSConnect();
  int DoSOCKSConnectComplete(int result);
  int DoTunnelConnect();
  int DoTunnelConnectComplete(int result);
  int DoSSLConnect();
  int DoSSLConnectComplete(int result);

  scoped_refptr<SSLSocketParams> params_;
  TCPClientSocketPool* const tcp_pool_;
  SOCKSClientSocketPool* const socks_pool_;
  HttpProxyClientSocketPool* const http_proxy_pool_;
  ClientSocketFactory* const client_socket_factory_;
  const scoped_refptr<HostResolver> resolver_;

  State next_state_;
  CompletionCallbackImpl<SSLConnectJob> callback_;
  scoped_ptr<ClientSocketHandle> transport_socket_handle_;
  scoped_ptr<SSLClientSocket> ssl_socket_;

  // The time the DoSSLConnect() method was called.
  base::TimeTicks ssl_connect_start_time_;

  HttpResponseInfo error_response_info_;

  DISALLOW_COPY_AND_ASSIGN(SSLConnectJob);
};

class SSLClientSocketPool : public ClientSocketPool,
                            public SSLConfigService::Observer {
 public:
  // Only the pools that will be used are required. i.e. if you never
  // try to create an SSL over SOCKS socket, |socks_pool| may be NULL.
  SSLClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      ClientSocketPoolHistograms* histograms,
      const scoped_refptr<HostResolver>& host_resolver,
      ClientSocketFactory* client_socket_factory,
      TCPClientSocketPool* tcp_pool,
      SOCKSClientSocketPool* socks_pool,
      HttpProxyClientSocketPool* http_proxy_pool,
      SSLConfigService* ssl_config_service,
      NetLog* net_log);

  virtual ~SSLClientSocketPool();

  // ClientSocketPool methods:
  virtual int RequestSocket(const std::string& group_name,
                            const void* connect_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback,
                            const BoundNetLog& net_log);

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle);

  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket,
                             int id);

  virtual void Flush();

  virtual void CloseIdleSockets();

  virtual int IdleSocketCount() const {
    return base_.idle_socket_count();
  }

  virtual int IdleSocketCountInGroup(const std::string& group_name) const;

  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const;

  virtual DictionaryValue* GetInfoAsValue(const std::string& name,
                                          const std::string& type,
                                          bool include_nested_pools) const;

  virtual base::TimeDelta ConnectionTimeout() const {
    return base_.ConnectionTimeout();
  }

  virtual ClientSocketPoolHistograms* histograms() const {
    return base_.histograms();
  };

 private:
  // SSLConfigService::Observer methods:

  // When the user changes the SSL config, we flush all idle sockets so they
  // won't get re-used.
  virtual void OnSSLConfigChanged();

  typedef ClientSocketPoolBase<SSLSocketParams> PoolBase;

  class SSLConnectJobFactory : public PoolBase::ConnectJobFactory {
   public:
    SSLConnectJobFactory(
        TCPClientSocketPool* tcp_pool,
        SOCKSClientSocketPool* socks_pool,
        HttpProxyClientSocketPool* http_proxy_pool,
        ClientSocketFactory* client_socket_factory,
        HostResolver* host_resolver,
        NetLog* net_log);

    virtual ~SSLConnectJobFactory() {}

    // ClientSocketPoolBase::ConnectJobFactory methods.
    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const;

    virtual base::TimeDelta ConnectionTimeout() const { return timeout_; }

   private:
    TCPClientSocketPool* const tcp_pool_;
    SOCKSClientSocketPool* const socks_pool_;
    HttpProxyClientSocketPool* const http_proxy_pool_;
    ClientSocketFactory* const client_socket_factory_;
    const scoped_refptr<HostResolver> host_resolver_;
    base::TimeDelta timeout_;
    NetLog* net_log_;

    DISALLOW_COPY_AND_ASSIGN(SSLConnectJobFactory);
  };

  TCPClientSocketPool* const tcp_pool_;
  SOCKSClientSocketPool* const socks_pool_;
  HttpProxyClientSocketPool* const http_proxy_pool_;
  PoolBase base_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientSocketPool);
};

REGISTER_SOCKET_PARAMS_FOR_POOL(SSLClientSocketPool, SSLSocketParams);

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_POOL_H_
