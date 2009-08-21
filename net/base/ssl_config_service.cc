// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service.h"

#if defined(OS_WIN)
#include "net/base/ssl_config_service_win.h"
#else
#include "net/base/ssl_config_service_defaults.h"
#endif

namespace net {

// static
SSLConfigService* SSLConfigService::CreateSystemSSLConfigService() {
#if defined(OS_WIN)
  return new SSLConfigServiceWin;
#else
  return new SSLConfigServiceDefaults;
#endif
}

}  // namespace net
