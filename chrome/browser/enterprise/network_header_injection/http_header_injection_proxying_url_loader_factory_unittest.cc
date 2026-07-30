// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_proxying_url_loader_factory.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/network_header_injection/core/features.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_rule.h"
#include "components/enterprise/network_header_injection/core/network_header_injection_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_custom_headers {

class HttpHeaderInjectionProxyingURLLoaderFactoryTest : public testing::Test {
 public:
  HttpHeaderInjectionProxyingURLLoaderFactoryTest() {
    scoped_feature_list_.InitAndEnableFeature(kHttpHeadersInjection);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that MaybeProxyRequest leaves the builder empty when no rules exist.
TEST_F(HttpHeaderInjectionProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_NoRules) {
  TestingProfile profile;
  network::URLLoaderFactoryBuilder builder;
  HttpHeaderInjectionProxyingURLLoaderFactory::MaybeProxyRequest(&profile,
                                                                 builder);
  EXPECT_EQ(0u, builder.num_interceptors());
}

// Tests that MaybeProxyRequest adds the proxy to the builder when rules exist.
TEST_F(HttpHeaderInjectionProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_WithRules) {
  TestingProfile profile;

  base::ListValue rules = base::ListValue().Append(
      base::DictValue()
          .Set(kKeyPatterns, base::ListValue().Append("example.com"))
          .Set(kKeyHeaders, base::ListValue().Append(
                                base::DictValue()
                                    .Set(kKeyHeaderName, "X-Custom-Header")
                                    .Set(kKeyHeaderValue, "value"))));

  profile.GetPrefs()->SetList(prefs::kHttpHeaderInjection, std::move(rules));

  network::URLLoaderFactoryBuilder builder;
  HttpHeaderInjectionProxyingURLLoaderFactory::MaybeProxyRequest(&profile,
                                                                 builder);
  EXPECT_EQ(1u, builder.num_interceptors());
}

// Tests that if rules exist, the proxy intercepts the request and adds
// the kURLLoadOptionUseHeaderClient flag to instruct header injection.
TEST_F(HttpHeaderInjectionProxyingURLLoaderFactoryTest,
       FlagAddedWhenRulesExist) {
  mojo::Remote<network::mojom::URLLoaderFactory> proxy_factory;

  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;
  test_url_loader_factory_.Clone(
      target_factory_remote.InitWithNewPipeAndPassReceiver());

  // Create the proxy factory, passing a callback that returns true (rules
  // exist).
  base::MakeSelfDeleting<HttpHeaderInjectionProxyingURLLoaderFactory>(
      proxy_factory.BindNewPipeAndPassReceiver(),
      std::move(target_factory_remote),
      base::BindRepeating([]() { return true; }));

  network::ResourceRequest request;
  request.url = GURL("https://example.com");

  mojo::Remote<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
  auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  proxy_factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), 0, 0, request,
      std::move(client_remote), net::MutableNetworkTrafficAnnotationTag());

  test_url_loader_factory_.WaitForRequest(GURL("https://example.com"));

  ASSERT_EQ(1u, test_url_loader_factory_.pending_requests()->size());
  uint32_t options = test_url_loader_factory_.pending_requests()->at(0).options;
  EXPECT_TRUE(options & network::mojom::kURLLoadOptionUseHeaderClient);
}

// Tests that if no rules exist, the proxy acts as a transparent pass-through
// and does not add the kURLLoadOptionUseHeaderClient flag.
TEST_F(HttpHeaderInjectionProxyingURLLoaderFactoryTest,
       FlagNotAddedWhenNoRules) {
  mojo::Remote<network::mojom::URLLoaderFactory> proxy_factory;

  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;
  test_url_loader_factory_.Clone(
      target_factory_remote.InitWithNewPipeAndPassReceiver());

  // Create the proxy factory, passing a callback that returns false (no rules).
  base::MakeSelfDeleting<HttpHeaderInjectionProxyingURLLoaderFactory>(
      proxy_factory.BindNewPipeAndPassReceiver(),
      std::move(target_factory_remote),
      base::BindRepeating([]() { return false; }));

  network::ResourceRequest request;
  request.url = GURL("https://example.com");

  mojo::Remote<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
  auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  proxy_factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), 0, 0, request,
      std::move(client_remote), net::MutableNetworkTrafficAnnotationTag());

  test_url_loader_factory_.WaitForRequest(GURL("https://example.com"));

  ASSERT_EQ(1u, test_url_loader_factory_.pending_requests()->size());
  uint32_t options = test_url_loader_factory_.pending_requests()->at(0).options;
  EXPECT_FALSE(options & network::mojom::kURLLoadOptionUseHeaderClient);
}

}  // namespace enterprise_custom_headers
