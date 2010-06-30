// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service.h"

#if defined(OS_WIN)
#include "net/base/ssl_config_service_win.h"
#elif defined(OS_MACOSX)
#include "net/base/ssl_config_service_mac.h"
#else
#include "net/base/ssl_config_service_defaults.h"
#endif

namespace net {

// static
SSLConfigService* SSLConfigService::CreateSystemSSLConfigService() {
#if defined(OS_WIN)
  return new SSLConfigServiceWin;
#elif defined(OS_MACOSX)
  return new SSLConfigServiceMac;
#else
  return new SSLConfigServiceDefaults;
#endif
}

// static
bool SSLConfigService::IsKnownStrictTLSServer(const std::string& hostname) {
  // If you wish to add an entry to this list, please contact agl AT chromium
  // DOT org.
  //
  // If this list starts growing, it'll need to be something more efficient
  // than a linear list.
  static const char kStrictServers[][20] = {
      "www.google.com",
      "mail.google.com",
      "www.gmail.com",
      "docs.google.com",
      "clients1.google.com",

      // Removed until we update the XMPP servers with the renegotiation
      // extension.
      // "gmail.com",
  };

  for (size_t i = 0; i < arraysize(kStrictServers); i++) {
    if (strcmp(hostname.c_str(), kStrictServers[i]) == 0)
      return true;
  }

  return false;
}

}  // namespace net
