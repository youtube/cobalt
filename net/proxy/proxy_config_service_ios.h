// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_IOS_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_IOS_H_

#include "net/proxy/polling_proxy_config_service.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {

class ProxyConfigServiceIOS : public PollingProxyConfigService {
 public:
  // Constructs a ProxyConfigService that watches the iOS system proxy settings.
  // This instance is expected to be operated and deleted on the IO thread
  // (however it may be constructed from a different thread).
  explicit ProxyConfigServiceIOS(
      base::SingleThreadTaskRunner* io_thread_task_runner);
  virtual ~ProxyConfigServiceIOS();

 private:
  // The thread that we expect to be operated on.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceIOS);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_IOS_H_
