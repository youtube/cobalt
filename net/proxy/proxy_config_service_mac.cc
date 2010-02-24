// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config_service_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "base/logging.h"
#include "base/mac_util.h"
#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"

namespace {

// Utility function to pull out a boolean value from a dictionary and return it,
// returning a default value if the key is not present.
bool GetBoolFromDictionary(CFDictionaryRef dict,
                           CFStringRef key,
                           bool default_value) {
  CFNumberRef number = (CFNumberRef)mac_util::GetValueFromDictionary(
      dict, key, CFNumberGetTypeID());
  if (!number)
    return default_value;

  int int_value;
  if (CFNumberGetValue(number, kCFNumberIntType, &int_value))
    return int_value;
  else
    return default_value;
}

}  // namespace

namespace net {

int ProxyConfigServiceMac::GetProxyConfig(ProxyConfig* config) {
  scoped_cftyperef<CFDictionaryRef> config_dict(
      SCDynamicStoreCopyProxies(NULL));
  DCHECK(config_dict);

  // auto-detect

  // There appears to be no UI for this configuration option, and we're not sure
  // if Apple's proxy code even takes it into account. But the constant is in
  // the header file so we'll use it.
  config->set_auto_detect(
      GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesProxyAutoDiscoveryEnable,
                            false));

  // PAC file

  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesProxyAutoConfigEnable,
                            false)) {
    CFStringRef pac_url_ref = (CFStringRef)mac_util::GetValueFromDictionary(
        config_dict.get(),
        kSCPropNetProxiesProxyAutoConfigURLString,
        CFStringGetTypeID());
    if (pac_url_ref)
      config->set_pac_url(GURL(base::SysCFStringRefToUTF8(pac_url_ref)));
  }

  // proxies (for now ftp, http, https, and SOCKS)

  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesFTPEnable,
                            false)) {
    ProxyServer proxy_server =
        ProxyServer::FromDictionary(ProxyServer::SCHEME_HTTP,
                                    config_dict.get(),
                                    kSCPropNetProxiesFTPProxy,
                                    kSCPropNetProxiesFTPPort);
    if (proxy_server.is_valid()) {
      config->proxy_rules().type =
          ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      config->proxy_rules().proxy_for_ftp = proxy_server;
    }
  }
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesHTTPEnable,
                            false)) {
    ProxyServer proxy_server =
        ProxyServer::FromDictionary(ProxyServer::SCHEME_HTTP,
                                    config_dict.get(),
                                    kSCPropNetProxiesHTTPProxy,
                                    kSCPropNetProxiesHTTPPort);
    if (proxy_server.is_valid()) {
      config->proxy_rules().type =
          ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      config->proxy_rules().proxy_for_http = proxy_server;
    }
  }
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesHTTPSEnable,
                            false)) {
    ProxyServer proxy_server =
        ProxyServer::FromDictionary(ProxyServer::SCHEME_HTTP,
                                    config_dict.get(),
                                    kSCPropNetProxiesHTTPSProxy,
                                    kSCPropNetProxiesHTTPSPort);
    if (proxy_server.is_valid()) {
      config->proxy_rules().type =
          ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      config->proxy_rules().proxy_for_https = proxy_server;
    }
  }
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesSOCKSEnable,
                            false)) {
    ProxyServer proxy_server =
        ProxyServer::FromDictionary(ProxyServer::SCHEME_SOCKS5,
                                    config_dict.get(),
                                    kSCPropNetProxiesSOCKSProxy,
                                    kSCPropNetProxiesSOCKSPort);
    if (proxy_server.is_valid()) {
      config->proxy_rules().type =
          ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      config->proxy_rules().socks_proxy = proxy_server;
    }
  }

  // proxy bypass list

  CFArrayRef bypass_array_ref =
      (CFArrayRef)mac_util::GetValueFromDictionary(
          config_dict.get(),
          kSCPropNetProxiesExceptionsList,
          CFArrayGetTypeID());
  if (bypass_array_ref) {
    CFIndex bypass_array_count = CFArrayGetCount(bypass_array_ref);
    for (CFIndex i = 0; i < bypass_array_count; ++i) {
      CFStringRef bypass_item_ref =
          (CFStringRef)CFArrayGetValueAtIndex(bypass_array_ref, i);
      if (CFGetTypeID(bypass_item_ref) != CFStringGetTypeID()) {
        LOG(WARNING) << "Expected value for item " << i
                     << " in the kSCPropNetProxiesExceptionsList"
                        " to be a CFStringRef but it was not";

      } else {
        config->proxy_rules().bypass_rules.AddRuleFromString(
            base::SysCFStringRefToUTF8(bypass_item_ref));
      }
    }
  }

  // proxy bypass boolean

  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesExcludeSimpleHostnames,
                            false)) {
    config->proxy_rules().bypass_rules.AddRuleToBypassLocal();
  }

  return OK;
}

}  // namespace net
