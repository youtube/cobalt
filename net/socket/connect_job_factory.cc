// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/socket/connect_job.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/ssl/ssl_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

template <typename T>
std::unique_ptr<T> CreateFactoryIfNull(std::unique_ptr<T> in) {
  if (in)
    return in;
  return std::make_unique<T>();
}

bool UsingSsl(const ConnectJobFactory::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return GURL::SchemeIsCryptographic(
        base::ToLowerASCII(absl::get<url::SchemeHostPort>(endpoint).scheme()));
  }

  DCHECK(
      absl::holds_alternative<ConnectJobFactory::SchemelessEndpoint>(endpoint));
  return absl::get<ConnectJobFactory::SchemelessEndpoint>(endpoint).using_ssl;
}

HostPortPair ToHostPortPair(const ConnectJobFactory::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return HostPortPair::FromSchemeHostPort(
        absl::get<url::SchemeHostPort>(endpoint));
  }

  DCHECK(
      absl::holds_alternative<ConnectJobFactory::SchemelessEndpoint>(endpoint));
  return absl::get<ConnectJobFactory::SchemelessEndpoint>(endpoint)
      .host_port_pair;
}

TransportSocketParams::Endpoint ToTransportEndpoint(
    const ConnectJobFactory::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint))
    return absl::get<url::SchemeHostPort>(endpoint);

  DCHECK(
      absl::holds_alternative<ConnectJobFactory::SchemelessEndpoint>(endpoint));
  return absl::get<ConnectJobFactory::SchemelessEndpoint>(endpoint)
      .host_port_pair;
}

base::flat_set<std::string> SupportedProtocolsFromSSLConfig(
    const SSLConfig& config) {
  // We convert because `SSLConfig` uses `NextProto` for ALPN protocols while
  // `TransportConnectJob` and DNS logic needs `std::string`. See
  // https://crbug.com/1286835.
  return base::MakeFlatSet<std::string>(config.alpn_protos, /*comp=*/{},
                                        NextProtoToString);
}

}  // namespace

ConnectJobFactory::ConnectJobFactory(
    std::unique_ptr<HttpProxyConnectJob::Factory>
        http_proxy_connect_job_factory,
    std::unique_ptr<SOCKSConnectJob::Factory> socks_connect_job_factory,
    std::unique_ptr<SSLConnectJob::Factory> ssl_connect_job_factory,
    std::unique_ptr<TransportConnectJob::Factory> transport_connect_job_factory)
    : http_proxy_connect_job_factory_(
          CreateFactoryIfNull(std::move(http_proxy_connect_job_factory))),
      socks_connect_job_factory_(
          CreateFactoryIfNull(std::move(socks_connect_job_factory))),
      ssl_connect_job_factory_(
          CreateFactoryIfNull(std::move(ssl_connect_job_factory))),
      transport_connect_job_factory_(
          CreateFactoryIfNull(std::move(transport_connect_job_factory))) {}

ConnectJobFactory::~ConnectJobFactory() = default;

std::unique_ptr<ConnectJob> ConnectJobFactory::CreateConnectJob(
    url::SchemeHostPort endpoint,
    const ProxyChain& proxy_chain,
    const absl::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const SSLConfig* ssl_config_for_origin,
    const SSLConfig* ssl_config_for_proxy,
    bool force_tunnel,
    PrivacyMode privacy_mode,
    const OnHostResolutionCallback& resolution_callback,
    RequestPriority request_priority,
    SocketTag socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) const {
  return CreateConnectJob(Endpoint(std::move(endpoint)), proxy_chain,
                          proxy_annotation_tag, ssl_config_for_origin,
                          ssl_config_for_proxy, force_tunnel, privacy_mode,
                          resolution_callback, request_priority, socket_tag,
                          network_anonymization_key, secure_dns_policy,
                          common_connect_job_params, delegate);
}

std::unique_ptr<ConnectJob> ConnectJobFactory::CreateConnectJob(
    bool using_ssl,
    HostPortPair endpoint,
    const ProxyChain& proxy_chain,
    const absl::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const SSLConfig* ssl_config_for_origin,
    const SSLConfig* ssl_config_for_proxy,
    bool force_tunnel,
    PrivacyMode privacy_mode,
    const OnHostResolutionCallback& resolution_callback,
    RequestPriority request_priority,
    SocketTag socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) const {
  SchemelessEndpoint schemeless_endpoint{using_ssl, std::move(endpoint)};
  return CreateConnectJob(std::move(schemeless_endpoint), proxy_chain,
                          proxy_annotation_tag, ssl_config_for_origin,
                          ssl_config_for_proxy, force_tunnel, privacy_mode,
                          resolution_callback, request_priority, socket_tag,
                          network_anonymization_key, secure_dns_policy,
                          common_connect_job_params, delegate);
}

std::unique_ptr<ConnectJob> ConnectJobFactory::CreateConnectJob(
    Endpoint endpoint,
    const ProxyChain& proxy_chain,
    const absl::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const SSLConfig* ssl_config_for_origin,
    const SSLConfig* ssl_config_for_proxy,
    bool force_tunnel,
    PrivacyMode privacy_mode,
    const OnHostResolutionCallback& resolution_callback,
    RequestPriority request_priority,
    SocketTag socket_tag,
    const NetworkAnonymizationKey& network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) const {
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SOCKSSocketParams> socks_params;
  base::flat_set<std::string> no_alpn_protocols;

  DCHECK(proxy_chain.IsValid());
  if (!proxy_chain.is_direct()) {
    // TODO(crbug.com/1206799): For an http-like proxy, should this pass a
    // `SchemeHostPort`, so proxies can participate in ECH? Note doing so with
    // `SCHEME_HTTP` requires handling the HTTPS record upgrade.
    const ProxyServer& first_proxy_server =
        proxy_chain.GetProxyServer(/*chain_index=*/0);
    auto proxy_tcp_params = base::MakeRefCounted<TransportSocketParams>(
        first_proxy_server.host_port_pair(),
        proxy_dns_network_anonymization_key_, secure_dns_policy,
        resolution_callback,
        first_proxy_server.is_secure_http_like()
            ? SupportedProtocolsFromSSLConfig(*ssl_config_for_proxy)
            : no_alpn_protocols);

    if (first_proxy_server.is_http_like()) {
      scoped_refptr<SSLSocketParams> ssl_params;
      if (first_proxy_server.is_secure_http_like()) {
        DCHECK(ssl_config_for_proxy);
        // Set `ssl_params`, and unset `proxy_tcp_params`.
        ssl_params = base::MakeRefCounted<SSLSocketParams>(
            std::move(proxy_tcp_params), nullptr, nullptr,
            first_proxy_server.host_port_pair(), *ssl_config_for_proxy,
            PRIVACY_MODE_DISABLED, network_anonymization_key);
        proxy_tcp_params = nullptr;
      }

      // TODO(crbug.com/1206799): Pass `endpoint` directly (preserving scheme
      // when available)?
      // TODO(https://crbug.com/1491092): The endpoint parameter here should
      // correspond to either `endpoint` (for one-hop proxies) or the proxy
      // server at index 1 (for n-hop proxies).
      http_proxy_params = base::MakeRefCounted<HttpProxySocketParams>(
          std::move(proxy_tcp_params), std::move(ssl_params),
          ToHostPortPair(endpoint), proxy_chain, /*proxy_chain_index=*/0,
          force_tunnel || UsingSsl(endpoint), *proxy_annotation_tag,
          network_anonymization_key, secure_dns_policy);
    } else {
      DCHECK(first_proxy_server.is_socks());
      // TODO(crbug.com/1206799): Pass `endpoint` directly (preserving scheme
      // when available)?
      socks_params = base::MakeRefCounted<SOCKSSocketParams>(
          std::move(proxy_tcp_params),
          first_proxy_server.scheme() == ProxyServer::SCHEME_SOCKS5,
          ToHostPortPair(endpoint), network_anonymization_key,
          *proxy_annotation_tag);
    }
  }

  // TODO(https://crbug.com/1491092): For nested proxies, create additional
  // SSLSocketParam and HttpProxySocketParam objects for the remaining hops.

  // Deal with SSL - which layers on top of any given proxy.
  if (UsingSsl(endpoint)) {
    DCHECK(ssl_config_for_origin);
    scoped_refptr<TransportSocketParams> ssl_tcp_params;
    if (proxy_chain.is_direct()) {
      ssl_tcp_params = base::MakeRefCounted<TransportSocketParams>(
          ToTransportEndpoint(endpoint), network_anonymization_key,
          secure_dns_policy, resolution_callback,
          SupportedProtocolsFromSSLConfig(*ssl_config_for_origin));
    }
    // TODO(crbug.com/1206799): Pass `endpoint` directly (preserving scheme
    // when available)?
    auto ssl_params = base::MakeRefCounted<SSLSocketParams>(
        std::move(ssl_tcp_params), std::move(socks_params),
        std::move(http_proxy_params), ToHostPortPair(endpoint),
        *ssl_config_for_origin, privacy_mode, network_anonymization_key);
    return ssl_connect_job_factory_->Create(
        request_priority, socket_tag, common_connect_job_params,
        std::move(ssl_params), delegate, /*net_log=*/nullptr);
  }

  // Only SSL/TLS-based endpoints have ALPN protocols.
  if (proxy_chain.is_direct()) {
    auto tcp_params = base::MakeRefCounted<TransportSocketParams>(
        ToTransportEndpoint(endpoint), network_anonymization_key,
        secure_dns_policy, resolution_callback, no_alpn_protocols);
    return transport_connect_job_factory_->Create(
        request_priority, socket_tag, common_connect_job_params, tcp_params,
        delegate, /*net_log=*/nullptr);
  }

  const ProxyServer& first_proxy_server =
      proxy_chain.GetProxyServer(/*chain_index=*/0);
  if (first_proxy_server.is_http_like()) {
    return http_proxy_connect_job_factory_->Create(
        request_priority, socket_tag, common_connect_job_params,
        std::move(http_proxy_params), delegate, /*net_log=*/nullptr);
  }

  DCHECK(first_proxy_server.is_socks());
  return socks_connect_job_factory_->Create(
      request_priority, socket_tag, common_connect_job_params,
      std::move(socks_params), delegate, /*net_log=*/nullptr);
}

}  // namespace net
