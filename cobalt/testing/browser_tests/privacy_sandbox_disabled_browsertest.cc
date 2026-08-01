// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {

using PrivacySandboxDisabledBrowserTest = content::ContentBrowserTest;

// Verifies that Privacy Sandbox features (Fenced Frames, Private State Tokens /
// Trust Tokens) are disabled in Cobalt builds.
IN_PROC_BROWSER_TEST_F(PrivacySandboxDisabledBrowserTest,
                       VerifyPrivacySandboxApisDisabled) {
  // 1. Verify buildflag is disabled for Cobalt.
  EXPECT_FALSE(BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS));

  // 2. Start test server and navigate to a page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // 3. Verify Fenced Frame APIs are not exposed to JavaScript.
  EXPECT_EQ(false, content::EvalJs(shell()->web_contents(),
                                   "'HTMLFencedFrameElement' in window"));

  // 4. Verify Private State Tokens / Trust Tokens APIs are not exposed.
  EXPECT_EQ(false, content::EvalJs(shell()->web_contents(),
                                   "'hasPrivateToken' in document"));
  EXPECT_EQ(false, content::EvalJs(shell()->web_contents(),
                                   "'hasTrustToken' in document"));
  EXPECT_EQ(false,
            content::EvalJs(shell()->web_contents(),
                            "'privateToken' in HTMLIFrameElement.prototype"));
}

// Verifies that fetch() with privateToken parameters is safely ignored by
// the renderer and loads the resource as a regular network request.
IN_PROC_BROWSER_TEST_F(PrivacySandboxDisabledBrowserTest,
                       FetchWithPrivateTokenParamIgnored) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // Since PrivateStateTokens is disabled, the 'privateToken' dictionary member
  // in RequestInit is ignored and fetch completes normally with 200 OK.
  EXPECT_EQ(200, content::EvalJs(shell()->web_contents(), R"(
    fetch('/title1.html', {
      privateToken: {
        version: 1,
        operation: 'token-request'
      }
    }).then(res => res.status);
  )"));
}

// Verifies that NetworkContext Private State Token / Trust Token Mojo methods
// return empty results without crashing when the feature is disabled.
IN_PROC_BROWSER_TEST_F(PrivacySandboxDisabledBrowserTest,
                       NetworkContextTrustTokenMethodsReturnEmpty) {
  network::mojom::NetworkContext* network_context =
      shell()
          ->web_contents()
          ->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext();
  ASSERT_TRUE(network_context);

  // 1. GetStoredTrustTokenCounts returns empty vector.
  base::test::TestFuture<
      std::vector<network::mojom::StoredTrustTokensForIssuerPtr>>
      counts_future;
  network_context->GetStoredTrustTokenCounts(counts_future.GetCallback());
  EXPECT_TRUE(counts_future.Get().empty());

  // 2. GetPrivateStateTokenRedemptionRecords returns empty map.
  base::test::TestFuture<base::flat_map<
      url::Origin, std::vector<network::mojom::ToplevelRedemptionRecordPtr>>>
      records_future;
  network_context->GetPrivateStateTokenRedemptionRecords(
      records_future.GetCallback());
  EXPECT_TRUE(records_future.Get().empty());

  // 3. ClearTrustTokenData runs callback immediately.
  base::test::TestFuture<void> clear_future;
  network_context->ClearTrustTokenData(nullptr, clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Wait());
}

// Verifies that a ResourceRequest containing trust_token_params is rejected
// with net::ERR_INVALID_ARGUMENT (-4) by the Network Service when Trust Tokens
// are disabled in Cobalt.
IN_PROC_BROWSER_TEST_F(PrivacySandboxDisabledBrowserTest,
                       UrlLoaderWithTrustTokenParamsRejectedWhenDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/title1.html");
  request->trust_token_params = network::OptionalTrustTokenParams(
      network::mojom::TrustTokenParams::New());
  request->trust_token_params->operation =
      network::mojom::TrustTokenOperationType::kRedemption;

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);

  auto* storage_partition = shell()
                                ->web_contents()
                                ->GetBrowserContext()
                                ->GetDefaultStoragePartition();

  base::test::TestFuture<std::unique_ptr<std::string>> future;
  url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      future.GetCallback());

  // Since Trust Tokens are disabled (context->trust_token_store() is null),
  // CorsURLLoaderFactory rejects the request with ERR_INVALID_ARGUMENT.
  EXPECT_FALSE(future.Get());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, url_loader->NetError());
}

}  // namespace cobalt
