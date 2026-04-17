// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/client_hint_headers/cobalt_trusted_url_loader_header_client.h"

#include "base/test/task_environment.h"
#include "cobalt/browser/client_hint_headers/cobalt_trusted_header_client.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {

class CobaltTrustedURLLoaderHeaderClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize Mojo Core for this test.
    mojo::core::Init();
    // Mojo requires an IPC thread to function properly.
    ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        task_environment_.GetMainThreadTaskRunner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  }

  void TearDown() override {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
};

TEST_F(CobaltTrustedURLLoaderHeaderClientTest,
       OnLoaderCreatedCreatesAndBindsClient) {
  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CobaltTrustedURLLoaderHeaderClient>(),
      remote.BindNewPipeAndPassReceiver());

  mojo::Remote<network::mojom::TrustedHeaderClient>
      trusted_header_client_remote;
  int32_t request_id = 123;

  remote->OnLoaderCreated(
      request_id, trusted_header_client_remote.BindNewPipeAndPassReceiver());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(trusted_header_client_remote.is_bound());
  EXPECT_TRUE(trusted_header_client_remote.is_connected());

  trusted_header_client_remote.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(CobaltTrustedURLLoaderHeaderClientTest,
       OnLoaderForCorsPreflightCreatedCreatesAndBindsClient) {
  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CobaltTrustedURLLoaderHeaderClient>(),
      remote.BindNewPipeAndPassReceiver());

  mojo::Remote<network::mojom::TrustedHeaderClient>
      trusted_header_client_remote;
  network::ResourceRequest request;

  remote->OnLoaderForCorsPreflightCreated(
      request, trusted_header_client_remote.BindNewPipeAndPassReceiver());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(trusted_header_client_remote.is_bound());
  EXPECT_TRUE(trusted_header_client_remote.is_connected());

  trusted_header_client_remote.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace browser
}  // namespace cobalt
