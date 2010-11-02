// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_handle.h"

#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_histograms.h"

namespace net {

ClientSocketHandle::ClientSocketHandle()
    : is_initialized_(false),
      is_reused_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &ClientSocketHandle::OnIOComplete)),
      is_ssl_error_(false) {}

ClientSocketHandle::~ClientSocketHandle() {
  Reset();
}

void ClientSocketHandle::Reset() {
  ResetInternal(true);
  ResetErrorState();
}

void ClientSocketHandle::ResetInternal(bool cancel) {
  if (group_name_.empty())  // Was Init called?
    return;
  if (is_initialized()) {
    // Because of http://crbug.com/37810 we may not have a pool, but have
    // just a raw socket.
    socket_->NetLog().EndEvent(NetLog::TYPE_SOCKET_IN_USE, NULL);
    if (pool_)
      // If we've still got a socket, release it back to the ClientSocketPool so
      // it can be deleted or reused.
      pool_->ReleaseSocket(group_name_, release_socket(), pool_id_);
  } else if (cancel) {
    // If we did not get initialized yet, we've got a socket request pending.
    // Cancel it.
    pool_->CancelRequest(group_name_, this);
  }
  is_initialized_ = false;
  group_name_.clear();
  is_reused_ = false;
  user_callback_ = NULL;
  pool_ = NULL;
  idle_time_ = base::TimeDelta();
  init_time_ = base::TimeTicks();
  setup_time_ = base::TimeDelta();
  pool_id_ = -1;
}

void ClientSocketHandle::ResetErrorState() {
  is_ssl_error_ = false;
  ssl_error_response_info_ = HttpResponseInfo();
  pending_http_proxy_connection_.reset();
}

LoadState ClientSocketHandle::GetLoadState() const {
  CHECK(!is_initialized());
  CHECK(!group_name_.empty());
  // Because of http://crbug.com/37810  we may not have a pool, but have
  // just a raw socket.
  if (!pool_)
    return LOAD_STATE_IDLE;
  return pool_->GetLoadState(group_name_, this);
}

void ClientSocketHandle::OnIOComplete(int result) {
  CompletionCallback* callback = user_callback_;
  user_callback_ = NULL;
  HandleInitCompletion(result);
  callback->Run(result);
}

void ClientSocketHandle::HandleInitCompletion(int result) {
  CHECK_NE(ERR_IO_PENDING, result);
  if (result != OK) {
    if (!socket_.get())
      ResetInternal(false);  // Nothing to cancel since the request failed.
    else
      is_initialized_ = true;
    return;
  }
  is_initialized_ = true;
  CHECK_NE(-1, pool_id_) << "Pool should have set |pool_id_| to a valid value.";
  setup_time_ = base::TimeTicks::Now() - init_time_;

  ClientSocketPoolHistograms* histograms = pool_->histograms();
  histograms->AddSocketType(reuse_type());
  switch (reuse_type()) {
    case ClientSocketHandle::UNUSED:
      histograms->AddRequestTime(setup_time());
      break;
    case ClientSocketHandle::UNUSED_IDLE:
      histograms->AddUnusedIdleTime(idle_time());
      break;
    case ClientSocketHandle::REUSED_IDLE:
      histograms->AddReusedIdleTime(idle_time());
      break;
    default:
      NOTREACHED();
      break;
  }

  // Broadcast that the socket has been acquired.
  // TODO(eroman): This logging is not complete, in particular set_socket() and
  // release() socket. It ends up working though, since those methods are being
  // used to layer sockets (and the destination sources are the same).
  DCHECK(socket_.get());
  socket_->NetLog().BeginEvent(
      NetLog::TYPE_SOCKET_IN_USE,
      make_scoped_refptr(new NetLogSourceParameter(
          "source_dependency", requesting_source_)));
}

}  // namespace net
