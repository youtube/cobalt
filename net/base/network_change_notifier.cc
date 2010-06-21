// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "net/base/network_change_notifier_win.h"
#elif defined(OS_LINUX)
#include "net/base/network_change_notifier_linux.h"
#elif defined(OS_MACOSX)
#include "net/base/network_change_notifier_mac.h"
#endif

namespace net {

// static
NetworkChangeNotifier*
NetworkChangeNotifier::CreateDefaultNetworkChangeNotifier(NetLog* net_log) {
#if defined(OS_WIN)
  return new NetworkChangeNotifierWin(net_log);
#elif defined(OS_LINUX)
  return new NetworkChangeNotifierLinux(net_log);
#elif defined(OS_MACOSX)
  return new NetworkChangeNotifierMac(net_log);
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

}  // namespace net
