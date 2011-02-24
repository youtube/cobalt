// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_DELEGATE_H_
#define NET_BASE_NETWORK_DELEGATE_H_
#pragma once

#include "base/threading/non_thread_safe.h"

namespace net {

// NOTE: Layering violations!
// We decided to accept these violations (depending
// on other net/ submodules from net/base/), because otherwise NetworkDelegate
// would have to be broken up into too many smaller interfaces targeted to each
// submodule. Also, since the lower levels in net/ may callback into higher
// levels, we may encounter dangerous casting issues.
//
// NOTE: It is not okay to add any compile-time dependencies on symbols outside
// of net/base here, because we have a net_base library. Forward declarations
// are ok.
class HttpRequestHeaders;
class URLRequest;

class NetworkDelegate : public base::NonThreadSafe {
 public:
  virtual ~NetworkDelegate() {}

  // Notification interface called by the network stack. Note that these
  // functions mostly forward to the private virtuals. They also add some sanity
  // checking on parameters.
  void NotifyBeforeURLRequest(URLRequest* request);
  void NotifySendHttpRequest(HttpRequestHeaders* headers);
  void NotifyResponseStarted(URLRequest* request);
  void NotifyReadCompleted(URLRequest* request, int bytes_read);

 private:
  // This is the interface for subclasses of NetworkDelegate to implement. This
  // member functions will be called by the respective public notification
  // member function, which will perform basic sanity checking.

  // Called before a request is sent.
  virtual void OnBeforeURLRequest(URLRequest* request) = 0;

  // Called right before the HTTP headers are sent.  Allows the delegate to
  // read/write |headers| before they get sent out.
  virtual void OnSendHttpRequest(HttpRequestHeaders* headers) = 0;

  // This corresponds to URLRequestDelegate::OnResponseStarted.
  virtual void OnResponseStarted(URLRequest* request) = 0;

  // This corresponds to URLRequestDelegate::OnReadCompleted.
  virtual void OnReadCompleted(URLRequest* request, int bytes_read) = 0;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_DELEGATE_H_
