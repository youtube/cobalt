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
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

namespace net {

class ProxyConfigService;
class URLRequestContext;

class NET_EXPORT URLRequestContextBuilder {
 public:
  struct NET_EXPORT HostResolverParams {
    HostResolverParams();
    ~HostResolverParams();

    // The limit on the number of parallel host resolutions.
    size_t parallelism;

    // When the host resolution is taking too long, we'll retry this many times,
    // in a backing off manner.
    size_t retry_attempts;
  };

  struct NET_EXPORT HttpCacheParams {
    enum Type {
      IN_MEMORY,
      DISK,
    };

    HttpCacheParams();
    ~HttpCacheParams();

    // The type of HTTP cache. Default is DISK.
    Type type;

    // The max size of the cache in bytes. Default is algorithmically determined
    // based off available disk space.
    int max_size;

    // The cache path (when type is DISK).
    FilePath path;
  };

  URLRequestContextBuilder();
  ~URLRequestContextBuilder();

#if defined(OS_LINUX)
  void set_proxy_config_service(ProxyConfigService* proxy_config_service);
#endif  // defined(OS_LINUX)

  // Call this function to specify a hard-coded User-Agent for all requests that
  // don't have a User-Agent already set.
  void set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
  }

  // Allows for overriding the default HostResolver params.
  void set_host_resolver_params(
      const HostResolverParams& host_resolver_params) {
    host_resolver_params_ = host_resolver_params;
  }

  // By default it's disabled.
  void set_ftp_enabled(bool enable) {
    ftp_enabled_ = enable;
  }

  // By default HttpCache is enabled with a default constructed HttpCacheParams.
  void EnableHttpCache(const HttpCacheParams& params);
  void DisableHttpCache();

  URLRequestContext* Build();

 private:
  std::string user_agent_;
  bool ftp_enabled_;
  HostResolverParams host_resolver_params_;
  bool http_cache_enabled_;
  HttpCacheParams http_cache_params_;
#if defined(OS_LINUX)
  scoped_ptr<ProxyConfigService> proxy_config_service_;
#endif  // defined(OS_LINUX)

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextBuilder);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_BUILDER_H_
