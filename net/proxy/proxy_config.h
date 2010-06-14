// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_H_
#define NET_PROXY_PROXY_CONFIG_H_

#include <ostream>
#include <string>
#include <vector>

#include "googleurl/src/gurl.h"
#include "net/proxy/proxy_bypass_rules.h"
#include "net/proxy/proxy_server.h"

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
    // &socks_proxy}, or NULL if it is a scheme that we don't have a mapping
    // for. If the scheme mapping is not present and socks_proxy is defined,
    // we fall back to using socks_proxy.
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

    // Set if the configuration has a SOCKS proxy fallback.
    ProxyServer socks_proxy;

   private:
    // Returns one of {&proxy_for_http, &proxy_for_https, &proxy_for_ftp,
    // &socks_proxy}, or NULL if it is a scheme that we don't have a mapping
    // for. Should only call this if the type is TYPE_PROXY_PER_SCHEME.
    ProxyServer* MapSchemeToProxy(const std::string& scheme);
  };

  typedef int ID;

  // Indicates an invalid proxy config.
  enum { INVALID_ID = 0 };

  ProxyConfig();

  // Used to numerically identify this configuration.
  ID id() const { return id_; }
  void set_id(int id) { id_ = id; }
  bool is_valid() { return id_ != INVALID_ID; }

  // Returns true if the given config is equivalent to this config.
  bool Equals(const ProxyConfig& other) const;

  // Returns true if this config could possibly require the proxy service to
  // use a PAC resolver.
  bool MayRequirePACResolver() const;

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

// Dumps a human-readable string representation of the configuration to |out|;
// used when logging the configuration changes.
std::ostream& operator<<(std::ostream& out, const net::ProxyConfig& config);

// Dumps a human-readable string representation of the |rules| to |out|;
// used for logging and for better unittest failure output.
std::ostream& operator<<(std::ostream& out,
                         const net::ProxyConfig::ProxyRules& rules);

#endif  // NET_PROXY_PROXY_CONFIG_H_
