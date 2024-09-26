// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/web_service_worker_fetch_context_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"

namespace blink {

class WebServiceWorkerFetchContextImplTest : public testing::Test {
 public:
  WebServiceWorkerFetchContextImplTest() = default;

  class FakeURLLoaderThrottle : public URLLoaderThrottle {
   public:
    FakeURLLoaderThrottle() = default;
  };

  class FakeURLLoaderThrottleProvider : public URLLoaderThrottleProvider {
    std::unique_ptr<URLLoaderThrottleProvider> Clone() override {
      NOTREACHED();
      return nullptr;
    }

    WebVector<std::unique_ptr<URLLoaderThrottle>> CreateThrottles(
        int render_frame_id,
        const WebURLRequest& request) override {
      WebVector<std::unique_ptr<URLLoaderThrottle>> throttles;
      throttles.emplace_back(std::make_unique<FakeURLLoaderThrottle>());
      return throttles;
    }

    void SetOnline(bool is_online) override { NOTREACHED(); }
  };
};

TEST_F(WebServiceWorkerFetchContextImplTest, SkipThrottling) {
  const KURL kScriptUrl("https://example.com/main.js");
  const KURL kScriptUrlToSkipThrottling("https://example.com/skip.js");
  auto context = WebServiceWorkerFetchContext::Create(
      RendererPreferences(), kScriptUrl,
      /*pending_url_loader_factory=*/nullptr,
      /*pending_script_loader_factory=*/nullptr, kScriptUrlToSkipThrottling,
      std::make_unique<FakeURLLoaderThrottleProvider>(),
      /*websocket_handshake_throttle_provider=*/nullptr, mojo::NullReceiver(),
      mojo::NullReceiver(),
      /*cors_exempt_header_list=*/WebVector<WebString>());

  {
    // Call WillSendRequest() for kScriptURL.
    WebURLRequest request;
    request.SetUrl(kScriptUrl);
    request.SetRequestContext(mojom::RequestContextType::SERVICE_WORKER);
    context->WillSendRequest(request);

    // Throttles should be created by the provider.
    auto* url_request_extra_data = static_cast<WebURLRequestExtraData*>(
        request.GetURLRequestExtraData().get());
    ASSERT_TRUE(url_request_extra_data);
    WebVector<std::unique_ptr<URLLoaderThrottle>> throttles =
        url_request_extra_data->TakeURLLoaderThrottles();
    EXPECT_EQ(1u, throttles.size());
  }
  {
    // Call WillSendRequest() for kScriptURLToSkipThrottling.
    WebURLRequest request;
    request.SetUrl(kScriptUrlToSkipThrottling);
    request.SetRequestContext(mojom::RequestContextType::SERVICE_WORKER);
    context->WillSendRequest(request);

    // Throttles should not be created by the provider.
    auto* url_request_extra_data = static_cast<WebURLRequestExtraData*>(
        request.GetURLRequestExtraData().get());
    ASSERT_TRUE(url_request_extra_data);
    WebVector<std::unique_ptr<URLLoaderThrottle>> throttles =
        url_request_extra_data->TakeURLLoaderThrottles();
    EXPECT_TRUE(throttles.empty());
  }
}

}  // namespace blink
