// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_context.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/cache_type.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/cookies/cookie_util.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/resolve_context.h"
#include "net/http/http_auth.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/mock_http_cache.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/gtest_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/referrer_policy.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/cookie_manager.h"
#include "services/network/net_log_exporter.h"
#include "services/network/network_qualities_pref_delegate.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/proxy_config.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_utils.h"
#include "services/network/test/udp_socket_test_util.h"
#include "services/network/test_mojo_proxy_resolver_factory.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "services/network/url_request_context_builder_mojo.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#endif

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_service.h"
#include "net/reporting/reporting_test_util.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/network/mock_mojo_dhcp_wpad_url_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_P2P_ENABLED)
#include "services/network/public/mojom/p2p.mojom.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"
#endif  // BUILDFLAG(IS_P2P_ENABLED)

namespace network {

namespace {

using net::CreateTestURLRequestContextBuilder;
using ::testing::Optional;

constexpr char kMockHost[] = "mock.host";

#if BUILDFLAG(ENABLE_REPORTING)
const base::FilePath::CharType kFilename[] =
    FILE_PATH_LITERAL("TempReportingAndNelStore");
#endif  // BUILDFLAG(ENABLE_REPORTING)

void StoreValue(base::Value::Dict* result,
                base::OnceClosure callback,
                base::Value::Dict value) {
  *result = std::move(value);
  std::move(callback).Run();
}

void SetContentSetting(const GURL& primary_pattern,
                       const GURL& secondary_pattern,
                       ContentSetting setting,
                       NetworkContext* network_context) {
  network_context->cookie_manager()->SetContentSettings(
      {ContentSettingPatternSource(
          ContentSettingsPattern::FromURL(primary_pattern),
          ContentSettingsPattern::FromURL(secondary_pattern),
          base::Value(setting), std::string(), false)});
}

void SetDefaultContentSetting(ContentSetting setting,
                              NetworkContext* network_context) {
  network_context->cookie_manager()->SetContentSettings(
      {ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                   ContentSettingsPattern::Wildcard(),
                                   base::Value(setting), std::string(),
                                   false)});
}

std::unique_ptr<TestURLLoaderClient> FetchRequest(
    const ResourceRequest& request,
    NetworkContext* network_context,
    int url_loader_options = mojom::kURLLoadOptionNone,
    int process_id = mojom::kBrowserProcessId,
    mojom::URLLoaderFactoryParamsPtr params = nullptr) {
  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  if (!params)
    params = mojom::URLLoaderFactoryParams::New();
  params->process_id = process_id;
  params->is_corb_enabled = false;

  // If |site_for_cookies| is null, any non-empty NIK is fine. Otherwise, the
  // NIK must be consistent with |site_for_cookies|.
  if (request.site_for_cookies.IsNull()) {
    params->isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url::Origin::Create(GURL("https://abc.com")),
        url::Origin::Create(GURL("https://xyz.com")), request.site_for_cookies);
  } else {
    params->isolation_info = net::IsolationInfo::CreateForInternalRequest(
        url::Origin::Create(request.site_for_cookies.RepresentativeUrl()));
  }

  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  auto client = std::make_unique<TestURLLoaderClient>();
  mojo::PendingRemote<mojom::URLLoader> loader;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      url_loader_options, request, client->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client->RunUntilComplete();
  return client;
}

// proxy_resolver::mojom::ProxyResolverFactory that captures the most recent PAC
// script passed to it, and the most recent URL/NetworkAnonymizationKey passed
// to the GetProxyForUrl() method of proxy_resolver::mojom::ProxyResolver it
// returns.
class CapturingMojoProxyResolverFactory
    : public proxy_resolver::mojom::ProxyResolverFactory,
      public proxy_resolver::mojom::ProxyResolver {
 public:
  CapturingMojoProxyResolverFactory() {}

  CapturingMojoProxyResolverFactory(const CapturingMojoProxyResolverFactory&) =
      delete;
  CapturingMojoProxyResolverFactory& operator=(
      const CapturingMojoProxyResolverFactory&) = delete;

  ~CapturingMojoProxyResolverFactory() override {}

  mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
  CreateFactoryRemote() {
    return proxy_factory_receiver_.BindNewPipeAndPassRemote();
  }

  // proxy_resolver::mojom::ProxyResolverFactory:
  void CreateResolver(
      const std::string& pac_script,
      mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
      mojo::PendingRemote<
          proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client)
      override {
    pac_script_ = pac_script;

    mojo::Remote<proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
        factory_request_client(std::move(client));
    proxy_resolver_receiver_.Bind(std::move(receiver));
    factory_request_client->ReportResult(net::OK);
  }

  // proxy_resolver::mojom::ProxyResolver:
  void GetProxyForUrl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverRequestClient>
          client) override {
    url_ = url;
    network_anonymization_key_ = network_anonymization_key;

    mojo::Remote<proxy_resolver::mojom::ProxyResolverRequestClient>
        resolver_request_client(std::move(client));
    net::ProxyInfo proxy_info;
    proxy_info.UseDirect();
    resolver_request_client->ReportResult(net::OK, proxy_info);
  }

  const std::string& pac_script() const { return pac_script_; }

  // Return the GURL and NetworkAnonymizationKey passed to the most recent
  // GetProxyForUrl() call.
  const GURL& url() const { return url_; }
  const net::NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }

 private:
  mojo::Receiver<ProxyResolverFactory> proxy_factory_receiver_{this};
  mojo::Receiver<ProxyResolver> proxy_resolver_receiver_{this};

  std::string pac_script_;

  GURL url_;
  net::NetworkAnonymizationKey network_anonymization_key_;
};

// ProxyLookupClient that drives proxy lookups and can wait for the responses to
// be received.
class TestProxyLookupClient : public mojom::ProxyLookupClient {
 public:
  TestProxyLookupClient() = default;

  TestProxyLookupClient(const TestProxyLookupClient&) = delete;
  TestProxyLookupClient& operator=(const TestProxyLookupClient&) = delete;

  ~TestProxyLookupClient() override = default;

  void StartLookUpProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojom::NetworkContext* network_context) {
    // Make sure this method is called at most once.
    EXPECT_FALSE(receiver_.is_bound());

    network_context->LookUpProxyForURL(url, network_anonymization_key,
                                       receiver_.BindNewPipeAndPassRemote());
  }

  void WaitForResult() { run_loop_.Run(); }

  // mojom::ProxyLookupClient implementation:
  void OnProxyLookupComplete(
      int32_t net_error,
      const absl::optional<net::ProxyInfo>& proxy_info) override {
    EXPECT_FALSE(is_done_);
    EXPECT_FALSE(proxy_info_);

    EXPECT_EQ(net_error == net::OK, proxy_info.has_value());

    is_done_ = true;
    proxy_info_ = proxy_info;
    net_error_ = net_error;
    receiver_.reset();
    run_loop_.Quit();
  }

  const absl::optional<net::ProxyInfo>& proxy_info() const {
    return proxy_info_;
  }

  int32_t net_error() const { return net_error_; }
  bool is_done() const { return is_done_; }

 private:
  mojo::Receiver<mojom::ProxyLookupClient> receiver_{this};

  bool is_done_ = false;
  absl::optional<net::ProxyInfo> proxy_info_;
  int32_t net_error_ = net::ERR_UNEXPECTED;

  base::RunLoop run_loop_;
};

#if BUILDFLAG(IS_P2P_ENABLED)
class MockP2PTrustedSocketManagerClient
    : public mojom::P2PTrustedSocketManagerClient {
 public:
  MockP2PTrustedSocketManagerClient() = default;
  ~MockP2PTrustedSocketManagerClient() override = default;

  // mojom::P2PTrustedSocketManagerClient:
  void InvalidSocketPortRangeRequested() override {}
  void DumpPacket(const std::vector<uint8_t>& packet_header,
                  uint64_t packet_length,
                  bool incoming) override {}
};
#endif  // BUILDFLAG(IS_P2P_ENABLED)

class HostResolverFactory final : public net::HostResolver::Factory {
 public:
  explicit HostResolverFactory(std::unique_ptr<net::HostResolver> resolver)
      : resolver_(std::move(resolver)) {}

  std::unique_ptr<net::HostResolver> CreateResolver(
      net::HostResolverManager* manager,
      base::StringPiece host_mapping_rules,
      bool enable_caching) override {
    DCHECK(resolver_);
    return std::move(resolver_);
  }

  // See HostResolver::CreateStandaloneResolver.
  std::unique_ptr<net::HostResolver> CreateStandaloneResolver(
      net::NetLog* net_log,
      const net::HostResolver::ManagerOptions& options,
      base::StringPiece host_mapping_rules,
      bool enable_caching) override {
    NOTREACHED();
    return nullptr;
  }

 private:
  std::unique_ptr<net::HostResolver> resolver_;
};

class NetworkContextTest : public testing::Test {
 public:
  explicit NetworkContextTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          time_source),
        network_change_notifier_(
            net::NetworkChangeNotifier::CreateMockIfNeeded()),
        network_service_(NetworkService::CreateForTesting()) {}
  ~NetworkContextTest() override {}

  std::unique_ptr<NetworkContext> CreateContextWithParams(
      mojom::NetworkContextParamsPtr context_params
#if BUILDFLAG(ENABLE_REPORTING)
      ,
      std::unique_ptr<net::ReportingService> reporting_service = nullptr
#endif
  ) {
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    DCHECK(!context_params->cert_verifier_params);
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    network_context_remote_.reset();
    return NetworkContext::CreateForTesting(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params),
        base::BindLambdaForTesting([&](net::URLRequestContextBuilder* builder) {
#if BUILDFLAG(ENABLE_REPORTING)
          builder->set_reporting_service(std::move(reporting_service));
#endif
        }));
  }

  // Searches through |backend|'s stats to discover its type. Only supports
  // blockfile and simple caches.
  net::URLRequestContextBuilder::HttpCacheParams::Type GetBackendType(
      disk_cache::Backend* backend) {
    base::StringPairs stats;
    backend->GetStats(&stats);
    for (const auto& pair : stats) {
      if (pair.first != "Cache type")
        continue;

      if (pair.second == "Simple Cache")
        return net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE;
      if (pair.second == "Blockfile Cache")
        return net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE;
      break;
    }

    NOTREACHED();
    return net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
  }

  mojom::NetworkService* network_service() const {
    return network_service_.get();
  }

  // Looks up a value with the given name from the NetworkContext's
  // TransportSocketPool info dictionary.
  int GetSocketPoolInfo(NetworkContext* context, base::StringPiece name) {
    return context->url_request_context()
        ->http_transaction_factory()
        ->GetSession()
        ->GetSocketPool(
            net::HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL,
            net::ProxyServer::Direct())
        ->GetInfoAsValue("", "")
        .GetDict()
        .FindInt(name)
        .value_or(-1);
  }

  int GetSocketCountForGroup(NetworkContext* context,
                             const net::ClientSocketPool::GroupId& group) {
    base::Value::Dict pool_info =
        context->url_request_context()
            ->http_transaction_factory()
            ->GetSession()
            ->GetSocketPool(
                net::HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL,
                net::ProxyServer::Direct())
            ->GetInfoAsValue("", "")
            .TakeDict();

    // "groups" dictionary should always exist.
    const base::Value::Dict& groups_dict = *pool_info.FindDict("groups");

    // The dictionary for the requested group may not exist.
    const base::Value::Dict* group_dict =
        groups_dict.FindDict(group.ToString());
    if (!group_dict) {
      return 0;
    }

    int count = group_dict->FindInt("active_socket_count").value_or(0);

    const base::Value::List* idle_sockets =
        group_dict->FindList("idle_sockets");
    if (idle_sockets)
      count += idle_sockets->size();

    const base::Value::List* connect_jobs =
        group_dict->FindList("connect_jobs");
    if (idle_sockets)
      count += connect_jobs->size();

    return count;
  }

  GURL GetHttpUrlFromHttps(const GURL& https_url) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr("http");
    return https_url.ReplaceComponents(replacements);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<NetworkService> network_service_;
  // Stores the mojo::Remote<NetworkContext> of the most recently created
  // NetworkContext. Not strictly needed, but seems best to mimic real-world
  // usage.
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
};

class NetworkContextTestWithMockTime : public NetworkContextTest {
 public:
  NetworkContextTestWithMockTime()
      : NetworkContextTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  }
};

TEST_F(NetworkContextTest, DestroyContextWithLiveRequest) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ResourceRequest request;
  request.url = test_server.GetURL("/hung-after-headers");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  mojo::PendingRemote<mojom::URLLoader> loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      0 /* options */, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilResponseReceived();
  EXPECT_TRUE(client.has_received_response());
  EXPECT_FALSE(client.has_received_completion());

  // Destroying the loader factory should not delete the URLLoader.
  loader_factory.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(client.has_received_completion());

  // Destroying the NetworkContext should result in destroying the loader and
  // the client receiving a connection error.
  network_context.reset();

  client.RunUntilDisconnect();
  EXPECT_FALSE(client.has_received_completion());
}

TEST_F(NetworkContextTest, DisableQuic) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableQuic);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  // By default, QUIC should be enabled for new NetworkContexts when the command
  // line indicates it should be.
  EXPECT_TRUE(network_context->url_request_context()
                  ->http_transaction_factory()
                  ->GetSession()
                  ->params()
                  .enable_quic);

  // Disabling QUIC should disable it on existing NetworkContexts.
  network_service()->DisableQuic();
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);

  // Disabling QUIC should disable it new NetworkContexts.
  std::unique_ptr<NetworkContext> network_context2 =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  EXPECT_FALSE(network_context2->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);

  // Disabling QUIC again should be harmless.
  network_service()->DisableQuic();
  std::unique_ptr<NetworkContext> network_context3 =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  EXPECT_FALSE(network_context3->url_request_context()
                   ->http_transaction_factory()
                   ->GetSession()
                   ->params()
                   .enable_quic);
}

TEST_F(NetworkContextTest, UserAgentAndLanguage) {
  const char kUserAgent[] = "Chromium Unit Test";
  const char kAcceptLanguage[] = "en-US,en;q=0.9,uk;q=0.8";
  mojom::NetworkContextParamsPtr params =
      CreateNetworkContextParamsForTesting();
  params->user_agent = kUserAgent;
  // Not setting accept_language, to test the default.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));
  EXPECT_EQ(kUserAgent, network_context->url_request_context()
                            ->http_user_agent_settings()
                            ->GetUserAgent());
  EXPECT_EQ("", network_context->url_request_context()
                    ->http_user_agent_settings()
                    ->GetAcceptLanguage());

  // Change accept-language.
  network_context->SetAcceptLanguage(kAcceptLanguage);
  EXPECT_EQ(kUserAgent, network_context->url_request_context()
                            ->http_user_agent_settings()
                            ->GetUserAgent());
  EXPECT_EQ(kAcceptLanguage, network_context->url_request_context()
                                 ->http_user_agent_settings()
                                 ->GetAcceptLanguage());

  // Create with custom accept-language configured.
  params = CreateNetworkContextParamsForTesting();
  params->user_agent = kUserAgent;
  params->accept_language = kAcceptLanguage;
  std::unique_ptr<NetworkContext> network_context2 =
      CreateContextWithParams(std::move(params));
  EXPECT_EQ(kUserAgent, network_context2->url_request_context()
                            ->http_user_agent_settings()
                            ->GetUserAgent());
  EXPECT_EQ(kAcceptLanguage, network_context2->url_request_context()
                                 ->http_user_agent_settings()
                                 ->GetAcceptLanguage());
}

TEST_F(NetworkContextTest, EnableBrotli) {
  for (bool enable_brotli : {true, false}) {
    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    context_params->enable_brotli = enable_brotli;
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));
    EXPECT_EQ(enable_brotli,
              network_context->url_request_context()->enable_brotli());
  }
}

TEST_F(NetworkContextTest, QuicUserAgentId) {
  const char kQuicUserAgentId[] = "007";
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->quic_user_agent_id = kQuicUserAgentId;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_EQ(kQuicUserAgentId, network_context->url_request_context()
                                  ->quic_context()
                                  ->params()
                                  ->user_agent_id);
}

TEST_F(NetworkContextTest, UnhandedProtocols) {
  const GURL kUnsupportedUrls[] = {
      // These are handled outside the network service.
      GURL("file:///not/a/path/that/leads/anywhere/but/it/should/not/matter/"
           "anyways"),

      // FTP is not supported natively by Chrome.
      GURL("ftp://foo.test/"),
  };

  for (const GURL& url : kUnsupportedUrls) {
    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    network_context->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    mojo::PendingRemote<mojom::URLLoader> loader;
    TestURLLoaderClient client;
    ResourceRequest request;
    request.url = url;
    loader_factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_TRUE(client.has_received_completion());
    EXPECT_EQ(net::ERR_UNKNOWN_URL_SCHEME,
              client.completion_status().error_code);
  }
}

#if BUILDFLAG(ENABLE_REPORTING)
TEST_F(NetworkContextTest, DisableReporting) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kReporting);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  EXPECT_FALSE(network_context->url_request_context()->reporting_service());
}

TEST_F(NetworkContextTest, EnableReportingWithoutStore) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kReporting);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  EXPECT_TRUE(network_context->url_request_context()->reporting_service());
  EXPECT_FALSE(network_context->url_request_context()
                   ->reporting_service()
                   ->GetContextForTesting()
                   ->store());
}

TEST_F(NetworkContextTest, EnableReportingWithStore) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kReporting);

  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath database_path = temp_dir.GetPath().Append(kFilename);
  context_params->file_paths = mojom::NetworkContextFilePaths::New();
  context_params->file_paths->data_directory = database_path.DirName();
  context_params->file_paths->reporting_and_nel_store_database_name =
      database_path.BaseName();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(network_context->url_request_context()->reporting_service());
  EXPECT_TRUE(network_context->url_request_context()
                  ->reporting_service()
                  ->GetContextForTesting()
                  ->store());
}

TEST_F(NetworkContextTest, DisableNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kNetworkErrorLogging);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  EXPECT_FALSE(
      network_context->url_request_context()->network_error_logging_service());
}

TEST_F(NetworkContextTest, EnableNetworkErrorLoggingWithoutStore) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kNetworkErrorLogging, features::kReporting}, {});

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  EXPECT_TRUE(
      network_context->url_request_context()->network_error_logging_service());
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_error_logging_service()
                   ->GetPersistentNelStoreForTesting());
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_error_logging_service()
                   ->GetReportingServiceForTesting()
                   ->GetContextForTesting()
                   ->store());
}

TEST_F(NetworkContextTest, EnableNetworkErrorLoggingWithStore) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kNetworkErrorLogging, features::kReporting}, {});

  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath reporting_and_nel_store = temp_dir.GetPath().Append(kFilename);
  context_params->file_paths = mojom::NetworkContextFilePaths::New();
  context_params->file_paths->data_directory =
      reporting_and_nel_store.DirName();
  context_params->file_paths->reporting_and_nel_store_database_name =
      reporting_and_nel_store.BaseName();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_TRUE(
      network_context->url_request_context()->network_error_logging_service());
  EXPECT_TRUE(network_context->url_request_context()
                  ->network_error_logging_service()
                  ->GetPersistentNelStoreForTesting());
  EXPECT_TRUE(network_context->url_request_context()
                  ->network_error_logging_service()
                  ->GetReportingServiceForTesting()
                  ->GetContextForTesting()
                  ->store());
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

TEST_F(NetworkContextTest, DefaultHttpNetworkSessionParams) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  const net::HttpNetworkSessionParams& params =
      network_context->url_request_context()
          ->http_transaction_factory()
          ->GetSession()
          ->params();
  const net::QuicParams* quic_params =
      network_context->url_request_context()->quic_context()->params();

  EXPECT_TRUE(params.enable_http2);
  EXPECT_TRUE(params.enable_quic);
  EXPECT_EQ(1250u, quic_params->max_packet_length);
  EXPECT_TRUE(quic_params->origins_to_force_quic_on.empty());
  EXPECT_FALSE(params.enable_user_alternate_protocol_ports);
  EXPECT_FALSE(params.ignore_certificate_errors);
  EXPECT_EQ(0, params.testing_fixed_http_port);
  EXPECT_EQ(0, params.testing_fixed_https_port);
}

// Make sure that network_session_configurator is hooked up.
TEST_F(NetworkContextTest, FixedHttpPort) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestingFixedHttpPort, "800");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestingFixedHttpsPort, "801");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  const net::HttpNetworkSessionParams& params =
      network_context->url_request_context()
          ->http_transaction_factory()
          ->GetSession()
          ->params();

  EXPECT_EQ(800, params.testing_fixed_http_port);
  EXPECT_EQ(801, params.testing_fixed_https_port);
}

TEST_F(NetworkContextTest, NoCache) {
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->http_cache_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  EXPECT_FALSE(network_context->url_request_context()
                   ->http_transaction_factory()
                   ->GetCache());
}

TEST_F(NetworkContextTest, MemoryCache) {
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->http_cache_enabled = true;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  EXPECT_EQ(net::MEMORY_CACHE, backend->GetCacheType());
}

TEST_F(NetworkContextTest, DiskCache) {
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_directory = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  EXPECT_EQ(net::DISK_CACHE, backend->GetCacheType());
  EXPECT_EQ(network_session_configurator::ChooseCacheType(),
            GetBackendType(backend));
}

class DiskCacheSizeTest : public NetworkContextTest {
 public:
  DiskCacheSizeTest() = default;
  ~DiskCacheSizeTest() override = default;

  int64_t VerifyDiskCacheSize(int scale = 100) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (scale != 100) {
      std::map<std::string, std::string> field_trial_params;
      field_trial_params["percent_relative_size"] = base::NumberToString(scale);
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          disk_cache::kChangeDiskCacheSizeExperiment, field_trial_params);
    }

    base::HistogramTester histogram_tester;

    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    context_params->http_cache_enabled = true;

    base::ScopedTempDir temp_dir;
    EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
    context_params->http_cache_directory = temp_dir.GetPath();

    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    net::HttpCache* cache = network_context->url_request_context()
                                ->http_transaction_factory()
                                ->GetCache();
    EXPECT_TRUE(cache);

    disk_cache::Backend* backend = nullptr;
    net::TestCompletionCallback callback;
    int rv = cache->GetBackend(&backend, callback.callback());
    EXPECT_EQ(net::OK, callback.GetResult(rv));
    EXPECT_TRUE(backend);

    EXPECT_EQ(net::DISK_CACHE, backend->GetCacheType());

    int64_t max_file_size = backend->MaxFileSize();
    histogram_tester.ExpectTotalCount("HttpCache.MaxFileSizeOnInit", 1);
    histogram_tester.ExpectUniqueSample("HttpCache.MaxFileSizeOnInit",
                                        max_file_size / 1024, 1);

    return max_file_size;
  }
};

TEST_F(DiskCacheSizeTest, DiskCacheSize) {
  int64_t max_file_size = VerifyDiskCacheSize();

  int64_t max_file_size_scaled = VerifyDiskCacheSize(200);

  // After scaling to 200%, the size will in most cases be twice of
  // |max_file_size| but it is dependent on the available size, and since we
  // cannot guarantee available size to be the same between the 2 runs to
  // VerifyDiskCacheSize(), only checking for the scaled size to be >=
  // max_file_size.
  EXPECT_GE(max_file_size_scaled, max_file_size);
}

// This makes sure that network_session_configurator::ChooseCacheType is
// connected to NetworkContext.
TEST_F(NetworkContextTest, SimpleCache) {
  base::FieldTrialList::CreateFieldTrial("SimpleCacheTrial", "ExperimentYes");
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_directory = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_TRUE(cache);

  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  base::StringPairs stats;
  backend->GetStats(&stats);
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            GetBackendType(backend));
}

TEST_F(NetworkContextTest, HttpServerPropertiesToDisk) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo");
  EXPECT_FALSE(base::PathExists(file_path));

  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  // Create a context with on-disk storage of HTTP server properties.
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->file_paths = mojom::NetworkContextFilePaths::New();
  context_params->file_paths->data_directory = file_path.DirName();
  context_params->file_paths->http_server_properties_file_name =
      file_path.BaseName();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk, and sanity check initial state.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey()));

  // Set a property.
  network_context->url_request_context()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey(), true);
  // Deleting the context will cause it to flush state. Wait for the pref
  // service to flush to disk.
  network_context.reset();
  task_environment_.RunUntilIdle();

  // Create a new NetworkContext using the same path for HTTP server properties.
  context_params = CreateNetworkContextParamsForTesting();
  context_params->file_paths = mojom::NetworkContextFilePaths::New();
  context_params->file_paths->data_directory = file_path.DirName();
  context_params->file_paths->http_server_properties_file_name =
      file_path.BaseName();
  network_context = CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey()));

  // Now check that ClearNetworkingHistoryBetween clears the data.
  base::RunLoop run_loop2;
  network_context->ClearNetworkingHistoryBetween(
      base::Time::Now() - base::Hours(1), base::Time::Max(),
      run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey()));

  // Destroy the network context and let any pending writes complete before
  // destroying |temp_dir|, to avoid leaking any files.
  network_context.reset();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_dir.Delete());
}

#if BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

TEST_F(NetworkContextTest, DataDirectoryAsHandle) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo");
  ASSERT_TRUE(base::CreateDirectory(file_path.DirName()));

  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  // Create a context with on-disk storage of HTTP server properties.
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->file_paths = mojom::NetworkContextFilePaths::New();

  // Make |data_directory| into a path-less directory handle.
  // Moving a TransferableDirectory once it's been opened will drop the
  // path from the original.
  context_params->file_paths->data_directory =
      TransferableDirectory(file_path.DirName());
  context_params->file_paths->data_directory.OpenForTransfer();
  EXPECT_TRUE(context_params->file_paths->data_directory.NeedsMount());

  context_params->file_paths->http_server_properties_file_name =
      file_path.BaseName();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk, and sanity check initial state.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey()));

  // Set a property.
  network_context->url_request_context()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey(), true);
  // Deleting the context will cause it to flush state. Wait for the pref
  // service to flush to disk.
  network_context.reset();
  task_environment_.RunUntilIdle();

  // Create a new NetworkContext using the same path for HTTP server properties.
  context_params = CreateNetworkContextParamsForTesting();
  context_params->file_paths = mojom::NetworkContextFilePaths::New();
  context_params->file_paths->data_directory = file_path.DirName();
  context_params->file_paths->http_server_properties_file_name =
      file_path.BaseName();
  network_context = CreateContextWithParams(std::move(context_params));

  // Wait for properties to load from disk.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey()));

  // Now check that ClearNetworkingHistoryBetween clears the data.
  base::RunLoop run_loop2;
  network_context->ClearNetworkingHistoryBetween(
      base::Time::Now() - base::Hours(1), base::Time::Max(),
      run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey()));

  // Destroy the network context and let any pending writes complete before
  // destroying |temp_dir|, to avoid leaking any files.
  network_context.reset();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_dir.Delete());
}

#endif  // BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

// Checks that ClearNetworkingHistoryBetween() clears in-memory pref stores and
// invokes the closure passed to it.
TEST_F(NetworkContextTest, ClearHttpServerPropertiesInMemory) {
  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey()));
  network_context->url_request_context()
      ->http_server_properties()
      ->SetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey(), true);
  EXPECT_TRUE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey()));

  base::RunLoop run_loop;
  network_context->ClearNetworkingHistoryBetween(
      base::Time::Now() - base::Hours(1), base::Time::Max(),
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(
      network_context->url_request_context()
          ->http_server_properties()
          ->GetSupportsSpdy(kSchemeHostPort, net::NetworkAnonymizationKey()));
}

// Checks that ClearNetworkingHistoryBetween() clears network quality prefs.
TEST_F(NetworkContextTest, ClearingNetworkingHistoryClearNetworkQualityPrefs) {
  const url::SchemeHostPort kSchemeHostPort("https", "foo", 443);
  net::TestNetworkQualityEstimator estimator;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  TestingPrefServiceSimple pref_service_simple;
  NetworkQualitiesPrefDelegate::RegisterPrefs(pref_service_simple.registry());

  std::unique_ptr<NetworkQualitiesPrefDelegate>
      network_qualities_pref_delegate =
          std::make_unique<NetworkQualitiesPrefDelegate>(&pref_service_simple,
                                                         &estimator);
  NetworkQualitiesPrefDelegate* network_qualities_pref_delegate_ptr =
      network_qualities_pref_delegate.get();
  network_context->set_network_qualities_pref_delegate_for_testing(
      std::move(network_qualities_pref_delegate));

  // Running the loop allows prefs to be set.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      network_qualities_pref_delegate_ptr->ForceReadPrefsForTesting().empty());

  // Clear the networking history.
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;
  network_context->ClearNetworkingHistoryBetween(
      base::Time::Now() - base::Hours(1), base::Time::Max(),
      run_loop.QuitClosure());
  run_loop.Run();

  // Running the loop should clear the network quality prefs.
  base::RunLoop().RunUntilIdle();
  // Prefs should be empty now.
  EXPECT_TRUE(
      network_qualities_pref_delegate_ptr->ForceReadPrefsForTesting().empty());
  histogram_tester.ExpectTotalCount("NQE.PrefsSizeOnClearing", 1);
}

// Test that TransportSecurity state is persisted (or not) as expected.
TEST_F(NetworkContextTest, TransportSecurityStatePersisted) {
  const char kDomain[] = "foo.test";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath transport_security_persister_file_path =
      temp_dir.GetPath().AppendASCII("TransportSecurity");
  EXPECT_FALSE(base::PathExists(transport_security_persister_file_path));

  for (bool on_disk : {false, true}) {
    // Create a NetworkContext.
    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    if (on_disk) {
      context_params->file_paths = mojom::NetworkContextFilePaths::New();
      context_params->file_paths->data_directory =
          transport_security_persister_file_path.DirName();
      context_params->file_paths->transport_security_persister_file_name =
          transport_security_persister_file_path.BaseName();
    }
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    // Add an STS entry.
    net::TransportSecurityState::STSState sts_state;
    net::TransportSecurityState* state =
        network_context->url_request_context()->transport_security_state();
    EXPECT_FALSE(state->GetDynamicSTSState(kDomain, &sts_state));
    state->AddHSTS(kDomain, base::Time::Now() + base::Seconds(1000),
                   false /* include subdomains */);
    EXPECT_TRUE(state->GetDynamicSTSState(kDomain, &sts_state));
    ASSERT_EQ(kDomain, sts_state.domain);

    // Destroy the network context, and wait for all tasks to write state to
    // disk to finish running.
    network_context.reset();
    task_environment_.RunUntilIdle();
    EXPECT_EQ(on_disk,
              base::PathExists(transport_security_persister_file_path));

    // Create a new NetworkContext,with the same parameters, and check if the
    // added STS entry still exists.
    context_params = CreateNetworkContextParamsForTesting();
    if (on_disk) {
      context_params->file_paths = mojom::NetworkContextFilePaths::New();
      context_params->file_paths->data_directory =
          transport_security_persister_file_path.DirName();
      context_params->file_paths->transport_security_persister_file_name =
          transport_security_persister_file_path.BaseName();
    }
    network_context = CreateContextWithParams(std::move(context_params));
    // Wait for the entry to load.
    task_environment_.RunUntilIdle();
    state = network_context->url_request_context()->transport_security_state();
    ASSERT_EQ(on_disk, state->GetDynamicSTSState(kDomain, &sts_state));
    if (on_disk)
      EXPECT_EQ(kDomain, sts_state.domain);
  }
}

// Test that PKP failures are reported if and only if certificate reporting is
// enabled.
TEST_F(NetworkContextTest, CertReporting) {
  const char kPreloadedPKPHost[] = "with-report-uri-pkp.preloaded.test";
  const char kReportHost[] = "report-uri.preloaded.test";
  const char kReportPath[] = "/pkp";

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);

  for (bool reporting_enabled : {false, true}) {
    // Server that PKP reports are sent to.
    net::test_server::EmbeddedTestServer report_test_server;
    net::test_server::ControllableHttpResponse controllable_response(
        &report_test_server, kReportPath);
    ASSERT_TRUE(report_test_server.Start());

    // Configure the TransportSecurityStateSource so that kPreloadedPKPHost will
    // have static PKP pins set, with a report URI on kReportHost.
    net::ScopedTransportSecurityStateSource scoped_security_state_source(
        report_test_server.port());

    // Configure a test HTTPS server.
    net::test_server::EmbeddedTestServer pkp_test_server(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    pkp_test_server.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    ASSERT_TRUE(pkp_test_server.Start());

    // Configure mock cert verifier to cause the PKP check to fail.
    net::CertVerifyResult result;
    result.verified_cert = net::CreateCertificateChainFromFile(
        net::GetTestCertsDirectory(), "ok_cert.pem",
        net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
    ASSERT_TRUE(result.verified_cert);
    net::SHA256HashValue hash = {{0x00, 0x01}};
    result.public_key_hashes.emplace_back(hash);
    result.is_issued_by_known_root = true;
    net::MockCertVerifier mock_verifier;
    mock_verifier.AddResultForCert(pkp_test_server.GetCertificate(), result,
                                   net::OK);
    NetworkContext::SetCertVerifierForTesting(&mock_verifier);

    // Configure a MockHostResolver to map requests to kPreloadedPKPHost and
    // kReportHost to the test servers:
    scoped_refptr<net::RuleBasedHostResolverProc> mock_resolver_proc =
        base::MakeRefCounted<net::RuleBasedHostResolverProc>(nullptr);
    mock_resolver_proc->AddIPLiteralRule(
        kPreloadedPKPHost, pkp_test_server.GetIPLiteralString(), std::string());
    mock_resolver_proc->AddIPLiteralRule(
        kReportHost, report_test_server.GetIPLiteralString(), std::string());
    net::ScopedDefaultHostResolverProc scoped_default_host_resolver(
        mock_resolver_proc.get());

    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    EXPECT_FALSE(context_params->enable_certificate_reporting);
    context_params->enable_certificate_reporting = reporting_enabled;
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    // Enable static pins so that requests made to kPreloadedPKPHost will check
    // the pins, and send a report if the pinning check fails.
    network_context->url_request_context()
        ->transport_security_state()
        ->EnableStaticPinsForTesting();
    network_context->url_request_context()
        ->transport_security_state()
        ->SetPinningListAlwaysTimelyForTesting(true);

    ResourceRequest request;
    request.url = pkp_test_server.GetURL(kPreloadedPKPHost, "/");

    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    network_context->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    mojo::PendingRemote<mojom::URLLoader> loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_TRUE(client.has_received_completion());
    EXPECT_EQ(net::ERR_INSECURE_RESPONSE,
              client.completion_status().error_code);

    if (reporting_enabled) {
      // If reporting is enabled, wait to see the request from the ReportSender.
      // Don't respond to the request, effectively making it a hung request.
      controllable_response.WaitForRequest();
    } else {
      // Otherwise, there should be no pending URLRequest.
      // |controllable_response| will cause requests to hang, so if there's no
      // URLRequest, then either a reporting request was never started. This
      // relies on reported being sent immediately for correctness.
      network_context->url_request_context()->AssertNoURLRequests();
    }

    // Destroy the network context. This serves to check the case that reporting
    // requests are alive when a NetworkContext is torn down.
    network_context.reset();

    // Remove global reference to the MockCertVerifier before it falls out of
    // scope.
    NetworkContext::SetCertVerifierForTesting(nullptr);
  }
}

// Test that host resolution error information is available.
TEST_F(NetworkContextTest, HostResolutionFailure) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  std::unique_ptr<net::MockHostResolver> resolver =
      std::make_unique<net::MockHostResolver>();
  resolver->rules()->AddSimulatedTimeoutFailure("*");
  context_builder->set_host_resolver(std::move(resolver));
  std::unique_ptr<net::URLRequestContext> url_request_context =
      context_builder->Build();

  network_context_remote_.reset();
  std::unique_ptr<NetworkContext> network_context =
      std::make_unique<NetworkContext>(
          network_service_.get(),
          network_context_remote_.BindNewPipeAndPassReceiver(),
          url_request_context.get(),
          /*cors_exempt_header_list=*/std::vector<std::string>());

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  ResourceRequest request;
  request.url = GURL("http://example.test");

  mojo::PendingRemote<mojom::URLLoader> loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      0 /* options */, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  EXPECT_TRUE(client.has_received_completion());
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, client.completion_status().error_code);
  EXPECT_EQ(net::ERR_DNS_TIMED_OUT,
            client.completion_status().resolve_error_info.error);
}

#if BUILDFLAG(IS_P2P_ENABLED)
// Test the P2PSocketManager::GetHostAddress() works and uses the correct
// NetworkAnonymizationKey.
TEST_F(NetworkContextTest, P2PHostResolution) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSplitHostCacheByNetworkIsolationKey);

  const char kHostname[] = "foo.test.";
  net::IPAddress ip_address;
  ASSERT_TRUE(ip_address.AssignFromIPLiteral("1.2.3.4"));
  net::NetworkAnonymizationKey network_anonymization_key =
      net::NetworkAnonymizationKey::CreateTransient();
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(
      std::make_unique<net::MockCachingHostResolver>());
  std::unique_ptr<net::URLRequestContext> url_request_context =
      context_builder->Build();
  auto& host_resolver = *static_cast<net::MockCachingHostResolver*>(
      url_request_context->host_resolver());
  host_resolver.rules()->AddRule(kHostname, ip_address.ToString());

  network_context_remote_.reset();
  std::unique_ptr<NetworkContext> network_context =
      std::make_unique<NetworkContext>(
          network_service_.get(),
          network_context_remote_.BindNewPipeAndPassReceiver(),
          url_request_context.get(),
          std::vector<std::string>() /* cors_exempt_header_list */);

  MockP2PTrustedSocketManagerClient client;
  mojo::Receiver<network::mojom::P2PTrustedSocketManagerClient> receiver(
      &client);

  mojo::Remote<mojom::P2PTrustedSocketManager> trusted_socket_manager;
  mojo::Remote<mojom::P2PSocketManager> socket_manager;
  network_context_remote_->CreateP2PSocketManager(
      network_anonymization_key, receiver.BindNewPipeAndPassRemote(),
      trusted_socket_manager.BindNewPipeAndPassReceiver(),
      socket_manager.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  std::vector<net::IPAddress> results;
  socket_manager->GetHostAddress(
      kHostname, false /* enable_mdns */,
      base::BindLambdaForTesting(
          [&](const std::vector<net::IPAddress>& addresses) {
            EXPECT_EQ(std::vector<net::IPAddress>{ip_address}, addresses);
            run_loop.Quit();
          }));
  run_loop.Run();

  // Check that the URL in kHostname is in the HostCache, with
  // |network_anonymization_key|.
  const net::HostPortPair kHostPortPair = net::HostPortPair(kHostname, 0);
  net::HostResolver::ResolveHostParameters params;
  params.source = net::HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<net::HostResolver::ResolveHostRequest> request1 =
      host_resolver.CreateRequest(kHostPortPair, network_anonymization_key,
                                  net::NetLogWithSource(), params);
  net::TestCompletionCallback callback1;
  int result = request1->Start(callback1.callback());
  EXPECT_EQ(net::OK, callback1.GetResult(result));

  // Check that the hostname is not in the DNS cache for other possible NIKs.
  const url::Origin kDestinationOrigin =
      url::Origin::Create(GURL(base::StringPrintf("https://%s", kHostname)));
  const net::NetworkAnonymizationKey kOtherNaks[] = {
      net::NetworkAnonymizationKey(),
      net::NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(kDestinationOrigin)),
  };
  for (const auto& other_nak : kOtherNaks) {
    std::unique_ptr<net::HostResolver::ResolveHostRequest> request2 =
        host_resolver.CreateRequest(kHostPortPair, other_nak,
                                    net::NetLogWithSource(), params);
    net::TestCompletionCallback callback2;
    result = request2->Start(callback2.callback());
    EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, callback2.GetResult(result));
  }
}

TEST_F(NetworkContextTest, P2PHostResolutionWithFamily) {
  net::NetworkAnonymizationKey network_anonymization_key =
      net::NetworkAnonymizationKey::CreateTransient();
  auto context_builder = CreateTestURLRequestContextBuilder();
  std::unique_ptr<net::MockHostResolver> resolver =
      std::make_unique<net::MockHostResolver>();
  auto& host_resolver = *resolver.get();
  context_builder->set_host_resolver(std::move(resolver));
  std::unique_ptr<net::URLRequestContext> url_request_context =
      context_builder->Build();

  network_context_remote_.reset();
  std::unique_ptr<NetworkContext> network_context =
      std::make_unique<NetworkContext>(
          network_service_.get(),
          network_context_remote_.BindNewPipeAndPassReceiver(),
          url_request_context.get(),
          std::vector<std::string>() /* cors_exempt_header_list */);

  MockP2PTrustedSocketManagerClient client;
  mojo::Receiver<network::mojom::P2PTrustedSocketManagerClient> receiver(
      &client);
  mojo::Remote<mojom::P2PTrustedSocketManager> trusted_socket_manager;
  mojo::Remote<mojom::P2PSocketManager> socket_manager;
  network_context_remote_->CreateP2PSocketManager(
      network_anonymization_key, receiver.BindNewPipeAndPassRemote(),
      trusted_socket_manager.BindNewPipeAndPassReceiver(),
      socket_manager.BindNewPipeAndPassReceiver());

  const char kIPv4Hostname[] = "ipv4.test.";
  net::MockHostResolverBase::RuleResolver::RuleKey ipv4_key;
  ipv4_key.hostname_pattern = kIPv4Hostname;
  ipv4_key.query_type = net::DnsQueryType::A;
  net::IPAddress ipv4_address;
  ASSERT_TRUE(ipv4_address.AssignFromIPLiteral("1.2.3.4"));
  host_resolver.rules()->AddRule(ipv4_key, ipv4_address.ToString());

  const char kIPv6Hostname[] = "ipv6.test.";
  net::MockHostResolverBase::RuleResolver::RuleKey ipv6_key;
  ipv6_key.hostname_pattern = kIPv6Hostname;
  ipv6_key.query_type = net::DnsQueryType::AAAA;
  net::IPAddress ipv6_address;
  ASSERT_TRUE(ipv6_address.AssignFromIPLiteral("::1234:5678"));
  host_resolver.rules()->AddRule(ipv6_key, ipv6_address.ToString());

  {
    base::RunLoop run_loop;
    std::vector<net::IPAddress> results;
    // Expect IPv4 address when family passed as AF_INET.
    socket_manager->GetHostAddressWithFamily(
        kIPv4Hostname, AF_INET, false /* enable_mdns */,
        base::BindLambdaForTesting(
            [&](const std::vector<net::IPAddress>& addresses) {
              EXPECT_EQ(std::vector<net::IPAddress>{ipv4_address}, addresses);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    std::vector<net::IPAddress> results;
    // Expect IPv6 address when family passed as AF_INET6.
    socket_manager->GetHostAddressWithFamily(
        kIPv6Hostname, AF_INET6, false /* enable_mdns */,
        base::BindLambdaForTesting(
            [&](const std::vector<net::IPAddress>& addresses) {
              EXPECT_EQ(std::vector<net::IPAddress>{ipv6_address}, addresses);
              run_loop.Quit();
            }));
    run_loop.Run();
  }
}
#endif  // BUILDFLAG(IS_P2P_ENABLED)

// Test that valid referrers are allowed, while invalid ones result in errors.
TEST_F(NetworkContextTest, Referrers) {
  const GURL kReferrer = GURL("http://referrer/");
  net::test_server::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  for (bool validate_referrer_policy_on_initial_request : {false, true}) {
    for (net::ReferrerPolicy referrer_policy :
         {net::ReferrerPolicy::NEVER_CLEAR, net::ReferrerPolicy::NO_REFERRER}) {
      mojom::NetworkContextParamsPtr context_params =
          CreateNetworkContextParamsForTesting();
      context_params->validate_referrer_policy_on_initial_request =
          validate_referrer_policy_on_initial_request;
      std::unique_ptr<NetworkContext> network_context =
          CreateContextWithParams(std::move(context_params));

      mojo::Remote<mojom::URLLoaderFactory> loader_factory;
      mojom::URLLoaderFactoryParamsPtr params =
          mojom::URLLoaderFactoryParams::New();
      params->process_id = 0;
      network_context->CreateURLLoaderFactory(
          loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

      ResourceRequest request;
      request.url = test_server.GetURL("/echoheader?Referer");
      request.referrer = kReferrer;
      request.referrer_policy = referrer_policy;

      mojo::PendingRemote<mojom::URLLoader> loader;
      TestURLLoaderClient client;
      loader_factory->CreateLoaderAndStart(
          loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
          0 /* options */, request, client.CreateRemote(),
          net::MutableNetworkTrafficAnnotationTag(
              TRAFFIC_ANNOTATION_FOR_TESTS));

      client.RunUntilComplete();
      EXPECT_TRUE(client.has_received_completion());

      // If validating referrers, and the referrer policy is not to send
      // referrers, the request should fail.
      if (validate_referrer_policy_on_initial_request &&
          referrer_policy == net::ReferrerPolicy::NO_REFERRER) {
        EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT,
                  client.completion_status().error_code);
        EXPECT_FALSE(client.response_body().is_valid());
        continue;
      }

      // Otherwise, the request should succeed.
      EXPECT_EQ(net::OK, client.completion_status().error_code);
      std::string response_body;
      ASSERT_TRUE(client.response_body().is_valid());
      EXPECT_TRUE(mojo::BlockingCopyToString(client.response_body_release(),
                                             &response_body));
      if (referrer_policy == net::ReferrerPolicy::NO_REFERRER) {
        // If not validating referrers, and the referrer policy is not to send
        // referrers, the referrer should be cleared.
        EXPECT_EQ("None", response_body);
      } else {
        // Otherwise, the referrer should be send.
        EXPECT_EQ(kReferrer.spec(), response_body);
      }
    }
  }
}

// Validates that clearing the HTTP cache when no cache exists does complete.
TEST_F(NetworkContextTest, ClearHttpCacheWithNoCache) {
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->http_cache_enabled = false;
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();
  ASSERT_EQ(nullptr, cache);
  base::RunLoop run_loop;
  network_context->ClearHttpCache(base::Time(), base::Time(),
                                  nullptr /* filter */,
                                  base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearHttpCache) {
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_directory = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::HttpCache* cache = network_context->url_request_context()
                              ->http_transaction_factory()
                              ->GetCache();

  std::vector<std::string> entry_urls = {
      "http://www.google.com",    "https://www.google.com",
      "http://www.wikipedia.com", "https://www.wikipedia.com",
      "http://localhost:1234",    "https://localhost:1234",
  };
  ASSERT_TRUE(cache);
  disk_cache::Backend* backend = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->GetBackend(&backend, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));
  ASSERT_TRUE(backend);

  for (const auto& url : entry_urls) {
    disk_cache::EntryResult result;
    base::RunLoop run_loop;

    result = backend->CreateEntry(
        url, net::HIGHEST,
        base::BindLambdaForTesting([&](disk_cache::EntryResult got_result) {
          result = std::move(got_result);
          run_loop.Quit();
        }));
    if (result.net_error() == net::ERR_IO_PENDING)
      run_loop.Run();

    result.ReleaseEntry()->Close();
  }
  EXPECT_EQ(entry_urls.size(), static_cast<size_t>(backend->GetEntryCount()));
  base::RunLoop run_loop;
  network_context->ClearHttpCache(base::Time(), base::Time(),
                                  nullptr /* filter */,
                                  base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(0U, static_cast<size_t>(backend->GetEntryCount()));
}

// Checks that when multiple calls are made to clear the HTTP cache, all
// callbacks are invoked.
TEST_F(NetworkContextTest, MultipleClearHttpCacheCalls) {
  constexpr int kNumberOfClearCalls = 10;

  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->http_cache_enabled = true;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  context_params->http_cache_directory = temp_dir.GetPath();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      kNumberOfClearCalls /* num_closures */, run_loop.QuitClosure());
  for (int i = 0; i < kNumberOfClearCalls; i++) {
    network_context->ClearHttpCache(base::Time(), base::Time(),
                                    nullptr /* filter */,
                                    base::BindOnce(barrier_closure));
  }
  run_loop.Run();
  // If all the callbacks were invoked, we should terminate.
}

TEST_F(NetworkContextTest, NotifyExternalCacheHit) {
  const std::vector<GURL> kUrls = {
      GURL("http://www.google.com/"),
      GURL("http://www.wikipedia.com/"),
      GURL("http://localhost:1234/"),
  };
  const url::Origin kOrigin = url::Origin::Create(GURL("http://a.com"));
  constexpr base::Time kNow1 = base::Time::UnixEpoch() + base::Hours(18);
  constexpr base::Time kNow2 = base::Time::UnixEpoch() + base::Hours(11);

  for (bool enabled : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    if (enabled) {
      feature_list.InitAndEnableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    }

    for (const GURL& url : kUrls) {
      for (bool is_subframe_document_resource : {false, true}) {
        mojom::NetworkContextParamsPtr context_params =
            CreateNetworkContextParamsForTesting();
        context_params->http_cache_enabled = true;
        std::unique_ptr<NetworkContext> network_context =
            CreateContextWithParams(std::move(context_params));
        base::SimpleTestClock clock;
        net::HttpCache* cache = network_context->url_request_context()
                                    ->http_transaction_factory()
                                    ->GetCache();
        disk_cache::Backend* backend = nullptr;
        // We expect that every cache operation below is done synchronously
        // because we're using an in-memory backend.

        // The disk cache is lazily instantiated, force it and ensure it's
        // valid.
        ASSERT_EQ(cache->GetBackend(&backend, base::BindOnce([](int rv) {})),
                  net::OK);
        ASSERT_NE(backend, nullptr);
        static_cast<disk_cache::MemBackendImpl*>(backend)->SetClockForTesting(
            &clock);

        clock.SetNow(kNow1);
        net::NetworkIsolationKey isolation_key(kOrigin, kOrigin);
        net::HttpRequestInfo request_info;
        request_info.url = url;
        request_info.is_subframe_document_resource =
            is_subframe_document_resource;
        request_info.network_isolation_key = isolation_key;
        request_info.network_anonymization_key =
            net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
                isolation_key);
        disk_cache::EntryResult result = backend->OpenOrCreateEntry(
            *net::HttpCache::GenerateCacheKeyForRequest(&request_info),
            net::LOWEST, base::BindOnce([](disk_cache::EntryResult) {}));
        ASSERT_EQ(result.net_error(), net::OK);

        disk_cache::ScopedEntryPtr entry(result.ReleaseEntry());
        EXPECT_EQ(entry->GetLastUsed(), kNow1);

        clock.SetNow(kNow2);
        network_context->NotifyExternalCacheHit(
            url, url.scheme(), isolation_key, is_subframe_document_resource,
            /*include_credentials=*/true);

        EXPECT_EQ(entry->GetLastUsed(), kNow2);
      }
    }
  }
}

TEST_F(NetworkContextTest, CountHttpCache) {
  // Just ensure that a couple of concurrent calls go through, and produce
  // the expected "it's empty!" result. More detailed testing is left to
  // HttpCacheDataCounter unit tests.

  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->http_cache_enabled = true;

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  int responses = 0;
  base::RunLoop run_loop;

  auto callback =
      base::BindLambdaForTesting([&](bool upper_bound, int64_t size_or_error) {
        // Don't expect approximation for full range.
        EXPECT_EQ(false, upper_bound);
        EXPECT_EQ(0, size_or_error);
        ++responses;
        if (responses == 2)
          run_loop.Quit();
      });

  network_context->ComputeHttpCacheSize(base::Time(), base::Time::Max(),
                                        callback);
  network_context->ComputeHttpCacheSize(base::Time(), base::Time::Max(),
                                        callback);
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearHostCache) {
  // List of domains added to the host cache before running each test case.
  const char* kDomains[] = {
      "domain0",
      "domain1",
      "domain2",
      "domain3",
  };

  // Each bit corresponds to one of the 4 domains above.
  enum Domains {
    NO_DOMAINS = 0x0,
    DOMAIN0 = 0x1,
    DOMAIN1 = 0x2,
    DOMAIN2 = 0x4,
    DOMAIN3 = 0x8,
  };

  const struct {
    // True if the ClearDataFilter should be a nullptr.
    bool null_filter;
    mojom::ClearDataFilter::Type type;
    // Bit field of Domains that appear in the filter. The origin vector is
    // never populated.
    int filter_domains;
    // Only domains that are expected to remain in the host cache.
    int expected_cached_domains;
  } kTestCases[] = {
      // A null filter should delete everything. The filter type and filter
      // domain lists are ignored.
      {
          true /* null_filter */, mojom::ClearDataFilter::Type::KEEP_MATCHES,
          NO_DOMAINS /* filter_domains */,
          NO_DOMAINS /* expected_cached_domains */
      },
      // An empty DELETE_MATCHES filter should delete nothing.
      {
          false /* null_filter */, mojom::ClearDataFilter::Type::DELETE_MATCHES,
          NO_DOMAINS /* filter_domains */,
          DOMAIN0 | DOMAIN1 | DOMAIN2 | DOMAIN3 /* expected_cached_domains */
      },
      // An empty KEEP_MATCHES filter should delete everything.
      {
          false /* null_filter */, mojom::ClearDataFilter::Type::KEEP_MATCHES,
          NO_DOMAINS /* filter_domains */,
          NO_DOMAINS /* expected_cached_domains */
      },
      // Test a non-empty DELETE_MATCHES filter.
      {
          false /* null_filter */, mojom::ClearDataFilter::Type::DELETE_MATCHES,
          DOMAIN0 | DOMAIN2 /* filter_domains */,
          DOMAIN1 | DOMAIN3 /* expected_cached_domains */
      },
      // Test a non-empty KEEP_MATCHES filter.
      {
          false /* null_filter */, mojom::ClearDataFilter::Type::KEEP_MATCHES,
          DOMAIN0 | DOMAIN2 /* filter_domains */,
          DOMAIN0 | DOMAIN2 /* expected_cached_domains */
      },
  };

  for (const auto& test_case : kTestCases) {
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(CreateNetworkContextParamsForTesting());
    net::HostCache* host_cache =
        network_context->url_request_context()->host_resolver()->GetHostCache();
    ASSERT_TRUE(host_cache);

    // Add the 4 test domains to the host cache, each once with scheme and once
    // without.
    for (const auto* domain : kDomains) {
      host_cache->Set(
          net::HostCache::Key(domain, net::DnsQueryType::UNSPECIFIED, 0,
                              net::HostResolverSource::ANY,
                              net::NetworkAnonymizationKey()),
          net::HostCache::Entry(net::OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                                net::HostCache::Entry::SOURCE_UNKNOWN),
          base::TimeTicks::Now(), base::Days(1));
      host_cache->Set(
          net::HostCache::Key(
              url::SchemeHostPort(url::kHttpsScheme, domain, 443),
              net::DnsQueryType::UNSPECIFIED, 0, net::HostResolverSource::ANY,
              net::NetworkAnonymizationKey()),
          net::HostCache::Entry(net::OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                                net::HostCache::Entry::SOURCE_UNKNOWN),
          base::TimeTicks::Now(), base::Days(1));
    }
    // Sanity check.
    EXPECT_EQ(std::size(kDomains) * 2, host_cache->entries().size());

    // Set up and run the filter, according to |test_case|.
    mojom::ClearDataFilterPtr clear_data_filter;
    if (!test_case.null_filter) {
      clear_data_filter = mojom::ClearDataFilter::New();
      clear_data_filter->type = test_case.type;
      for (size_t i = 0; i < std::size(kDomains); ++i) {
        if (test_case.filter_domains & (1 << i))
          clear_data_filter->domains.push_back(kDomains[i]);
      }
    }
    base::RunLoop run_loop;
    network_context->ClearHostCache(std::move(clear_data_filter),
                                    run_loop.QuitClosure());
    run_loop.Run();

    // Check that only the expected domains remain in the cache.
    size_t expected_cached = 0;
    for (size_t i = 0; i < std::size(kDomains); ++i) {
      bool expect_domain_cached =
          ((test_case.expected_cached_domains & (1 << i)) != 0);
      EXPECT_EQ(expect_domain_cached,
                (host_cache->GetMatchingKeyForTesting(
                     kDomains[i], nullptr /* source_out */,
                     nullptr /* stale_out */) != nullptr));
      if (expect_domain_cached)
        expected_cached += 2;
    }

    EXPECT_EQ(host_cache->entries().size(), expected_cached);
  }
}

TEST_F(NetworkContextTest, ClearHttpAuthCache) {
  url::SchemeHostPort scheme_host_port(GURL("http://google.com"));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:00:00", &start_time));
  base::SimpleTestClock test_clock;
  test_clock.SetNow(start_time);
  cache->set_clock_for_testing(&test_clock);

  std::u16string user = u"user";
  std::u16string password = u"pass";
  cache->Add(scheme_host_port, net::HttpAuth::AUTH_SERVER, "Realm1",
             net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkAnonymizationKey(),
             "basic realm=Realm1", net::AuthCredentials(user, password), "/");

  test_clock.Advance(base::Hours(1));  // Time now 13:00
  cache->Add(scheme_host_port, net::HttpAuth::AUTH_PROXY, "Realm2",
             net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkAnonymizationKey(),
             "basic realm=Realm2", net::AuthCredentials(user, password), "/");

  ASSERT_EQ(2u, cache->GetEntriesSizeForTesting());
  ASSERT_NE(nullptr, cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_SERVER,
                                   "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkAnonymizationKey()));
  ASSERT_NE(nullptr, cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_PROXY,
                                   "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkAnonymizationKey()));
  {
    base::RunLoop run_loop;
    base::Time test_time;
    ASSERT_TRUE(base::Time::FromString("30 May 2018 12:30:00", &test_time));
    network_context->ClearHttpAuthCache(base::Time(), test_time,
                                        run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_EQ(1u, cache->GetEntriesSizeForTesting());
    EXPECT_EQ(nullptr,
              cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_SERVER,
                            "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC,
                            net::NetworkAnonymizationKey()));
    EXPECT_NE(nullptr,
              cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_PROXY,
                            "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC,
                            net::NetworkAnonymizationKey()));
  }
  {
    base::RunLoop run_loop;
    base::Time test_time;
    ASSERT_TRUE(base::Time::FromString("30 May 2018 12:30:00", &test_time));
    network_context->ClearHttpAuthCache(test_time, base::Time::Max(),
                                        run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_EQ(0u, cache->GetEntriesSizeForTesting());
    EXPECT_EQ(nullptr,
              cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_SERVER,
                            "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC,
                            net::NetworkAnonymizationKey()));
    EXPECT_EQ(nullptr,
              cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_PROXY,
                            "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC,
                            net::NetworkAnonymizationKey()));
  }
}

TEST_F(NetworkContextTest, ClearAllHttpAuthCache) {
  url::SchemeHostPort scheme_host_port(GURL("http://google.com"));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString("30 May 2018 12:00:00", &start_time));
  base::SimpleTestClock test_clock;
  test_clock.SetNow(start_time);
  cache->set_clock_for_testing(&test_clock);

  std::u16string user = u"user";
  std::u16string password = u"pass";
  cache->Add(scheme_host_port, net::HttpAuth::AUTH_SERVER, "Realm1",
             net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkAnonymizationKey(),
             "basic realm=Realm1", net::AuthCredentials(user, password), "/");

  test_clock.Advance(base::Hours(1));  // Time now 13:00
  cache->Add(scheme_host_port, net::HttpAuth::AUTH_PROXY, "Realm2",
             net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkAnonymizationKey(),
             "basic realm=Realm2", net::AuthCredentials(user, password), "/");

  ASSERT_EQ(2u, cache->GetEntriesSizeForTesting());
  ASSERT_NE(nullptr, cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_SERVER,
                                   "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkAnonymizationKey()));
  ASSERT_NE(nullptr, cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_PROXY,
                                   "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkAnonymizationKey()));

  base::RunLoop run_loop;
  network_context->ClearHttpAuthCache(base::Time(), base::Time::Max(),
                                      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0u, cache->GetEntriesSizeForTesting());
  EXPECT_EQ(nullptr, cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_SERVER,
                                   "Realm1", net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_PROXY,
                                   "Realm2", net::HttpAuth::AUTH_SCHEME_BASIC,
                                   net::NetworkAnonymizationKey()));
}

TEST_F(NetworkContextTest, ClearEmptyHttpAuthCache) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  ASSERT_EQ(0u, cache->GetEntriesSizeForTesting());

  base::RunLoop run_loop;
  network_context->ClearHttpAuthCache(base::Time::UnixEpoch(),
                                      base::Time::Max(),
                                      base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(0u, cache->GetEntriesSizeForTesting());
}

absl::optional<net::AuthCredentials> GetAuthCredentials(
    NetworkContext* network_context,
    const GURL& origin,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  base::RunLoop run_loop;
  absl::optional<net::AuthCredentials> result;
  network_context->LookupServerBasicAuthCredentials(
      origin, network_anonymization_key,
      base::BindLambdaForTesting(
          [&](const absl::optional<net::AuthCredentials>& credentials) {
            result = credentials;
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

TEST_F(NetworkContextTest, LookupServerBasicAuthCredentials) {
  GURL origin("http://foo.test");
  GURL origin2("http://bar.test");
  GURL origin3("http://baz.test");
  const auto network_anonymization_key1 =
      net::NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(url::Origin::Create(origin)));
  const auto network_anonymization_key2 =
      net::NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(url::Origin::Create(origin2)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  network_context->SetSplitAuthCacheByNetworkAnonymizationKey(true);
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  std::u16string user = u"user";
  std::u16string password = u"pass";
  cache->Add(url::SchemeHostPort(origin), net::HttpAuth::AUTH_SERVER, "Realm",
             net::HttpAuth::AUTH_SCHEME_BASIC, network_anonymization_key1,
             "basic realm=Realm", net::AuthCredentials(user, password), "/");
  cache->Add(url::SchemeHostPort(origin2), net::HttpAuth::AUTH_PROXY, "Realm",
             net::HttpAuth::AUTH_SCHEME_BASIC, network_anonymization_key1,
             "basic realm=Realm", net::AuthCredentials(user, password), "/");

  absl::optional<net::AuthCredentials> result = GetAuthCredentials(
      network_context.get(), origin, network_anonymization_key1);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(user, result->username());
  EXPECT_EQ(password, result->password());

  // Nothing should be returned when using a different NIK.
  EXPECT_FALSE(GetAuthCredentials(network_context.get(), origin,
                                  network_anonymization_key2)
                   .has_value());

  // Proxy credentials should not be returned
  result = GetAuthCredentials(network_context.get(), origin2,
                              network_anonymization_key1);
  EXPECT_FALSE(result.has_value());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
absl::optional<net::AuthCredentials> GetProxyAuthCredentials(
    NetworkContext* network_context,
    const net::ProxyServer& proxy_server,
    const std::string& scheme,
    const std::string& realm) {
  base::RunLoop run_loop;
  absl::optional<net::AuthCredentials> result;
  network_context->LookupProxyAuthCredentials(
      proxy_server, scheme, realm,
      base::BindLambdaForTesting(
          [&](const absl::optional<net::AuthCredentials>& credentials) {
            result = credentials;
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

TEST_F(NetworkContextTest, LookupProxyAuthCredentials) {
  GURL http_proxy("http://bar.test:1080");
  GURL https_proxy("https://bar.test:443");
  GURL http_proxy2("http://bar.test:443");
  GURL foo_proxy("foo://bar.test:1080");
  GURL server_origin("http://foo.test:3128");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  network_context->SetSplitAuthCacheByNetworkAnonymizationKey(true);
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  std::u16string user = u"user";
  std::u16string password = u"pass";
  cache->Add(url::SchemeHostPort(http_proxy), net::HttpAuth::AUTH_PROXY,
             "Realm", net::HttpAuth::AUTH_SCHEME_BASIC,
             net::NetworkAnonymizationKey(), "basic realm=Realm",
             net::AuthCredentials(user, password),
             /* path = */ "");
  cache->Add(url::SchemeHostPort(https_proxy), net::HttpAuth::AUTH_PROXY,
             "Realm", net::HttpAuth::AUTH_SCHEME_BASIC,
             net::NetworkAnonymizationKey(), "basic realm=Realm",
             net::AuthCredentials(user, password),
             /* path = */ "");
  cache->Add(url::SchemeHostPort(server_origin), net::HttpAuth::AUTH_SERVER,
             "Realm", net::HttpAuth::AUTH_SCHEME_BASIC,
             net::NetworkAnonymizationKey(), "basic realm=Realm",
             net::AuthCredentials(user, password),
             /* path = */ "/");
  absl::optional<net::AuthCredentials> result = GetProxyAuthCredentials(
      network_context.get(),
      net::ProxyServer(net::ProxyServer::Scheme::SCHEME_HTTP,
                       net::HostPortPair::FromURL(http_proxy)),
      "bAsIc", "Realm");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(user, result->username());
  EXPECT_EQ(password, result->password());

  result = GetProxyAuthCredentials(
      network_context.get(),
      net::ProxyServer(net::ProxyServer::Scheme::SCHEME_HTTPS,
                       net::HostPortPair::FromURL(https_proxy)),
      "bAsIc", "Realm");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(user, result->username());
  EXPECT_EQ(password, result->password());

  // Check that the proxy scheme is taken into account when looking for
  // credentials
  result = GetProxyAuthCredentials(
      network_context.get(),
      net::ProxyServer(net::ProxyServer::Scheme::SCHEME_HTTP,
                       net::HostPortPair::FromURL(http_proxy2)),
      "basic", "Realm");
  EXPECT_FALSE(result.has_value());

  // Check that the proxy authentication method is taken into account when
  // looking for credentials
  result = GetProxyAuthCredentials(
      network_context.get(),
      net::ProxyServer(net::ProxyServer::Scheme::SCHEME_HTTP,
                       net::HostPortPair::FromURL(http_proxy)),
      "digest", "Realm");
  EXPECT_FALSE(result.has_value());

  // Check that the realm is taken into account when looking for credentials
  result = GetProxyAuthCredentials(
      network_context.get(),
      net::ProxyServer(net::ProxyServer::Scheme::SCHEME_HTTP,
                       net::HostPortPair::FromURL(http_proxy)),
      "basic", "Realm 2");
  EXPECT_FALSE(result.has_value());

  // All non-https proxies are cached as "http://" proxies
  result = GetProxyAuthCredentials(
      network_context.get(),
      net::ProxyServer(net::ProxyServer::Scheme::SCHEME_HTTP,
                       net::HostPortPair::FromURL(foo_proxy)),
      "basic", "Realm");
  EXPECT_FALSE(result.has_value());

  // Server credentials should not be returned
  result = GetProxyAuthCredentials(
      network_context.get(),
      net::ProxyServer(net::ProxyServer::Scheme::SCHEME_HTTP,
                       net::HostPortPair::FromURL(server_origin)),
      "basic", "Realm");
  EXPECT_FALSE(result.has_value());
}
#endif

#if BUILDFLAG(ENABLE_REPORTING)
TEST_F(NetworkContextTest, ClearReportingCacheReports) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<NetworkContext> network_context = CreateContextWithParams(
      CreateNetworkContextParamsForTesting(),
      net::ReportingService::CreateForTesting(std::move(reporting_context)));

  GURL domain("http://google.com");
  network_context->url_request_context()->reporting_service()->QueueReport(
      domain, absl::nullopt, net::NetworkAnonymizationKey(), "Mozilla/1.0",
      "group", "type", base::Value::Dict(), 0);

  std::vector<const net::ReportingReport*> reports;
  reporting_cache->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetReports(&reports);
  EXPECT_EQ(0u, reports.size());
}

TEST_F(NetworkContextTest, ClearReportingCacheReportsWithFilter) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<NetworkContext> network_context = CreateContextWithParams(
      CreateNetworkContextParamsForTesting(),
      net::ReportingService::CreateForTesting(std::move(reporting_context)));

  net::ReportingService* reporting_service =
      network_context->url_request_context()->reporting_service();
  GURL url1("http://google.com");
  reporting_service->QueueReport(url1, absl::nullopt,
                                 net::NetworkAnonymizationKey(), "Mozilla/1.0",
                                 "group", "type", base::Value::Dict(), 0);
  GURL url2("http://chromium.org");
  reporting_service->QueueReport(url2, absl::nullopt,
                                 net::NetworkAnonymizationKey(), "Mozilla/1.0",
                                 "group", "type", base::Value::Dict(), 0);

  std::vector<const net::ReportingReport*> reports;
  reporting_cache->GetReports(&reports);
  ASSERT_EQ(2u, reports.size());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("chromium.org");

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(std::move(filter),
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());
  EXPECT_EQ(url2, reports.front()->url);
}

TEST_F(NetworkContextTest,
       ClearReportingCacheReportsWithNonRegisterableFilter) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<NetworkContext> network_context = CreateContextWithParams(
      CreateNetworkContextParamsForTesting(),
      net::ReportingService::CreateForTesting(std::move(reporting_context)));

  net::ReportingService* reporting_service =
      network_context->url_request_context()->reporting_service();
  GURL url1("http://192.168.0.1");
  reporting_service->QueueReport(url1, absl::nullopt,
                                 net::NetworkAnonymizationKey(), "Mozilla/1.0",
                                 "group", "type", base::Value::Dict(), 0);
  GURL url2("http://192.168.0.2");
  reporting_service->QueueReport(url2, absl::nullopt,
                                 net::NetworkAnonymizationKey(), "Mozilla/1.0",
                                 "group", "type", base::Value::Dict(), 0);

  std::vector<const net::ReportingReport*> reports;
  reporting_cache->GetReports(&reports);
  ASSERT_EQ(2u, reports.size());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("192.168.0.2");

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(std::move(filter),
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());
  EXPECT_EQ(url2, reports.front()->url);
}

TEST_F(NetworkContextTest, ClearEmptyReportingCacheReports) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<NetworkContext> network_context = CreateContextWithParams(
      CreateNetworkContextParamsForTesting(),
      net::ReportingService::CreateForTesting(std::move(reporting_context)));

  std::vector<const net::ReportingReport*> reports;
  reporting_cache->GetReports(&reports);
  ASSERT_TRUE(reports.empty());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  reporting_cache->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_F(NetworkContextTest, ClearReportingCacheReportsWithNoService) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kReporting);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ASSERT_EQ(nullptr,
            network_context->url_request_context()->reporting_service());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheReports(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearReportingCacheClients) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<NetworkContext> network_context = CreateContextWithParams(
      CreateNetworkContextParamsForTesting(),
      net::ReportingService::CreateForTesting(std::move(reporting_context)));

  GURL domain("https://google.com");
  net::ReportingEndpointGroupKey group_key(
      net::NetworkAnonymizationKey(), url::Origin::Create(domain), "group");
  reporting_cache->SetEndpointForTesting(
      group_key, domain, net::OriginSubdomains::DEFAULT, base::Time::Max(),
      1 /* priority */, 1 /* weight */);

  ASSERT_EQ(1u, reporting_cache->GetEndpointCount());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0u, reporting_cache->GetEndpointCount());
}

TEST_F(NetworkContextTest, ClearReportingCacheClientsWithFilter) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<NetworkContext> network_context = CreateContextWithParams(
      CreateNetworkContextParamsForTesting(),
      net::ReportingService::CreateForTesting(std::move(reporting_context)));

  GURL domain1("https://google.com");
  net::ReportingEndpointGroupKey group_key1(
      net::NetworkAnonymizationKey(), url::Origin::Create(domain1), "group");
  reporting_cache->SetEndpointForTesting(
      group_key1, domain1, net::OriginSubdomains::DEFAULT, base::Time::Max(),
      1 /* priority */, 1 /* weight */);
  GURL domain2("https://chromium.org");
  net::ReportingEndpointGroupKey group_key2(
      net::NetworkAnonymizationKey(), url::Origin::Create(domain2), "group");
  reporting_cache->SetEndpointForTesting(
      group_key2, domain2, net::OriginSubdomains::DEFAULT, base::Time::Max(),
      1 /* priority */, 1 /* weight */);

  ASSERT_EQ(2u, reporting_cache->GetEndpointCount());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("chromium.org");

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(std::move(filter),
                                              run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(1u, reporting_cache->GetEndpointCount());
  EXPECT_TRUE(reporting_cache->GetEndpointForTesting(group_key2, domain2));
  EXPECT_FALSE(reporting_cache->GetEndpointForTesting(group_key1, domain1));
}

TEST_F(NetworkContextTest, ClearEmptyReportingCacheClients) {
  auto reporting_context = std::make_unique<net::TestReportingContext>(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance(),
      net::ReportingPolicy());
  net::ReportingCache* reporting_cache = reporting_context->cache();
  std::unique_ptr<NetworkContext> network_context = CreateContextWithParams(
      CreateNetworkContextParamsForTesting(),
      net::ReportingService::CreateForTesting(std::move(reporting_context)));

  ASSERT_EQ(0u, reporting_cache->GetEndpointCount());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_EQ(0u, reporting_cache->GetEndpointCount());
}

TEST_F(NetworkContextTest, ClearReportingCacheClientsWithNoService) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kReporting);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ASSERT_EQ(nullptr,
            network_context->url_request_context()->reporting_service());

  base::RunLoop run_loop;
  network_context->ClearReportingCacheClients(nullptr /* filter */,
                                              run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(NetworkContextTest, ClearNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  net::NetworkErrorLoggingService* logging_service =
      network_context->url_request_context()->network_error_logging_service();
  ASSERT_TRUE(logging_service);

  GURL domain("https://google.com");
  logging_service->OnHeader(net::NetworkAnonymizationKey(),
                            url::Origin::Create(domain),
                            net::IPAddress(192, 168, 0, 1),
                            "{\"report_to\":\"group\",\"max_age\":86400}");

  ASSERT_EQ(1u, logging_service->GetPolicyKeysForTesting().size());

  base::RunLoop run_loop;
  network_context->ClearNetworkErrorLogging(nullptr /* filter */,
                                            run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(logging_service->GetPolicyKeysForTesting().empty());
}

TEST_F(NetworkContextTest, ClearNetworkErrorLoggingWithFilter) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  net::NetworkErrorLoggingService* logging_service =
      network_context->url_request_context()->network_error_logging_service();
  ASSERT_TRUE(logging_service);

  GURL domain1("https://google.com");
  logging_service->OnHeader(net::NetworkAnonymizationKey(),
                            url::Origin::Create(domain1),
                            net::IPAddress(192, 168, 0, 1),
                            "{\"report_to\":\"group\",\"max_age\":86400}");
  GURL domain2("https://chromium.org");
  logging_service->OnHeader(net::NetworkAnonymizationKey(),
                            url::Origin::Create(domain2),
                            net::IPAddress(192, 168, 0, 1),
                            "{\"report_to\":\"group\",\"max_age\":86400}");

  ASSERT_EQ(2u, logging_service->GetPolicyKeysForTesting().size());

  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("chromium.org");

  base::RunLoop run_loop;
  network_context->ClearNetworkErrorLogging(std::move(filter),
                                            run_loop.QuitClosure());
  run_loop.Run();

  std::set<net::NetworkErrorLoggingService::NelPolicyKey> policy_keys =
      logging_service->GetPolicyKeysForTesting();
  EXPECT_EQ(1u, policy_keys.size());
  EXPECT_THAT(
      policy_keys,
      testing::ElementsAre(net::NetworkErrorLoggingService::NelPolicyKey(
          net::NetworkAnonymizationKey(), url::Origin::Create(domain2))));
}

TEST_F(NetworkContextTest, ClearEmptyNetworkErrorLogging) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  net::NetworkErrorLoggingService* logging_service =
      network_context->url_request_context()->network_error_logging_service();
  ASSERT_TRUE(logging_service);

  ASSERT_TRUE(logging_service->GetPolicyKeysForTesting().empty());

  base::RunLoop run_loop;
  network_context->ClearNetworkErrorLogging(nullptr /* filter */,
                                            run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(logging_service->GetPolicyKeysForTesting().empty());
}

TEST_F(NetworkContextTest, ClearEmptyNetworkErrorLoggingWithNoService) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(features::kNetworkErrorLogging);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ASSERT_FALSE(
      network_context->url_request_context()->network_error_logging_service());

  base::RunLoop run_loop;
  network_context->ClearNetworkErrorLogging(nullptr /* filter */,
                                            run_loop.QuitClosure());
  run_loop.Run();
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

void SetCookieCallback(base::RunLoop* run_loop,
                       bool* result_out,
                       net::CookieAccessResult result) {
  *result_out = result.status.IsInclude();
  run_loop->Quit();
}

void GetCookieListCallback(
    base::RunLoop* run_loop,
    net::CookieList* result_out,
    const net::CookieAccessResultList& result,
    const net::CookieAccessResultList& excluded_cookies) {
  *result_out = net::cookie_util::StripAccessResults(result);
  run_loop->Quit();
}

bool SetCookieHelper(NetworkContext* network_context,
                     const GURL& url,
                     const std::string& key,
                     const std::string& value) {
  mojo::Remote<mojom::CookieManager> cookie_manager;
  network_context->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());
  base::RunLoop run_loop;
  bool result = false;
  cookie_manager->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          key, value, url.host(), "/", base::Time(), base::Time(), base::Time(),
          base::Time(), true, false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_LOW, false),
      url, net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&SetCookieCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

TEST_F(NetworkContextTest, CookieManager) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  mojo::Remote<mojom::CookieManager> cookie_manager_remote;
  network_context->GetCookieManager(
      cookie_manager_remote.BindNewPipeAndPassReceiver());

  // Set a cookie through the cookie interface.
  base::RunLoop run_loop1;
  bool result = false;
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "TestCookie", "1", "www.test.com", "/", base::Time(), base::Time(),
      base::Time(), base::Time(), false, false, net::CookieSameSite::LAX_MODE,
      net::COOKIE_PRIORITY_LOW, false);
  cookie_manager_remote->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&SetCookieCallback, &run_loop1, &result));
  run_loop1.Run();
  EXPECT_TRUE(result);

  // Confirm that cookie is visible directly through the store associated with
  // the network context.
  base::RunLoop run_loop2;
  net::CookieList cookies;
  network_context->url_request_context()
      ->cookie_store()
      ->GetCookieListWithOptionsAsync(
          GURL("http://www.test.com/whatever"),
          net::CookieOptions::MakeAllInclusive(),
          net::CookiePartitionKeyCollection(),
          base::BindOnce(&GetCookieListCallback, &run_loop2, &cookies));
  run_loop2.Run();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("TestCookie", cookies[0].Name());
}

TEST_F(NetworkContextTest, ProxyConfig) {
  // Each ProxyConfigSet consists of a net::ProxyConfig, and the net::ProxyInfos
  // that it will result in for http and ftp URLs. All that matters is that each
  // ProxyConfig is different. It's important that none of these configs require
  // fetching a PAC scripts, as this test checks
  // ConfiguredProxyResolutionService::config(), which is only updated after
  // fetching PAC scripts (if applicable).
  struct ProxyConfigSet {
    net::ProxyConfig proxy_config;
    net::ProxyInfo http_proxy_info;
    net::ProxyInfo ftp_proxy_info;
  } proxy_config_sets[3];

  proxy_config_sets[0].proxy_config.proxy_rules().ParseFromString(
      "http=foopy:80");
  proxy_config_sets[0].http_proxy_info.UsePacString("PROXY foopy:80");
  proxy_config_sets[0].ftp_proxy_info.UseDirect();

  proxy_config_sets[1].proxy_config.proxy_rules().ParseFromString(
      "http=foopy:80;ftp=foopy2");
  proxy_config_sets[1].http_proxy_info.UsePacString("PROXY foopy:80");
  proxy_config_sets[1].ftp_proxy_info.UsePacString("PROXY foopy2");

  proxy_config_sets[2].proxy_config = net::ProxyConfig::CreateDirect();
  proxy_config_sets[2].http_proxy_info.UseDirect();
  proxy_config_sets[2].ftp_proxy_info.UseDirect();

  // Sanity check.
  EXPECT_FALSE(proxy_config_sets[0].proxy_config.Equals(
      proxy_config_sets[1].proxy_config));
  EXPECT_FALSE(proxy_config_sets[0].proxy_config.Equals(
      proxy_config_sets[2].proxy_config));
  EXPECT_FALSE(proxy_config_sets[1].proxy_config.Equals(
      proxy_config_sets[2].proxy_config));

  // Try each proxy config as the initial config, to make sure setting the
  // initial config works.
  for (const auto& initial_proxy_config_set : proxy_config_sets) {
    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        initial_proxy_config_set.proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    mojo::Remote<mojom::ProxyConfigClient> config_client;
    context_params->proxy_config_client_receiver =
        config_client.BindNewPipeAndPassReceiver();
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    net::ConfiguredProxyResolutionService* proxy_resolution_service = nullptr;
    ASSERT_TRUE(network_context->url_request_context()
                    ->proxy_resolution_service()
                    ->CastToConfiguredProxyResolutionService(
                        &proxy_resolution_service));
    // Need to do proxy resolutions before can check the ProxyConfig, as the
    // ProxyService doesn't start updating its config until it's first used.
    // This also gives some test coverage of LookUpProxyForURL.
    TestProxyLookupClient http_proxy_lookup_client;
    http_proxy_lookup_client.StartLookUpProxyForURL(
        GURL("http://foo"), net::NetworkAnonymizationKey(),
        network_context.get());
    http_proxy_lookup_client.WaitForResult();
    ASSERT_TRUE(http_proxy_lookup_client.proxy_info());
    EXPECT_EQ(initial_proxy_config_set.http_proxy_info.ToPacString(),
              http_proxy_lookup_client.proxy_info()->ToPacString());

    TestProxyLookupClient ftp_proxy_lookup_client;
    ftp_proxy_lookup_client.StartLookUpProxyForURL(
        GURL("ftp://foo"), net::NetworkAnonymizationKey(),
        network_context.get());
    ftp_proxy_lookup_client.WaitForResult();
    ASSERT_TRUE(ftp_proxy_lookup_client.proxy_info());
    EXPECT_EQ(initial_proxy_config_set.ftp_proxy_info.ToPacString(),
              ftp_proxy_lookup_client.proxy_info()->ToPacString());

    EXPECT_TRUE(proxy_resolution_service->config());
    EXPECT_TRUE(proxy_resolution_service->config()->value().Equals(
        initial_proxy_config_set.proxy_config));

    // Always go through the other configs in the same order. This has the
    // advantage of testing the case where there's no change, for
    // proxy_config[0].
    for (const auto& proxy_config_set : proxy_config_sets) {
      config_client->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
          proxy_config_set.proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
      task_environment_.RunUntilIdle();

      TestProxyLookupClient http_proxy_lookup_client2;
      http_proxy_lookup_client2.StartLookUpProxyForURL(
          GURL("http://foo"), net::NetworkAnonymizationKey(),
          network_context.get());
      http_proxy_lookup_client2.WaitForResult();
      ASSERT_TRUE(http_proxy_lookup_client2.proxy_info());
      EXPECT_EQ(proxy_config_set.http_proxy_info.ToPacString(),
                http_proxy_lookup_client2.proxy_info()->ToPacString());

      TestProxyLookupClient ftp_proxy_lookup_client2;
      ftp_proxy_lookup_client2.StartLookUpProxyForURL(
          GURL("ftp://foo"), net::NetworkAnonymizationKey(),
          network_context.get());
      ftp_proxy_lookup_client2.WaitForResult();
      ASSERT_TRUE(ftp_proxy_lookup_client2.proxy_info());
      EXPECT_EQ(proxy_config_set.ftp_proxy_info.ToPacString(),
                ftp_proxy_lookup_client2.proxy_info()->ToPacString());

      EXPECT_TRUE(proxy_resolution_service->config());
      EXPECT_TRUE(proxy_resolution_service->config()->value().Equals(
          proxy_config_set.proxy_config));
    }
  }
}

// Verify that a proxy config works without a ProxyConfigClient PendingReceiver.
TEST_F(NetworkContextTest, StaticProxyConfig) {
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString("http=foopy:80;ftp=foopy2");

  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  net::ConfiguredProxyResolutionService* proxy_resolution_service = nullptr;
  ASSERT_TRUE(
      network_context->url_request_context()
          ->proxy_resolution_service()
          ->CastToConfiguredProxyResolutionService(&proxy_resolution_service));
  // Kick the ConfiguredProxyResolutionService into action, as it doesn't start
  // updating its config until it's first used.
  proxy_resolution_service->ForceReloadProxyConfig();
  EXPECT_TRUE(proxy_resolution_service->config());
  EXPECT_TRUE(proxy_resolution_service->config()->value().Equals(proxy_config));
}

TEST_F(NetworkContextTest, NoInitialProxyConfig) {
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->initial_proxy_config.reset();
  mojo::Remote<mojom::ProxyConfigClient> config_client;
  context_params->proxy_config_client_receiver =
      config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  net::ConfiguredProxyResolutionService* proxy_resolution_service = nullptr;
  ASSERT_TRUE(
      network_context->url_request_context()
          ->proxy_resolution_service()
          ->CastToConfiguredProxyResolutionService(&proxy_resolution_service));
  EXPECT_FALSE(proxy_resolution_service->config());
  EXPECT_FALSE(proxy_resolution_service->fetched_config());

  // Before there's a proxy configuration, proxy requests should hang.
  // Create two lookups, to make sure two simultaneous lookups can be handled at
  // once.
  TestProxyLookupClient http_proxy_lookup_client;
  http_proxy_lookup_client.StartLookUpProxyForURL(
      GURL("http://foo/"), net::NetworkAnonymizationKey(),
      network_context.get());
  TestProxyLookupClient ftp_proxy_lookup_client;
  ftp_proxy_lookup_client.StartLookUpProxyForURL(GURL("ftp://foo/"),
                                                 net::NetworkAnonymizationKey(),
                                                 network_context.get());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(proxy_resolution_service->config());
  EXPECT_FALSE(proxy_resolution_service->fetched_config());
  EXPECT_FALSE(http_proxy_lookup_client.is_done());
  EXPECT_FALSE(ftp_proxy_lookup_client.is_done());
  EXPECT_EQ(2u, network_context->pending_proxy_lookup_requests_for_testing());

  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString("http=foopy:80");
  config_client->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));

  http_proxy_lookup_client.WaitForResult();
  ASSERT_TRUE(http_proxy_lookup_client.proxy_info());
  EXPECT_EQ("PROXY foopy:80",
            http_proxy_lookup_client.proxy_info()->ToPacString());

  ftp_proxy_lookup_client.WaitForResult();
  ASSERT_TRUE(ftp_proxy_lookup_client.proxy_info());
  EXPECT_EQ("DIRECT", ftp_proxy_lookup_client.proxy_info()->ToPacString());

  EXPECT_EQ(0u, network_context->pending_proxy_lookup_requests_for_testing());
}

TEST_F(NetworkContextTest, DestroyedWithoutProxyConfig) {
  // Create a NetworkContext without an initial proxy configuration.
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->initial_proxy_config.reset();
  mojo::Remote<mojom::ProxyConfigClient> config_client;
  context_params->proxy_config_client_receiver =
      config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Proxy requests should hang.
  TestProxyLookupClient proxy_lookup_client;
  proxy_lookup_client.StartLookUpProxyForURL(GURL("http://foo/"),
                                             net::NetworkAnonymizationKey(),
                                             network_context.get());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, network_context->pending_proxy_lookup_requests_for_testing());
  EXPECT_FALSE(proxy_lookup_client.is_done());

  // Destroying the NetworkContext should cause the pending lookup to fail with
  // ERR_ABORTED.
  network_context.reset();
  proxy_lookup_client.WaitForResult();
  EXPECT_FALSE(proxy_lookup_client.proxy_info());
  EXPECT_EQ(net::ERR_ABORTED, proxy_lookup_client.net_error());
}

TEST_F(NetworkContextTest, CancelPendingProxyLookup) {
  // Create a NetworkContext without an initial proxy configuration.
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->initial_proxy_config.reset();
  mojo::Remote<mojom::ProxyConfigClient> config_client;
  context_params->proxy_config_client_receiver =
      config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Proxy requests should hang.
  std::unique_ptr<TestProxyLookupClient> proxy_lookup_client =
      std::make_unique<TestProxyLookupClient>();
  proxy_lookup_client->StartLookUpProxyForURL(GURL("http://foo/"),
                                              net::NetworkAnonymizationKey(),
                                              network_context.get());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(proxy_lookup_client->is_done());
  EXPECT_EQ(1u, network_context->pending_proxy_lookup_requests_for_testing());

  // Cancelling the proxy lookup should cause the proxy lookup request objects
  // to be deleted.
  proxy_lookup_client.reset();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0u, network_context->pending_proxy_lookup_requests_for_testing());
}

// Test to make sure the NetworkAnonymizationKey passed to LookUpProxyForURL()
// makes it to the proxy resolver.
TEST_F(NetworkContextTest, ProxyLookupWithNetworkIsolationKey) {
  const GURL kUrl("http://bar.test/");
  const net::SchemefulSite kSite =
      net::SchemefulSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      net::NetworkAnonymizationKey::CreateSameSite(kSite);

  // Pac scripts must contain this string to be passed to the
  // ProxyResolverFactory.
  const std::string kPacScript("FindProxyForURL");

  // Create a NetworkContext without an initial proxy configuration.
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  CapturingMojoProxyResolverFactory proxy_resolver_factory;
  context_params->proxy_resolver_factory =
      proxy_resolver_factory.CreateFactoryRemote();
  context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
      net::ProxyConfig::CreateFromCustomPacURL(GURL("data:," + kPacScript)),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  mojo::Remote<mojom::ProxyConfigClient> config_client;
  context_params->proxy_config_client_receiver =
      config_client.BindNewPipeAndPassReceiver();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  context_params->dhcp_wpad_url_client =
      network::MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(
          std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  TestProxyLookupClient proxy_lookup_client;
  proxy_lookup_client.StartLookUpProxyForURL(kUrl, kNetworkAnonymizationKey,
                                             network_context.get());
  proxy_lookup_client.WaitForResult();
  ASSERT_TRUE(proxy_lookup_client.proxy_info());
  EXPECT_TRUE(proxy_lookup_client.proxy_info()->is_direct_only());

  EXPECT_EQ(kPacScript, proxy_resolver_factory.pac_script());
  EXPECT_EQ(kUrl, proxy_resolver_factory.url());
  EXPECT_EQ(kNetworkAnonymizationKey,
            proxy_resolver_factory.network_anonymization_key());
}

// Test mojom::ProxyResolver that completes calls to GetProxyForUrl() with a
// DIRECT "proxy". It additionally emits a script error on line 42 for every
// call to GetProxyForUrl().
class MockMojoProxyResolver : public proxy_resolver::mojom::ProxyResolver {
 public:
  MockMojoProxyResolver() {}

  MockMojoProxyResolver(const MockMojoProxyResolver&) = delete;
  MockMojoProxyResolver& operator=(const MockMojoProxyResolver&) = delete;

 private:
  // Overridden from proxy_resolver::mojom::ProxyResolver:
  void GetProxyForUrl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverRequestClient>
          pending_client) override {
    // Report a Javascript error and then complete the request successfully,
    // having chosen DIRECT connections.
    mojo::Remote<proxy_resolver::mojom::ProxyResolverRequestClient> client(
        std::move(pending_client));
    client->OnError(42, "Failed: FindProxyForURL(url=" + url.spec() + ")");

    net::ProxyInfo result;
    result.UseDirect();

    client->ReportResult(net::OK, result);
  }
};

// Test mojom::ProxyResolverFactory implementation that successfully completes
// any CreateResolver() requests, and binds the request to a new
// MockMojoProxyResolver.
class MockMojoProxyResolverFactory
    : public proxy_resolver::mojom::ProxyResolverFactory {
 public:
  MockMojoProxyResolverFactory() {}

  MockMojoProxyResolverFactory(const MockMojoProxyResolverFactory&) = delete;
  MockMojoProxyResolverFactory& operator=(const MockMojoProxyResolverFactory&) =
      delete;

  // Binds and returns a mock ProxyResolverFactory whose lifetime is bound to
  // the message pipe.
  static mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
  Create() {
    mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory> remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<MockMojoProxyResolverFactory>(),
        remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

 private:
  void CreateResolver(
      const std::string& pac_url,
      mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
      mojo::PendingRemote<
          proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
          pending_client) override {
    // Bind |receiver| to a new MockMojoProxyResolver, and return success.
    mojo::MakeSelfOwnedReceiver(std::make_unique<MockMojoProxyResolver>(),
                                std::move(receiver));

    mojo::Remote<proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
        client(std::move(pending_client));
    client->ReportResult(net::OK);
  }
};

TEST_F(NetworkContextTest, PacQuickCheck) {
  // Check the default value.
  // Note that unless we explicitly create a proxy resolver factory, the code
  // will assume that we should use a system proxy resolver (i.e. use system
  // APIs to resolve a proxy). This isn't supported on all platforms. On
  // unsupported platforms, we'd simply ignore the PAC quick check input and
  // default to false.
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  context_params->dhcp_wpad_url_client =
      network::MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(
          std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  context_params->proxy_resolver_factory =
      MockMojoProxyResolverFactory::Create();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));
  net::ConfiguredProxyResolutionService* proxy_resolution_service = nullptr;
  ASSERT_TRUE(
      network_context->url_request_context()
          ->proxy_resolution_service()
          ->CastToConfiguredProxyResolutionService(&proxy_resolution_service));
  EXPECT_TRUE(proxy_resolution_service->quick_check_enabled_for_testing());

  // Explicitly enable.
  context_params = CreateNetworkContextParamsForTesting();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  context_params->dhcp_wpad_url_client =
      network::MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(
          std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  context_params->proxy_resolver_factory =
      MockMojoProxyResolverFactory::Create();
  context_params->pac_quick_check_enabled = true;
  network_context = CreateContextWithParams(std::move(context_params));
  proxy_resolution_service = nullptr;
  ASSERT_TRUE(
      network_context->url_request_context()
          ->proxy_resolution_service()
          ->CastToConfiguredProxyResolutionService(&proxy_resolution_service));
  EXPECT_TRUE(proxy_resolution_service->quick_check_enabled_for_testing());

  // Explicitly disable.
  context_params = CreateNetworkContextParamsForTesting();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  context_params->dhcp_wpad_url_client =
      network::MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(
          std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  context_params->proxy_resolver_factory =
      MockMojoProxyResolverFactory::Create();
  context_params->pac_quick_check_enabled = false;
  network_context = CreateContextWithParams(std::move(context_params));
  proxy_resolution_service = nullptr;
  ASSERT_TRUE(
      network_context->url_request_context()
          ->proxy_resolution_service()
          ->CastToConfiguredProxyResolutionService(&proxy_resolution_service));
  EXPECT_FALSE(proxy_resolution_service->quick_check_enabled_for_testing());
}

net::IPEndPoint GetLocalHostWithAnyPort() {
  return net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 0);
}

std::vector<uint8_t> CreateTestMessage(uint8_t initial, size_t size) {
  std::vector<uint8_t> array(size);
  for (size_t i = 0; i < size; ++i)
    array[i] = static_cast<uint8_t>((i + initial) % 256);
  return array;
}

TEST_F(NetworkContextTest, CreateUDPSocket) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Create a server socket to listen for incoming datagrams.
  test::UDPSocketListenerImpl listener;
  mojo::Receiver<mojom::UDPSocketListener> listener_receiver(&listener);

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  mojo::Remote<mojom::UDPSocket> server_socket;
  network_context->CreateUDPSocket(
      server_socket.BindNewPipeAndPassReceiver(),
      listener_receiver.BindNewPipeAndPassRemote());
  test::UDPSocketTestHelper helper(&server_socket);
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, nullptr, &server_addr));

  // Create a client socket to send datagrams.
  mojo::Remote<mojom::UDPSocket> client_socket;
  network_context->CreateUDPSocket(client_socket.BindNewPipeAndPassReceiver(),
                                   mojo::NullRemote());

  net::IPEndPoint client_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper client_helper(&client_socket);
  ASSERT_EQ(net::OK,
            client_helper.ConnectSync(server_addr, nullptr, &client_addr));

  // This test assumes that the loopback interface doesn't drop UDP packets for
  // a small number of packets.
  const size_t kDatagramCount = 6;
  const size_t kDatagramSize = 255;
  server_socket->ReceiveMore(kDatagramCount);

  for (size_t i = 0; i < kDatagramCount; ++i) {
    std::vector<uint8_t> test_msg(
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize));
    int result = client_helper.SendSync(test_msg);
    EXPECT_EQ(net::OK, result);
  }

  listener.WaitForReceivedResults(kDatagramCount);
  EXPECT_EQ(kDatagramCount, listener.results().size());

  int i = 0;
  for (const auto& result : listener.results()) {
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(result.src_addr, client_addr);
    EXPECT_EQ(CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize),
              result.data.value());
    i++;
  }
}

TEST_F(NetworkContextTest, CreateRestrictedUDPSocket) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Create a server socket to listen for incoming datagrams.
  test::UDPSocketListenerImpl socket_listener;
  mojo::Receiver<mojom::UDPSocketListener> socket_listener_receiver(
      &socket_listener);

  mojo::Remote<mojom::RestrictedUDPSocket> server_socket;
  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  {
    base::test::TestFuture<int32_t, const absl::optional<net::IPEndPoint>&>
        create_future;
    network_context->CreateRestrictedUDPSocket(
        server_addr, mojom::RestrictedUDPSocketMode::BOUND,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        /*params=*/nullptr, server_socket.BindNewPipeAndPassReceiver(),
        socket_listener_receiver.BindNewPipeAndPassRemote(),
        create_future.GetCallback());
    ASSERT_EQ(create_future.Get<0>(), net::OK);
    server_addr = *create_future.Get<1>();
  }

  // Create a client socket to send datagrams.
  test::UDPSocketListenerImpl client_listener;
  mojo::Receiver<mojom::UDPSocketListener> client_listener_receiver(
      &client_listener);

  mojo::Remote<mojom::RestrictedUDPSocket> client_socket;
  net::IPEndPoint client_addr(GetLocalHostWithAnyPort());
  {
    base::test::TestFuture<int32_t, const absl::optional<net::IPEndPoint>&>
        create_future;
    network_context->CreateRestrictedUDPSocket(
        server_addr, mojom::RestrictedUDPSocketMode::CONNECTED,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        /*params=*/nullptr, client_socket.BindNewPipeAndPassReceiver(),
        client_listener_receiver.BindNewPipeAndPassRemote(),
        create_future.GetCallback());
    ASSERT_EQ(create_future.Get<0>(), net::OK);
    client_addr = *create_future.Get<1>();
  }

  // This test assumes that the loopback interface doesn't drop UDP packets for
  // a small number of packets.
  const size_t kDatagramCount = 6;
  const size_t kDatagramSize = 255;
  server_socket->ReceiveMore(kDatagramCount);

  for (size_t i = 0; i < kDatagramCount; ++i) {
    std::vector<uint8_t> test_msg(
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize));
    {
      base::test::TestFuture<int32_t> send_future;
      client_socket->Send(test_msg, send_future.GetCallback());
      ASSERT_EQ(send_future.Get(), net::OK);
    }
  }

  socket_listener.WaitForReceivedResults(kDatagramCount);
  EXPECT_EQ(kDatagramCount, socket_listener.results().size());

  int i = 0;
  for (const auto& result : socket_listener.results()) {
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(result.src_addr, client_addr);
    EXPECT_EQ(CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize),
              result.data.value());
    i++;
  }

  // And now the other way round.
  client_socket->ReceiveMore(kDatagramCount);

  for (size_t j = 0; j < kDatagramCount; ++j) {
    std::vector<uint8_t> test_msg(
        CreateTestMessage(static_cast<uint8_t>(j), kDatagramSize));
    {
      base::test::TestFuture<int32_t> send_future;
      server_socket->SendTo(
          test_msg, net::HostPortPair::FromIPEndPoint(client_addr),
          net::DnsQueryType::UNSPECIFIED, send_future.GetCallback());
      ASSERT_EQ(send_future.Get(), net::OK);
    }
  }

  client_listener.WaitForReceivedResults(kDatagramCount);
  EXPECT_EQ(kDatagramCount, client_listener.results().size());

  int j = 0;
  for (const auto& result : client_listener.results()) {
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_FALSE(result.src_addr);
    EXPECT_EQ(CreateTestMessage(static_cast<uint8_t>(j), kDatagramSize),
              result.data.value());
    j++;
  }
}

TEST_F(NetworkContextTest, CreateNetLogExporter) {
  // Basic flow around start/stop.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  mojo::Remote<mojom::NetLogExporter> net_log_exporter;
  network_context->CreateNetLogExporter(
      net_log_exporter.BindNewPipeAndPassReceiver());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath out_path(temp_dir.GetPath().AppendASCII("out.json"));
  base::File out_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(out_file.IsValid());

  base::Value::Dict dict_start;
  const char kKeyEarly[] = "early";
  const char kValEarly[] = "morning";
  dict_start.Set(kKeyEarly, kValEarly);

  net::TestCompletionCallback start_callback;
  net_log_exporter->Start(std::move(out_file), std::move(dict_start),
                          net::NetLogCaptureMode::kDefault, 100 * 1024,
                          start_callback.callback());
  EXPECT_EQ(net::OK, start_callback.WaitForResult());

  base::Value::Dict dict_late;
  const char kKeyLate[] = "late";
  const char kValLate[] = "snowval";
  dict_late.Set(kKeyLate, kValLate);

  net::TestCompletionCallback stop_callback;
  net_log_exporter->Stop(std::move(dict_late), stop_callback.callback());
  EXPECT_EQ(net::OK, stop_callback.WaitForResult());

  // Check that file got written.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));

  // Contents should have net constants, without the client needing any
  // net:: methods.
  EXPECT_NE(std::string::npos, contents.find("ERR_IO_PENDING")) << contents;

  // The additional stuff inject should also occur someplace.
  EXPECT_NE(std::string::npos, contents.find(kKeyEarly)) << contents;
  EXPECT_NE(std::string::npos, contents.find(kValEarly)) << contents;
  EXPECT_NE(std::string::npos, contents.find(kKeyLate)) << contents;
  EXPECT_NE(std::string::npos, contents.find(kValLate)) << contents;
}

TEST_F(NetworkContextTest, CreateNetLogExporterUnbounded) {
  // Make sure that exporting without size limit works.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  mojo::Remote<mojom::NetLogExporter> net_log_exporter;
  network_context->CreateNetLogExporter(
      net_log_exporter.BindNewPipeAndPassReceiver());

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File out_file(temp_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(out_file.IsValid());

  net::TestCompletionCallback start_callback;
  net_log_exporter->Start(std::move(out_file), base::Value::Dict(),
                          net::NetLogCaptureMode::kDefault,
                          mojom::NetLogExporter::kUnlimitedFileSize,
                          start_callback.callback());
  EXPECT_EQ(net::OK, start_callback.WaitForResult());

  net::TestCompletionCallback stop_callback;
  net_log_exporter->Stop(base::Value::Dict(), stop_callback.callback());
  EXPECT_EQ(net::OK, stop_callback.WaitForResult());

  // Check that file got written.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(temp_path, &contents));

  // Contents should have net constants, without the client needing any
  // net:: methods.
  EXPECT_NE(std::string::npos, contents.find("ERR_IO_PENDING")) << contents;

  base::DeleteFile(temp_path);
}

TEST_F(NetworkContextTest, CreateNetLogExporterErrors) {
  // Some basic state machine misuses.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  mojo::Remote<mojom::NetLogExporter> net_log_exporter;
  network_context->CreateNetLogExporter(
      net_log_exporter.BindNewPipeAndPassReceiver());

  net::TestCompletionCallback stop_callback;
  net_log_exporter->Stop(base::Value::Dict(), stop_callback.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, stop_callback.WaitForResult());

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp_file(temp_path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(temp_file.IsValid());

  net::TestCompletionCallback start_callback;
  net_log_exporter->Start(std::move(temp_file), base::Value::Dict(),
                          net::NetLogCaptureMode::kDefault, 100 * 1024,
                          start_callback.callback());
  EXPECT_EQ(net::OK, start_callback.WaitForResult());

  // Can't start twice.
  base::FilePath temp_path2;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path2));
  base::File temp_file2(
      temp_path2, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(temp_file2.IsValid());

  net::TestCompletionCallback start_callback2;
  net_log_exporter->Start(std::move(temp_file2), base::Value::Dict(),
                          net::NetLogCaptureMode::kDefault, 100 * 1024,
                          start_callback2.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, start_callback2.WaitForResult());

  base::DeleteFile(temp_path);
  base::DeleteFile(temp_path2);

  // Forgetting to stop is recovered from.
}

TEST_F(NetworkContextTest, DestroyNetLogExporterWhileCreatingScratchDir) {
  // Make sure that things behave OK if NetLogExporter is destroyed during the
  // brief window it owns the scratch directory.
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  std::unique_ptr<NetLogExporter> net_log_exporter =
      std::make_unique<NetLogExporter>(network_context.get());

  base::WaitableEvent block_mktemp(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath path = dir.Take();
  EXPECT_TRUE(base::PathExists(path));

  net_log_exporter->SetCreateScratchDirHandlerForTesting(base::BindRepeating(
      [](base::WaitableEvent* block_on,
         const base::FilePath& path) -> base::FilePath {
        base::ScopedAllowBaseSyncPrimitivesForTesting need_to_block;
        block_on->Wait();
        return path;
      },
      &block_mktemp, path));

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp_file(temp_path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(temp_file.IsValid());

  net_log_exporter->Start(std::move(temp_file), base::Value::Dict(),
                          net::NetLogCaptureMode::kDefault, 100,
                          base::BindOnce([](int) {}));
  net_log_exporter = nullptr;
  block_mktemp.Signal();

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(path));
  base::DeleteFile(temp_path);
}

net::IPEndPoint CreateExpectedEndPoint(const std::string& address,
                                       uint16_t port) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(address));
  return net::IPEndPoint(ip_address, port);
}

class TestResolveHostClient : public ResolveHostClientBase {
 public:
  TestResolveHostClient(mojo::PendingRemote<mojom::ResolveHostClient>* remote,
                        base::RunLoop* run_loop)
      : receiver_(this, remote->InitWithNewPipeAndPassReceiver()),
        complete_(false),
        run_loop_(run_loop) {
    DCHECK(run_loop_);
  }

  void CloseReceiver() { receiver_.reset(); }

  void OnComplete(int error,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    DCHECK(!complete_);

    complete_ = true;
    top_level_result_error_ = error;
    result_error_ = resolve_error_info.error;
    result_addresses_ = addresses;
    run_loop_->Quit();
  }

  bool complete() const { return complete_; }

  int top_level_result_error() const {
    DCHECK(complete_);
    return top_level_result_error_;
  }

  int result_error() const {
    DCHECK(complete_);
    return result_error_;
  }

  const absl::optional<net::AddressList>& result_addresses() const {
    DCHECK(complete_);
    return result_addresses_;
  }

 private:
  mojo::Receiver<mojom::ResolveHostClient> receiver_;

  bool complete_;
  int top_level_result_error_;
  int result_error_;
  absl::optional<net::AddressList> result_addresses_;
  const raw_ptr<base::RunLoop> run_loop_;
};

TEST_F(NetworkContextTest, ResolveHost_Sync) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  resolver->rules()->AddRule("sync.test", "1.2.3.4");
  resolver->set_synchronous_mode(true);
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("sync.test", 160)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.top_level_result_error());
  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("1.2.3.4", 160)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Async) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  resolver->rules()->AddRule("async.test", "1.2.3.4");
  resolver->set_synchronous_mode(false);
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("async.test", 160)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.top_level_result_error());
  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("1.2.3.4", 160)));
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Failure_Sync) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  resolver->rules()->AddSimulatedTimeoutFailure("example.com");
  resolver->set_synchronous_mode(true);
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("example.com", 160)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED,
            response_client.top_level_result_error());
  EXPECT_EQ(net::ERR_DNS_TIMED_OUT, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Failure_Async) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  resolver->rules()->AddSimulatedTimeoutFailure("example.com");
  resolver->set_synchronous_mode(false);
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("example.com", 160)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED,
            response_client.top_level_result_error());
  EXPECT_EQ(net::ERR_DNS_TIMED_OUT, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_NetworkAnonymizationKey) {
  const net::SchemefulSite kSite =
      net::SchemefulSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      net::NetworkAnonymizationKey::CreateSameSite(kSite);

  auto resolver = std::make_unique<net::MockHostResolver>();
  resolver->rules()->AddRule("nik.test", "1.2.3.4");
  net::MockHostResolver* raw_resolver = resolver.get();
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("nik.test", 160)),
      kNetworkAnonymizationKey, std::move(optional_parameters),
      std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("1.2.3.4", 160)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
  EXPECT_EQ(kNetworkAnonymizationKey,
            raw_resolver->last_request_network_anonymization_key());
}

TEST_F(NetworkContextTest, ResolveHost_NoControlHandle) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 80)),
      net::NetworkAnonymizationKey(), nullptr,
      std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_CloseControlHandle) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 160)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  control_handle.reset();
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 160),
                                    CreateExpectedEndPoint("::1", 160)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_Cancellation) {
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  scoped_refptr<const net::HangingHostResolver::State> state =
      resolver->state();
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ASSERT_EQ(0, state->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 80)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  control_handle->Cancel(net::ERR_ABORTED);
  run_loop.Run();

  // On cancellation, should receive an ERR_FAILED result, and the internal
  // resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_ABORTED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, state->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, ResolveHost_DestroyContext) {
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  scoped_refptr<const net::HangingHostResolver::State> state =
      resolver->state();
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ASSERT_EQ(0, state->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 80)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  network_context = nullptr;
  run_loop.Run();

  // On context destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, state->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
}

TEST_F(NetworkContextTest, ResolveHost_CloseClient) {
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  scoped_refptr<const net::HangingHostResolver::State> state =
      resolver->state();
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ASSERT_EQ(0, state->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 80)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  response_client.CloseReceiver();
  run_loop.RunUntilIdle();

  // Response pipe is closed, so no results to check. Internal request should be
  // cancelled.
  EXPECT_FALSE(response_client.complete());
  EXPECT_EQ(1, state->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

// Test factory of net::HostResolvers. Creates standard (but potentially non-
// caching) net::ContextHostResolver. Keeps pointers to all created resolvers.
class TestResolverFactory : public net::HostResolver::Factory {
 public:
  static TestResolverFactory* CreateAndSetFactory(NetworkService* service) {
    auto factory = std::make_unique<TestResolverFactory>();
    auto* factory_ptr = factory.get();
    service->set_host_resolver_factory_for_testing(std::move(factory));
    return factory_ptr;
  }

  std::unique_ptr<net::HostResolver> CreateResolver(
      net::HostResolverManager* manager,
      base::StringPiece host_mapping_rules,
      bool enable_caching) override {
    DCHECK(host_mapping_rules.empty());
    auto resolve_context = std::make_unique<net::ResolveContext>(
        /*url_request_context=*/nullptr, /*enable_caching=*/false);
    auto resolver = std::make_unique<net::ContextHostResolver>(
        manager, std::move(resolve_context));
    resolvers_.push_back(resolver.get());
    return resolver;
  }

  std::unique_ptr<net::HostResolver> CreateStandaloneResolver(
      net::NetLog* net_log,
      const net::HostResolver::ManagerOptions& options,
      base::StringPiece host_mapping_rules,
      bool enable_caching) override {
    DCHECK(host_mapping_rules.empty());
    std::unique_ptr<net::ContextHostResolver> resolver =
        net::HostResolver::CreateStandaloneContextResolver(net_log, options,
                                                           enable_caching);
    resolvers_.push_back(resolver.get());
    return resolver;
  }

  const std::vector<net::ContextHostResolver*>& resolvers() const {
    return resolvers_;
  }

  void ForgetResolvers() { resolvers_.clear(); }

 private:
  std::vector<net::ContextHostResolver*> resolvers_;
};

TEST_F(NetworkContextTest, CreateHostResolver) {
  // Inject a factory to control and capture created net::HostResolvers.
  TestResolverFactory* factory =
      TestResolverFactory::CreateAndSetFactory(network_service_.get());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Creates single shared (within the NetworkContext) internal HostResolver.
  EXPECT_EQ(1u, factory->resolvers().size());
  factory->ForgetResolvers();

  mojo::Remote<mojom::HostResolver> resolver;
  network_context->CreateHostResolver(absl::nullopt,
                                      resolver.BindNewPipeAndPassReceiver());

  // Expected to reuse shared (within the NetworkContext) internal HostResolver.
  EXPECT_TRUE(factory->resolvers().empty());

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                            net::HostPortPair("localhost", 80)),
                        net::NetworkAnonymizationKey(), nullptr,
                        std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u,
            network_context->GetNumOutstandingResolveHostRequestsForTesting());
}

TEST_F(NetworkContextTest, CreateHostResolver_CloseResolver) {
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  scoped_refptr<const net::HangingHostResolver::State> state =
      resolver->state();
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  mojo::Remote<mojom::HostResolver> resolver_remote;
  network_context->CreateHostResolver(
      absl::nullopt, resolver_remote.BindNewPipeAndPassReceiver());

  ASSERT_EQ(0, state->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver_remote->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 80)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  resolver_remote.reset();
  run_loop.Run();

  // On resolver destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, state->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
}

TEST_F(NetworkContextTest, CreateHostResolver_CloseContext) {
  // Override the HostResolver with a hanging one, so the test can ensure the
  // request won't be completed before the cancellation arrives.
  auto resolver = std::make_unique<net::HangingHostResolver>();
  scoped_refptr<const net::HangingHostResolver::State> state =
      resolver->state();
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  mojo::Remote<mojom::HostResolver> resolver_remote;
  network_context->CreateHostResolver(
      absl::nullopt, resolver_remote.BindNewPipeAndPassReceiver());

  ASSERT_EQ(0, state->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver_remote->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 80)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  // Run a bit to ensure the resolve request makes it to the resolver. Otherwise
  // the resolver will be destroyed and close its pipe before it even knows
  // about the request to send a failure.
  task_environment_.RunUntilIdle();

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);
  bool resolver_closed = false;
  auto resolver_closed_callback =
      base::BindLambdaForTesting([&]() { resolver_closed = true; });
  resolver_remote.set_disconnect_handler(resolver_closed_callback);

  network_context = nullptr;
  run_loop.Run();

  // On context destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, state->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_TRUE(resolver_closed);
}

// Config overrides are not supported on iOS.
#if !BUILDFLAG(IS_IOS)
TEST_F(NetworkContextTest, CreateHostResolverWithConfigOverrides) {
  // Inject a factory to control and capture created net::HostResolvers.
  TestResolverFactory* factory =
      TestResolverFactory::CreateAndSetFactory(network_service_.get());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Creates single shared (within the NetworkContext) internal HostResolver.
  EXPECT_EQ(1u, factory->resolvers().size());
  factory->ForgetResolvers();

  net::DnsConfigOverrides overrides;
  overrides.nameservers = std::vector<net::IPEndPoint>{
      CreateExpectedEndPoint("100.100.100.100", 22)};

  mojo::Remote<mojom::HostResolver> resolver;
  network_context->CreateHostResolver(overrides,
                                      resolver.BindNewPipeAndPassReceiver());

  // Should create 1 private resolver with a DnsClient (if DnsClient is
  // enablable for the build config).
  ASSERT_EQ(1u, factory->resolvers().size());
  net::ContextHostResolver* internal_resolver = factory->resolvers().front();

  // Override DnsClient with a basic mock.
  net::DnsConfig base_configuration;
  base_configuration.nameservers = {CreateExpectedEndPoint("12.12.12.12", 53)};
  const std::string kQueryHostname = "example.com";
  const std::string kResult = "1.2.3.4";
  net::IPAddress result;
  CHECK(result.AssignFromIPLiteral(kResult));
  net::MockDnsClientRuleList rules;
  rules.emplace_back(
      kQueryHostname, net::dns_protocol::kTypeA, false /* secure */,
      net::MockDnsClientRule::Result(
          net::BuildTestDnsAddressResponse(kQueryHostname, result)),
      false /* delay */);
  rules.emplace_back(kQueryHostname, net::dns_protocol::kTypeAAAA,
                     false /* secure */,
                     net::MockDnsClientRule::Result(
                         net::MockDnsClientRule::ResultType::kEmpty),
                     false /* delay */);
  auto mock_dns_client = std::make_unique<net::MockDnsClient>(
      base_configuration, std::move(rules));
  mock_dns_client->SetInsecureEnabled(/*enabled=*/true,
                                      /*additional_types_enabled=*/false);
  mock_dns_client->set_ignore_system_config_changes(true);
  auto* mock_dns_client_ptr = mock_dns_client.get();
  internal_resolver->GetManagerForTesting()->SetDnsClientForTesting(
      std::move(mock_dns_client));

  // Test that the DnsClient is getting the overridden configuration.
  EXPECT_TRUE(overrides.ApplyOverrides(base_configuration)
                  .Equals(*mock_dns_client_ptr->GetEffectiveConfig()));

  // Ensure we are using the private resolver by testing that we get results
  // from the overridden DnsClient.
  base::RunLoop run_loop;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->dns_query_type = net::DnsQueryType::A;
  optional_parameters->source = net::HostResolverSource::DNS;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);
  resolver->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                            net::HostPortPair(kQueryHostname, 80)),
                        net::NetworkAnonymizationKey(),
                        std::move(optional_parameters),
                        std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kResult, 80)));
}
#endif  // BUILDFLAG(IS_IOS)

TEST_F(NetworkContextTest, ActivateDohProbes) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  scoped_refptr<const net::MockHostResolver::State> state = resolver->state();
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  mojom::NetworkContextParamsPtr params =
      CreateNetworkContextParamsForTesting();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));

  ASSERT_FALSE(state->IsDohProbeRunning());

  network_context->ActivateDohProbes();
  EXPECT_TRUE(state->IsDohProbeRunning());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(state->IsDohProbeRunning());

  network_context.reset();

  EXPECT_FALSE(state->IsDohProbeRunning());
}

TEST_F(NetworkContextTest, ActivateDohProbes_NotPrimaryContext) {
  auto resolver = std::make_unique<net::MockHostResolver>();
  scoped_refptr<const net::MockHostResolver::State> state = resolver->state();
  network_service_->set_host_resolver_factory_for_testing(
      std::make_unique<HostResolverFactory>(std::move(resolver)));
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ASSERT_FALSE(state->IsDohProbeRunning());

  network_context->ActivateDohProbes();
  EXPECT_TRUE(state->IsDohProbeRunning());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(state->IsDohProbeRunning());

  network_context.reset();

  EXPECT_FALSE(state->IsDohProbeRunning());
}

TEST_F(NetworkContextTest, PrivacyModeDisabledByDefault) {
  const GURL kURL("http://foo.com");
  const GURL kOtherURL("http://other.com");
  std::unique_ptr<net::URLRequestContext> request_context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
      kURL, net::DEFAULT_PRIORITY, /*delegate=*/nullptr,
      TRAFFIC_ANNOTATION_FOR_TESTS);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateAllowed,
            network_context->url_request_context()
                ->network_delegate()
                ->ForcePrivacyMode(*request));
}

TEST_F(NetworkContextTest, PrivacyModeEnabledIfCookiesBlocked) {
  const GURL kURL("http://foo.com");
  const GURL kOtherURL("http://other.com");
  std::unique_ptr<net::URLRequestContext> request_context =
      CreateTestURLRequestContextBuilder()->Build();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  SetContentSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK,
                    network_context.get());

  {
    std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
        kURL, net::DEFAULT_PRIORITY, /*delegate=*/nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_site_for_cookies(net::SiteForCookies::FromUrl(kOtherURL));
    EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateDisallowed,
              network_context->url_request_context()
                  ->network_delegate()
                  ->ForcePrivacyMode(*request));
  }

  {
    std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
        kOtherURL, net::DEFAULT_PRIORITY,
        /*delegate=*/nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_site_for_cookies(net::SiteForCookies::FromUrl(kURL));
    EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateAllowed,
              network_context->url_request_context()
                  ->network_delegate()
                  ->ForcePrivacyMode(*request));
  }
}

TEST_F(NetworkContextTest, PrivacyModeDisabledIfCookiesAllowed) {
  const GURL kURL("http://foo.com");
  const GURL kOtherURL("http://other.com");
  std::unique_ptr<net::URLRequestContext> request_context =
      CreateTestURLRequestContextBuilder()->Build();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  SetContentSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW,
                    network_context.get());

  std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
      kURL, net::DEFAULT_PRIORITY, /*delegate=*/nullptr,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_site_for_cookies(net::SiteForCookies::FromUrl(kOtherURL));
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateAllowed,
            network_context->url_request_context()
                ->network_delegate()
                ->ForcePrivacyMode(*request));
}

TEST_F(NetworkContextTest, PrivacyModeDisabledIfCookiesSettingForOtherURL) {
  const GURL kURL("http://foo.com");
  const GURL kOtherURL("http://other.com");
  std::unique_ptr<net::URLRequestContext> request_context =
      CreateTestURLRequestContextBuilder()->Build();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // URLs are switched so setting should not apply.
  SetContentSetting(kOtherURL, kURL, CONTENT_SETTING_BLOCK,
                    network_context.get());

  std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
      kURL, net::DEFAULT_PRIORITY, /*delegate=*/nullptr,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_site_for_cookies(net::SiteForCookies::FromUrl(kOtherURL));
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateAllowed,
            network_context->url_request_context()
                ->network_delegate()
                ->ForcePrivacyMode(*request));
}

TEST_F(NetworkContextTest, PrivacyModeEnabledIfThirdPartyCookiesBlocked) {
  const GURL kURL("http://foo.com");
  const GURL kOtherURL("http://other.com");
  std::unique_ptr<net::URLRequestContext> request_context =
      CreateTestURLRequestContextBuilder()->Build();

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  net::NetworkDelegate* delegate =
      network_context->url_request_context()->network_delegate();

  network_context->cookie_manager()->BlockThirdPartyCookies(true);

  {
    std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
        kURL, net::DEFAULT_PRIORITY, /*delegate=*/nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_site_for_cookies(net::SiteForCookies::FromUrl(kOtherURL));
    EXPECT_EQ(
        net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly,
        delegate->ForcePrivacyMode(*request));
  }

  {
    std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
        kURL, net::DEFAULT_PRIORITY, /*delegate=*/nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_site_for_cookies(net::SiteForCookies::FromUrl(kURL));
    EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateAllowed,
              delegate->ForcePrivacyMode(*request));
  }

  network_context->cookie_manager()->BlockThirdPartyCookies(false);
  {
    std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
        kURL, net::DEFAULT_PRIORITY, /*delegate=*/nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_site_for_cookies(net::SiteForCookies::FromUrl(kOtherURL));
    EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateAllowed,
              delegate->ForcePrivacyMode(*request));
  }

  {
    std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
        kURL, net::DEFAULT_PRIORITY, /*delegate=*/nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_site_for_cookies(net::SiteForCookies::FromUrl(kURL));
    EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateAllowed,
              delegate->ForcePrivacyMode(*request));
  }
}

TEST_F(NetworkContextTest, CanSetCookieFalseIfCookiesBlocked) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  std::unique_ptr<net::URLRequestContext> context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<net::URLRequest> request = context->CreateRequest(
      GURL("http://foo.com"), net::DEFAULT_PRIORITY,
      /*delegate=*/nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "TestCookie", "1", "www.test.com", "/", base::Time(), base::Time(),
      base::Time(), base::Time(), false, false, net::CookieSameSite::LAX_MODE,
      net::COOKIE_PRIORITY_LOW, false);
  EXPECT_TRUE(
      network_context->url_request_context()->network_delegate()->CanSetCookie(
          *request, *cookie, nullptr));
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK, network_context.get());
  EXPECT_FALSE(
      network_context->url_request_context()->network_delegate()->CanSetCookie(
          *request, *cookie, nullptr));
}

TEST_F(NetworkContextTest, CanSetCookieTrueIfCookiesAllowed) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  std::unique_ptr<net::URLRequestContext> context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<net::URLRequest> request = context->CreateRequest(
      GURL("http://foo.com"), net::DEFAULT_PRIORITY,
      /*delegate=*/nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "TestCookie", "1", "www.test.com", "/", base::Time(), base::Time(),
      base::Time(), base::Time(), false, false, net::CookieSameSite::LAX_MODE,
      net::COOKIE_PRIORITY_LOW, false);

  SetDefaultContentSetting(CONTENT_SETTING_ALLOW, network_context.get());
  EXPECT_TRUE(
      network_context->url_request_context()->network_delegate()->CanSetCookie(
          *request, *cookie, nullptr));
}

TEST_F(NetworkContextTest, CanSetCookieTrueIfBlockThirdPartyCookiesOverridden) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  std::unique_ptr<net::URLRequestContext> context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<net::URLRequest> request = context->CreateRequest(
      GURL("http://foo.com"), net::DEFAULT_PRIORITY,
      /*delegate=*/nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_site_for_cookies(
      net::SiteForCookies::FromUrl(GURL("http://bar.com")));
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "ThirdPartyCookie", "1", "www.foo.com", "/", base::Time(), base::Time(),
      base::Time(), base::Time(), false, false, net::CookieSameSite::LAX_MODE,
      net::COOKIE_PRIORITY_LOW, false);

  network_context->cookie_manager()->BlockThirdPartyCookies(true);
  EXPECT_FALSE(
      network_context->url_request_context()->network_delegate()->CanSetCookie(
          *request, *cookie, nullptr));

  // Now the cookie should be allowed if the request includes the override that
  // the user is forcing 3PCs.
  request->cookie_setting_overrides().Clear();
  request->cookie_setting_overrides().Put(
      net::CookieSettingOverride::kForceThirdPartyByUser);
  EXPECT_TRUE(
      network_context->url_request_context()->network_delegate()->CanSetCookie(
          *request, *cookie, nullptr));
}

TEST_F(NetworkContextTest,
       AnnotateAndMoveUserBlockedCookies_FalseIfCookiesBlocked) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  std::unique_ptr<net::URLRequestContext> context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<net::URLRequest> request = context->CreateRequest(
      GURL("http://foo.com"), net::DEFAULT_PRIORITY,
      /*delegate=*/nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);

  net::CookieAccessResultList included;
  net::CookieAccessResultList excluded;

  // Cookies are allowed, so call returns true.
  EXPECT_TRUE(network_context->url_request_context()
                  ->network_delegate()
                  ->AnnotateAndMoveUserBlockedCookies(
                      *request,
                      net::FirstPartySetMetadata(
                          net::SamePartyContext(
                              net::SamePartyContext::Type::kCrossParty),
                          /*frame_entry=*/nullptr,
                          /*top_frame_entry=*/nullptr),
                      included, excluded));

  // Cookies are blocked, so call returns false.
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK, network_context.get());
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->AnnotateAndMoveUserBlockedCookies(
                       *request,
                       net::FirstPartySetMetadata(
                           net::SamePartyContext(
                               net::SamePartyContext::Type::kCrossParty),
                           /*frame_entry=*/nullptr,
                           /*top_frame_entry=*/nullptr),
                       included, excluded));

  // Even with override, third party cookies blocked by content setting.
  request->cookie_setting_overrides().Clear();
  request->cookie_setting_overrides().Put(
      net::CookieSettingOverride::kForceThirdPartyByUser);
  EXPECT_FALSE(network_context->url_request_context()
                   ->network_delegate()
                   ->AnnotateAndMoveUserBlockedCookies(
                       *request,
                       net::FirstPartySetMetadata(
                           net::SamePartyContext(
                               net::SamePartyContext::Type::kCrossParty),
                           /*frame_entry=*/nullptr,
                           /*top_frame_entry=*/nullptr),
                       included, excluded));

  // Set blocking third party cookies instead of content setting.
  // Now override should apply.
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW, network_context.get());
  network_context->cookie_manager()->BlockThirdPartyCookies(true);
  EXPECT_TRUE(network_context->url_request_context()
                  ->network_delegate()
                  ->AnnotateAndMoveUserBlockedCookies(
                      *request,
                      net::FirstPartySetMetadata(
                          net::SamePartyContext(
                              net::SamePartyContext::Type::kCrossParty),
                          /*frame_entry=*/nullptr,
                          /*top_frame_entry=*/nullptr),
                      included, excluded));
}

TEST_F(NetworkContextTest,
       AnnotateAndMoveUserBlockedCookies_TrueIfCookiesAllowed) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  std::unique_ptr<net::URLRequestContext> context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<net::URLRequest> request = context->CreateRequest(
      GURL("http://foo.com"), net::DEFAULT_PRIORITY,
      /*delegate=*/nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  net::CookieAccessResultList included;
  net::CookieAccessResultList excluded;

  SetDefaultContentSetting(CONTENT_SETTING_ALLOW, network_context.get());
  EXPECT_TRUE(network_context->url_request_context()
                  ->network_delegate()
                  ->AnnotateAndMoveUserBlockedCookies(
                      *request,
                      net::FirstPartySetMetadata(
                          net::SamePartyContext(
                              net::SamePartyContext::Type::kCrossParty),
                          /*frame_entry=*/nullptr,
                          /*top_frame_entry=*/nullptr),
                      included, excluded));
}

// Gets notified by the EmbeddedTestServer on incoming connections being
// accepted or read from, keeps track of them and exposes that info to
// the tests.
// A port being reused is currently considered an error.  If a test
// needs to verify multiple connections are opened in sequence, that will need
// to be changed.
class ConnectionListener
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  ConnectionListener() = default;

  ConnectionListener(const ConnectionListener&) = delete;
  ConnectionListener& operator=(const ConnectionListener&) = delete;

  ~ConnectionListener() override = default;

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was accepted.
  std::unique_ptr<net::StreamSocket> AcceptedSocket(
      std::unique_ptr<net::StreamSocket> connection) override {
    base::AutoLock lock(lock_);
    uint16_t socket = GetPort(*connection);
    EXPECT_TRUE(sockets_.find(socket) == sockets_.end());

    sockets_[socket] = SOCKET_ACCEPTED;
    total_sockets_seen_++;
    CheckAccepted();
    return connection;
  }

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was read from.
  void ReadFromSocket(const net::StreamSocket& connection, int rv) override {
    EXPECT_GE(rv, net::OK);
  }

  // Wait for exactly |n| items in |sockets_|. |n| must be greater than 0.
  void WaitForAcceptedConnections(size_t num_connections) {
    DCHECK(on_done_accepting_connections_.is_null());
    DCHECK_GT(num_connections, 0u);
    base::RunLoop run_loop;
    {
      base::AutoLock lock(lock_);
      EXPECT_GE(num_connections, sockets_.size() - total_sockets_waited_for_);
      // QuitWhenIdle() instead of regular Quit() because in Preconnect tests we
      // count "idle_socket_count" but tasks posted synchronously after
      // AcceptedSocket() need to resolve before the new sockets are considered
      // idle.
      on_done_accepting_connections_ = run_loop.QuitWhenIdleClosure();
      num_accepted_connections_needed_ = num_connections;
      CheckAccepted();
    }
    // Note that the previous call to CheckAccepted can quit this run loop
    // before this call, which will make this call a no-op.
    run_loop.Run();

    // Grab the mutex again and make sure that the number of accepted sockets is
    // indeed |num_connections|.
    base::AutoLock lock(lock_);
    total_sockets_waited_for_ += num_connections;
    EXPECT_EQ(total_sockets_seen_, total_sockets_waited_for_);
  }

  // Helper function to stop the waiting for sockets to be accepted for
  // WaitForAcceptedConnections. |num_accepted_connections_loop_| spins
  // until |num_accepted_connections_needed_| sockets are accepted by the test
  // server. The values will be null/0 if the loop is not running.
  void CheckAccepted() {
    lock_.AssertAcquired();
    // |num_accepted_connections_loop_| null implies
    // |num_accepted_connections_needed_| == 0.
    DCHECK(!on_done_accepting_connections_.is_null() ||
           num_accepted_connections_needed_ == 0);
    if (on_done_accepting_connections_.is_null() ||
        num_accepted_connections_needed_ !=
            sockets_.size() - total_sockets_waited_for_) {
      return;
    }

    num_accepted_connections_needed_ = 0;
    std::move(on_done_accepting_connections_).Run();
  }

  int GetTotalSocketsSeen() const {
    base::AutoLock lock(lock_);
    return total_sockets_seen_;
  }

 private:
  static uint16_t GetPort(const net::StreamSocket& connection) {
    // Get the remote port of the peer, since the local port will always be the
    // port the test server is listening on. This isn't strictly correct - it's
    // possible for multiple peers to connect with the same remote port but
    // different remote IPs - but the tests here assume that connections to the
    // test server (running on localhost) will always come from localhost, and
    // thus the peer port is all thats needed to distinguish two connections.
    // This also would be problematic if the OS reused ports, but that's not
    // something to worry about for these tests.
    net::IPEndPoint address;
    EXPECT_EQ(net::OK, connection.GetPeerAddress(&address));
    return address.port();
  }

  int total_sockets_seen_ = 0;
  int total_sockets_waited_for_ = 0;

  enum SocketStatus { SOCKET_ACCEPTED, SOCKET_READ_FROM };

  // This lock protects all the members below, which each are used on both the
  // IO and UI thread. Members declared after the lock are protected by it.
  mutable base::Lock lock_;
  typedef std::map<uint16_t, SocketStatus> SocketContainer;
  SocketContainer sockets_;

  // If |num_accepted_connections_needed_| is non zero, then the object is
  // waiting for |num_accepted_connections_needed_| sockets to be accepted
  // before invoking |on_done_accepting_connections_|.
  size_t num_accepted_connections_needed_ = 0;
  base::OnceClosure on_done_accepting_connections_;
};

TEST_F(NetworkContextTest, PreconnectOne) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(1, test_server.base_url(),
                                     /*allow_credentials=*/true,
                                     net::NetworkAnonymizationKey());
  connection_listener.WaitForAcceptedConnections(1u);
}

TEST_F(NetworkContextTest, PreconnectHSTS) {
  net::NetworkAnonymizationKey network_anonymization_key =
      net::NetworkAnonymizationKey::CreateTransient();

  for (bool partition_connections : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    if (partition_connections) {
      feature_list.InitAndEnableFeature(
          net::features::kPartitionConnectionsByNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          net::features::kPartitionConnectionsByNetworkIsolationKey);
    }
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(CreateNetworkContextParamsForTesting());

    ConnectionListener connection_listener;
    net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
    test_server.SetConnectionListener(&connection_listener);
    ASSERT_TRUE(test_server.Start());

    ASSERT_TRUE(test_server.base_url().SchemeIs(url::kHttpsScheme));
    net::ClientSocketPool::GroupId ssl_group(
        url::SchemeHostPort(test_server.base_url()),
        net::PrivacyMode::PRIVACY_MODE_ENABLED,
        partition_connections ? network_anonymization_key
                              : net::NetworkAnonymizationKey(),
        net::SecureDnsPolicy::kAllow);

    const GURL server_http_url = GetHttpUrlFromHttps(test_server.base_url());
    ASSERT_TRUE(server_http_url.SchemeIs(url::kHttpScheme));
    net::ClientSocketPool::GroupId group(url::SchemeHostPort(server_http_url),
                                         net::PrivacyMode::PRIVACY_MODE_ENABLED,
                                         partition_connections
                                             ? network_anonymization_key
                                             : net::NetworkAnonymizationKey(),
                                         net::SecureDnsPolicy::kAllow);

    network_context->PreconnectSockets(1, server_http_url,
                                       /*allow_credentials=*/false,
                                       network_anonymization_key);
    connection_listener.WaitForAcceptedConnections(1u);

    int num_sockets = GetSocketCountForGroup(network_context.get(), group);
    EXPECT_EQ(num_sockets, 1);

    const base::Time expiry = base::Time::Now() + base::Seconds(1000);
    network_context->url_request_context()->transport_security_state()->AddHSTS(
        server_http_url.host(), expiry, false);
    network_context->PreconnectSockets(1, server_http_url,
                                       /*allow_credentials=*/false,
                                       network_anonymization_key);
    connection_listener.WaitForAcceptedConnections(1u);

    // If HSTS weren't respected, the initial connection would have been reused.
    num_sockets = GetSocketCountForGroup(network_context.get(), ssl_group);
    EXPECT_EQ(num_sockets, 1);
  }
}

TEST_F(NetworkContextTest, PreconnectZero) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(0, test_server.base_url(),
                                     /*allow_credentials=*/true,
                                     net::NetworkAnonymizationKey());
  base::RunLoop().RunUntilIdle();

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, 0);
  int num_connecting_sockets =
      GetSocketPoolInfo(network_context.get(), "connecting_socket_count");
  ASSERT_EQ(num_connecting_sockets, 0);
}

TEST_F(NetworkContextTest, PreconnectTwo) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(2, test_server.base_url(),
                                     /*allow_credentials=*/true,
                                     net::NetworkAnonymizationKey());
  connection_listener.WaitForAcceptedConnections(2u);

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, 2);
}

TEST_F(NetworkContextTest, PreconnectFour) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  network_context->PreconnectSockets(4, test_server.base_url(),
                                     /*allow_credentials=*/true,
                                     net::NetworkAnonymizationKey());

  connection_listener.WaitForAcceptedConnections(4u);

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, 4);
}

TEST_F(NetworkContextTest, PreconnectMax) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  int max_num_sockets =
      GetSocketPoolInfo(network_context.get(), "max_sockets_per_group");
  EXPECT_GT(76, max_num_sockets);

  network_context->PreconnectSockets(76, test_server.base_url(),
                                     /*allow_credentials=*/true,
                                     net::NetworkAnonymizationKey());

  // Wait until |max_num_sockets| have been connected.
  connection_listener.WaitForAcceptedConnections(max_num_sockets);

  // This is not guaranteed to wait long enough if more than |max_num_sockets|
  // connections are actually made, but experimentally, it fails consistently if
  // that's the case.
  base::RunLoop().RunUntilIdle();

  int num_sockets =
      GetSocketPoolInfo(network_context.get(), "idle_socket_count");
  ASSERT_EQ(num_sockets, max_num_sockets);
}

// Make sure preconnects for the same URL but with different network isolation
// keys are not merged.
TEST_F(NetworkContextTest, PreconnectNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kPartitionConnectionsByNetworkIsolationKey);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ConnectionListener connection_listener;
  net::EmbeddedTestServer test_server;
  test_server.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server.Start());

  const auto kSiteFoo = net::SchemefulSite(GURL("http://foo.test"));
  const auto kSiteBar = net::SchemefulSite(GURL("http://bar.test"));
  const auto kKey1 = net::NetworkAnonymizationKey::CreateSameSite(kSiteFoo);
  const auto kKey2 = net::NetworkAnonymizationKey::CreateSameSite(kSiteBar);
  const auto kNak1 = net::NetworkAnonymizationKey::CreateSameSite(kSiteFoo);
  const auto kNak2 = net::NetworkAnonymizationKey::CreateSameSite(kSiteBar);
  network_context->PreconnectSockets(1, test_server.base_url(),
                                     /*allow_credentials=*/false, kKey1);
  network_context->PreconnectSockets(2, test_server.base_url(),
                                     /*allow_credentials=*/false, kKey2);
  connection_listener.WaitForAcceptedConnections(3u);

  url::SchemeHostPort destination(test_server.base_url());
  net::ClientSocketPool::GroupId group_id1(
      destination, net::PrivacyMode::PRIVACY_MODE_ENABLED, kNak1,
      net::SecureDnsPolicy::kAllow);
  EXPECT_EQ(1, GetSocketCountForGroup(network_context.get(), group_id1));
  net::ClientSocketPool::GroupId group_id2(
      destination, net::PrivacyMode::PRIVACY_MODE_ENABLED, kNak2,
      net::SecureDnsPolicy::kAllow);
  EXPECT_EQ(2, GetSocketCountForGroup(network_context.get(), group_id2));
}

// This tests both ClostAllConnetions and CloseIdleConnections.
TEST_F(NetworkContextTest, CloseConnections) {
  // Have to close all connections first, as CloseIdleConnections leaves around
  // a connection at the end of the test.
  for (bool close_all_connections : {true, false}) {
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(CreateNetworkContextParamsForTesting());

    // Use different paths to avoid running into the cache lock.
    const char kPath1[] = "/foo";
    const char kPath2[] = "/bar";
    const char kPath3[] = "/baz";
    net::EmbeddedTestServer test_server;
    net::test_server::ControllableHttpResponse controllable_response1(
        &test_server, kPath1);
    net::test_server::ControllableHttpResponse controllable_response2(
        &test_server, kPath2);
    net::test_server::ControllableHttpResponse controllable_response3(
        &test_server, kPath3);
    ASSERT_TRUE(test_server.Start());

    // Start three network requests. Requests have to all be started before any
    // one of them receives a response to be sure none of them tries to reuse
    // the socket created by another one.

    net::TestDelegate delegate1;
    base::RunLoop run_loop1;
    delegate1.set_on_complete(run_loop1.QuitClosure());
    std::unique_ptr<net::URLRequest> request1 =
        network_context->url_request_context()->CreateRequest(
            test_server.GetURL(kPath1), net::DEFAULT_PRIORITY, &delegate1,
            TRAFFIC_ANNOTATION_FOR_TESTS);
    request1->Start();
    controllable_response1.WaitForRequest();
    EXPECT_EQ(
        1, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    net::TestDelegate delegate2;
    base::RunLoop run_loop2;
    delegate2.set_on_complete(run_loop2.QuitClosure());
    std::unique_ptr<net::URLRequest> request2 =
        network_context->url_request_context()->CreateRequest(
            test_server.GetURL(kPath2), net::DEFAULT_PRIORITY, &delegate2,
            TRAFFIC_ANNOTATION_FOR_TESTS);
    request2->Start();
    controllable_response2.WaitForRequest();
    EXPECT_EQ(
        2, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    net::TestDelegate delegate3;
    base::RunLoop run_loop3;
    delegate3.set_on_complete(run_loop3.QuitClosure());
    std::unique_ptr<net::URLRequest> request3 =
        network_context->url_request_context()->CreateRequest(
            test_server.GetURL(kPath3), net::DEFAULT_PRIORITY, &delegate3,
            TRAFFIC_ANNOTATION_FOR_TESTS);
    request3->Start();
    controllable_response3.WaitForRequest();
    EXPECT_EQ(
        3, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    // Complete the first two requests successfully, with a keep-alive response.
    // The EmbeddedTestServer doesn't actually support connection reuse, but
    // this will send a raw response that will make the network stack think it
    // does, and will cause the connection not to be closed.
    controllable_response1.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 0\r\n\r\n");
    controllable_response2.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 0\r\n\r\n");
    run_loop1.Run();
    run_loop2.Run();
    // There should now be 2 idle and one handed out socket.
    EXPECT_EQ(2, GetSocketPoolInfo(network_context.get(), "idle_socket_count"));
    EXPECT_EQ(
        1, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    // Closing all or idle connections should result in closing the idle
    // sockets, but the handed out socket can't be closed.
    base::RunLoop run_loop;
    if (close_all_connections) {
      network_context->CloseAllConnections(run_loop.QuitClosure());
    } else {
      network_context->CloseIdleConnections(run_loop.QuitClosure());
    }
    run_loop.Run();
    EXPECT_EQ(0, GetSocketPoolInfo(network_context.get(), "idle_socket_count"));
    EXPECT_EQ(
        1, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));

    // The final request completes. In the close all connections case, its
    // socket should be closed as soon as it is returned to the pool, but in the
    // CloseIdleConnections case, it is added to the pool as an idle socket.
    controllable_response3.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 0\r\n\r\n");
    run_loop3.Run();
    EXPECT_EQ(close_all_connections ? 0 : 1,
              GetSocketPoolInfo(network_context.get(), "idle_socket_count"));
    EXPECT_EQ(
        0, GetSocketPoolInfo(network_context.get(), "handed_out_socket_count"));
  }
}

// Test that only trusted URLLoaderFactories accept
// ResourceRequest::trusted_params.
TEST_F(NetworkContextTest, TrustedParams) {
  for (bool trusted_factory : {false, true}) {
    ConnectionListener connection_listener;
    net::EmbeddedTestServer test_server;
    test_server.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("services/test/data")));
    test_server.SetConnectionListener(&connection_listener);
    ASSERT_TRUE(test_server.Start());

    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(CreateNetworkContextParamsForTesting());

    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    // URLLoaderFactories should not be trusted by default.
    EXPECT_FALSE(params->is_trusted);
    params->is_trusted = trusted_factory;
    network_context->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    ResourceRequest request;
    request.url = test_server.GetURL("/echo");
    request.trusted_params = ResourceRequest::TrustedParams();
    mojo::PendingRemote<mojom::URLLoader> loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();

    // If the factory was trusted, the request should have succeeded. Otherwise,
    // it should have failed.
    EXPECT_EQ(trusted_factory, client.has_received_response());

    if (trusted_factory) {
      EXPECT_THAT(client.completion_status().error_code, net::test::IsOk());
      EXPECT_EQ(1, connection_listener.GetTotalSocketsSeen());
    } else {
      EXPECT_THAT(client.completion_status().error_code,
                  net::test::IsError(net::ERR_INVALID_ARGUMENT));
      // No connection should have been made to the test server.
      EXPECT_EQ(0, connection_listener.GetTotalSocketsSeen());
    }
  }
}

// Test that the disable_secure_dns trusted param is passed through to the
// host resolver.
TEST_F(NetworkContextTest, TrustedParams_DisableSecureDns) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(std::make_unique<net::MockHostResolver>());
  std::unique_ptr<net::URLRequestContext> url_request_context =
      context_builder->Build();
  auto& resolver = *static_cast<net::MockHostResolver*>(
      url_request_context->host_resolver());
  resolver.rules()->AddRule("example.test", test_server.GetIPLiteralString());

  network_context_remote_.reset();
  std::unique_ptr<NetworkContext> network_context =
      std::make_unique<NetworkContext>(
          network_service_.get(),
          network_context_remote_.BindNewPipeAndPassReceiver(),
          url_request_context.get(),
          /*cors_exempt_header_list=*/std::vector<std::string>());

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  params->is_trusted = true;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  for (bool disable_secure_dns : {false, true}) {
    ResourceRequest request;
    request.url = GURL("http://example.test/echo");
    request.load_flags = net::LOAD_BYPASS_CACHE;
    request.trusted_params = ResourceRequest::TrustedParams();
    request.trusted_params->disable_secure_dns = disable_secure_dns;
    mojo::PendingRemote<mojom::URLLoader> loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    if (disable_secure_dns) {
      EXPECT_EQ(net::SecureDnsPolicy::kDisable,
                resolver.last_secure_dns_policy());
    } else {
      EXPECT_EQ(net::SecureDnsPolicy::kAllow,
                resolver.last_secure_dns_policy());
    }
  }
}

// Test that the disable_secure_dns factory param is passed through to the
// host resolver.
TEST_F(NetworkContextTest, FactoryParams_DisableSecureDns) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(std::make_unique<net::MockHostResolver>());
  std::unique_ptr<net::URLRequestContext> url_request_context =
      context_builder->Build();
  auto& resolver = *static_cast<net::MockHostResolver*>(
      url_request_context->host_resolver());
  resolver.rules()->AddRule("example.test", test_server.GetIPLiteralString());

  network_context_remote_.reset();
  NetworkContext network_context(
      network_service_.get(),
      network_context_remote_.BindNewPipeAndPassReceiver(),
      url_request_context.get(),
      /*cors_exempt_header_list=*/std::vector<std::string>());

  for (bool disable_secure_dns : {false, true}) {
    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    params->disable_secure_dns = disable_secure_dns;
    network_context.CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    ResourceRequest request;
    request.url = GURL("http://example.test/echo");
    request.load_flags = net::LOAD_BYPASS_CACHE;
    auto client = std::make_unique<TestURLLoaderClient>();
    mojo::Remote<mojom::URLLoader> loader;
    loader_factory->CreateLoaderAndStart(
        loader.BindNewPipeAndPassReceiver(), 0 /* request_id */,
        0 /* options */, request, client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client->RunUntilComplete();
    if (disable_secure_dns) {
      EXPECT_EQ(net::SecureDnsPolicy::kDisable,
                resolver.last_secure_dns_policy());
    } else {
      EXPECT_EQ(net::SecureDnsPolicy::kAllow,
                resolver.last_secure_dns_policy());
    }
  }
}

TEST_F(NetworkContextTest, QueryHSTS) {
  const char kTestDomain[] = "example.com";

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  bool result = false, got_result = false;
  network_context->IsHSTSActiveForHost(
      kTestDomain, base::BindLambdaForTesting([&](bool is_hsts) {
        result = is_hsts;
        got_result = true;
      }));
  EXPECT_TRUE(got_result);
  EXPECT_FALSE(result);

  base::RunLoop run_loop;
  network_context->AddHSTS(kTestDomain, base::Time::Now() + base::Days(1000),
                           false /*include_subdomains*/,
                           run_loop.QuitClosure());
  run_loop.Run();

  bool result2 = false, got_result2 = false;
  network_context->IsHSTSActiveForHost(
      kTestDomain, base::BindLambdaForTesting([&](bool is_hsts) {
        result2 = is_hsts;
        got_result2 = true;
      }));
  EXPECT_TRUE(got_result2);
  EXPECT_TRUE(result2);
}

TEST_F(NetworkContextTest, GetHSTSState) {
  const char kTestDomain[] = "example.com";
  const base::Time expiry = base::Time::Now() + base::Seconds(1000);
  const GURL report_uri = GURL("https://example.com/foo/bar");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::Value::Dict state;
  {
    base::RunLoop run_loop;
    network_context->GetHSTSState(
        kTestDomain,
        base::BindOnce(&StoreValue, &state, run_loop.QuitClosure()));
    run_loop.Run();
  }

  absl::optional<bool> result = state.FindBool("result");
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(*result);

  {
    base::RunLoop run_loop;
    network_context->AddHSTS(kTestDomain, expiry, false /*include_subdomains*/,
                             run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    network_context->GetHSTSState(
        kTestDomain,
        base::BindOnce(&StoreValue, &state, run_loop.QuitClosure()));
    run_loop.Run();
  }

  result = state.FindBool("result");
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(*result);

  // Not checking all values - only enough to ensure the underlying call
  // was made.
  const std::string* dynamic_sts_domain =
      state.FindString("dynamic_sts_domain");
  ASSERT_TRUE(dynamic_sts_domain);
  EXPECT_EQ(kTestDomain, *dynamic_sts_domain);

  absl::optional<double> dynamic_sts_expiry =
      state.FindDouble("dynamic_sts_expiry");
  EXPECT_EQ(expiry.ToDoubleT(), dynamic_sts_expiry);
}

TEST_F(NetworkContextTest, ForceReloadProxyConfig) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  auto net_log_exporter =
      std::make_unique<network::NetLogExporter>(network_context.get());
  base::FilePath net_log_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&net_log_path));

  {
    base::File net_log_file(
        net_log_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    EXPECT_TRUE(net_log_file.IsValid());
    base::RunLoop run_loop;
    int32_t start_param = 0;
    auto start_callback = base::BindLambdaForTesting([&](int32_t result) {
      start_param = result;
      run_loop.Quit();
    });
    net_log_exporter->Start(std::move(net_log_file),
                            /*extra_constants=*/base::Value::Dict(),
                            net::NetLogCaptureMode::kDefault,
                            network::mojom::NetLogExporter::kUnlimitedFileSize,
                            start_callback);
    run_loop.Run();
    EXPECT_EQ(net::OK, start_param);
  }

  {
    base::RunLoop run_loop;
    network_context->ForceReloadProxyConfig(run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    int32_t stop_param = 0;
    auto stop_callback = base::BindLambdaForTesting([&](int32_t result) {
      stop_param = result;
      run_loop.Quit();
    });
    net_log_exporter->Stop(
        /*polled_data=*/base::Value::Dict(), stop_callback);
    run_loop.Run();
    EXPECT_EQ(net::OK, stop_param);
  }

  std::string log_contents;
  EXPECT_TRUE(base::ReadFileToString(net_log_path, &log_contents));

  EXPECT_NE(std::string::npos, log_contents.find("\"new_config\""))
      << log_contents;
  base::DeleteFile(net_log_path);
}

TEST_F(NetworkContextTest, ClearBadProxiesCache) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  net::ProxyResolutionService* proxy_resolution_service =
      network_context->url_request_context()->proxy_resolution_service();

  // Very starting conditions: zero bad proxies.
  EXPECT_EQ(0UL, proxy_resolution_service->proxy_retry_info().size());

  // Simulate network error to add one proxy to the bad proxy list.
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("http://foo1.com");
  proxy_resolution_service->ReportSuccess(proxy_info);
  std::vector<net::ProxyServer> proxies;
  proxies.push_back(net::ProxyUriToProxyServer("http://foo1.com",
                                               net::ProxyServer::SCHEME_HTTP));
  proxy_resolution_service->MarkProxiesAsBadUntil(
      proxy_info, base::Days(1), proxies, net::NetLogWithSource());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1UL, proxy_resolution_service->proxy_retry_info().size());

  // Clear the bad proxies.
  base::RunLoop run_loop;
  network_context->ClearBadProxiesCache(run_loop.QuitClosure());
  run_loop.Run();

  // Verify all cleared.
  EXPECT_EQ(0UL, proxy_resolution_service->proxy_retry_info().size());
}

// This is a test ProxyErrorClient that records the sequence of calls made to
// OnPACScriptError() and OnRequestMaybeFailedDueToProxySettings().
class TestProxyErrorClient final : public mojom::ProxyErrorClient {
 public:
  struct PacScriptError {
    int line = -1;
    std::string details;
  };

  TestProxyErrorClient() = default;

  TestProxyErrorClient(const TestProxyErrorClient&) = delete;
  TestProxyErrorClient& operator=(const TestProxyErrorClient&) = delete;

  ~TestProxyErrorClient() override {}

  void OnPACScriptError(int32_t line_number,
                        const std::string& details) override {
    on_pac_script_error_calls_.push_back({line_number, details});
  }

  void OnRequestMaybeFailedDueToProxySettings(int32_t net_error) override {
    on_request_maybe_failed_calls_.push_back(net_error);
  }

  const std::vector<int>& on_request_maybe_failed_calls() const {
    return on_request_maybe_failed_calls_;
  }

  const std::vector<PacScriptError>& on_pac_script_error_calls() const {
    return on_pac_script_error_calls_;
  }

  // Creates an mojo::PendingRemote, binds it to |*this| and returns it.
  mojo::PendingRemote<mojom::ProxyErrorClient> CreateRemote() {
    mojo::PendingRemote<mojom::ProxyErrorClient> client_remote =
        receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        &TestProxyErrorClient::OnMojoPipeError, base::Unretained(this)));
    return client_remote;
  }

  // Runs until the message pipe is closed due to an error.
  void RunUntilMojoPipeError() {
    if (has_received_mojo_pipe_error_)
      return;
    base::RunLoop run_loop;
    quit_closure_for_on_mojo_pipe_error_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnMojoPipeError() {
    if (has_received_mojo_pipe_error_)
      return;
    has_received_mojo_pipe_error_ = true;
    if (quit_closure_for_on_mojo_pipe_error_)
      std::move(quit_closure_for_on_mojo_pipe_error_).Run();
  }

  mojo::Receiver<mojom::ProxyErrorClient> receiver_{this};

  base::OnceClosure quit_closure_for_on_mojo_pipe_error_;
  bool has_received_mojo_pipe_error_ = false;
  std::vector<int> on_request_maybe_failed_calls_;
  std::vector<PacScriptError> on_pac_script_error_calls_;
};

// While in scope, all host resolutions will fail with ERR_NAME_NOT_RESOLVED,
// including localhost (so this precludes the use of embedded test server).
class ScopedFailAllHostResolutions {
 public:
  ScopedFailAllHostResolutions()
      : mock_resolver_proc_(new net::RuleBasedHostResolverProc(nullptr)),
        default_resolver_proc_(mock_resolver_proc_.get()) {
    mock_resolver_proc_->AddSimulatedFailure("*");
  }

 private:
  scoped_refptr<net::RuleBasedHostResolverProc> mock_resolver_proc_;
  net::ScopedDefaultHostResolverProc default_resolver_proc_;
};

// Tests that when a ProxyErrorClient is provided to NetworkContextParams, this
// client's OnRequestMaybeFailedDueToProxySettings() method is called exactly
// once when a request fails due to a proxy server connectivity failure.
TEST_F(NetworkContextTest, ProxyErrorClientNotifiedOfProxyConnection) {
  // Avoid the test having a network dependency on DNS.
  ScopedFailAllHostResolutions fail_dns;

  // Set up the NetworkContext, such that it uses an unreachable proxy
  // (proxy and is configured to send "proxy errors" to
  // |proxy_error_client|.
  TestProxyErrorClient proxy_error_client;
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->proxy_error_client = proxy_error_client.CreateRemote();
  net::ProxyConfig proxy_config;
  // Set the proxy to an unreachable address (host resolution fails).
  proxy_config.proxy_rules().ParseFromString("proxy.bad.dns");
  context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Issue an HTTP request. It doesn't matter exactly what the URL is, since it
  // will be sent to the proxy.
  ResourceRequest request;
  request.url = GURL("http://example.test");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr loader_params =
      mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = mojom::kBrowserProcessId;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(loader_params));

  mojo::PendingRemote<mojom::URLLoader> loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      0 /* options */, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Confirm the the resource request failed due to an unreachable proxy.
  client.RunUntilComplete();
  EXPECT_THAT(client.completion_status().error_code,
              net::test::IsError(net::ERR_PROXY_CONNECTION_FAILED));

  // Tear down the network context and wait for a pipe error to ensure
  // that all queued messages on |proxy_error_client| have been processed.
  network_context.reset();
  proxy_error_client.RunUntilMojoPipeError();

  // Confirm that the ProxyErrorClient received the expected calls.
  const auto& request_errors =
      proxy_error_client.on_request_maybe_failed_calls();
  const auto& pac_errors = proxy_error_client.on_pac_script_error_calls();

  ASSERT_EQ(1u, request_errors.size());
  EXPECT_THAT(request_errors[0],
              net::test::IsError(net::ERR_PROXY_CONNECTION_FAILED));
  EXPECT_EQ(0u, pac_errors.size());
}

// Tests that when a ProxyErrorClient is provided to NetworkContextParams, this
// client's OnRequestMaybeFailedDueToProxySettings() method is
// NOT called when a request fails due to a non-proxy related error (in this
// case the target host is unreachable).
TEST_F(NetworkContextTest, ProxyErrorClientNotNotifiedOfUnreachableError) {
  // Avoid the test having a network dependency on DNS.
  ScopedFailAllHostResolutions fail_dns;

  // Set up the NetworkContext that uses the default DIRECT proxy
  // configuration.
  TestProxyErrorClient proxy_error_client;
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->proxy_error_client = proxy_error_client.CreateRemote();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Issue an HTTP request to an unreachable URL.
  ResourceRequest request;
  request.url = GURL("http://server.bad.dns/fail");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr loader_params =
      mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = mojom::kBrowserProcessId;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(loader_params));

  mojo::PendingRemote<mojom::URLLoader> loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      0 /* options */, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Confirm the the resource request failed.
  client.RunUntilComplete();
  EXPECT_THAT(client.completion_status().error_code,
              net::test::IsError(net::ERR_NAME_NOT_RESOLVED));

  // Tear down the network context and wait for a pipe error to ensure
  // that all queued messages on |proxy_error_client| have been processed.
  network_context.reset();
  proxy_error_client.RunUntilMojoPipeError();

  // Confirm that the ProxyErrorClient received no calls.
  const auto& request_errors =
      proxy_error_client.on_request_maybe_failed_calls();
  const auto& pac_errors = proxy_error_client.on_pac_script_error_calls();

  EXPECT_EQ(0u, request_errors.size());
  EXPECT_EQ(0u, pac_errors.size());
}

// Tests that when a ProxyErrorClient is provided to NetworkContextParams, this
// client's OnPACScriptError() method is called whenever the PAC script throws
// an error.
TEST_F(NetworkContextTest, ProxyErrorClientNotifiedOfPacError) {
  // Avoid the test having a network dependency on DNS.
  ScopedFailAllHostResolutions fail_dns;

  // Set up the NetworkContext so that it sends "proxy errors" to
  // |proxy_error_client|, and uses a mock ProxyResolverFactory that emits
  // script errors.
  TestProxyErrorClient proxy_error_client;
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->proxy_error_client = proxy_error_client.CreateRemote();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  context_params->dhcp_wpad_url_client =
      network::MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(
          std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // The PAC URL doesn't matter, since the test is configured to use a
  // mock ProxyResolverFactory which doesn't actually evaluate it. It just
  // needs to be a data: URL to ensure the network fetch doesn't fail.
  //
  // That said, the mock PAC evaluator being used behaves similarly to the
  // script embedded in the data URL below.
  net::ProxyConfig proxy_config = net::ProxyConfig::CreateFromCustomPacURL(
      GURL("data:,function FindProxyForURL(url,host){throw url}"));
  context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
      proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
  context_params->proxy_resolver_factory =
      MockMojoProxyResolverFactory::Create();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  // Issue an HTTP request. This will end up being sent DIRECT since the PAC
  // script is broken.
  ResourceRequest request;
  request.url = GURL("http://server.bad.dns");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr loader_params =
      mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = mojom::kBrowserProcessId;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(loader_params));

  mojo::PendingRemote<mojom::URLLoader> loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      0 /* options */, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Confirm the the resource request failed.
  client.RunUntilComplete();
  EXPECT_THAT(client.completion_status().error_code,
              net::test::IsError(net::ERR_NAME_NOT_RESOLVED));

  // Tear down the network context and wait for a pipe error to ensure
  // that all queued messages on |proxy_error_client| have been processed.
  network_context.reset();
  proxy_error_client.RunUntilMojoPipeError();

  // Confirm that the ProxyErrorClient received the expected calls.
  const auto& request_errors =
      proxy_error_client.on_request_maybe_failed_calls();
  const auto& pac_errors = proxy_error_client.on_pac_script_error_calls();

  EXPECT_EQ(0u, request_errors.size());

  ASSERT_EQ(1u, pac_errors.size());
  EXPECT_EQ(pac_errors[0].line, 42);
  EXPECT_EQ(pac_errors[0].details,
            "Failed: FindProxyForURL(url=http://server.bad.dns/)");
}

// Test ensures that ProxyServer data is populated correctly across Mojo calls.
// Basically it performs a set of URLLoader network requests, whose requests
// configure proxies. Then it checks whether the expected proxy scheme is
// respected.
TEST_F(NetworkContextTest, EnsureProperProxyServerIsUsed) {
  net::test_server::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  struct ProxyConfigSet {
    net::ProxyConfig proxy_config;
    GURL url;
    net::ProxyServer::Scheme expected_proxy_config_scheme;
  } proxy_config_set[2];

  proxy_config_set[0].proxy_config.proxy_rules().ParseFromString(
      "http=" + test_server.host_port_pair().ToString());
  proxy_config_set[0].url = GURL("http://does.not.matter/echo");
  proxy_config_set[0].expected_proxy_config_scheme =
      net::ProxyServer::SCHEME_HTTP;

  proxy_config_set[1].proxy_config.proxy_rules().ParseFromString(
      "http=direct://");
  proxy_config_set[1]
      .proxy_config.proxy_rules()
      .bypass_rules.AddRulesToSubtractImplicit();
  proxy_config_set[1].url = test_server.GetURL("/echo");
  proxy_config_set[1].expected_proxy_config_scheme =
      net::ProxyServer::SCHEME_DIRECT;

  for (const auto& proxy_data : proxy_config_set) {
    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        proxy_data.proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    mojo::Remote<mojom::ProxyConfigClient> config_client;
    context_params->proxy_config_client_receiver =
        config_client.BindNewPipeAndPassReceiver();
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(context_params));

    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = 0;
    network_context->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

    ResourceRequest request;
    request.url = proxy_data.url;

    mojo::PendingRemote<mojom::URLLoader> loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();

    EXPECT_TRUE(client.has_received_completion());
    EXPECT_EQ(client.response_head()->proxy_server.scheme(),
              proxy_data.expected_proxy_config_scheme);
  }
}

class TestURLLoaderHeaderClient : public mojom::TrustedURLLoaderHeaderClient {
 public:
  class TestHeaderClient : public mojom::TrustedHeaderClient {
   public:
    TestHeaderClient() {}

    TestHeaderClient(const TestHeaderClient&) = delete;
    TestHeaderClient& operator=(const TestHeaderClient&) = delete;

    // network::mojom::TrustedHeaderClient:
    void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                             OnBeforeSendHeadersCallback callback) override {
      auto new_headers = headers;
      new_headers.SetHeader("foo", "bar");
      std::move(callback).Run(on_before_send_headers_result_, new_headers);
    }

    void OnHeadersReceived(const std::string& headers,
                           const net::IPEndPoint& endpoint,
                           OnHeadersReceivedCallback callback) override {
      auto new_headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(headers);
      new_headers->SetHeader("baz", "qux");
      std::move(callback).Run(on_headers_received_result_,
                              new_headers->raw_headers(), GURL());
    }

    void set_on_before_send_headers_result(int result) {
      on_before_send_headers_result_ = result;
    }

    void set_on_headers_received_result(int result) {
      on_headers_received_result_ = result;
    }

    void Bind(
        mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
      receiver_.reset();
      receiver_.Bind(std::move(receiver));
    }

   private:
    int on_before_send_headers_result_ = net::OK;
    int on_headers_received_result_ = net::OK;
    mojo::Receiver<mojom::TrustedHeaderClient> receiver_{this};
  };

  explicit TestURLLoaderHeaderClient(
      mojo::PendingReceiver<mojom::TrustedURLLoaderHeaderClient> receiver)
      : receiver_(this, std::move(receiver)) {}

  TestURLLoaderHeaderClient(const TestURLLoaderHeaderClient&) = delete;
  TestURLLoaderHeaderClient& operator=(const TestURLLoaderHeaderClient&) =
      delete;

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {
    header_client_.Bind(std::move(receiver));
  }
  void OnLoaderForCorsPreflightCreated(
      const ResourceRequest& request,
      mojo::PendingReceiver<mojom::TrustedHeaderClient> receiver) override {
    header_client_.Bind(std::move(receiver));
  }

  void set_on_before_send_headers_result(int result) {
    header_client_.set_on_before_send_headers_result(result);
  }

  void set_on_headers_received_result(int result) {
    header_client_.set_on_headers_received_result(result);
  }

 private:
  TestHeaderClient header_client_;
  mojo::Receiver<mojom::TrustedURLLoaderHeaderClient> receiver_;
};

TEST_F(NetworkContextTest, HeaderClientModifiesHeaders) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ResourceRequest request;
  request.url = test_server.GetURL("/echoheader?foo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  TestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  // First, do a request with kURLLoadOptionUseHeaderClient set.
  {
    mojo::PendingRemote<mojom::URLLoader> loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();

    // Make sure request header was modified. The value will be in the body
    // since we used the /echoheader endpoint.
    std::string response;
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client.response_body_release(), &response));
    EXPECT_EQ(response, "bar");

    // Make sure response header was modified.
    EXPECT_TRUE(client.response_head()->headers->HasHeaderValue("baz", "qux"));
  }

  // Next, do a request without kURLLoadOptionUseHeaderClient set, headers
  // should not be modified.
  {
    mojo::PendingRemote<mojom::URLLoader> loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        0 /* options */, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();

    // Make sure request header was not set.
    std::string response;
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client.response_body_release(), &response));
    EXPECT_EQ(response, "None");

    // Make sure response header was not set.
    EXPECT_FALSE(client.response_head()->headers->HasHeaderValue("foo", "bar"));
  }
}

TEST_F(NetworkContextTest, HeaderClientFailsRequest) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ResourceRequest request;
  request.url = test_server.GetURL("/echo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  TestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  // First, fail request on OnBeforeSendHeaders.
  {
    header_client.set_on_before_send_headers_result(net::ERR_FAILED);
    mojo::PendingRemote<mojom::URLLoader> loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_EQ(client.completion_status().error_code, net::ERR_FAILED);
  }

  // Next, fail request on OnHeadersReceived.
  {
    header_client.set_on_before_send_headers_result(net::OK);
    header_client.set_on_headers_received_result(net::ERR_FAILED);
    mojo::PendingRemote<mojom::URLLoader> loader;
    TestURLLoaderClient client;
    loader_factory->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    client.RunUntilComplete();
    EXPECT_EQ(client.completion_status().error_code, net::ERR_FAILED);
  }
}

class HangingTestURLLoaderHeaderClient
    : public mojom::TrustedURLLoaderHeaderClient {
 public:
  class TestHeaderClient : public mojom::TrustedHeaderClient {
   public:
    TestHeaderClient() {}

    TestHeaderClient(const TestHeaderClient&) = delete;
    TestHeaderClient& operator=(const TestHeaderClient&) = delete;

    // network::mojom::TrustedHeaderClient:
    void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                             OnBeforeSendHeadersCallback callback) override {
      saved_request_headers_ = headers;
      saved_on_before_send_headers_callback_ = std::move(callback);
      on_before_send_headers_loop_.Quit();
    }

    void OnHeadersReceived(const std::string& headers,
                           const net::IPEndPoint& endpoint,
                           OnHeadersReceivedCallback callback) override {
      saved_received_headers_ = headers;
      saved_on_headers_received_callback_ = std::move(callback);
      on_headers_received_loop_.Quit();
    }

    void CallOnBeforeSendHeadersCallback() {
      net::HttpRequestHeaders new_headers = std::move(saved_request_headers_);
      new_headers.SetHeader("foo", "bar");
      std::move(saved_on_before_send_headers_callback_)
          .Run(net::OK, new_headers);
    }

    void WaitForOnBeforeSendHeaders() { on_before_send_headers_loop_.Run(); }

    void CallOnHeadersReceivedCallback() {
      auto new_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
          saved_received_headers_);
      new_headers->SetHeader("baz", "qux");
      std::move(saved_on_headers_received_callback_)
          .Run(net::OK, new_headers->raw_headers(), GURL());
    }

    void WaitForOnHeadersReceived() { on_headers_received_loop_.Run(); }

    void Bind(
        mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
      receiver_.Bind(std::move(receiver));
    }

   private:
    base::RunLoop on_before_send_headers_loop_;
    net::HttpRequestHeaders saved_request_headers_;
    OnBeforeSendHeadersCallback saved_on_before_send_headers_callback_;

    base::RunLoop on_headers_received_loop_;
    std::string saved_received_headers_;
    OnHeadersReceivedCallback saved_on_headers_received_callback_;
    mojo::Receiver<mojom::TrustedHeaderClient> receiver_{this};
  };

  explicit HangingTestURLLoaderHeaderClient(
      mojo::PendingReceiver<mojom::TrustedURLLoaderHeaderClient> receiver)
      : receiver_(this, std::move(receiver)) {}

  HangingTestURLLoaderHeaderClient(const HangingTestURLLoaderHeaderClient&) =
      delete;
  HangingTestURLLoaderHeaderClient& operator=(
      const HangingTestURLLoaderHeaderClient&) = delete;

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override {
    header_client_.Bind(std::move(receiver));
  }
  void OnLoaderForCorsPreflightCreated(
      const ResourceRequest& request,
      mojo::PendingReceiver<mojom::TrustedHeaderClient> receiver) override {
    header_client_.Bind(std::move(receiver));
  }

  void CallOnBeforeSendHeadersCallback() {
    header_client_.CallOnBeforeSendHeadersCallback();
  }

  void WaitForOnBeforeSendHeaders() {
    header_client_.WaitForOnBeforeSendHeaders();
  }

  void CallOnHeadersReceivedCallback() {
    header_client_.CallOnHeadersReceivedCallback();
  }

  void WaitForOnHeadersReceived() { header_client_.WaitForOnHeadersReceived(); }

 private:
  TestHeaderClient header_client_;
  mojo::Receiver<mojom::TrustedURLLoaderHeaderClient> receiver_;
};

// Test waiting on the OnHeadersReceived event, then proceeding to call the
// OnHeadersReceivedCallback asynchronously. This mostly just verifies that
// HangingTestURLLoaderHeaderClient works.
TEST_F(NetworkContextTest, HangingHeaderClientModifiesHeadersAsynchronously) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ResourceRequest request;
  request.url = test_server.GetURL("/echoheader?foo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  HangingTestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  mojo::PendingRemote<mojom::URLLoader> loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  header_client.WaitForOnBeforeSendHeaders();
  header_client.CallOnBeforeSendHeadersCallback();

  header_client.WaitForOnHeadersReceived();
  header_client.CallOnHeadersReceivedCallback();

  client.RunUntilComplete();

  EXPECT_EQ(client.completion_status().error_code, net::OK);
  // Make sure request header was modified. The value will be in the body
  // since we used the /echoheader endpoint.
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client.response_body_release(), &response));
  EXPECT_EQ(response, "bar");

  // Make sure response header was modified.
  EXPECT_TRUE(client.response_head()->headers->HasHeaderValue("baz", "qux"));
}

// Test destroying the mojom::URLLoader after the OnBeforeSendHeaders event and
// then calling the OnBeforeSendHeadersCallback.
TEST_F(NetworkContextTest, HangingHeaderClientAbortDuringOnBeforeSendHeaders) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ResourceRequest request;
  request.url = test_server.GetURL("/echoheader?foo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  HangingTestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  mojo::PendingRemote<mojom::URLLoader> loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  header_client.WaitForOnBeforeSendHeaders();

  loader.reset();

  // Ensure the loader is destroyed before the callback is run.
  base::RunLoop().RunUntilIdle();

  header_client.CallOnBeforeSendHeadersCallback();

  client.RunUntilComplete();

  EXPECT_EQ(client.completion_status().error_code, net::ERR_ABORTED);
}

// Test destroying the mojom::URLLoader after the OnHeadersReceived event and
// then calling the OnHeadersReceivedCallback.
TEST_F(NetworkContextTest, HangingHeaderClientAbortDuringOnHeadersReceived) {
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  ResourceRequest request;
  request.url = test_server.GetURL("/echoheader?foo");

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  HangingTestURLLoaderHeaderClient header_client(
      params->header_client.InitWithNewPipeAndPassReceiver());
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  mojo::PendingRemote<mojom::URLLoader> loader;
  TestURLLoaderClient client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      mojom::kURLLoadOptionUseHeaderClient, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  header_client.WaitForOnBeforeSendHeaders();
  header_client.CallOnBeforeSendHeadersCallback();

  header_client.WaitForOnHeadersReceived();

  loader.reset();

  // Ensure the loader is destroyed before the callback is run.
  base::RunLoop().RunUntilIdle();

  header_client.CallOnHeadersReceivedCallback();

  client.RunUntilComplete();

  EXPECT_EQ(client.completion_status().error_code, net::ERR_ABORTED);
}

// Custom proxy does not apply to localhost, so resolve kMockHost to localhost,
// and use that instead.
class NetworkContextMockHostTest : public NetworkContextTest {
 public:
  NetworkContextMockHostTest() {
    net::MockHostResolverBase::RuleResolver rules;
    rules.AddRule(kMockHost, "127.0.0.1");

    network_service_->set_host_resolver_factory_for_testing(
        std::make_unique<net::MockHostResolverFactory>(std::move(rules)));
  }

 protected:
  GURL GetURLWithMockHost(const net::EmbeddedTestServer& server,
                          const std::string& relative_url) {
    GURL server_base_url = server.base_url();
    GURL base_url =
        GURL(base::StrCat({server_base_url.scheme(), "://", kMockHost, ":",
                           server_base_url.port()}));
    EXPECT_TRUE(base_url.is_valid()) << base_url.possibly_invalid_spec();
    return base_url.Resolve(relative_url);
  }

  net::ProxyServer ConvertToProxyServer(const net::EmbeddedTestServer& server) {
    std::string base_url = server.base_url().spec();
    // Remove slash from URL.
    base_url.pop_back();
    auto proxy_server =
        net::ProxyUriToProxyServer(base_url, net::ProxyServer::SCHEME_HTTP);
    EXPECT_TRUE(proxy_server.is_valid()) << base_url;
    return proxy_server;
  }
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Flaky crashes on Linux: https://crbug.com/1115201
#define MAYBE_CustomProxyUsesSpecifiedProxyList \
  DISABLED_CustomProxyUsesSpecifiedProxyList
#else
#define MAYBE_CustomProxyUsesSpecifiedProxyList \
  CustomProxyUsesSpecifiedProxyList
#endif
TEST_F(NetworkContextMockHostTest, MAYBE_CustomProxyUsesSpecifiedProxyList) {
  net::EmbeddedTestServer proxy_test_server;
  net::test_server::RegisterDefaultHandlers(&proxy_test_server);
  ASSERT_TRUE(proxy_test_server.Start());

  mojo::Remote<mojom::CustomProxyConfigClient> proxy_config_client;
  mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParamsForTesting();
  context_params->custom_proxy_config_client_receiver =
      proxy_config_client.BindNewPipeAndPassReceiver();
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(context_params));

  auto config = mojom::CustomProxyConfig::New();
  config->rules.ParseFromString(
      "http=" +
      net::ProxyServerToProxyUri(ConvertToProxyServer(proxy_test_server)));
  base::RunLoop loop;
  proxy_config_client->OnCustomProxyConfigUpdated(std::move(config),
                                                  loop.QuitClosure());
  loop.Run();

  ResourceRequest request;
  request.url = GURL("http://does.not.resolve/echo");
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));

  // |invalid_server| has no handlers set up so would return an empty response.
  EXPECT_EQ(response, "Echo");
  EXPECT_EQ(client->response_head()->proxy_server,
            ConvertToProxyServer(proxy_test_server));
}

TEST_F(NetworkContextTest, MaximumCount) {
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));

  const char kPath1[] = "/foobar";
  const char kPath2[] = "/hung";
  const char kPath3[] = "/hello.html";
  net::test_server::ControllableHttpResponse controllable_response1(
      &test_server, kPath1);

  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  network_context->set_max_loaders_per_process_for_testing(2);

  mojo::Remote<mojom::URLLoaderFactory> loader_factory;
  mojom::URLLoaderFactoryParamsPtr params =
      mojom::URLLoaderFactoryParams::New();
  params->process_id = mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  ResourceRequest request;
  request.url = test_server.GetURL(kPath1);
  auto client1 = std::make_unique<TestURLLoaderClient>();
  mojo::PendingRemote<mojom::URLLoader> loader1;
  loader_factory->CreateLoaderAndStart(
      loader1.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      0 /* options */, request, client1->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  request.url = test_server.GetURL(kPath2);
  auto client2 = std::make_unique<TestURLLoaderClient>();
  mojo::PendingRemote<mojom::URLLoader> loader2;
  loader_factory->CreateLoaderAndStart(
      loader2.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
      0 /* options */, request, client2->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // A third request should fail, since the first two are outstanding and the
  // limit is 2.
  request.url = test_server.GetURL(kPath3);
  auto client3 = std::make_unique<TestURLLoaderClient>();
  mojo::Remote<mojom::URLLoader> loader3;
  loader_factory->CreateLoaderAndStart(
      loader3.BindNewPipeAndPassReceiver(), 0 /* request_id */, 0 /* options */,
      request, client3->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client3->RunUntilComplete();
  ASSERT_EQ(client3->completion_status().error_code,
            net::ERR_INSUFFICIENT_RESOURCES);

  // Complete the first request and try the third again.
  controllable_response1.WaitForRequest();
  controllable_response1.Send("HTTP/1.1 200 OK\r\n");
  controllable_response1.Done();

  client1->RunUntilComplete();
  ASSERT_EQ(client1->completion_status().error_code, net::OK);

  client3 = std::make_unique<TestURLLoaderClient>();
  loader3.reset();
  loader_factory->CreateLoaderAndStart(
      loader3.BindNewPipeAndPassReceiver(), 0 /* request_id */, 0 /* options */,
      request, client3->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client3->RunUntilComplete();
  ASSERT_EQ(client3->completion_status().error_code, net::OK);
}

TEST_F(NetworkContextTest, AllowAllCookies) {
  net::test_server::EmbeddedTestServer test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  GURL server_url = test_server.GetURL("/echoheader?Cookie");
  GURL first_party_url(server_url);
  GURL third_party_url("http://www.some.other.origin.test/");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  EXPECT_TRUE(
      SetCookieHelper(network_context.get(), server_url, "TestCookie", "1"));

  int url_loader_options = mojom::kURLLoadOptionNone;

  ResourceRequest first_party_request;
  first_party_request.url = server_url;
  first_party_request.site_for_cookies =
      net::SiteForCookies::FromUrl(first_party_url);

  std::unique_ptr<TestURLLoaderClient> client = FetchRequest(
      first_party_request, network_context.get(), url_loader_options);

  std::string response_body;
  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("TestCookie=1", response_body);

  ResourceRequest third_party_request;
  third_party_request.url = server_url;
  third_party_request.site_for_cookies =
      net::SiteForCookies::FromUrl(third_party_url);

  client = FetchRequest(third_party_request, network_context.get(),
                        url_loader_options);

  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("TestCookie=1", response_body);
}

TEST_F(NetworkContextTest, BlockThirdPartyCookies) {
  net::test_server::EmbeddedTestServer test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  GURL server_url = test_server.GetURL("/echoheader?Cookie");
  GURL first_party_url(server_url);
  GURL third_party_url("http://www.some.other.origin.test/");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  EXPECT_TRUE(
      SetCookieHelper(network_context.get(), server_url, "TestCookie", "1"));

  int url_loader_options = mojom::kURLLoadOptionBlockThirdPartyCookies;

  ResourceRequest first_party_request;
  first_party_request.url = server_url;
  first_party_request.site_for_cookies =
      net::SiteForCookies::FromUrl(first_party_url);

  std::unique_ptr<TestURLLoaderClient> client = FetchRequest(
      first_party_request, network_context.get(), url_loader_options);

  std::string response_body;
  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("TestCookie=1", response_body);

  ResourceRequest third_party_request;
  third_party_request.url = server_url;
  third_party_request.site_for_cookies =
      net::SiteForCookies::FromUrl(third_party_url);

  client = FetchRequest(third_party_request, network_context.get(),
                        url_loader_options);

  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("None", response_body);
}

TEST_F(NetworkContextTest, BlockAllCookies) {
  net::test_server::EmbeddedTestServer test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  GURL server_url = test_server.GetURL("/echoheader?Cookie");
  GURL first_party_url(server_url);
  GURL third_party_url("http://www.some.other.origin.test/");

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  EXPECT_TRUE(
      SetCookieHelper(network_context.get(), server_url, "TestCookie", "1"));

  int url_loader_options = mojom::kURLLoadOptionBlockAllCookies;

  ResourceRequest first_party_request;
  first_party_request.url = server_url;
  first_party_request.site_for_cookies =
      net::SiteForCookies::FromUrl(first_party_url);

  std::unique_ptr<TestURLLoaderClient> client = FetchRequest(
      first_party_request, network_context.get(), url_loader_options);

  std::string response_body;
  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("None", response_body);

  ResourceRequest third_party_request;
  third_party_request.url = server_url;
  third_party_request.site_for_cookies =
      net::SiteForCookies::FromUrl(third_party_url);

  client = FetchRequest(third_party_request, network_context.get(),
                        url_loader_options);

  ASSERT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(mojo::BlockingCopyToString(client->response_body_release(),
                                         &response_body));
  EXPECT_EQ("None", response_body);
}

#if BUILDFLAG(IS_CT_SUPPORTED)
TEST_F(NetworkContextTest, CertificateTransparencyConfig) {
  // Configure CT logs in network service.
  std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;

  // The log public keys do not matter for the test, so invalid keys are used.
  // However, because the log IDs are derived from the SHA-256 hash of the log
  // key, the log keys are generated such that qualified logs are in the form
  // of four digits (e.g. "0000", "1111"), while disqualified logs are in the
  // form of four letters (e.g. "AAAA", "BBBB").

  for (int i = 0; i < 6; ++i) {
    network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
    // Shift to ASCII '0' (0x30)
    log_info->public_key = std::string(4, 0x30 + static_cast<char>(i));
    log_info->name = std::string(4, 0x30 + static_cast<char>(i));
    log_info->operated_by_google = i % 2;
    if (log_info->operated_by_google) {
      log_info->current_operator = "Google";
    } else {
      log_info->current_operator = "Not Google";
    }
    log_list_mojo.push_back(std::move(log_info));
  }
  for (int i = 0; i < 3; ++i) {
    network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
    // Shift to ASCII 'A' (0x41)
    log_info->public_key = std::string(4, 0x41 + static_cast<char>(i));
    log_info->name = std::string(4, 0x41 + static_cast<char>(i));
    log_info->operated_by_google = false;
    log_info->disqualified_at = base::Time::FromTimeT(i);
    log_info->current_operator = "Not Google Either";

    log_list_mojo.push_back(std::move(log_info));
  }
  base::RunLoop run_loop;
  network_service()->UpdateCtLogList(std::move(log_list_mojo),
                                     base::Time::Now(), run_loop.QuitClosure());
  run_loop.Run();

  // Configure CT params in network context.
  mojom::NetworkContextParamsPtr params =
      CreateNetworkContextParamsForTesting();
  params->enforce_chrome_ct_policy = true;

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));

  net::CTPolicyEnforcer* request_enforcer =
      network_context->url_request_context()->ct_policy_enforcer();
  ASSERT_TRUE(request_enforcer);

  // Completely unsafe if |enforce_chrome_ct_policy| is false.
  certificate_transparency::ChromeCTPolicyEnforcer* policy_enforcer =
      reinterpret_cast<certificate_transparency::ChromeCTPolicyEnforcer*>(
          request_enforcer);

  EXPECT_TRUE(std::is_sorted(
      policy_enforcer->operated_by_google_logs_for_testing().begin(),
      policy_enforcer->operated_by_google_logs_for_testing().end()));
  EXPECT_TRUE(
      std::is_sorted(policy_enforcer->disqualified_logs_for_testing().begin(),
                     policy_enforcer->disqualified_logs_for_testing().end()));

  EXPECT_THAT(
      policy_enforcer->operated_by_google_logs_for_testing(),
      ::testing::UnorderedElementsAreArray({crypto::SHA256HashString("1111"),
                                            crypto::SHA256HashString("3333"),
                                            crypto::SHA256HashString("5555")}));
  EXPECT_THAT(policy_enforcer->disqualified_logs_for_testing(),
              ::testing::UnorderedElementsAre(
                  ::testing::Pair(crypto::SHA256HashString("AAAA"),
                                  base::Time::FromTimeT(0)),
                  ::testing::Pair(crypto::SHA256HashString("BBBB"),
                                  base::Time::FromTimeT(1)),
                  ::testing::Pair(crypto::SHA256HashString("CCCC"),
                                  base::Time::FromTimeT(2))));

  std::map<std::string, certificate_transparency::OperatorHistoryEntry>
      operator_history = policy_enforcer->operator_history_for_testing();

  for (auto log : policy_enforcer->operated_by_google_logs_for_testing()) {
    EXPECT_EQ(operator_history[log].current_operator_, "Google");
    EXPECT_TRUE(operator_history[log].previous_operators_.empty());
  }

  for (auto log : policy_enforcer->disqualified_logs_for_testing()) {
    EXPECT_EQ(operator_history[log.first].current_operator_,
              "Not Google Either");
    EXPECT_TRUE(operator_history[log.first].previous_operators_.empty());
  }
}

TEST_F(NetworkContextTest, CertificateTransparencyConfigWithOperatorSwitches) {
  // Configure CT logs in network service.
  std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;

  // The log public keys do not matter for the test, so invalid keys are used.
  // However, because the log IDs are derived from the SHA-256 hash of the log
  // key, the log keys are generated such that the log that never switched
  // operator is "0000", while the one that did is "AAAA".
  network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
  // Shift to ASCII '0' (0x30)
  log_info->public_key = std::string(4, 0x30);
  log_info->name = std::string(4, 0x30);
  log_info->current_operator = "Forever Operator";
  log_list_mojo.push_back(std::move(log_info));

  log_info = network::mojom::CTLogInfo::New();
  // Shift to ASCII 'A' (0x41)
  log_info->public_key = std::string(4, 0x41);
  log_info->name = std::string(4, 0x41);
  log_info->current_operator = "Changed Operator";
  for (int i = 0; i < 3; i++) {
    network::mojom::PreviousOperatorEntryPtr previous_operator =
        network::mojom::PreviousOperatorEntry::New();
    previous_operator->name = "Operator " + base::NumberToString(i);
    previous_operator->end_time = base::Time::FromTimeT(i);
    log_info->previous_operators.push_back(std::move(previous_operator));
  }
  log_list_mojo.push_back(std::move(log_info));

  base::RunLoop run_loop;
  network_service()->UpdateCtLogList(std::move(log_list_mojo),
                                     base::Time::Now(), run_loop.QuitClosure());
  run_loop.Run();

  // Configure CT params in network context.
  mojom::NetworkContextParamsPtr params =
      CreateNetworkContextParamsForTesting();
  params->enforce_chrome_ct_policy = true;

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));

  net::CTPolicyEnforcer* request_enforcer =
      network_context->url_request_context()->ct_policy_enforcer();
  ASSERT_TRUE(request_enforcer);

  // Completely unsafe if |enforce_chrome_ct_policy| is false.
  certificate_transparency::ChromeCTPolicyEnforcer* policy_enforcer =
      reinterpret_cast<certificate_transparency::ChromeCTPolicyEnforcer*>(
          request_enforcer);

  std::map<std::string, certificate_transparency::OperatorHistoryEntry>
      operator_history = policy_enforcer->operator_history_for_testing();

  EXPECT_EQ(
      operator_history[crypto::SHA256HashString("0000")].current_operator_,
      "Forever Operator");
  EXPECT_TRUE(operator_history[crypto::SHA256HashString("0000")]
                  .previous_operators_.empty());

  EXPECT_EQ(
      operator_history[crypto::SHA256HashString("AAAA")].current_operator_,
      "Changed Operator");
  EXPECT_THAT(
      operator_history[crypto::SHA256HashString("AAAA")].previous_operators_,
      ::testing::ElementsAre(
          ::testing::Pair("Operator 0", base::Time::FromTimeT(0)),
          ::testing::Pair("Operator 1", base::Time::FromTimeT(1)),
          ::testing::Pair("Operator 2", base::Time::FromTimeT(2))));
}
#endif

TEST_F(NetworkContextTest, AddHttpAuthCacheEntry) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();
  ASSERT_TRUE(cache);
  // |key_server_entries_by_network_anonymization_key| should be disabled by
  // default, so the passed in NetworkIsolationKeys don't matter.
  EXPECT_FALSE(cache->key_server_entries_by_network_anonymization_key());

  // Add an AUTH_SERVER cache entry.
  url::SchemeHostPort scheme_host_port(GURL("http://example.test/"));
  net::AuthChallengeInfo challenge;
  challenge.is_proxy = false;
  challenge.challenger = scheme_host_port;
  challenge.scheme = "basic";
  challenge.realm = "testrealm";
  const char16_t kUsername[] = u"test_user";
  const char16_t kPassword[] = u"test_pass";
  ASSERT_FALSE(cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_SERVER,
                             challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkAnonymizationKey()));
  base::RunLoop run_loop;
  network_context->AddAuthCacheEntry(challenge, net::NetworkAnonymizationKey(),
                                     net::AuthCredentials(kUsername, kPassword),
                                     run_loop.QuitClosure());
  run_loop.Run();
  net::HttpAuthCache::Entry* entry = cache->Lookup(
      scheme_host_port, net::HttpAuth::AUTH_SERVER, challenge.realm,
      net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(scheme_host_port, entry->scheme_host_port());
  EXPECT_EQ(challenge.realm, entry->realm());
  EXPECT_EQ(net::HttpAuth::StringToScheme(challenge.scheme), entry->scheme());
  EXPECT_EQ(kUsername, entry->credentials().username());
  EXPECT_EQ(kPassword, entry->credentials().password());
  // Entry should only have been added for server auth.
  EXPECT_FALSE(cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_PROXY,
                             challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkAnonymizationKey()));

  // Add an AUTH_PROXY cache entry.
  url::SchemeHostPort proxy_scheme_host_port(GURL("http://proxy.test/"));
  challenge.is_proxy = true;
  challenge.challenger = proxy_scheme_host_port;
  const char16_t kProxyUsername[] = u"test_proxy_user";
  const char16_t kProxyPassword[] = u"test_proxy_pass";
  ASSERT_FALSE(cache->Lookup(proxy_scheme_host_port, net::HttpAuth::AUTH_PROXY,
                             challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkAnonymizationKey()));
  base::RunLoop run_loop2;
  network_context->AddAuthCacheEntry(
      challenge, net::NetworkAnonymizationKey(),
      net::AuthCredentials(kProxyUsername, kProxyPassword),
      run_loop2.QuitClosure());
  run_loop2.Run();
  entry = cache->Lookup(proxy_scheme_host_port, net::HttpAuth::AUTH_PROXY,
                        challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                        net::NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(proxy_scheme_host_port, entry->scheme_host_port());
  EXPECT_EQ(challenge.realm, entry->realm());
  EXPECT_EQ(net::HttpAuth::StringToScheme(challenge.scheme), entry->scheme());
  EXPECT_EQ(kProxyUsername, entry->credentials().username());
  EXPECT_EQ(kProxyPassword, entry->credentials().password());
  // Entry should only have been added for proxy auth.
  EXPECT_FALSE(cache->Lookup(proxy_scheme_host_port, net::HttpAuth::AUTH_SERVER,
                             challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkAnonymizationKey()));
}

TEST_F(NetworkContextTest, AddHttpAuthCacheEntryWithNetworkIsolationKey) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  network_context->SetSplitAuthCacheByNetworkAnonymizationKey(true);

  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();
  ASSERT_TRUE(cache);
  // If this isn't true, the rest of this test is pretty meaningless.
  ASSERT_TRUE(cache->key_server_entries_by_network_anonymization_key());

  // Add an AUTH_SERVER cache entry.
  url::Origin origin = url::Origin::Create(GURL("http://example.test/"));
  net::SchemefulSite site = net::SchemefulSite(GURL("http://example.test/"));
  url::SchemeHostPort scheme_host_port =
      origin.GetTupleOrPrecursorTupleIfOpaque();
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  net::NetworkAnonymizationKey network_anonymization_key =
      net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          network_isolation_key);
  net::AuthChallengeInfo challenge;
  challenge.is_proxy = false;
  challenge.challenger = scheme_host_port;
  challenge.scheme = "basic";
  challenge.realm = "testrealm";
  const char16_t kUsername[] = u"test_user";
  const char16_t kPassword[] = u"test_pass";
  ASSERT_FALSE(cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_SERVER,
                             challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                             network_anonymization_key));
  base::RunLoop run_loop;
  network_context->AddAuthCacheEntry(challenge, network_anonymization_key,
                                     net::AuthCredentials(kUsername, kPassword),
                                     run_loop.QuitClosure());
  run_loop.Run();
  net::HttpAuthCache::Entry* entry = cache->Lookup(
      scheme_host_port, net::HttpAuth::AUTH_SERVER, challenge.realm,
      net::HttpAuth::AUTH_SCHEME_BASIC, network_anonymization_key);
  ASSERT_TRUE(entry);
  EXPECT_EQ(scheme_host_port, entry->scheme_host_port());
  EXPECT_EQ(challenge.realm, entry->realm());
  EXPECT_EQ(net::HttpAuth::StringToScheme(challenge.scheme), entry->scheme());
  EXPECT_EQ(kUsername, entry->credentials().username());
  EXPECT_EQ(kPassword, entry->credentials().password());
  // Entry should only be accessibly when using the correct
  // NetworkAnonymizationKey.
  EXPECT_FALSE(cache->Lookup(scheme_host_port, net::HttpAuth::AUTH_SERVER,
                             challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkAnonymizationKey()));
}

TEST_F(NetworkContextTest, CopyHttpAuthCacheProxyEntries) {
  const url::SchemeHostPort kSchemeHostPort(GURL("http://foo.com"));

  std::unique_ptr<NetworkContext> network_context1 =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  net::AuthChallengeInfo challenge;
  challenge.is_proxy = true;
  challenge.challenger = kSchemeHostPort;
  challenge.scheme = "basic";
  challenge.realm = "testrealm";
  const char16_t kProxyUsername[] = u"proxy_user";
  const char16_t kProxyPassword[] = u"proxy_pass";

  base::RunLoop run_loop1;
  network_context1->AddAuthCacheEntry(
      challenge, net::NetworkAnonymizationKey(),
      net::AuthCredentials(kProxyUsername, kProxyPassword),
      run_loop1.QuitClosure());
  run_loop1.Run();

  challenge.is_proxy = false;
  const char16_t kServerUsername[] = u"server_user";
  const char16_t kServerPassword[] = u"server_pass";

  base::RunLoop run_loop2;
  network_context1->AddAuthCacheEntry(
      challenge, net::NetworkAnonymizationKey(),
      net::AuthCredentials(kServerUsername, kServerPassword),
      run_loop2.QuitClosure());
  run_loop2.Run();

  base::UnguessableToken token;
  base::RunLoop run_loop3;
  network_context1->SaveHttpAuthCacheProxyEntries(base::BindLambdaForTesting(
      [&](const base::UnguessableToken& returned_token) {
        token = returned_token;
        run_loop3.Quit();
      }));
  run_loop3.Run();

  // Delete first NetworkContext, to make sure saved credentials outlast it.
  network_context1.reset();
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<NetworkContext> network_context2 =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop4;
  network_context2->LoadHttpAuthCacheProxyEntries(token,
                                                  run_loop4.QuitClosure());
  run_loop4.Run();

  // Check cached credentials directly, since there's no API to check proxy
  // credentials.
  net::HttpAuthCache* cache = network_context2->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();
  // The server credentials should not have been copied.
  EXPECT_FALSE(cache->Lookup(kSchemeHostPort, net::HttpAuth::AUTH_SERVER,
                             challenge.realm, net::HttpAuth::AUTH_SCHEME_BASIC,
                             net::NetworkAnonymizationKey()));
  net::HttpAuthCache::Entry* entry = cache->Lookup(
      kSchemeHostPort, net::HttpAuth::AUTH_PROXY, challenge.realm,
      net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkAnonymizationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(kProxyUsername, entry->credentials().username());
  EXPECT_EQ(kProxyPassword, entry->credentials().password());
}

TEST_F(NetworkContextTest, SplitAuthCacheByNetworkIsolationKey) {
  const url::SchemeHostPort kSchemeHostPort(GURL("http://foo.com"));

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  net::HttpAuthCache* cache = network_context->url_request_context()
                                  ->http_transaction_factory()
                                  ->GetSession()
                                  ->http_auth_cache();

  EXPECT_FALSE(cache->key_server_entries_by_network_anonymization_key());

  // Add proxy credentials, which should never be deleted.
  net::AuthChallengeInfo challenge;
  challenge.is_proxy = true;
  challenge.challenger = kSchemeHostPort;
  challenge.scheme = "basic";
  challenge.realm = "testrealm";
  const char16_t kProxyUsername[] = u"proxy_user";
  const char16_t kProxyPassword[] = u"proxy_pass";
  base::RunLoop run_loop1;
  network_context->AddAuthCacheEntry(
      challenge, net::NetworkAnonymizationKey(),
      net::AuthCredentials(kProxyUsername, kProxyPassword),
      run_loop1.QuitClosure());
  run_loop1.Run();

  // Set up challenge to add server credentials.
  challenge.is_proxy = false;

  for (bool set_split_cache_by_network_isolation_key : {true, false}) {
    // In each loop iteration, the setting should change, which should clear
    // server credentials.
    EXPECT_NE(set_split_cache_by_network_isolation_key,
              cache->key_server_entries_by_network_anonymization_key());

    // Add server credentials.
    const char16_t kServerUsername[] = u"server_user";
    const char16_t kServerPassword[] = u"server_pass";
    base::RunLoop run_loop2;
    network_context->AddAuthCacheEntry(
        challenge, net::NetworkAnonymizationKey(),
        net::AuthCredentials(kServerUsername, kServerPassword),
        run_loop2.QuitClosure());
    run_loop2.Run();

    // Toggle setting.
    network_context->SetSplitAuthCacheByNetworkAnonymizationKey(
        set_split_cache_by_network_isolation_key);
    EXPECT_EQ(set_split_cache_by_network_isolation_key,
              cache->key_server_entries_by_network_anonymization_key());

    // The server credentials should have been deleted.
    EXPECT_FALSE(cache->Lookup(
        kSchemeHostPort, net::HttpAuth::AUTH_SERVER, challenge.realm,
        net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkAnonymizationKey()));

    // The proxy credentials should still be in the cache.
    net::HttpAuthCache::Entry* entry = cache->Lookup(
        kSchemeHostPort, net::HttpAuth::AUTH_PROXY, challenge.realm,
        net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkAnonymizationKey());
    ASSERT_TRUE(entry);
    EXPECT_EQ(kProxyUsername, entry->credentials().username());
    EXPECT_EQ(kProxyPassword, entry->credentials().password());
  }
}

TEST_F(NetworkContextTest, HSTSPolicyBypassList) {
  // The default test preload list includes "example" as a preloaded TLD
  // (including subdomains).
  net::ScopedTransportSecurityStateSource scoped_security_state_source;

  mojom::NetworkContextParamsPtr params =
      CreateNetworkContextParamsForTesting();
  params->hsts_policy_bypass_list.push_back("example");
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(std::move(params));
  net::TransportSecurityState* transport_security_state =
      network_context->url_request_context()->transport_security_state();
  // With the policy set, example should no longer upgrade to HTTPS.
  EXPECT_FALSE(transport_security_state->ShouldUpgradeToSSL("example"));
  // But the policy shouldn't apply to subdomains.
  EXPECT_TRUE(transport_security_state->ShouldUpgradeToSSL("sub.example"));
}

TEST_F(NetworkContextTest, FactoriesDeletedWhenBindingsCleared) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  auto loader_params = mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = 1;
  mojo::Remote<mojom::URLLoaderFactory> remote1;
  network_context->CreateURLLoaderFactory(remote1.BindNewPipeAndPassReceiver(),
                                          std::move(loader_params));

  loader_params = mojom::URLLoaderFactoryParams::New();
  loader_params->process_id = 1;
  mojo::Remote<mojom::URLLoaderFactory> remote2;
  network_context->CreateURLLoaderFactory(remote2.BindNewPipeAndPassReceiver(),
                                          std::move(loader_params));

  // We should have at least 2 loader factories.
  EXPECT_GT(network_context->num_url_loader_factories_for_testing(), 1u);
  network_context->ResetURLLoaderFactories();
  EXPECT_EQ(network_context->num_url_loader_factories_for_testing(), 0u);
}

static ResourceRequest CreateResourceRequest(const char* method,
                                             const GURL& url) {
  ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.request_initiator =
      url::Origin::Create(url);  // ensure initiator is set
  return request;
}

class NetworkContextSplitCacheTest : public NetworkContextTest {
 protected:
  NetworkContextSplitCacheTest() {
    feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("services/test/data")));
    EXPECT_TRUE(test_server_.Start());

    // Set up a scoped host resolver to access other origins.
    scoped_refptr<net::RuleBasedHostResolverProc> mock_resolver_proc =
        base::MakeRefCounted<net::RuleBasedHostResolverProc>(nullptr);
    mock_resolver_proc->AddRule("*", "127.0.0.1");
    mock_host_resolver_ = std::make_unique<net::ScopedDefaultHostResolverProc>(
        mock_resolver_proc.get());

    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    network_context_ = CreateContextWithParams(std::move(context_params));
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }

  void LoadAndVerifyCached(const GURL& url,
                           const net::IsolationInfo& isolation_info,
                           bool was_cached,
                           bool expect_redirect = false,
                           absl::optional<GURL> new_url = absl::nullopt,
                           bool automatically_assign_isolation_info = false) {
    ResourceRequest request = CreateResourceRequest("GET", url);
    request.load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;

    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    auto params = mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    if (isolation_info.request_type() ==
        net::IsolationInfo::RequestType::kOther) {
      params->isolation_info = isolation_info;
    } else {
      request.trusted_params = ResourceRequest::TrustedParams();
      request.trusted_params->isolation_info = isolation_info;
      params->is_trusted = true;
      // These params must be individually set, to be consistent with the
      // IsolationInfo if its request type is a main frame navigation.
      // TODO(crbug.com/1172314): Unify these to avoid inconsistencies.
      if (isolation_info.request_type() ==
          net::IsolationInfo::RequestType::kMainFrame) {
        request.is_outermost_main_frame = true;
        request.update_first_party_url_on_redirect = true;
      }
    }

    params->automatically_assign_isolation_info =
        automatically_assign_isolation_info;

    request.site_for_cookies = isolation_info.site_for_cookies();

    network_context_->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));
    auto client = std::make_unique<TestURLLoaderClient>();
    mojo::Remote<mojom::URLLoader> loader;
    loader_factory->CreateLoaderAndStart(
        loader.BindNewPipeAndPassReceiver(), 0 /* request_id */,
        mojom::kURLLoadOptionNone, request, client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    if (expect_redirect) {
      client->RunUntilRedirectReceived();
      loader->FollowRedirect({}, {}, {}, new_url);
      client->ClearHasReceivedRedirect();
    }

    if (new_url) {
      client->RunUntilRedirectReceived();
      loader->FollowRedirect({}, {}, {}, absl::nullopt);
    }

    client->RunUntilComplete();

    EXPECT_EQ(net::OK, client->completion_status().error_code);
    EXPECT_EQ(was_cached, client->completion_status().exists_in_cache);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_;
  std::unique_ptr<NetworkContext> network_context_;
};

TEST_F(NetworkContextSplitCacheTest, CachedUsingNetworkIsolationKey) {
  GURL url = test_server()->GetURL("/resource");
  url::Origin origin_a = url::Origin::Create(GURL("http://a.test/"));
  net::IsolationInfo info_a =
      net::IsolationInfo::CreateForInternalRequest(origin_a);
  LoadAndVerifyCached(url, info_a, false /* was_cached */);

  // Load again with a different isolation key. The cached entry should not be
  // loaded.
  url::Origin origin_b = url::Origin::Create(GURL("http://b.test/"));
  net::IsolationInfo info_b =
      net::IsolationInfo::CreateForInternalRequest(origin_b);
  LoadAndVerifyCached(url, info_b, false /* was_cached */);

  // Load again with the same isolation key. The cached entry should be loaded.
  LoadAndVerifyCached(url, info_b, true /* was_cached */);
}

TEST_F(NetworkContextSplitCacheTest,
       NavigationResourceCachedUsingNetworkIsolationKey) {
  GURL url = test_server()->GetURL("othersite.test", "/main.html");
  url::Origin origin_a = url::Origin::Create(url);
  net::IsolationInfo info_a =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kSubFrame,
                                 origin_a, origin_a, net::SiteForCookies());
  LoadAndVerifyCached(url, info_a, false /* was_cached */);

  // Load again with a different isolation key. The cached entry should not be
  // loaded.
  GURL url_b = test_server()->GetURL("/main.html");
  url::Origin origin_b = url::Origin::Create(url_b);
  net::IsolationInfo info_b =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kSubFrame,
                                 origin_b, origin_b, net::SiteForCookies());
  LoadAndVerifyCached(url_b, info_b, false /* was_cached */);

  // Load again with the same isolation key. The cached entry should be loaded.
  LoadAndVerifyCached(url_b, info_b, true /* was_cached */);
}

TEST_F(NetworkContextSplitCacheTest,
       CachedUsingNetworkIsolationKeyWithFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSplitCacheByNetworkIsolationKey);

  GURL url = test_server()->GetURL("/resource");
  url::Origin origin_a = url::Origin::Create(GURL("http://a.test/"));
  net::IsolationInfo info_a =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin_a, origin_a, net::SiteForCookies());
  LoadAndVerifyCached(url, info_a, false /* was_cached */);

  // Load again with a different isolation key. The cached entry should not be
  // loaded.
  url::Origin origin_b = url::Origin::Create(GURL("http://b.test/"));
  net::IsolationInfo info_b =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin_a, origin_b, net::SiteForCookies());
  LoadAndVerifyCached(url, info_b, false /* was_cached */);
}

TEST_F(NetworkContextSplitCacheTest,
       NavigationResourceRedirectNetworkIsolationKey) {
  // Create a request that redirects.
  GURL url = test_server()->GetURL(
      "/server-redirect?" +
      test_server()->GetURL("othersite.test", "/title1.html").spec());
  url::Origin origin = url::Origin::Create(url);
  net::IsolationInfo info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, origin, origin,
      net::SiteForCookies::FromOrigin(origin));
  LoadAndVerifyCached(url, info, false /* was_cached */,
                      true /* expect_redirect */);

  // Now directly load with the key using the redirected URL. This should be a
  // cache hit.
  GURL redirected_url = test_server()->GetURL("othersite.test", "/title1.html");
  url::Origin redirected_origin = url::Origin::Create(redirected_url);
  LoadAndVerifyCached(redirected_url, info.CreateForRedirect(redirected_origin),
                      true /* was_cached */);

  // A non-navigation resource with the same key and url should also be cached.
  net::IsolationInfo non_navigation_redirected_info =
      net::IsolationInfo::Create(
          net::IsolationInfo::RequestType::kOther, redirected_origin,
          redirected_origin,
          net::SiteForCookies::FromOrigin(redirected_origin));
  LoadAndVerifyCached(redirected_url, non_navigation_redirected_info,
                      true /* was_cached */);
}

TEST_F(NetworkContextSplitCacheTest, AutomaticallyAssignIsolationInfo) {
  GURL url = test_server()->GetURL("/resource");
  // Load with an automatically assigned IsolationInfo, which should populate
  // the cache using the IsolationInfo for |url|'s origin.
  LoadAndVerifyCached(url, net::IsolationInfo(), false /* was_cached */,
                      false /* expect_redirect */, absl::nullopt /* new_url */,
                      true /* automatically_assign_isolation_info */);

  // Load again with a different isolation info. The cached entry should not be
  // loaded.
  url::Origin other_origin = url::Origin::Create(GURL("http://other.test/"));
  net::IsolationInfo other_info =
      net::IsolationInfo::CreateForInternalRequest(other_origin);
  LoadAndVerifyCached(url, other_info, false /* was_cached */);

  // Load explicitly using the requested URL's own IsolationInfo, which should
  // use the cached entry.
  url::Origin origin = url::Origin::Create(GURL(url));
  net::IsolationInfo info =
      net::IsolationInfo::CreateForInternalRequest(origin);
  LoadAndVerifyCached(url, info, true /* was_cached */);
}

TEST_F(NetworkContextTest, EnableTrustTokens) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  EXPECT_TRUE(network_context->trust_token_store());

  base::RunLoop run_loop;
  bool success = false;
  network_context->trust_token_store()->ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* store) {
        success = !!store;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(success);
}

TEST_F(NetworkContextTest, EnableTrustTokensForFledge) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kFledgePst},
                                       {features::kPrivateStateTokens});

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  EXPECT_TRUE(network_context->trust_token_store());

  base::RunLoop run_loop;
  bool success = false;
  network_context->trust_token_store()->ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* store) {
        success = !!store;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(success);
}

TEST_F(NetworkContextTestWithMockTime, EnableTrustTokensWithStoreOnDisk) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath database_name(FILE_PATH_LITERAL("my_token_store"));
  {
    auto params = CreateNetworkContextParamsForTesting();
    params->file_paths = mojom::NetworkContextFilePaths::New();
    params->file_paths->data_directory = dir.GetPath();
    params->file_paths->trust_token_database_name = database_name;
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(params));

    base::RunLoop run_loop;

    network_context->trust_token_store()->ExecuteOrEnqueue(
        base::BindLambdaForTesting([&](TrustTokenStore* store) {
          DCHECK(store);
          store->AddTokens(*SuitableTrustTokenOrigin::Create(
                               GURL("https://trusttoken.com/")),
                           std::vector<std::string>{"token"}, "issuing key");
          run_loop.Quit();
        }));

    // Allow the store time to initialize asynchronously and execute the
    // operation.
    run_loop.Run();

    // Allow the write time to propagate to disk.
    task_environment_.FastForwardBy(2 * kTrustTokenWriteBufferingWindow);
  }

  // Allow the context's backing store time to be torn down asynchronously.
  task_environment_.RunUntilIdle();
  {
    auto params = CreateNetworkContextParamsForTesting();
    params->file_paths = mojom::NetworkContextFilePaths::New();
    params->file_paths->data_directory = dir.GetPath();
    params->file_paths->trust_token_database_name = database_name;
    std::unique_ptr<NetworkContext> network_context =
        CreateContextWithParams(std::move(params));

    base::RunLoop run_loop;
    absl::optional<int> obtained_num_tokens;
    network_context->trust_token_store()->ExecuteOrEnqueue(
        base::BindLambdaForTesting(
            [&obtained_num_tokens, &run_loop](TrustTokenStore* store) {
              DCHECK(store);
              obtained_num_tokens =
                  store->CountTokens(*SuitableTrustTokenOrigin::Create(
                      GURL("https://trusttoken.com/")));
              run_loop.Quit();
            }));

    // Allow the store time to initialize asynchronously.
    run_loop.Run();

    EXPECT_THAT(obtained_num_tokens, Optional(1));
  }

  // Allow the context's backing store time to be destroyed asynchronously.
  task_environment_.RunUntilIdle();
}

TEST_F(NetworkContextTest, DisableTrustTokens) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow the store time to initialize asynchronously.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(network_context->trust_token_store());
}

class NetworkContextExpectBadMessageTest : public NetworkContextTest {
 public:
  NetworkContextExpectBadMessageTest() {
    mojo::SetDefaultProcessErrorHandler(
        base::BindLambdaForTesting([&](const std::string&) {
          EXPECT_FALSE(got_bad_message_);
          got_bad_message_ = true;
        }));
  }
  ~NetworkContextExpectBadMessageTest() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

 protected:
  void AssertBadMessage() { EXPECT_TRUE(got_bad_message_); }

  bool got_bad_message_ = false;
};

TEST_F(NetworkContextExpectBadMessageTest,
       FailsTrustTokenBearingRequestWhenTrustTokensIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow the store time to initialize asynchronously.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(network_context->trust_token_store());

  ResourceRequest my_request;
  my_request.request_initiator =
      url::Origin::Create(GURL("https://initiator.com"));
  my_request.trust_token_params =
      OptionalTrustTokenParams(mojom::TrustTokenParams::New());

  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(my_request, network_context.get());

  AssertBadMessage();
}

TEST_F(NetworkContextExpectBadMessageTest,
       FailsTrustTokenRedemptionWhenForbidden) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow |network_context|'s Trust Tokens store time to initialize
  // asynchronously, if necessary.
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(network_context->trust_token_store());

  ResourceRequest my_request;
  my_request.trust_token_params =
      OptionalTrustTokenParams(mojom::TrustTokenParams::New());
  my_request.trust_token_params->version =
      mojom::TrustTokenMajorVersion::kPrivateStateTokenV1;
  my_request.trust_token_params->operation =
      mojom::TrustTokenOperationType::kRedemption;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->trust_token_redemption_policy =
      mojom::TrustTokenOperationPolicyVerdict::kForbid;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(my_request, network_context.get(), mojom::kURLLoadOptionNone,
                   mojom::kBrowserProcessId, std::move(factory_params));

  AssertBadMessage();
}

TEST_F(NetworkContextExpectBadMessageTest,
       FailsTrustTokenSigningWhenForbidden) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow |network_context|'s Trust Tokens store time to initialize
  // asynchronously, if necessary.
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(network_context->trust_token_store());

  ResourceRequest my_request;
  my_request.trust_token_params =
      OptionalTrustTokenParams(mojom::TrustTokenParams::New());
  my_request.trust_token_params->operation =
      mojom::TrustTokenOperationType::kSigning;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->trust_token_redemption_policy =
      mojom::TrustTokenOperationPolicyVerdict::kForbid;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(my_request, network_context.get(), mojom::kURLLoadOptionNone,
                   mojom::kBrowserProcessId, std::move(factory_params));

  AssertBadMessage();
}

TEST_F(NetworkContextExpectBadMessageTest,
       FailsTrustTokenIssuanceWhenForbidden) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow |network_context|'s Trust Tokens store time to initialize
  // asynchronously, if necessary.
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(network_context->trust_token_store());

  ResourceRequest my_request;
  my_request.trust_token_params =
      OptionalTrustTokenParams(mojom::TrustTokenParams::New());
  my_request.trust_token_params->version =
      mojom::TrustTokenMajorVersion::kPrivateStateTokenV1;
  my_request.trust_token_params->operation =
      mojom::TrustTokenOperationType::kIssuance;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->trust_token_issuance_policy =
      mojom::TrustTokenOperationPolicyVerdict::kForbid;
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(my_request, network_context.get(), mojom::kURLLoadOptionNone,
                   mojom::kBrowserProcessId, std::move(factory_params));

  AssertBadMessage();
}

TEST_F(NetworkContextTest,
       AttemptsTrustTokenBearingRequestWhenTrustTokensIsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow the store time to initialize asynchronously.
  task_environment_.RunUntilIdle();

  ResourceRequest my_request;
  my_request.trust_token_params =
      OptionalTrustTokenParams(mojom::TrustTokenParams::New());

  // Since the request doesn't have a destination URL suitable for use as a
  // Trust Tokens issuer, it should fail.
  std::unique_ptr<TestURLLoaderClient> client = FetchRequest(
      my_request, network_context.get(), mojom::kURLLoadOptionNone,
      mojom::kBrowserProcessId, mojom::URLLoaderFactoryParams::New());
  EXPECT_EQ(client->completion_status().error_code,
            net::ERR_TRUST_TOKEN_OPERATION_FAILED);
  EXPECT_EQ(client->completion_status().trust_token_operation_status,
            mojom::TrustTokenOperationStatus::kInvalidArgument);
}

TEST_F(NetworkContextTest,
       RejectsTrustTokenBearingRequestWhenTrustTokensAreBlocked) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow the store time to initialize asynchronously.
  base::RunLoop run_loop;
  network_context->trust_token_store()->ExecuteOrEnqueue(
      base::BindLambdaForTesting(
          [&run_loop](TrustTokenStore* unused) { run_loop.Quit(); }));
  run_loop.Run();

  network_context->SetBlockTrustTokens(true);

  ResourceRequest my_request;
  my_request.trust_token_params =
      OptionalTrustTokenParams(mojom::TrustTokenParams::New());

  std::unique_ptr<TestURLLoaderClient> client = FetchRequest(
      my_request, network_context.get(), mojom::kURLLoadOptionNone,
      mojom::kBrowserProcessId, mojom::URLLoaderFactoryParams::New());
  EXPECT_EQ(client->completion_status().error_code,
            net::ERR_TRUST_TOKEN_OPERATION_FAILED);
  EXPECT_EQ(client->completion_status().trust_token_operation_status,
            mojom::TrustTokenOperationStatus::kUnauthorized);
}

TEST_F(NetworkContextTest,
       RejectsTrustTokenBearingRequestWhenStorageIsBlocked) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow the store time to initialize asynchronously.
  base::RunLoop run_loop;
  network_context->trust_token_store()->ExecuteOrEnqueue(
      base::BindLambdaForTesting(
          [&run_loop](TrustTokenStore* unused) { run_loop.Quit(); }));
  run_loop.Run();

  network_context->SetBlockTrustTokens(false);
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK, network_context.get());

  ResourceRequest my_request;
  my_request.trust_token_params =
      OptionalTrustTokenParams(mojom::TrustTokenParams::New());

  std::unique_ptr<TestURLLoaderClient> client = FetchRequest(
      my_request, network_context.get(), mojom::kURLLoadOptionNone,
      mojom::kBrowserProcessId, mojom::URLLoaderFactoryParams::New());
  EXPECT_EQ(client->completion_status().error_code,
            net::ERR_TRUST_TOKEN_OPERATION_FAILED);
  EXPECT_EQ(client->completion_status().trust_token_operation_status,
            mojom::TrustTokenOperationStatus::kUnauthorized);
}

TEST_F(NetworkContextTest, NoAvailableTrustTokensWhenTrustTokensAreDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow the store time to initialize asynchronously.
  base::RunLoop run_loop;
  absl::optional<std::vector<mojom::StoredTrustTokensForIssuerPtr>>
      trust_tokens;
  network_context->GetStoredTrustTokenCounts(base::BindLambdaForTesting(
      [&trust_tokens,
       &run_loop](std::vector<mojom::StoredTrustTokensForIssuerPtr> tokens) {
        trust_tokens = std::move(tokens);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(trust_tokens.has_value());
  EXPECT_TRUE(trust_tokens->empty());
}

TEST_F(NetworkContextTest, GetStoredTrustTokens) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;

  // Query Trust Tokens before adding the mock token.
  absl::optional<std::vector<mojom::StoredTrustTokensForIssuerPtr>>
      trust_tokens_before_adding;
  network_context->GetStoredTrustTokenCounts(base::BindLambdaForTesting(
      [&](std::vector<mojom::StoredTrustTokensForIssuerPtr> tokens) {
        trust_tokens_before_adding = std::move(tokens);
      }));

  // Add a mock token.
  network_context->trust_token_store()->ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* store) {
        DCHECK(store);
        store->AddTokens(
            *SuitableTrustTokenOrigin::Create(GURL("https://trusttoken.com")),
            std::vector<std::string>{"token"}, "issuing key");
      }));

  // Query Trust Tokens after adding the mock token.
  absl::optional<std::vector<mojom::StoredTrustTokensForIssuerPtr>>
      trust_tokens_after_adding;
  network_context->GetStoredTrustTokenCounts(base::BindLambdaForTesting(
      [&](std::vector<mojom::StoredTrustTokensForIssuerPtr> tokens) {
        trust_tokens_after_adding = std::move(tokens);
        run_loop.Quit();
      }));

  // Allow the store time to initialize asynchronously and execute the
  // operations.
  run_loop.Run();

  ASSERT_TRUE(trust_tokens_before_adding.has_value());
  EXPECT_EQ(trust_tokens_before_adding->size(), 0ul);

  ASSERT_TRUE(trust_tokens_after_adding.has_value());
  ASSERT_EQ(trust_tokens_after_adding->size(), 1ul);
  EXPECT_EQ(trust_tokens_after_adding.value()[0]->issuer.Serialize(),
            "https://trusttoken.com");
}

TEST_F(NetworkContextTest, GetStoredTrustTokensReentrant) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Add a mock token.
  base::RunLoop run_loop;
  network_context->trust_token_store()->ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* store) {
        DCHECK(store);
        store->AddTokens(
            *SuitableTrustTokenOrigin::Create(GURL("https://trusttoken.com")),
            std::vector<std::string>{"token"}, "issuing key");
      }));

  absl::optional<std::vector<mojom::StoredTrustTokensForIssuerPtr>>
      trust_tokens;
  absl::optional<std::vector<mojom::StoredTrustTokensForIssuerPtr>>
      reentrant_trust_tokens;
  network_context->GetStoredTrustTokenCounts(base::BindLambdaForTesting(
      [&](std::vector<mojom::StoredTrustTokensForIssuerPtr> tokens) {
        network_context->GetStoredTrustTokenCounts(base::BindLambdaForTesting(
            [&](std::vector<mojom::StoredTrustTokensForIssuerPtr> tokens) {
              reentrant_trust_tokens = std::move(tokens);
              run_loop.Quit();
            }));
        trust_tokens = std::move(tokens);
      }));

  // Allow the store time to initialize asynchronously and execute the
  // operations.
  run_loop.Run();

  ASSERT_TRUE(trust_tokens.has_value());
  ASSERT_TRUE(reentrant_trust_tokens.has_value());
  EXPECT_EQ(trust_tokens->size(), reentrant_trust_tokens->size());
  EXPECT_EQ(trust_tokens.value()[0]->issuer,
            reentrant_trust_tokens.value()[0]->issuer);
  EXPECT_EQ(trust_tokens.value()[0]->count,
            reentrant_trust_tokens.value()[0]->count);
}

TEST_F(NetworkContextTest,
       DeleteStoredTrustTokensReportsErrorWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow the store time to initialize asynchronously.
  base::RunLoop run_loop;
  absl::optional<mojom::DeleteStoredTrustTokensStatus> actual_status;
  network_context->DeleteStoredTrustTokens(
      url::Origin::Create(GURL("https://example.com")),
      base::BindLambdaForTesting(
          [&](mojom::DeleteStoredTrustTokensStatus status) {
            actual_status = status;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_THAT(
      actual_status,
      Optional(mojom::DeleteStoredTrustTokensStatus::kFailureFeatureDisabled));
}

TEST_F(NetworkContextTest,
       DeleteStoredTrustTokensReportsErrorWithInvalidOrigin) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  // Allow the store time to initialize asynchronously.
  base::RunLoop run_loop;
  absl::optional<mojom::DeleteStoredTrustTokensStatus> actual_status;
  network_context->DeleteStoredTrustTokens(
      url::Origin::Create(GURL("ws://example.com")),
      base::BindLambdaForTesting(
          [&](mojom::DeleteStoredTrustTokensStatus status) {
            actual_status = status;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_THAT(
      actual_status,
      Optional(mojom::DeleteStoredTrustTokensStatus::kFailureInvalidOrigin));
}

TEST_F(NetworkContextTest, DeleteStoredTrustTokens) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;

  const SuitableTrustTokenOrigin issuer_origin_to_delete =
      *SuitableTrustTokenOrigin::Create(GURL("https://trusttoken-delete.com"));
  const SuitableTrustTokenOrigin issuer_origin_to_keep =
      *SuitableTrustTokenOrigin::Create(GURL("https://trusttoken-keep.com"));

  // Add two mock tokens from different issuers.
  network_context->trust_token_store()->ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* store) {
        DCHECK(store);
        store->AddTokens(issuer_origin_to_delete,
                         std::vector<std::string>{"token"}, "issuing key");
        store->AddTokens(issuer_origin_to_keep,
                         std::vector<std::string>{"token"}, "issuing key");

        ASSERT_EQ(store->GetStoredTrustTokenCounts().size(), 2ul);
      }));

  // Delete all Trust Tokens for one of the issuers.
  absl::optional<mojom::DeleteStoredTrustTokensStatus> delete_status;
  network_context->DeleteStoredTrustTokens(
      issuer_origin_to_delete.origin(),
      base::BindLambdaForTesting(
          [&](mojom::DeleteStoredTrustTokensStatus status) {
            delete_status = status;
          }));

  // Query Trust Tokens after deleting one of the mock token.
  absl::optional<base::flat_map<SuitableTrustTokenOrigin, int>> trust_tokens;
  network_context->trust_token_store()->ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* store) {
        trust_tokens = store->GetStoredTrustTokenCounts();
        run_loop.Quit();
      }));

  // Allow the store time to initialize asynchronously and execute the
  // operations.
  run_loop.Run();

  EXPECT_THAT(
      delete_status,
      Optional(mojom::DeleteStoredTrustTokensStatus::kSuccessTokensDeleted));

  ASSERT_TRUE(trust_tokens->contains(issuer_origin_to_keep));
  EXPECT_EQ(trust_tokens->at(issuer_origin_to_keep), 1);
}

TEST_F(NetworkContextTest, DeleteStoredTrustTokensReentrant) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPrivateStateTokens);

  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());

  base::RunLoop run_loop;

  const SuitableTrustTokenOrigin issuer_origin_foo =
      *SuitableTrustTokenOrigin::Create(GURL("https://trusttoken-foo.com"));
  const SuitableTrustTokenOrigin issuer_origin_bar =
      *SuitableTrustTokenOrigin::Create(GURL("https://trusttoken-bar.com"));

  // Add two mock tokens from different issuers.
  network_context->trust_token_store()->ExecuteOrEnqueue(
      base::BindLambdaForTesting([&](TrustTokenStore* store) {
        DCHECK(store);
        store->AddTokens(issuer_origin_foo, std::vector<std::string>{"token"},
                         "issuing key");
        store->AddTokens(issuer_origin_bar, std::vector<std::string>{"token"},
                         "issuing key");

        ASSERT_EQ(store->GetStoredTrustTokenCounts().size(), 2ul);
      }));

  // Delete all Trust Tokens for both issuers simultaneously.
  absl::optional<mojom::DeleteStoredTrustTokensStatus> delete_status_foo;
  absl::optional<mojom::DeleteStoredTrustTokensStatus> delete_status_bar;
  network_context->DeleteStoredTrustTokens(
      issuer_origin_foo.origin(),
      base::BindLambdaForTesting(
          [&](mojom::DeleteStoredTrustTokensStatus status) {
            delete_status_foo = status;
            network_context->DeleteStoredTrustTokens(
                issuer_origin_bar,
                base::BindLambdaForTesting(
                    [&](mojom::DeleteStoredTrustTokensStatus status) {
                      delete_status_bar = status;
                      run_loop.Quit();
                    }));
          }));

  // Allow the store time to initialize asynchronously and execute the
  // operations.
  run_loop.Run();

  EXPECT_THAT(
      delete_status_foo,
      Optional(mojom::DeleteStoredTrustTokensStatus::kSuccessTokensDeleted));

  EXPECT_THAT(
      delete_status_bar,
      Optional(mojom::DeleteStoredTrustTokensStatus::kSuccessTokensDeleted));
}

TEST_F(NetworkContextTest, HttpAuthUrlFilter) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  const GURL kGoogle("https://www.google.com");
  const GURL kGoogleSubdomain("https://subdomain.google.com");
  const GURL kBlocked("https://www.blocked.com");
  auto is_url_allowed_to_use_auth_schemes =
      [&network_context](const GURL& url) {
        return network_context->GetHttpAuthPreferences()
            ->IsAllowedToUseAllHttpAuthSchemes(url::SchemeHostPort(url));
      };

  network::mojom::HttpAuthDynamicParamsPtr auth_dynamic_params =
      network::mojom::HttpAuthDynamicParams::New();
  EXPECT_TRUE(is_url_allowed_to_use_auth_schemes(kGoogle));
  EXPECT_TRUE(is_url_allowed_to_use_auth_schemes(kGoogleSubdomain));
  EXPECT_TRUE(is_url_allowed_to_use_auth_schemes(kBlocked));

  auth_dynamic_params->patterns_allowed_to_use_all_schemes =
      std::vector<std::string>{"subdomain.google.com"};
  network_context->OnHttpAuthDynamicParamsChanged(auth_dynamic_params.get());
  EXPECT_FALSE(is_url_allowed_to_use_auth_schemes(kGoogle));
  EXPECT_TRUE(is_url_allowed_to_use_auth_schemes(kGoogleSubdomain));
  EXPECT_FALSE(is_url_allowed_to_use_auth_schemes(
      GURL("https://subdomain.blocked.com")));

  auth_dynamic_params->patterns_allowed_to_use_all_schemes =
      std::vector<std::string>{};
  network_context->OnHttpAuthDynamicParamsChanged(auth_dynamic_params.get());
  EXPECT_FALSE(is_url_allowed_to_use_auth_schemes(kGoogle));
  EXPECT_FALSE(is_url_allowed_to_use_auth_schemes(kGoogleSubdomain));
  EXPECT_FALSE(is_url_allowed_to_use_auth_schemes(kBlocked));

  auth_dynamic_params->patterns_allowed_to_use_all_schemes =
      std::vector<std::string>{"google.com"};
  network_context->OnHttpAuthDynamicParamsChanged(auth_dynamic_params.get());
  EXPECT_TRUE(is_url_allowed_to_use_auth_schemes(kGoogle));
  EXPECT_TRUE(is_url_allowed_to_use_auth_schemes(kGoogleSubdomain));
  EXPECT_FALSE(is_url_allowed_to_use_auth_schemes(kBlocked));

  auth_dynamic_params->patterns_allowed_to_use_all_schemes =
      std::vector<std::string>{"https://google.com/path"};
  network_context->OnHttpAuthDynamicParamsChanged(auth_dynamic_params.get());
  EXPECT_FALSE(is_url_allowed_to_use_auth_schemes(kGoogle));
  EXPECT_FALSE(is_url_allowed_to_use_auth_schemes(kGoogleSubdomain));
  EXPECT_FALSE(is_url_allowed_to_use_auth_schemes(kBlocked));

  auth_dynamic_params->patterns_allowed_to_use_all_schemes =
      std::vector<std::string>{"*"};
  network_context->OnHttpAuthDynamicParamsChanged(auth_dynamic_params.get());
  EXPECT_TRUE(is_url_allowed_to_use_auth_schemes(kGoogle));
  EXPECT_TRUE(is_url_allowed_to_use_auth_schemes(kGoogleSubdomain));
  EXPECT_TRUE(is_url_allowed_to_use_auth_schemes(kBlocked));
}

TEST_F(NetworkContextExpectBadMessageTest, DataUrl) {
  std::unique_ptr<NetworkContext> network_context =
      CreateContextWithParams(CreateNetworkContextParamsForTesting());
  ResourceRequest request;
  request.url = GURL("data:,foo");
  auto factory_params = mojom::URLLoaderFactoryParams::New();
  std::unique_ptr<TestURLLoaderClient> client =
      FetchRequest(request, network_context.get(), mojom::kURLLoadOptionNone,
                   mojom::kBrowserProcessId, std::move(factory_params));

  AssertBadMessage();
}

class NetworkContextBrowserCookieTest
    : public NetworkContextTest,
      public testing::WithParamInterface</*should_add_browser_cookies=*/bool> {
 public:
  NetworkContextBrowserCookieTest() = default;
  ~NetworkContextBrowserCookieTest() override = default;

  bool AllowCookiesFromBrowser() const { return GetParam(); }

  void StartLoadingURL(
      const GURL& url,
      net::HttpRequestHeaders headers,
      int options = mojom::kURLLoadOptionNone,
      mojom::RequestMode mode = mojom::RequestMode::kNoCors,
      net::HttpRequestHeaders cors_exempt_headers = net::HttpRequestHeaders(),
      bool set_request_body = false) {
    CreateLoaderAndStart(
        InitializeURLLoaderFactory(options),
        BuildResourceRequest(url, std::move(headers), mode,
                             std::move(cors_exempt_headers), set_request_body),
        options);
  }

  ResourceRequest BuildResourceRequest(
      const GURL& url,
      net::HttpRequestHeaders headers,
      mojom::RequestMode mode,
      net::HttpRequestHeaders cors_exempt_headers,
      bool set_request_body = false) {
    ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.request_initiator = url::Origin::Create(test_server()->base_url());
    request.mode = mode;
    request.headers = std::move(headers);
    request.cors_exempt_headers = std::move(cors_exempt_headers);
    request.trusted_params = ResourceRequest::TrustedParams();
    request.trusted_params->allow_cookies_from_browser =
        AllowCookiesFromBrowser();
    if (set_request_body) {
      request.request_body = new network::ResourceRequestBody();
    }
    return request;
  }

  mojo::Remote<mojom::URLLoaderFactory> InitializeURLLoaderFactory(
      int options) {
    mojo::Remote<mojom::URLLoaderFactory> loader_factory;
    mojom::URLLoaderFactoryParamsPtr params =
        mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    params->is_trusted = true;
    if (options & mojom::kURLLoadOptionUseHeaderClient) {
      header_client_receiver_ =
          params->header_client.InitWithNewPipeAndPassReceiver();
    }
    network_context_->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));
    return loader_factory;
  }

  void CreateLoaderAndStart(
      mojo::Remote<mojom::URLLoaderFactory> loader_factory,
      ResourceRequest request,
      int options) {
    url_loader_client_ = std::make_unique<TestURLLoaderClient>();
    loader_.reset();
    loader_factory->CreateLoaderAndStart(
        loader_.BindNewPipeAndPassReceiver(), 1, options, request,
        url_loader_client_->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  net::HttpRequestHeaders GenerateTestRequestHeaders() {
    net::HttpRequestHeaders headers;
    headers.SetHeader(net::HttpRequestHeaders::kCookie,
                      base::JoinString({kCookie2Updated, kCookie3}, "; "));
    headers.SetHeader(kHeader1Name, kHeader1Value);
    headers.SetHeader(kHeader2Name, kHeader2Value);
    return headers;
  }

  net::HttpRequestHeaders GenerateTestCorsExemptRequestHeaders() {
    net::HttpRequestHeaders headers;
    headers.SetHeader(kCorsHeaderName, kCorsHeaderValue);
    headers.SetHeader(net::HttpRequestHeaders::kCookie, kCorsCookie);
    return headers;
  }

  void ValidateRequestHeaderValue(const std::string& name,
                                  const std::string& expected_value) {
    SCOPED_TRACE(name);
    // EmbeddedTestServer callbacks run on another thread, so protect this with
    // a lock.
    base::AutoLock lock(server_headers_lock_);
    ASSERT_NE(recently_observed_headers_.end(),
              recently_observed_headers_.find(name));
    EXPECT_EQ(expected_value, recently_observed_headers_.find(name)->second);
  }

  void ValidateRequestHeaderIsUnset(const std::string& name) {
    SCOPED_TRACE(name);
    // EmbeddedTestServer callbacks run on another thread, so protect this with
    // a lock.
    base::AutoLock lock(server_headers_lock_);
    ASSERT_EQ(recently_observed_headers_.end(),
              recently_observed_headers_.find(name));
  }

  net::EmbeddedTestServer* test_server() { return test_server_.get(); }
  TestURLLoaderClient* url_loader_client() { return url_loader_client_.get(); }

 protected:
  void SetUp() override {
    // Set up HTTPS server.
    test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    test_server_->RegisterRequestMonitor(base::BindRepeating(
        &NetworkContextBrowserCookieTest::SaveRequestHeaders,
        base::Unretained(this)));
    test_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("services/test/data")));
    ASSERT_TRUE(test_server_->Start());

    // Initialize `NetworkContext` and bind the cookie manager.
    auto context_params = CreateNetworkContextParamsForTesting();
    context_params->cors_exempt_header_list.push_back(
        net::HttpRequestHeaders::kCookie);
    context_params->cors_exempt_header_list.push_back(kCorsHeaderName);
    network_context_ = CreateContextWithParams(std::move(context_params));
    network_context_->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());

    // Add cookies to the cookie store.
    EXPECT_TRUE(SetCookieHelper(network_context_.get(),
                                test_server()->GetURL("/echo"), kCookie1Name,
                                kCookie1Value));
    EXPECT_TRUE(SetCookieHelper(network_context_.get(),
                                test_server()->GetURL("/echo"), kCookie2Name,
                                kCookie2Value));
  }

  void SaveRequestHeaders(const net::test_server::HttpRequest& request) {
    // EmbeddedTestServer callbacks run on another thread, so protect this with
    // a lock.
    base::AutoLock lock(server_headers_lock_);
    recently_observed_headers_ = request.headers;
  }

  // Header constants.
  static constexpr char kCookie1Name[] = "A";
  static constexpr char kCookie1Value[] = "value-1";
  const std::string kCookie1 =
      base::JoinString({kCookie1Name, kCookie1Value}, "=");

  static constexpr char kCookie2Name[] = "B";
  static constexpr char kCookie2Value[] = "value-2";
  const std::string kCookie2 =
      base::JoinString({kCookie2Name, kCookie2Value}, "=");

  // Used to test whether the value of an existing cookie (`kCookie2Name`) is
  // overridden by this updated value when set via `ResourceRequest::headers`.
  static constexpr char kCookie2ValueUpdated[] = "new-value-2";
  const std::string kCookie2Updated =
      base::JoinString({kCookie2Name, kCookie2ValueUpdated}, "=");

  static constexpr char kCookie3Name[] = "C";
  static constexpr char kCookie3Value[] = "value-3";
  const std::string kCookie3 =
      base::JoinString({kCookie3Name, kCookie3Value}, "=");

  static constexpr char kCorsCookieName[] = "X-A";
  static constexpr char kCorsCookieValue[] = "cors-value";
  const std::string kCorsCookie =
      base::JoinString({kCorsCookieName, kCorsCookieValue}, "=");

  static constexpr char kHeader1Name[] = "header-1";
  static constexpr char kHeader1Value[] = "header-value-1";

  static constexpr char kHeader2Name[] = "header-2";
  static constexpr char kHeader2Value[] = "header-value-2";

  static constexpr char kCorsHeaderName[] = "cors-header";
  static constexpr char kCorsHeaderValue[] = "cors-header-value";

  std::unique_ptr<net::EmbeddedTestServer> test_server_;
  std::unique_ptr<TestURLLoaderClient> url_loader_client_;
  std::unique_ptr<NetworkContext> network_context_;
  mojo::Remote<mojom::URLLoader> loader_;
  mojo::Remote<mojom::CookieManager> cookie_manager_;
  mojo::PendingReceiver<mojom::TrustedURLLoaderHeaderClient>
      header_client_receiver_;
  base::Lock server_headers_lock_;
  net::test_server::HttpRequest::HeaderMap recently_observed_headers_
      GUARDED_BY(server_headers_lock_);
};

TEST_P(NetworkContextBrowserCookieTest, Request) {
  StartLoadingURL(test_server()->GetURL("/echo"), GenerateTestRequestHeaders());
  url_loader_client()->RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // Confirm that the processed request contains the expected headers.
  if (AllowCookiesFromBrowser()) {
    // Only the non-existing new cookie is added.
    ValidateRequestHeaderValue(
        net::HttpRequestHeaders::kCookie,
        base::JoinString({kCookie1, kCookie2, kCookie3}, "; "));
  } else {
    ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                               base::JoinString({kCookie1, kCookie2}, "; "));
  }
  ValidateRequestHeaderValue(kHeader1Name, kHeader1Value);
  ValidateRequestHeaderValue(kHeader2Name, kHeader2Value);
}

TEST_P(NetworkContextBrowserCookieTest, RequestWithBody) {
  StartLoadingURL(test_server()->GetURL("/echo"), GenerateTestRequestHeaders(),
                  mojom::kURLLoadOptionNone, mojom::RequestMode::kNoCors,
                  net::HttpRequestHeaders(),
                  /*set_request_body=*/true);
  url_loader_client()->RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // Confirm that the processed request contains the expected headers.
  if (AllowCookiesFromBrowser()) {
    // Only the non-existing new cookie is added.
    ValidateRequestHeaderValue(
        net::HttpRequestHeaders::kCookie,
        base::JoinString({kCookie1, kCookie2, kCookie3}, "; "));
  } else {
    ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                               base::JoinString({kCookie1, kCookie2}, "; "));
  }
  ValidateRequestHeaderValue(kHeader1Name, kHeader1Value);
  ValidateRequestHeaderValue(kHeader2Name, kHeader2Value);
}

// Request cookies should still be stored even when a header client is present.
TEST_P(NetworkContextBrowserCookieTest, HeaderClient) {
  ResourceRequest request = BuildResourceRequest(
      test_server()->GetURL("/echo"), GenerateTestRequestHeaders(),
      mojom::RequestMode::kNoCors, net::HttpRequestHeaders());
  int options = mojom::kURLLoadOptionUseHeaderClient;
  mojo::Remote<mojom::URLLoaderFactory> factory =
      InitializeURLLoaderFactory(options);
  // Initialize the test header client before starting the request to prevent a
  // race condition.
  TestURLLoaderHeaderClient header_client(std::move(header_client_receiver_));
  CreateLoaderAndStart(std::move(factory), std::move(request), options);

  url_loader_client()->RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  if (AllowCookiesFromBrowser()) {
    // Only the non-existing new cookie is added.
    ValidateRequestHeaderValue(
        net::HttpRequestHeaders::kCookie,
        base::JoinString({kCookie1, kCookie2, kCookie3}, "; "));
  } else {
    ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                               base::JoinString({kCookie1, kCookie2}, "; "));
  }
  ValidateRequestHeaderValue(kHeader1Name, kHeader1Value);
  ValidateRequestHeaderValue(kHeader2Name, kHeader2Value);
}

// Test that browser cookies are added to the request after a redirect.
TEST_P(NetworkContextBrowserCookieTest, Redirect) {
  // Don't set any headers initially.
  StartLoadingURL(test_server()->GetURL("/redirect307-to-echo"),
                  net::HttpRequestHeaders());
  url_loader_client()->RunUntilRedirectReceived();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // The cookie store cookies should be set.
  ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                             base::JoinString({kCookie1, kCookie2}, "; "));
  ValidateRequestHeaderIsUnset(kHeader1Name);
  ValidateRequestHeaderIsUnset(kHeader2Name);

  // Redirect with cookies added to the modified headers.
  loader_->FollowRedirect({}, {GenerateTestRequestHeaders()}, {},
                          absl::nullopt);
  url_loader_client()->RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // Confirm that the redirected request contains the expected headers.
  if (AllowCookiesFromBrowser()) {
    // Only the non-existing new cookie is added.
    ValidateRequestHeaderValue(
        net::HttpRequestHeaders::kCookie,
        base::JoinString({kCookie1, kCookie2, kCookie3}, "; "));
  } else {
    ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                               base::JoinString({kCookie1, kCookie2}, "; "));
  }
  ValidateRequestHeaderValue(kHeader1Name, kHeader1Value);
  ValidateRequestHeaderValue(kHeader2Name, kHeader2Value);
}

// Test that browser cookies are cleared after a redirect.
TEST_P(NetworkContextBrowserCookieTest, RedirectClear) {
  StartLoadingURL(test_server()->GetURL("/redirect307-to-echo"),
                  GenerateTestRequestHeaders());
  url_loader_client()->RunUntilRedirectReceived();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // Confirm that the processed request contains the expected headers.
  if (AllowCookiesFromBrowser()) {
    // Only the non-existing new cookie is added.
    ValidateRequestHeaderValue(
        net::HttpRequestHeaders::kCookie,
        base::JoinString({kCookie1, kCookie2, kCookie3}, "; "));
  } else {
    ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                               base::JoinString({kCookie1, kCookie2}, "; "));
  }
  ValidateRequestHeaderValue(kHeader1Name, kHeader1Value);
  ValidateRequestHeaderValue(kHeader2Name, kHeader2Value);

  // Redirect with some header changes.
  net::HttpRequestHeaders modified_headers;
  const char kHeader1ValueUpdated[] = "new-header-value-1";
  modified_headers.SetHeader(kHeader1Name, kHeader1ValueUpdated);
  loader_->FollowRedirect({kHeader2Name}, {modified_headers}, {},
                          absl::nullopt);
  url_loader_client()->RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // Because the browser cookies are cleared on redirect, only the cookie store
  // cookies should be set regardless of whether the feature is enabled.
  ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                             base::JoinString({kCookie1, kCookie2}, "; "));
  ValidateRequestHeaderValue(kHeader1Name, kHeader1ValueUpdated);
  ValidateRequestHeaderIsUnset(kHeader2Name);
}

// Test that browser cookies are added to the request after a CORS redirect.
TEST_P(NetworkContextBrowserCookieTest, CorsRedirect) {
  // Don't set any headers initially.
  StartLoadingURL(test_server()->GetURL("/redirect307-to-echo"),
                  net::HttpRequestHeaders(), mojom::kURLLoadOptionNone,
                  mojom::RequestMode::kCors);
  url_loader_client()->RunUntilRedirectReceived();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // The cookie store cookies should be set.
  ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                             base::JoinString({kCookie1, kCookie2}, "; "));
  ValidateRequestHeaderIsUnset(kHeader1Name);
  ValidateRequestHeaderIsUnset(kHeader2Name);
  ValidateRequestHeaderIsUnset(kCorsHeaderName);

  // Redirect with cookies added to the modified headers.
  loader_->FollowRedirect({kHeader1Name}, {GenerateTestRequestHeaders()},
                          {GenerateTestCorsExemptRequestHeaders()},
                          absl::nullopt);
  url_loader_client()->RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // Confirm that the processed request contains the expected headers.
  if (AllowCookiesFromBrowser()) {
    // Only the non-existing new CORS cookie is added.
    ValidateRequestHeaderValue(
        net::HttpRequestHeaders::kCookie,
        base::JoinString({kCookie1, kCookie2, kCorsCookie}, "; "));
  } else {
    ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                               base::JoinString({kCookie1, kCookie2}, "; "));
  }
  ValidateRequestHeaderValue(kHeader1Name, kHeader1Value);
  ValidateRequestHeaderValue(kHeader2Name, kHeader2Value);
  ValidateRequestHeaderValue(kCorsHeaderName, kCorsHeaderValue);
}

// Test that browser cookies are cleared after a CORS redirect.
TEST_P(NetworkContextBrowserCookieTest, CorsRedirectClear) {
  StartLoadingURL(test_server()->GetURL("/redirect307-to-echo"),
                  GenerateTestRequestHeaders(), mojom::kURLLoadOptionNone,
                  mojom::RequestMode::kCors,
                  GenerateTestCorsExemptRequestHeaders());
  url_loader_client()->RunUntilRedirectReceived();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // Confirm that the processed request contains the expected headers.
  if (AllowCookiesFromBrowser()) {
    // Only the non-existing new CORS cookie is added.
    ValidateRequestHeaderValue(
        net::HttpRequestHeaders::kCookie,
        base::JoinString({kCookie1, kCookie2, kCorsCookie}, "; "));
  } else {
    ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                               base::JoinString({kCookie1, kCookie2}, "; "));
  }
  ValidateRequestHeaderValue(kHeader1Name, kHeader1Value);
  ValidateRequestHeaderValue(kHeader2Name, kHeader2Value);
  ValidateRequestHeaderValue(kCorsHeaderName, kCorsHeaderValue);

  // Redirect with some header changes.
  net::HttpRequestHeaders modified_headers;
  const char kHeader2ValueUpdated[] = "new-header-value-2";
  modified_headers.SetHeader(kHeader2Name, kHeader2ValueUpdated);
  net::HttpRequestHeaders modified_cors_exempt_headers;
  const char kCorsHeaderValueUpdated[] = "new-cors-header-value";
  modified_cors_exempt_headers.SetHeader(kCorsHeaderName,
                                         kCorsHeaderValueUpdated);
  loader_->FollowRedirect({kHeader1Name}, {modified_headers},
                          {modified_cors_exempt_headers}, absl::nullopt);
  url_loader_client()->RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client()->completion_status().error_code);

  // Because the browser cookies are cleared on redirect, only the cookie store
  // cookies should be set regardless of whether the feature is enabled.
  ValidateRequestHeaderValue(net::HttpRequestHeaders::kCookie,
                             base::JoinString({kCookie1, kCookie2}, "; "));
  ValidateRequestHeaderIsUnset(kHeader1Name);
  ValidateRequestHeaderValue(kHeader2Name, kHeader2ValueUpdated);
  ValidateRequestHeaderValue(kCorsHeaderName, kCorsHeaderValueUpdated);
}

INSTANTIATE_TEST_SUITE_P(NetworkContextBrowserCookieTestInstance,
                         NetworkContextBrowserCookieTest,
                         testing::Bool());

}  // namespace

}  // namespace network
