// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_H_
#define NET_PROXY_PROXY_CONFIG_H_
#pragma once

#include <string>

#include "googleurl/src/gurl.h"
#include "net/proxy/proxy_bypass_rules.h"
#include "net/proxy/proxy_server.h"

class Value;

namespace net {

class ProxyInfo;

// ProxyConfig describes a user's proxy settings.
//
// There are two categories of proxy settings:
//   (1) Automatic (indicates the methods to obtain a PAC script)
//   (2) Manual (simple set of proxy servers per scheme, and bypass patterns)
//
// When both automatic and manual settings are specified, the Automatic ones
// take precedence over the manual ones.
//
// For more details see:
// http://www.chromium.org/developers/design-documents/proxy-settings-fallback
class ProxyConfig {
 public:
  // ProxyRules describes the "manual" proxy settings.
  // TODO(eroman): Turn this into a class.
  struct ProxyRules {
    enum Type {
      TYPE_NO_RULES,
      TYPE_SINGLE_PROXY,
      TYPE_PROXY_PER_SCHEME,
    };

    // Note that the default of TYPE_NO_RULES results in direct connections
    // being made when using this ProxyConfig.
    ProxyRules() : reverse_bypass(false), type(TYPE_NO_RULES) {}

    bool empty() const {
      return type == TYPE_NO_RULES;
    }

    // Sets |result| with the proxy to use for |url| based on the current rules.
    void Apply(const GURL& url, ProxyInfo* result);

    // Parses the rules from a string, indicating which proxies to use.
    //
    //   proxy-uri = [<proxy-scheme>"://"]<proxy-host>[":"<proxy-port>]
    //
    // If the proxy to use depends on the scheme of the URL, can instead specify
    // a semicolon separated list of:
    //
    //   <url-scheme>"="<proxy-uri>
    //
    // For example:
    //   "http=foopy:80;ftp=foopy2"  -- use HTTP proxy "foopy:80" for http://
    //                                  URLs, and HTTP proxy "foopy2:80" for
    //                                  ftp:// URLs.
    //   "foopy:80"                  -- use HTTP proxy "foopy:80" for all URLs.
    //   "socks4://foopy"            -- use SOCKS v4 proxy "foopy:1080" for all
    //                                  URLs.
    void ParseFromString(const std::string& proxy_rules);

    // Returns one of {&proxy_for_http, &proxy_for_https, &proxy_for_ftp,
    // &fallback_proxy}, or NULL if there is no proxy to use.
    // Should only call this if the type is TYPE_PROXY_PER_SCHEME.
    const ProxyServer* MapUrlSchemeToProxy(const std::string& url_scheme) const;

    // Returns true if |*this| describes the same configuration as |other|.
    bool Equals(const ProxyRules& other) const;

    // Exceptions for when not to use a proxy.
    ProxyBypassRules bypass_rules;

    // Reverse the meaning of |bypass_rules|.
    bool reverse_bypass;

    Type type;

    // Set if |type| is TYPE_SINGLE_PROXY.
    ProxyServer single_proxy;

    // Set if |type| is TYPE_PROXY_PER_SCHEME.
    ProxyServer proxy_for_http;
    ProxyServer proxy_for_https;
    ProxyServer proxy_for_ftp;

    // Used when there isn't a more specific per-scheme proxy server.
    ProxyServer fallback_proxy;

   private:
    // Returns one of {&proxy_for_http, &proxy_for_https, &proxy_for_ftp}
    // or NULL if it is a scheme that we don't have a mapping
    // for. Should only call this if the type is TYPE_PROXY_PER_SCHEME.
    ProxyServer* MapUrlSchemeToProxyNoFallback(const std::string& scheme);
  };

  typedef int ID;

  // Indicates an invalid proxy config.
  enum { INVALID_ID = 0 };

  ProxyConfig();

  // Used to numerically identify this configuration.
  ID id() const { return id_; }
  void set_id(int id) { id_ = id; }
  bool is_valid() const { return id_ != INVALID_ID; }

  // Returns true if the given config is equivalent to this config.
  bool Equals(const ProxyConfig& other) const;

  // Returns true if this config contains any "automatic" settings. See the
  // class description for what that means.
  bool HasAutomaticSettings() const;

  void ClearAutomaticSettings();

  // Creates a Value dump of this configuration. The caller is responsible for
  // deleting the returned value.
  Value* ToValue() const;

  ProxyRules& proxy_rules() {
    return proxy_rules_;
  }

  const ProxyRules& proxy_rules() const {
    return proxy_rules_;
  }

  void set_pac_url(const GURL& url) {
    pac_url_ = url;
  }

  const GURL& pac_url() const {
    return pac_url_;
  }

  bool has_pac_url() const {
    return pac_url_.is_valid();
  }

  void set_auto_detect(bool enable_auto_detect) {
    auto_detect_ = enable_auto_detect;
  }

  bool auto_detect() const {
    return auto_detect_;
  }

  // Helpers to construct some common proxy configurations.

  static ProxyConfig CreateDirect() {
    return ProxyConfig();
  }

  static ProxyConfig CreateAutoDetect() {
    ProxyConfig config;
    config.set_auto_detect(true);
    return config;
  }

  static ProxyConfig CreateFromCustomPacURL(const GURL& pac_url) {
    ProxyConfig config;
    config.set_pac_url(pac_url);
    return config;
  }

 private:
  // True if the proxy configuration should be auto-detected.
  bool auto_detect_;

  // If non-empty, indicates the URL of the proxy auto-config file to use.
  GURL pac_url_;

  // Manual proxy settings.
  ProxyRules proxy_rules_;

  int id_;
};

}  // namespace net



#endif  // NET_PROXY_PROXY_CONFIG_H_
