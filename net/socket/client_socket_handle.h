// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_HANDLE_H_
#define NET_SOCKET_CLIENT_SOCKET_HANDLE_H_
#pragma once

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/stream_socket.h"

namespace net {

// A container for a StreamSocket.
//
// The handle's |group_name| uniquely identifies the origin and type of the
// connection.  It is used by the ClientSocketPool to group similar connected
// client socket objects.
//
class NET_EXPORT ClientSocketHandle {
 public:
  enum SocketReuseType {
    UNUSED = 0,   // unused socket that just finished connecting
    UNUSED_IDLE,  // unused socket that has been idle for awhile
    REUSED_IDLE,  // previously used socket
    NUM_TYPES,
  };

  ClientSocketHandle();
  ~ClientSocketHandle();

  // Initializes a ClientSocketHandle object, which involves talking to the
  // ClientSocketPool to obtain a connected socket, possibly reusing one.  This
  // method returns either OK or ERR_IO_PENDING.  On ERR_IO_PENDING, |priority|
  // is used to determine the placement in ClientSocketPool's wait list.
  //
  // If this method succeeds, then the socket member will be set to an existing
  // connected socket if an existing connected socket was available to reuse,
  // otherwise it will be set to a new connected socket.  Consumers can then
  // call is_reused() to see if the socket was reused.  If not reusing an
  // existing socket, ClientSocketPool may need to establish a new
  // connection using |socket_params|.
  //
  // This method returns ERR_IO_PENDING if it cannot complete synchronously, in
  // which case the consumer will be notified of completion via |callback|.
  //
  // If the pool was not able to reuse an existing socket, the new socket
  // may report a recoverable error.  In this case, the return value will
  // indicate an error and the socket member will be set.  If it is determined
  // that the error is not recoverable, the Disconnect method should be used
  // on the socket, so that it does not get reused.
  //
  // A non-recoverable error may set additional state in the ClientSocketHandle
  // to allow the caller to determine what went wrong.
  //
  // Init may be called multiple times.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  //
  template <typename SocketParams, typename PoolType>
  int Init(const std::string& group_name,
           const scoped_refptr<SocketParams>& socket_params,
           RequestPriority priority,
           OldCompletionCallback* callback,
           PoolType* pool,
           const BoundNetLog& net_log);

  // An initialized handle can be reset, which causes it to return to the
  // un-initialized state.  This releases the underlying socket, which in the
  // case of a socket that still has an established connection, indicates that
  // the socket may be kept alive for use by a subsequent ClientSocketHandle.
  //
  // NOTE: To prevent the socket from being kept alive, be sure to call its
  // Disconnect method.  This will result in the ClientSocketPool deleting the
  // StreamSocket.
  void Reset();

  // Used after Init() is called, but before the ClientSocketPool has
  // initialized the ClientSocketHandle.
  LoadState GetLoadState() const;

  // Returns true when Init() has completed successfully.
  bool is_initialized() const { return is_initialized_; }

  // Returns the time tick when Init() was called.
  base::TimeTicks init_time() const { return init_time_; }

  // Returns the time between Init() and when is_initialized() becomes true.
  base::TimeDelta setup_time() const { return setup_time_; }

  // Used by ClientSocketPool to initialize the ClientSocketHandle.
  void set_is_reused(bool is_reused) { is_reused_ = is_reused; }
  void set_socket(StreamSocket* s) { socket_.reset(s); }
  void set_idle_time(base::TimeDelta idle_time) { idle_time_ = idle_time; }
  void set_pool_id(int id) { pool_id_ = id; }
  void set_is_ssl_error(bool is_ssl_error) { is_ssl_error_ = is_ssl_error; }
  void set_ssl_error_response_info(const HttpResponseInfo& ssl_error_state) {
    ssl_error_response_info_ = ssl_error_state;
  }
  void set_pending_http_proxy_connection(ClientSocketHandle* connection) {
    pending_http_proxy_connection_.reset(connection);
  }

  // Only valid if there is no |socket_|.
  bool is_ssl_error() const {
    DCHECK(socket_.get() == NULL);
    return is_ssl_error_;
  }
  // On an ERR_PROXY_AUTH_REQUESTED error, the |headers| and |auth_challenge|
  // fields are filled in. On an ERR_SSL_CLIENT_AUTH_CERT_NEEDED error,
  // the |cert_request_info| field is set.
  const HttpResponseInfo& ssl_error_response_info() const {
    return ssl_error_response_info_;
  }
  ClientSocketHandle* release_pending_http_proxy_connection() {
    return pending_http_proxy_connection_.release();
  }

  // These may only be used if is_initialized() is true.
  const std::string& group_name() const { return group_name_; }
  int id() const { return pool_id_; }
  StreamSocket* socket() { return socket_.get(); }
  StreamSocket* release_socket() { return socket_.release(); }
  bool is_reused() const { return is_reused_; }
  base::TimeDelta idle_time() const { return idle_time_; }
  SocketReuseType reuse_type() const {
    if (is_reused()) {
      return REUSED_IDLE;
    } else if (idle_time() == base::TimeDelta()) {
      return UNUSED;
    } else {
      return UNUSED_IDLE;
    }
  }

 private:
  // Called on asynchronous completion of an Init() request.
  void OnIOComplete(int result);

  // Called on completion (both asynchronous & synchronous) of an Init()
  // request.
  void HandleInitCompletion(int result);

  // Resets the state of the ClientSocketHandle.  |cancel| indicates whether or
  // not to try to cancel the request with the ClientSocketPool.  Does not
  // reset the supplemental error state.
  void ResetInternal(bool cancel);

  // Resets the supplemental error state.
  void ResetErrorState();

  bool is_initialized_;
  ClientSocketPool* pool_;
  scoped_ptr<StreamSocket> socket_;
  std::string group_name_;
  bool is_reused_;
  OldCompletionCallbackImpl<ClientSocketHandle> callback_;
  OldCompletionCallback* user_callback_;
  base::TimeDelta idle_time_;
  int pool_id_;  // See ClientSocketPool::ReleaseSocket() for an explanation.
  bool is_ssl_error_;
  HttpResponseInfo ssl_error_response_info_;
  scoped_ptr<ClientSocketHandle> pending_http_proxy_connection_;
  base::TimeTicks init_time_;
  base::TimeDelta setup_time_;

  NetLog::Source requesting_source_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketHandle);
};

// Template function implementation:
template <typename SocketParams, typename PoolType>
int ClientSocketHandle::Init(const std::string& group_name,
                             const scoped_refptr<SocketParams>& socket_params,
                             RequestPriority priority,
                             OldCompletionCallback* callback,
                             PoolType* pool,
                             const BoundNetLog& net_log) {
  requesting_source_ = net_log.source();

  CHECK(!group_name.empty());
  // Note that this will result in a compile error if the SocketParams has not
  // been registered for the PoolType via REGISTER_SOCKET_PARAMS_FOR_POOL
  // (defined in client_socket_pool.h).
  CheckIsValidSocketParamsForPool<PoolType, SocketParams>();
  ResetInternal(true);
  ResetErrorState();
  pool_ = pool;
  group_name_ = group_name;
  init_time_ = base::TimeTicks::Now();
  int rv = pool_->RequestSocket(
      group_name, &socket_params, priority, this, &callback_, net_log);
  if (rv == ERR_IO_PENDING) {
    user_callback_ = callback;
  } else {
    HandleInitCompletion(rv);
  }
  return rv;
}

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_HANDLE_H_
