// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config.h"

#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "net/proxy/proxy_info.h"

namespace net {

namespace {

// If |proxy| is valid, sets it in |dict| under the key |name|.
void AddProxyToValue(const char* name,
                     const ProxyServer& proxy,
                     DictionaryValue* dict) {
  if (proxy.is_valid())
    dict->SetString(name, proxy.ToURI());
}

}  // namespace

ProxyConfig::ProxyRules::ProxyRules()
    : reverse_bypass(false),
      type(TYPE_NO_RULES) {
}

ProxyConfig::ProxyRules::~ProxyRules() {
}

void ProxyConfig::ProxyRules::Apply(const GURL& url, ProxyInfo* result) {
  if (empty()) {
    result->UseDirect();
    return;
  }

  bool bypass_proxy = bypass_rules.Matches(url);
  if (reverse_bypass)
    bypass_proxy = !bypass_proxy;
  if (bypass_proxy) {
    result->UseDirect();
    return;
  }

  switch (type) {
    case ProxyRules::TYPE_SINGLE_PROXY: {
      result->UseProxyServer(single_proxy);
      return;
    }
    case ProxyRules::TYPE_PROXY_PER_SCHEME: {
      const ProxyServer* entry = MapUrlSchemeToProxy(url.scheme());
      if (entry) {
        result->UseProxyServer(*entry);
      } else {
        // We failed to find a matching proxy server for the current URL
        // scheme. Default to direct.
        result->UseDirect();
      }
      return;
    }
    default: {
      result->UseDirect();
      NOTREACHED();
      return;
    }
  }
}

void ProxyConfig::ProxyRules::ParseFromString(const std::string& proxy_rules) {
  // Reset.
  type = TYPE_NO_RULES;
  single_proxy = ProxyServer();
  proxy_for_http = ProxyServer();
  proxy_for_https = ProxyServer();
  proxy_for_ftp = ProxyServer();
  fallback_proxy = ProxyServer();

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
      ProxyServer* entry = MapUrlSchemeToProxyNoFallback(url_scheme);
      ProxyServer::Scheme default_scheme = ProxyServer::SCHEME_HTTP;

      // socks=XXX is inconsistent with the other formats, since "socks"
      // is not a URL scheme. Rather this means "for everything else, send
      // it to the SOCKS proxy server XXX".
      if (url_scheme == "socks") {
        DCHECK(!entry);
        entry = &fallback_proxy;
        default_scheme = ProxyServer::SCHEME_SOCKS4;
      }

      if (entry) {
        *entry = ProxyServer::FromURI(proxy_server_for_scheme.token(),
                                      default_scheme);
      }
    }
  }
}

const ProxyServer* ProxyConfig::ProxyRules::MapUrlSchemeToProxy(
    const std::string& url_scheme) const {
  const ProxyServer* proxy_server =
      const_cast<ProxyRules*>(this)->MapUrlSchemeToProxyNoFallback(url_scheme);
  if (proxy_server && proxy_server->is_valid())
    return proxy_server;
  if (fallback_proxy.is_valid())
    return &fallback_proxy;
  return NULL;  // No mapping for this scheme. Use direct.
}

bool ProxyConfig::ProxyRules::Equals(const ProxyRules& other) const {
  return type == other.type &&
         single_proxy == other.single_proxy &&
         proxy_for_http == other.proxy_for_http &&
         proxy_for_https == other.proxy_for_https &&
         proxy_for_ftp == other.proxy_for_ftp &&
         fallback_proxy == other.fallback_proxy &&
         bypass_rules.Equals(other.bypass_rules) &&
         reverse_bypass == other.reverse_bypass;
}

ProxyServer* ProxyConfig::ProxyRules::MapUrlSchemeToProxyNoFallback(
    const std::string& scheme) {
  DCHECK_EQ(TYPE_PROXY_PER_SCHEME, type);
  if (scheme == "http")
    return &proxy_for_http;
  if (scheme == "https")
    return &proxy_for_https;
  if (scheme == "ftp")
    return &proxy_for_ftp;
  return NULL;  // No mapping for this scheme.
}

ProxyConfig::ProxyConfig() : auto_detect_(false), id_(INVALID_ID) {
}

ProxyConfig::ProxyConfig(const ProxyConfig& config)
    : auto_detect_(config.auto_detect_),
      pac_url_(config.pac_url_),
      proxy_rules_(config.proxy_rules_),
      id_(config.id_) {
}

ProxyConfig::~ProxyConfig() {
}

ProxyConfig& ProxyConfig::operator=(const ProxyConfig& config) {
  auto_detect_ = config.auto_detect_;
  pac_url_ = config.pac_url_;
  proxy_rules_ = config.proxy_rules_;
  id_ = config.id_;
  return *this;
}

bool ProxyConfig::Equals(const ProxyConfig& other) const {
  // The two configs can have different IDs.  We are just interested in if they
  // have the same settings.
  return auto_detect_ == other.auto_detect_ &&
         pac_url_ == other.pac_url_ &&
         proxy_rules_.Equals(other.proxy_rules());
}

bool ProxyConfig::HasAutomaticSettings() const {
  return auto_detect_ || has_pac_url();
}

void ProxyConfig::ClearAutomaticSettings() {
  auto_detect_ = false;
  pac_url_ = GURL();
}

Value* ProxyConfig::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();

  // Output the automatic settings.
  if (auto_detect_)
    dict->SetBoolean("auto_detect", auto_detect_);
  if (has_pac_url())
    dict->SetString("pac_url", pac_url_.possibly_invalid_spec());

  // Output the manual settings.
  if (proxy_rules_.type != ProxyRules::TYPE_NO_RULES) {
    switch (proxy_rules_.type) {
      case ProxyRules::TYPE_SINGLE_PROXY:
        AddProxyToValue("single_proxy", proxy_rules_.single_proxy, dict);
        break;
      case ProxyRules::TYPE_PROXY_PER_SCHEME: {
        DictionaryValue* dict2 = new DictionaryValue();
        AddProxyToValue("http", proxy_rules_.proxy_for_http, dict2);
        AddProxyToValue("https", proxy_rules_.proxy_for_https, dict2);
        AddProxyToValue("ftp", proxy_rules_.proxy_for_ftp, dict2);
        AddProxyToValue("fallback", proxy_rules_.fallback_proxy, dict2);
        dict->Set("proxy_per_scheme", dict2);
        break;
      }
      default:
        NOTREACHED();
    }

    // Output the bypass rules.
    const ProxyBypassRules& bypass = proxy_rules_.bypass_rules;
    if (!bypass.rules().empty()) {
      if (proxy_rules_.reverse_bypass)
        dict->SetBoolean("reverse_bypass", true);

      ListValue* list = new ListValue();

      for (ProxyBypassRules::RuleList::const_iterator it =
              bypass.rules().begin();
           it != bypass.rules().end(); ++it) {
        list->Append(Value::CreateStringValue((*it)->ToString()));
      }

      dict->Set("bypass_list", list);
    }
  }

  return dict;
}

}  // namespace net

