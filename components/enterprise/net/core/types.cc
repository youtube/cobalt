// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/types.h"

namespace enterprise_net {

ProxyProvisioningDomainPolicy::AuthConfig::AuthConfig() = default;
ProxyProvisioningDomainPolicy::AuthConfig::AuthConfig(const AuthConfig&) =
    default;
ProxyProvisioningDomainPolicy::AuthConfig&
ProxyProvisioningDomainPolicy::AuthConfig::operator=(const AuthConfig&) =
    default;
ProxyProvisioningDomainPolicy::AuthConfig::AuthConfig(AuthConfig&&) noexcept =
    default;
ProxyProvisioningDomainPolicy::AuthConfig&
ProxyProvisioningDomainPolicy::AuthConfig::operator=(AuthConfig&&) noexcept =
    default;
ProxyProvisioningDomainPolicy::AuthConfig::~AuthConfig() = default;

ProxyProvisioningDomainPolicy::ExtraHeader::ExtraHeader() = default;
ProxyProvisioningDomainPolicy::ExtraHeader::ExtraHeader(const ExtraHeader&) =
    default;
ProxyProvisioningDomainPolicy::ExtraHeader&
ProxyProvisioningDomainPolicy::ExtraHeader::operator=(const ExtraHeader&) =
    default;
ProxyProvisioningDomainPolicy::ExtraHeader::ExtraHeader(
    ExtraHeader&&) noexcept = default;
ProxyProvisioningDomainPolicy::ExtraHeader&
ProxyProvisioningDomainPolicy::ExtraHeader::operator=(ExtraHeader&&) noexcept =
    default;
ProxyProvisioningDomainPolicy::ExtraHeader::~ExtraHeader() = default;

ProxyProvisioningDomainPolicy::ProxyProvisioningDomainPolicy() = default;
ProxyProvisioningDomainPolicy::ProxyProvisioningDomainPolicy(
    const ProxyProvisioningDomainPolicy&) = default;
ProxyProvisioningDomainPolicy& ProxyProvisioningDomainPolicy::operator=(
    const ProxyProvisioningDomainPolicy&) = default;
ProxyProvisioningDomainPolicy::ProxyProvisioningDomainPolicy(
    ProxyProvisioningDomainPolicy&&) noexcept = default;
ProxyProvisioningDomainPolicy& ProxyProvisioningDomainPolicy::operator=(
    ProxyProvisioningDomainPolicy&&) noexcept = default;
ProxyProvisioningDomainPolicy::~ProxyProvisioningDomainPolicy() = default;

ProvisioningDomainProxyConfig::RouteMatch::RouteMatch() = default;
ProvisioningDomainProxyConfig::RouteMatch::RouteMatch(const RouteMatch&) =
    default;
ProvisioningDomainProxyConfig::RouteMatch&
ProvisioningDomainProxyConfig::RouteMatch::operator=(const RouteMatch&) =
    default;
ProvisioningDomainProxyConfig::RouteMatch::RouteMatch(RouteMatch&&) noexcept =
    default;
ProvisioningDomainProxyConfig::RouteMatch&
ProvisioningDomainProxyConfig::RouteMatch::operator=(RouteMatch&&) noexcept =
    default;
ProvisioningDomainProxyConfig::RouteMatch::~RouteMatch() = default;

ProvisioningDomainProxyConfig::ProvisioningDomainProxyConfig() = default;
ProvisioningDomainProxyConfig::ProvisioningDomainProxyConfig(
    const ProvisioningDomainProxyConfig&) = default;
ProvisioningDomainProxyConfig& ProvisioningDomainProxyConfig::operator=(
    const ProvisioningDomainProxyConfig&) = default;
ProvisioningDomainProxyConfig::ProvisioningDomainProxyConfig(
    ProvisioningDomainProxyConfig&&) noexcept = default;
ProvisioningDomainProxyConfig& ProvisioningDomainProxyConfig::operator=(
    ProvisioningDomainProxyConfig&&) noexcept = default;
ProvisioningDomainProxyConfig::~ProvisioningDomainProxyConfig() = default;

}  // namespace enterprise_net
