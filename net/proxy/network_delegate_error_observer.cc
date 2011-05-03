// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/proxy/network_delegate_error_observer.h"

namespace net {

// NetworkDelegateErrorObserver::Core -----------------------------------------

class NetworkDelegateErrorObserver::Core
    : public base::RefCountedThreadSafe<NetworkDelegateErrorObserver::Core> {
 public:
  Core(NetworkDelegate* network_delegate, MessageLoop* io_loop);

  void NotifyPACScriptError(int line_number, const string16& error);

  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<NetworkDelegateErrorObserver::Core>;

  virtual ~Core();

  NetworkDelegate* network_delegate_;
  MessageLoop* const io_loop_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NetworkDelegateErrorObserver::Core::Core(NetworkDelegate* network_delegate,
                                         MessageLoop* io_loop)
  : network_delegate_(network_delegate),
    io_loop_(io_loop) {
  DCHECK(io_loop_);
}

NetworkDelegateErrorObserver::Core::~Core() {}


void NetworkDelegateErrorObserver::Core::NotifyPACScriptError(
    int line_number,
    const string16& error) {
  if (MessageLoop::current() != io_loop_) {
    io_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &Core::NotifyPACScriptError,
                          line_number, error));
    return;
  }
  if (network_delegate_)
    network_delegate_->NotifyPACScriptError(line_number, error);
}

void NetworkDelegateErrorObserver::Core::Shutdown() {
  CHECK_EQ(MessageLoop::current(), io_loop_);
  network_delegate_ = NULL;
}

// NetworkDelegateErrorObserver -----------------------------------------------

NetworkDelegateErrorObserver::NetworkDelegateErrorObserver(
    NetworkDelegate* network_delegate,
    MessageLoop* io_loop)
    : core_(new Core(network_delegate, io_loop)) {}

NetworkDelegateErrorObserver::~NetworkDelegateErrorObserver() {
  core_->Shutdown();
}

void NetworkDelegateErrorObserver::OnPACScriptError(int line_number,
                                                    const string16& error) {
  core_->NotifyPACScriptError(line_number, error);
}

}  // namespace net
