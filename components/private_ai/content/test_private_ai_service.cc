// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/content/test_private_ai_service.h"

#include "components/private_ai/content/private_ai_network_driver_content.h"
#include "components/private_ai/content/private_ai_oak_session_driver_content.h"
#include "components/private_ai/features.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace private_ai {

TestBlindSignAuthFactory::TestBlindSignAuthFactory() = default;
TestBlindSignAuthFactory::~TestBlindSignAuthFactory() = default;

std::unique_ptr<quiche::BlindSignAuthInterface>
TestBlindSignAuthFactory::CreateBlindSignAuth(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory) {
  auto bsa = std::make_unique<phosphor::MockBlindSignAuth>();
  bsa_ = bsa.get();
  return bsa;
}

TestPrivateAiService::TestPrivateAiService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::mojom::NetworkContext* network_context,
    const std::string& api_key,
    std::unique_ptr<TestBlindSignAuthFactory> test_bsa_factory)
    : PrivateAiService(
          identity_manager,
          test_bsa_factory.get(),
          std::move(url_loader_factory),
          std::make_unique<PrivateAiNetworkDriverContent>(),
          std::make_unique<PrivateAiOakSessionDriverContent>(),
          network_context,
          kPrivateAiUrl.Get(),
          api_key,
          kPrivateAiProxyServerUrl.Get(),
          base::FeatureList::IsEnabled(kPrivateAiUseTokenAttestation)),
      test_bsa_factory_(std::move(test_bsa_factory)) {}

TestPrivateAiService::~TestPrivateAiService() = default;

void TestPrivateAiService::Shutdown() {
  test_bsa_factory_->ResetBsa();
  PrivateAiService::Shutdown();
}

}  // namespace private_ai
