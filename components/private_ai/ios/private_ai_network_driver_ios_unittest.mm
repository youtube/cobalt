// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/ios/private_ai_network_driver_ios.h"

#include "ios/web/public/test/web_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace private_ai {

using PrivateAiNetworkDriverIOSTest = PlatformTest;

TEST_F(PrivateAiNetworkDriverIOSTest, GetCertVerifierParams) {
  web::WebTaskEnvironment task_environment(
      web::WebTaskEnvironment::MainThreadType::IO);
  PrivateAiNetworkDriverIOS driver;
  auto params = driver.GetCertVerifierParams();
  EXPECT_TRUE(params);
}

TEST_F(PrivateAiNetworkDriverIOSTest, CreateNetworkContext) {
  web::WebTaskEnvironment task_environment(
      web::WebTaskEnvironment::MainThreadType::IO);
  PrivateAiNetworkDriverIOS driver;
  mojo::PendingRemote<network::mojom::NetworkContext> network_context;
  auto params = network::mojom::NetworkContextParams::New();
  params->cert_verifier_params = driver.GetCertVerifierParams();
  driver.CreateNetworkContext(network_context.InitWithNewPipeAndPassReceiver(),
                              std::move(params));
  EXPECT_TRUE(network_context.is_valid());
}

}  // namespace private_ai
