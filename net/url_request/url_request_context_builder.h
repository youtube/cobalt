// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is useful for building a simple URLRequestContext. Most creators
// of new URLRequestContexts should use this helper class to construct it. Call
// any configuration params, and when done, invoke Build() to construct the
// URLRequestContext. This URLRequestContext will own all its own storage.
//
// URLRequestContextBuilder and its associated params classes are initially
// populated with "sane" default values. Read through the comments to figure out
// what these are.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

namespace net {

class HostMappingRules;
class ProxyConfigService;
class URLRequestContext;
class NetworkDelegate;

class NET_EXPORT URLRequestContextBuilder {
 public:
  struct NET_EXPORT HttpCacheParams {
    enum Type {
      IN_MEMORY,
      DISK,
    };

    HttpCacheParams();
    ~HttpCacheParams();

    // The type of HTTP cache. Default is IN_MEMORY.
    Type type;

    // The max size of the cache in bytes. Default is algorithmically determined
    // based off available disk space.
    int max_size;

    // The cache path (when type is DISK).
    FilePath path;
  };

  struct NET_EXPORT HttpNetworkSessionParams {
    HttpNetworkSessionParams();
    ~HttpNetworkSessionParams();

    // These fields mirror those in net::HttpNetworkSession::Params;
    bool ignore_certificate_errors;
    HostMappingRules* host_mapping_rules;
    bool http_pipelining_enabled;
    uint16 testing_fixed_http_port;
    uint16 testing_fixed_https_port;
    std::string trusted_spdy_proxy;
  };

  URLRequestContextBuilder();
  ~URLRequestContextBuilder();

#if defined(OS_LINUX) || defined(OS_ANDROID)
  void set_proxy_config_service(ProxyConfigService* proxy_config_service);
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

  // Call these functions to specify hard-coded Accept-Language,
  // Accept-Charset, or User-Agent header values for all requests that don't
  // have the headers already set.
  void set_accept_language(const std::string& accept_language) {
    accept_language_ = accept_language;
  }
  void set_accept_charset(const std::string& accept_charset) {
    accept_charset_ = accept_charset;
  }
  void set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
  }

  // By default it's disabled.
  void set_ftp_enabled(bool enable) {
    ftp_enabled_ = enable;
  }

  // Uses BasicNetworkDelegate by default. Note that calling Build will unset
  // any custom delegate in builder, so this must be called each time before
  // Build is called.
  void set_network_delegate(NetworkDelegate* delegate) {
    network_delegate_.reset(delegate);
  }

  // By default HttpCache is enabled with a default constructed HttpCacheParams.
  void EnableHttpCache(const HttpCacheParams& params) {
    http_cache_params_ = params;
  }

  void DisableHttpCache() {
    http_cache_params_ = HttpCacheParams();
  }

  // Override default net::HttpNetworkSession::Params settings.
  void set_http_network_session_params(
      const HttpNetworkSessionParams& http_network_session_params) {
    http_network_session_params_ = http_network_session_params;
  }

  URLRequestContext* Build();

 private:
  std::string accept_language_;
  std::string accept_charset_;
  std::string user_agent_;
  bool ftp_enabled_;
  bool http_cache_enabled_;
  HttpCacheParams http_cache_params_;
  HttpNetworkSessionParams http_network_session_params_;
#if defined(OS_LINUX) || defined(OS_ANDROID)
  scoped_ptr<ProxyConfigService> proxy_config_service_;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
  scoped_ptr<NetworkDelegate> network_delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextBuilder);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_
