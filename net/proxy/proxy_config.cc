// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config.h"

#include "base/string_tokenizer.h"
#include "base/string_util.h"

namespace net {

ProxyConfig::ProxyConfig()
    : auto_detect(false),
      id_(INVALID_ID) {
}

bool ProxyConfig::Equals(const ProxyConfig& other) const {
  // The two configs can have different IDs.  We are just interested in if they
  // have the same settings.
  return auto_detect == other.auto_detect &&
         pac_url == other.pac_url &&
         proxy_rules == other.proxy_rules &&
         bypass_rules.Equals(other.bypass_rules);
}

bool ProxyConfig::MayRequirePACResolver() const {
  return auto_detect || pac_url.is_valid();
}

void ProxyConfig::ProxyRules::ParseFromString(const std::string& proxy_rules) {
  // Reset.
  type = TYPE_NO_RULES;
  single_proxy = ProxyServer();
  proxy_for_http = ProxyServer();
  proxy_for_https = ProxyServer();
  proxy_for_ftp = ProxyServer();
  socks_proxy = ProxyServer();

  StringTokenizer proxy_server_list(proxy_rules, ";");
  while (proxy_server_list.GetNext()) {
    StringTokenizer proxy_server_for_scheme(
        proxy_server_list.token_begin(), proxy_server_list.token_end(), "=");

    while (proxy_server_for_scheme.GetNext()) {
      std::string url_scheme = proxy_server_for_scheme.token();

      // If we fail to get the proxy server here, it means that
      // this is a regular proxy server configuration, i.e. proxies
      // are not configured per protocol.
      if (!proxy_server_for_scheme.GetNext()) {
        if (type == TYPE_PROXY_PER_SCHEME)
          continue;  // Unexpected.
        single_proxy = ProxyServer::FromURI(url_scheme,
                                            ProxyServer::SCHEME_HTTP);
        type = TYPE_SINGLE_PROXY;
        return;
      }

      // Trim whitespace off the url scheme.
      TrimWhitespaceASCII(url_scheme, TRIM_ALL, &url_scheme);

      // Add it to the per-scheme mappings (if supported scheme).
      type = TYPE_PROXY_PER_SCHEME;
      if (ProxyServer* entry = MapSchemeToProxy(url_scheme)) {
        std::string proxy_server_token = proxy_server_for_scheme.token();
        ProxyServer::Scheme scheme = (entry == &socks_proxy) ?
            ProxyServer::SCHEME_SOCKS4 : ProxyServer::SCHEME_HTTP;
        *entry = ProxyServer::FromURI(proxy_server_token, scheme);
      }
    }
  }
}

const ProxyServer* ProxyConfig::ProxyRules::MapUrlSchemeToProxy(
    const std::string& url_scheme) const {
  const ProxyServer* proxy_server =
      const_cast<ProxyRules*>(this)->MapSchemeToProxy(url_scheme);
  if (proxy_server && proxy_server->is_valid())
    return proxy_server;
  if (socks_proxy.is_valid())
    return &socks_proxy;
  return NULL;  // No mapping for this scheme. Use direct.
}

ProxyServer* ProxyConfig::ProxyRules::MapSchemeToProxy(
    const std::string& scheme) {
  DCHECK(type == TYPE_PROXY_PER_SCHEME);
  if (scheme == "http")
    return &proxy_for_http;
  if (scheme == "https")
    return &proxy_for_https;
  if (scheme == "ftp")
    return &proxy_for_ftp;
  if (scheme == "socks")
    return &socks_proxy;
  return NULL;  // No mapping for this scheme.
}

}  // namespace net

namespace {

// Helper to stringize a ProxyServer.
std::ostream& operator<<(std::ostream& out,
                         const net::ProxyServer& proxy_server) {
  if (proxy_server.is_valid())
    out << proxy_server.ToURI();
  return out;
}

const char* BoolToYesNoString(bool b) {
  return b ? "Yes" : "No";
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         const net::ProxyConfig::ProxyRules& rules) {
  // Stringize the type enum.
  std::string type;
  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::TYPE_NO_RULES:
      type = "TYPE_NO_RULES";
      break;
    case net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME:
      type = "TYPE_PROXY_PER_SCHEME";
      break;
    case net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY:
      type = "TYPE_SINGLE_PROXY";
      break;
    default:
      type = IntToString(rules.type);
      break;
  }
  return out << "  {\n"
             << "    type: " << type << "\n"
             << "    single_proxy: " << rules.single_proxy << "\n"
             << "    proxy_for_http: " << rules.proxy_for_http << "\n"
             << "    proxy_for_https: " << rules.proxy_for_https << "\n"
             << "    proxy_for_ftp: " << rules.proxy_for_ftp << "\n"
             << "    socks_proxy: " << rules.socks_proxy << "\n"
             << "  }";
}

std::ostream& operator<<(std::ostream& out, const net::ProxyConfig& config) {
  // "Automatic" settings.
  out << "Automatic settings:\n";
  out << "  Auto-detect: " << BoolToYesNoString(config.auto_detect) << "\n";
  out << "  Custom PAC script: ";
  if (config.pac_url.is_valid())
    out << config.pac_url;
  else
    out << "[None]";
  out << "\n";

  // "Manual" settings.
  out << "Manual settings:\n";
  out << "  Proxy server: ";

  switch (config.proxy_rules.type) {
    case net::ProxyConfig::ProxyRules::TYPE_NO_RULES:
      out << "[None]\n";
      break;
    case net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY:
      out << config.proxy_rules.single_proxy;
      out << "\n";
      break;
    case net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME:
      out << "\n";
      if (config.proxy_rules.proxy_for_http.is_valid())
        out << "    HTTP: " << config.proxy_rules.proxy_for_http << "\n";
      if (config.proxy_rules.proxy_for_https.is_valid())
        out << "    HTTPS: " << config.proxy_rules.proxy_for_https << "\n";
      if (config.proxy_rules.proxy_for_ftp.is_valid())
        out << "    FTP: " << config.proxy_rules.proxy_for_ftp << "\n";
      if (config.proxy_rules.socks_proxy.is_valid())
        out << "    SOCKS: " << config.proxy_rules.socks_proxy << "\n";
      break;
  }

  out << "  Bypass list: ";
  if (config.bypass_rules.rules().empty()) {
    out << "[None]";
  } else {
    net::ProxyBypassRules::RuleList::const_iterator it;
    for (it = config.bypass_rules.rules().begin();
         it != config.bypass_rules.rules().end(); ++it) {
      out << "\n    " << (*it)->ToString();
    }
  }

  return out;
}
