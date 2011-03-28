// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_H_
#pragma once

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "base/template_util.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/load_states.h"
#include "net/base/request_priority.h"

class DictionaryValue;

namespace net {

class ClientSocket;
class ClientSocketHandle;
class ClientSocketPoolHistograms;

// A ClientSocketPool is used to restrict the number of sockets open at a time.
// It also maintains a list of idle persistent sockets.
//
class ClientSocketPool {
 public:
  // Requests a connected socket for a group_name.
  //
  // There are five possible results from calling this function:
  // 1) RequestSocket returns OK and initializes |handle| with a reused socket.
  // 2) RequestSocket returns OK with a newly connected socket.
  // 3) RequestSocket returns ERR_IO_PENDING.  The handle will be added to a
  // wait list until a socket is available to reuse or a new socket finishes
  // connecting.  |priority| will determine the placement into the wait list.
  // 4) An error occurred early on, so RequestSocket returns an error code.
  // 5) A recoverable error occurred while setting up the socket.  An error
  // code is returned, but the |handle| is initialized with the new socket.
  // The caller must recover from the error before using the connection, or
  // Disconnect the socket before releasing or resetting the |handle|.
  // The current recoverable errors are: the errors accepted by
  // IsCertificateError(err) and PROXY_AUTH_REQUESTED, or
  // HTTPS_PROXY_TUNNEL_RESPONSE when reported by HttpProxyClientSocketPool.
  //
  // If this function returns OK, then |handle| is initialized upon return.
  // The |handle|'s is_initialized method will return true in this case.  If a
  // ClientSocket was reused, then ClientSocketPool will call
  // |handle|->set_reused(true).  In either case, the socket will have been
  // allocated and will be connected.  A client might want to know whether or
  // not the socket is reused in order to request a new socket if he encounters
  // an error with the reused socket.
  //
  // If ERR_IO_PENDING is returned, then the callback will be used to notify the
  // client of completion.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  virtual int RequestSocket(const std::string& group_name,
                            const void* params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback,
                            const BoundNetLog& net_log) = 0;

  // RequestSockets is used to request that |num_sockets| be connected in the
  // connection group for |group_name|.  If the connection group already has
  // |num_sockets| idle sockets / active sockets / currently connecting sockets,
  // then this function doesn't do anything.  Otherwise, it will start up as
  // many connections as necessary to reach |num_sockets| total sockets for the
  // group.  It uses |params| to control how to connect the sockets.   The
  // ClientSocketPool will assign a priority to the new connections, if any.
  // This priority will probably be lower than all others, since this method
  // is intended to make sure ahead of time that |num_sockets| sockets are
  // available to talk to a host.
  virtual void RequestSockets(const std::string& group_name,
                              const void* params,
                              int num_sockets,
                              const BoundNetLog& net_log) = 0;

  // Called to cancel a RequestSocket call that returned ERR_IO_PENDING.  The
  // same handle parameter must be passed to this method as was passed to the
  // RequestSocket call being cancelled.  The associated CompletionCallback is
  // not run.  However, for performance, we will let one ConnectJob complete
  // and go idle.
  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) = 0;

  // Called to release a socket once the socket is no longer needed.  If the
  // socket still has an established connection, then it will be added to the
  // set of idle sockets to be used to satisfy future RequestSocket calls.
  // Otherwise, the ClientSocket is destroyed.  |id| is used to differentiate
  // between updated versions of the same pool instance.  The pool's id will
  // change when it flushes, so it can use this |id| to discard sockets with
  // mismatched ids.
  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket,
                             int id) = 0;

  // This flushes all state from the ClientSocketPool.  This means that all
  // idle and connecting sockets are discarded.  Active sockets being
  // held by ClientSocketPool clients will be discarded when released back to
  // the pool.  Does not flush any pools wrapped by |this|.
  virtual void Flush() = 0;

  // Called to close any idle connections held by the connection manager.
  virtual void CloseIdleSockets() = 0;

  // The total number of idle sockets in the pool.
  virtual int IdleSocketCount() const = 0;

  // The total number of idle sockets in a connection group.
  virtual int IdleSocketCountInGroup(const std::string& group_name) const = 0;

  // Determine the LoadState of a connecting ClientSocketHandle.
  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const = 0;

  // Retrieves information on the current state of the pool as a
  // DictionaryValue.  Caller takes possession of the returned value.
  // If |include_nested_pools| is true, the states of any nested
  // ClientSocketPools will be included.
  virtual DictionaryValue* GetInfoAsValue(const std::string& name,
                                          const std::string& type,
                                          bool include_nested_pools) const = 0;

  // Returns the maximum amount of time to wait before retrying a connect.
  static const int kMaxConnectRetryIntervalMs = 250;

  // The set of histograms specific to this pool.  We can't use the standard
  // UMA_HISTOGRAM_* macros because they are callsite static.
  virtual ClientSocketPoolHistograms* histograms() const = 0;

  static int unused_idle_socket_timeout();
  static void set_unused_idle_socket_timeout(int timeout);

 protected:
  ClientSocketPool();
  virtual ~ClientSocketPool();

  // Return the connection timeout for this pool.
  virtual base::TimeDelta ConnectionTimeout() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientSocketPool);
};

// ClientSocketPool subclasses should indicate valid SocketParams via the
// REGISTER_SOCKET_PARAMS_FOR_POOL macro below.  By default, any given
// <PoolType,SocketParams> pair will have its SocketParamsTrait inherit from
// base::false_type, but REGISTER_SOCKET_PARAMS_FOR_POOL will specialize that
// pairing to inherit from base::true_type.  This provides compile time
// verification that the correct SocketParams type is used with the appropriate
// PoolType.
template <typename PoolType, typename SocketParams>
struct SocketParamTraits : public base::false_type {
};

template <typename PoolType, typename SocketParams>
void CheckIsValidSocketParamsForPool() {
  COMPILE_ASSERT(!base::is_pointer<scoped_refptr<SocketParams> >::value,
                 socket_params_cannot_be_pointer);
  COMPILE_ASSERT((SocketParamTraits<PoolType,
                                    scoped_refptr<SocketParams> >::value),
                 invalid_socket_params_for_pool);
}

// Provides an empty definition for CheckIsValidSocketParamsForPool() which
// should be optimized out by the compiler.
#define REGISTER_SOCKET_PARAMS_FOR_POOL(pool_type, socket_params)             \
template<>                                                                    \
struct SocketParamTraits<pool_type, scoped_refptr<socket_params> >            \
    : public base::true_type {                                                \
}

template <typename PoolType, typename SocketParams>
void RequestSocketsForPool(PoolType* pool,
                           const std::string& group_name,
                           const scoped_refptr<SocketParams>& params,
                           int num_sockets,
                           const BoundNetLog& net_log) {
  CheckIsValidSocketParamsForPool<PoolType, SocketParams>();
  pool->RequestSockets(group_name, &params, num_sockets, net_log);
}

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_H_
