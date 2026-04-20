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

#include "cobalt/browser/client_hint_headers/cobalt_trusted_header_client.h"

#include <optional>
#include <tuple>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace browser {

class CobaltTrustedHeaderClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize Mojo Core for this test.
    mojo::core::Init();
    // Setup IPC support thread.
    ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        task_environment_.GetMainThreadTaskRunner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    header_provider_ = CobaltHeaderValueProvider::GetInstance();
    header_provider_->SetHeaderValue("Cobalt-Client-Hint-Header", "Value");
  }

  void TearDown() override {}

  // Use IO thread for TaskEnvironment when dealing with Mojo IPC.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  // Manages the IPC thread required by Mojo Core.
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;

  CobaltHeaderValueProvider* header_provider_ = nullptr;
};

TEST_F(CobaltTrustedHeaderClientTest, OnBeforeSendHeadersAddsHeaders) {
  mojo::Remote<network::mojom::TrustedHeaderClient> remote;

  mojo::MakeSelfOwnedReceiver(std::make_unique<CobaltTrustedHeaderClient>(),
                              remote.BindNewPipeAndPassReceiver());

  net::HttpRequestHeaders initial_headers;
  initial_headers.SetHeader("Existing-Header", "Existing-Value");

  base::test::TestFuture<int32_t, const std::optional<net::HttpRequestHeaders>&>
      future;
  remote->OnBeforeSendHeaders(initial_headers, future.GetCallback());

  ASSERT_TRUE(future.Wait()) << "Callback was not run.";
  EXPECT_EQ(net::OK, std::get<0>(future.Get()));

  const auto& headers_optional = std::get<1>(future.Get());
  ASSERT_TRUE(headers_optional.has_value());
  const auto& modified_headers = headers_optional.value();

  std::optional<std::string> existing_value =
      modified_headers.GetHeader("Existing-Header");
  ASSERT_TRUE(existing_value.has_value());
  EXPECT_EQ("Existing-Value", existing_value.value());

  std::optional<std::string> value =
      modified_headers.GetHeader("Cobalt-Client-Hint-Header");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ("Value", value.value());
}

}  // namespace browser
}  // namespace cobalt
