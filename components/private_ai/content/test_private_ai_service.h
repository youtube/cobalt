// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CONTENT_TEST_PRIVATE_AI_SERVICE_H_
#define COMPONENTS_PRIVATE_AI_CONTENT_TEST_PRIVATE_AI_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory.h"
#include "components/private_ai/phosphor/mock_blind_sign_auth.h"
#include "components/private_ai/private_ai_service.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace network::mojom {
class NetworkContext;
}

namespace signin {
class IdentityManager;
}

namespace private_ai {

class TestBlindSignAuthFactory : public phosphor::BlindSignAuthFactory {
 public:
  TestBlindSignAuthFactory();
  ~TestBlindSignAuthFactory() override;

  std::unique_ptr<quiche::BlindSignAuthInterface> CreateBlindSignAuth(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory) override;

  phosphor::MockBlindSignAuth* mock_bsa() { return bsa_; }

  void ResetBsa() { bsa_ = nullptr; }

 private:
  raw_ptr<phosphor::MockBlindSignAuth> bsa_ = nullptr;
};

class TestPrivateAiService : public PrivateAiService {
 public:
  TestPrivateAiService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::mojom::NetworkContext* network_context,
      const std::string& api_key,
      std::unique_ptr<TestBlindSignAuthFactory> test_bsa_factory);

  ~TestPrivateAiService() override;

  // PrivateAiService override:
  void Shutdown() override;

  phosphor::MockBlindSignAuth* mock_bsa() {
    return test_bsa_factory_->mock_bsa();
  }

 private:
  std::unique_ptr<TestBlindSignAuthFactory> test_bsa_factory_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CONTENT_TEST_PRIVATE_AI_SERVICE_H_
