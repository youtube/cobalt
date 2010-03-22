// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_handle.h"

#include "base/compiler_specific.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_pool.h"

namespace net {

ClientSocketHandle::ClientSocketHandle()
    : socket_(NULL),
      is_reused_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &ClientSocketHandle::OnIOComplete)) {}

ClientSocketHandle::~ClientSocketHandle() {
  Reset();
}

void ClientSocketHandle::Reset() {
  ResetInternal(true);
}

void ClientSocketHandle::ResetInternal(bool cancel) {
  if (group_name_.empty())  // Was Init called?
    return;
  if (socket_.get()) {
    // Because of http://crbug.com/37810 we may not have a pool, but have
    // just a raw socket.
    if (pool_)
      // If we've still got a socket, release it back to the ClientSocketPool so
      // it can be deleted or reused.
      pool_->ReleaseSocket(group_name_, release_socket());
  } else if (cancel) {
    // If we did not get initialized yet, so we've got a socket request pending.
    // Cancel it.
    pool_->CancelRequest(group_name_, this);
  }
  group_name_.clear();
  is_reused_ = false;
  user_callback_ = NULL;
  pool_ = NULL;
  idle_time_ = base::TimeDelta();
  init_time_ = base::TimeTicks();
  setup_time_ = base::TimeDelta();
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
  // TODO(vandebo) remove when bug 31096 is resolved
  CHECK(socket_.get() || result != OK);
  if (result != OK) {
    ResetInternal(false);  // The request failed, so there's nothing to cancel.
    return;
  }
  setup_time_ = base::TimeTicks::Now() - init_time_;

  std::string metric = "Net." + pool_->name() + "SocketType";
  UMA_HISTOGRAM_ENUMERATION(metric, reuse_type(), NUM_TYPES);
  switch (reuse_type()) {
    case ClientSocketHandle::UNUSED:
      metric = "Net." + pool_->name() + "SocketRequestTime";
      UMA_HISTOGRAM_CLIPPED_TIMES(metric, setup_time(),
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromMinutes(10), 100);
      break;
    case ClientSocketHandle::UNUSED_IDLE:
      metric = "Net." + pool_->name() +
          "SocketIdleTimeBeforeNextUse_UnusedSocket";
      UMA_HISTOGRAM_CUSTOM_TIMES(metric, idle_time(),
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(6), 100);
      break;
    case ClientSocketHandle::REUSED_IDLE:
      metric = "Net." + pool_->name() +
          "SocketIdleTimeBeforeNextUse_ReusedSocket";
      UMA_HISTOGRAM_CUSTOM_TIMES(metric, idle_time(),
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(6), 100);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace net
