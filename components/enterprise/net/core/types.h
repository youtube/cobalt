// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_TYPES_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_TYPES_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace enterprise_net {

// Supported authentication types for Provisioning Domains.
enum class AuthType {
  kNone,
  kProfileBearerToken,
};

// Supported OAuth scopes for Provisioning Domains authentication.
enum class AuthScope {
  kNone,
  kCloudSecureGateway,
};

// A single domain entry defined in the "ProxyProvisioningDomains" policy.
struct ProxyProvisioningDomainPolicy {
  struct AuthConfig {
    AuthConfig();
    AuthConfig(const AuthConfig&);
    AuthConfig& operator=(const AuthConfig&);
    AuthConfig(AuthConfig&&) noexcept;
    AuthConfig& operator=(AuthConfig&&) noexcept;
    ~AuthConfig();

    AuthType type = AuthType::kNone;
    AuthScope scope = AuthScope::kNone;
  };

  struct ExtraHeader {
    ExtraHeader();
    ExtraHeader(const ExtraHeader&);
    ExtraHeader& operator=(const ExtraHeader&);
    ExtraHeader(ExtraHeader&&) noexcept;
    ExtraHeader& operator=(ExtraHeader&&) noexcept;
    ~ExtraHeader();

    std::string key;
    std::string value;
  };

  ProxyProvisioningDomainPolicy();
  ProxyProvisioningDomainPolicy(const ProxyProvisioningDomainPolicy&);
  ProxyProvisioningDomainPolicy& operator=(
      const ProxyProvisioningDomainPolicy&);
  ProxyProvisioningDomainPolicy(ProxyProvisioningDomainPolicy&&) noexcept;
  ProxyProvisioningDomainPolicy& operator=(
      ProxyProvisioningDomainPolicy&&) noexcept;
  ~ProxyProvisioningDomainPolicy();

  std::string pvd_id;
  AuthConfig auth_config;
  std::vector<ExtraHeader> extra_headers;
};

// Public structure representing a fetched Provisioning Domain configuration
// alongside its current fetch state.
struct ProvisioningDomainProxyConfig {
  enum class State {
    kRefreshNeeded,
    kFetching,
    kValid,
    kFailed,
  };

  // Defines routing rules matching a set of traffic patterns to a set of proxy
  // servers, as defined in RFC 8801.
  struct RouteMatch {
    RouteMatch();
    RouteMatch(const RouteMatch&);
    RouteMatch& operator=(const RouteMatch&);
    RouteMatch(RouteMatch&&) noexcept;
    RouteMatch& operator=(RouteMatch&&) noexcept;
    ~RouteMatch();

    // A list of proxy identifiers.
    std::vector<std::string> proxies;

    // A list of domain patterns (e.g. "*.example.com") that match this rule.
    std::vector<std::string> domains;

    // A list of IP prefix ranges in CIDR notation (e.g. "192.0.0.0/24",
    // "2000:ab1::/32") that match this rule.
    std::vector<std::string> prefixes;

    // A list of destination ports (e.g. 80, 443) that match this rule.
    std::vector<uint16_t> ports;
  };

  ProvisioningDomainProxyConfig();
  ProvisioningDomainProxyConfig(const ProvisioningDomainProxyConfig&);
  ProvisioningDomainProxyConfig& operator=(
      const ProvisioningDomainProxyConfig&);
  ProvisioningDomainProxyConfig(ProvisioningDomainProxyConfig&&) noexcept;
  ProvisioningDomainProxyConfig& operator=(
      ProvisioningDomainProxyConfig&&) noexcept;
  ~ProvisioningDomainProxyConfig();

  std::string pvd_id;
  std::map<std::string, std::string> proxies;
  std::vector<RouteMatch> proxy_match;
  State state = State::kRefreshNeeded;
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_TYPES_H_
