// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/single_request_host_resolver.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/client_socket_pool.h"

namespace net {

class ClientSocketFactory;

class NET_EXPORT_PRIVATE TransportSocketParams
    : public base::RefCounted<TransportSocketParams> {
 public:
  TransportSocketParams(const HostPortPair& host_port_pair,
                        RequestPriority priority,
                        bool disable_resolver_cache,
                        bool ignore_limits);

  const HostResolver::RequestInfo& destination() const { return destination_; }
  bool ignore_limits() const { return ignore_limits_; }

 private:
  friend class base::RefCounted<TransportSocketParams>;
  ~TransportSocketParams();

  void Initialize(RequestPriority priority, bool disable_resolver_cache);

  HostResolver::RequestInfo destination_;
  bool ignore_limits_;

  DISALLOW_COPY_AND_ASSIGN(TransportSocketParams);
};

// TransportConnectJob handles the host resolution necessary for socket creation
// and the transport (likely TCP) connect. TransportConnectJob also has fallback
// logic for IPv6 connect() timeouts (which may happen due to networks / routers
// with broken IPv6 support). Those timeouts take 20s, so rather than make the
// user wait 20s for the timeout to fire, we use a fallback timer
// (kIPv6FallbackTimerInMs) and start a connect() to a IPv4 address if the timer
// fires. Then we race the IPv4 connect() against the IPv6 connect() (which has
// a headstart) and return the one that completes first to the socket pool.
class NET_EXPORT_PRIVATE TransportConnectJob : public ConnectJob {
 public:
  TransportConnectJob(const std::string& group_name,
                      const scoped_refptr<TransportSocketParams>& params,
                      base::TimeDelta timeout_duration,
                      ClientSocketFactory* client_socket_factory,
                      HostResolver* host_resolver,
                      Delegate* delegate,
                      NetLog* net_log);
  virtual ~TransportConnectJob();

  // ConnectJob methods.
  virtual LoadState GetLoadState() const OVERRIDE;

  // Rolls |addrlist| forward until the first IPv4 address, if any.
  // WARNING: this method should only be used to implement the prefer-IPv4 hack.
  static void MakeAddressListStartWithIPv4(AddressList* addrlist);

  static const int kIPv6FallbackTimerInMs;

 private:
  enum State {
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
    STATE_NONE,
  };

  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoTransportConnect();
  int DoTransportConnectComplete(int result);

  // Not part of the state machine.
  void DoIPv6FallbackTransportConnect();
  void DoIPv6FallbackTransportConnectComplete(int result);

  // Begins the host resolution and the TCP connect.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  virtual int ConnectInternal() OVERRIDE;

  scoped_refptr<TransportSocketParams> params_;
  ClientSocketFactory* const client_socket_factory_;
  SingleRequestHostResolver resolver_;
  AddressList addresses_;
  State next_state_;

  // The time Connect() was called.
  base::TimeTicks start_time_;

  // The time the connect was started (after DNS finished).
  base::TimeTicks connect_start_time_;

  scoped_ptr<StreamSocket> transport_socket_;

  scoped_ptr<StreamSocket> fallback_transport_socket_;
  scoped_ptr<AddressList> fallback_addresses_;
  base::TimeTicks fallback_connect_start_time_;
  base::OneShotTimer<TransportConnectJob> fallback_timer_;

  DISALLOW_COPY_AND_ASSIGN(TransportConnectJob);
};

class NET_EXPORT_PRIVATE TransportClientSocketPool : public ClientSocketPool {
 public:
  TransportClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      ClientSocketPoolHistograms* histograms,
      HostResolver* host_resolver,
      ClientSocketFactory* client_socket_factory,
      NetLog* net_log);

  virtual ~TransportClientSocketPool();

  // ClientSocketPool implementation.
  virtual int RequestSocket(const std::string& group_name,
                            const void* resolve_info,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            const CompletionCallback& callback,
                            const BoundNetLog& net_log) OVERRIDE;
  virtual void RequestSockets(const std::string& group_name,
                              const void* params,
                              int num_sockets,
                              const BoundNetLog& net_log) OVERRIDE;
  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) OVERRIDE;
  virtual void ReleaseSocket(const std::string& group_name,
                             StreamSocket* socket,
                             int id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual bool IsStalled() const OVERRIDE;
  virtual void CloseIdleSockets() OVERRIDE;
  virtual int IdleSocketCount() const OVERRIDE;
  virtual int IdleSocketCountInGroup(
      const std::string& group_name) const OVERRIDE;
  virtual LoadState GetLoadState(
      const std::string& group_name,
      const ClientSocketHandle* handle) const OVERRIDE;
  virtual void AddLayeredPool(LayeredPool* layered_pool) OVERRIDE;
  virtual void RemoveLayeredPool(LayeredPool* layered_pool) OVERRIDE;
  virtual base::DictionaryValue* GetInfoAsValue(
      const std::string& name,
      const std::string& type,
      bool include_nested_pools) const OVERRIDE;
  virtual base::TimeDelta ConnectionTimeout() const OVERRIDE;
  virtual ClientSocketPoolHistograms* histograms() const OVERRIDE;

 private:
  typedef ClientSocketPoolBase<TransportSocketParams> PoolBase;

  class TransportConnectJobFactory
      : public PoolBase::ConnectJobFactory {
   public:
    TransportConnectJobFactory(ClientSocketFactory* client_socket_factory,
                         HostResolver* host_resolver,
                         NetLog* net_log)
        : client_socket_factory_(client_socket_factory),
          host_resolver_(host_resolver),
          net_log_(net_log) {}

    virtual ~TransportConnectJobFactory() {}

    // ClientSocketPoolBase::ConnectJobFactory methods.

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const OVERRIDE;

    virtual base::TimeDelta ConnectionTimeout() const OVERRIDE;

   private:
    ClientSocketFactory* const client_socket_factory_;
    HostResolver* const host_resolver_;
    NetLog* net_log_;

    DISALLOW_COPY_AND_ASSIGN(TransportConnectJobFactory);
  };

  PoolBase base_;

  DISALLOW_COPY_AND_ASSIGN(TransportClientSocketPool);
};

REGISTER_SOCKET_PARAMS_FOR_POOL(TransportClientSocketPool,
                                TransportSocketParams);

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
