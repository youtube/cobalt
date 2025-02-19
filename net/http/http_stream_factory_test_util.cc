// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_test_util.h"

#include <utility>

#include "net/proxy_resolution/proxy_info.h"
#include "url/scheme_host_port.h"

using ::testing::_;

namespace net {
MockHttpStreamRequestDelegate::MockHttpStreamRequestDelegate() = default;

MockHttpStreamRequestDelegate::~MockHttpStreamRequestDelegate() = default;

MockHttpStreamFactoryJob::MockHttpStreamFactoryJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    ProxyInfo proxy_info,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    url::SchemeHostPort destination,
    GURL origin_url,
    NextProto alternative_protocol,
    quic::ParsedQuicVersion quic_version,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log)
    : HttpStreamFactory::Job(delegate,
                             job_type,
                             session,
                             request_info,
                             priority,
                             proxy_info,
                             server_ssl_config,
                             proxy_ssl_config,
                             std::move(destination),
                             origin_url,
                             alternative_protocol,
                             quic_version,
                             is_websocket,
                             enable_ip_based_pooling,
#if defined(STARBOARD)
                             net_log,
                             /*protocol_filter_override=*/false) {
#else
                             net_log) {
#endif  // defined(STARBOARD)
  DCHECK(!is_waiting());
}

MockHttpStreamFactoryJob::~MockHttpStreamFactoryJob() = default;

void MockHttpStreamFactoryJob::DoResume() {
  HttpStreamFactory::Job::Resume();
}

TestJobFactory::TestJobFactory() = default;

TestJobFactory::~TestJobFactory() = default;

std::unique_ptr<HttpStreamFactory::Job> TestJobFactory::CreateJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const ProxyInfo& proxy_info,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    url::SchemeHostPort destination,
    GURL origin_url,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log,
    NextProto alternative_protocol = kProtoUnknown,
#if defined(STARBOARD)
    quic::ParsedQuicVersion quic_version =
        quic::ParsedQuicVersion::Unsupported(),
    bool protocol_filter_override = false) {
#else
    quic::ParsedQuicVersion quic_version =
        quic::ParsedQuicVersion::Unsupported()) {
#endif  // defined(STARBOARD)
  auto job = std::make_unique<MockHttpStreamFactoryJob>(
      delegate, job_type, session, request_info, priority, proxy_info,
      SSLConfig(), SSLConfig(), std::move(destination), origin_url,
      alternative_protocol, quic_version, is_websocket, enable_ip_based_pooling,
      net_log);

  // Keep raw pointer to Job but pass ownership.
  switch (job_type) {
    case HttpStreamFactory::MAIN:
      main_job_ = job.get();
      break;
    case HttpStreamFactory::ALTERNATIVE:
      alternative_job_ = job.get();
      break;
    case HttpStreamFactory::DNS_ALPN_H3:
      dns_alpn_h3_job_ = job.get();
      break;
    case HttpStreamFactory::PRECONNECT:
      main_job_ = job.get();
      break;
    case HttpStreamFactory::PRECONNECT_DNS_ALPN_H3:
      main_job_ = job.get();
      break;
  }
  return job;
}

}  // namespace net
