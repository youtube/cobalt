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
  static const char kStrictServers[][22] = {
      "www.google.com",
      "mail.google.com",
      "www.gmail.com",
      "docs.google.com",
      "clients1.google.com",
      "sunshinepress.org",
      "www.sunshinepress.org",

      // Removed until we update the XMPP servers with the renegotiation
      // extension.
      // "gmail.com",
  };

  for (size_t i = 0; i < arraysize(kStrictServers); i++) {
    // Note that the hostname is normalised to lower-case by this point.
    if (strcmp(hostname.c_str(), kStrictServers[i]) == 0)
      return true;
  }

  return false;
}

// static
bool SSLConfigService::IsKnownFalseStartIncompatibleServer(
    const std::string& hostname) {
  // If this list starts growing, it'll need to be something more efficient
  // than a linear list.
  static const char kFalseStartIncompatibleServers[][15] = {
      "www.picnik.com",
  };

  static const char kFalseStartIncompatibleDomains[][11] = {
      // Added at the request of A10.
      "yodlee.com",
  };

  // Note that the hostname is normalised to lower-case by this point.
  for (size_t i = 0; i < arraysize(kFalseStartIncompatibleServers); i++) {
    if (strcmp(hostname.c_str(), kFalseStartIncompatibleServers[i]) == 0)
      return true;
  }

  for (size_t i = 0; i < arraysize(kFalseStartIncompatibleDomains); i++) {
    const char* domain = kFalseStartIncompatibleDomains[i];
    const size_t len = strlen(domain);
    if (hostname.size() >= len &&
        memcmp(&hostname[hostname.size() - len], domain, len) == 0 &&
        (hostname.size() == len ||
         hostname[hostname.size() - len - 1] == '.')) {
      return true;
    }
  }

  return false;
}

static bool g_dnssec_enabled = false;
static bool g_false_start_enabled = true;
static bool g_mitm_proxies_allowed = false;

// static
void SSLConfigService::SetSSLConfigFlags(SSLConfig* ssl_config) {
  ssl_config->dnssec_enabled = g_dnssec_enabled;
  ssl_config->false_start_enabled = g_false_start_enabled;
  ssl_config->mitm_proxies_allowed = g_mitm_proxies_allowed;
}

// static
void SSLConfigService::EnableDNSSEC() {
  g_dnssec_enabled = true;
}

// static
bool SSLConfigService::dnssec_enabled() {
  return g_dnssec_enabled;
}

// static
void SSLConfigService::DisableFalseStart() {
  g_false_start_enabled = false;
}

// static
bool SSLConfigService::false_start_enabled() {
  return g_false_start_enabled;
}

// static
void SSLConfigService::AllowMITMProxies() {
  g_mitm_proxies_allowed = true;
}

// static
bool SSLConfigService::mitm_proxies_allowed() {
  return g_mitm_proxies_allowed;
}

}  // namespace net
