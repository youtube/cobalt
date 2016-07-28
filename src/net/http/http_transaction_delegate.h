// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_DELEGATE_H_
#define NET_HTTP_HTTP_TRANSACTION_DELEGATE_H_

namespace net {

// Delegate class receiving notifications when cache or network actions start
// and finish, i.e. when the object starts and finishes waiting on an
// underlying cache or the network.  The owner of a HttpTransaction can use
// this to register a delegate to receive notifications when these events
// happen.
class HttpTransactionDelegate {
 public:
  virtual ~HttpTransactionDelegate() {}
  virtual void OnCacheActionStart() = 0;
  virtual void OnCacheActionFinish() = 0;
  virtual void OnNetworkActionStart() = 0;
  virtual void OnNetworkActionFinish() = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_DELEGATE_H_
