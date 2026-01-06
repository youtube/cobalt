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

#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/overloaded.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_shell_main_delegate.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/main_function_params.h"
#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/utility/content_utility_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
class VariationsIdsProvider;
}

namespace content {

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using InvokedIn = ContentMainDelegate::InvokedIn;
using VariationsIdsProvider = variations::VariationsIdsProvider;

// Mocks only the cross-platform methods of ContentMainDelegate. Also calls the
// parent implementation of each method, since the test setup may depend on it.
class MockContentMainDelegate : public ContentBrowserTestShellMainDelegate {
 public:
  using Super = ContentBrowserTestShellMainDelegate;

  MOCK_METHOD(std::optional<int>, MockBasicStartupComplete, ());
  std::optional<int> BasicStartupComplete() override {
    std::optional<int> result = MockBasicStartupComplete();
    // Check for early exit code.
    if (result.has_value()) {
      return result;
    }
    return Super::BasicStartupComplete();
  }

  MOCK_METHOD(void, MockPreSandboxStartup, ());
  void PreSandboxStartup() override {
    MockPreSandboxStartup();
    Super::PreSandboxStartup();
  }

  MOCK_METHOD(void, MockSandboxInitialized, (const std::string&));
  void SandboxInitialized(const std::string& process_type) override {
    MockSandboxInitialized(process_type);
    Super::SandboxInitialized(process_type);
  }

  // The return value of RunProcess is platform-dependent and the startup
  // sequence depends heavily on it, so don't allow it to be mocked.
  MOCK_METHOD(void, MockRunProcess, (const std::string&, MainFunctionParams));
  std::variant<int, MainFunctionParams> RunProcess(
      const std::string& process_type,
      MainFunctionParams main_function_params) override {
    // MainFunctionParams is move-only so pass a dummy to the mock.
    MainFunctionParams dummy_main_function_params(
        base::CommandLine::ForCurrentProcess());
    MockRunProcess(process_type, std::move(dummy_main_function_params));
    return Super::RunProcess(process_type, std::move(main_function_params));
  }

  MOCK_METHOD(void, MockProcessExiting, (const std::string&));
  void ProcessExiting(const std::string& process_type) override {
    MockProcessExiting(process_type);
    Super::ProcessExiting(process_type);
  }

  // The return value of ShouldLockSchemeRegistry is dangerous to override so
  // don't allow it to be mocked.
  MOCK_METHOD(void, MockShouldLockSchemeRegistry, ());
  bool ShouldLockSchemeRegistry() override {
    MockShouldLockSchemeRegistry();
    return Super::ShouldLockSchemeRegistry();
  }

  MOCK_METHOD(std::optional<int>, MockPreBrowserMain, ());
  std::optional<int> PreBrowserMain() override {
    std::optional<int> result = MockPreBrowserMain();
    // Check for early exit code.
    if (result.has_value()) {
      return result;
    }
    return Super::PreBrowserMain();
  }

  // No need to call the parent delegate for these methods since they have no
  // side effects.
  MOCK_METHOD(bool, ShouldCreateFeatureList, (InvokedIn), (override));
  MOCK_METHOD(bool, ShouldInitializeMojo, (InvokedIn), (override));

  MOCK_METHOD(VariationsIdsProvider*, MockCreateVariationsIdsProvider, ());
  VariationsIdsProvider* CreateVariationsIdsProvider() override {
    VariationsIdsProvider* result = MockCreateVariationsIdsProvider();
    if (result) {
      return result;
    }
    return Super::CreateVariationsIdsProvider();
  }

  MOCK_METHOD(std::optional<int>, MockPostEarlyInitialization, (InvokedIn));
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override {
    std::optional<int> result = MockPostEarlyInitialization(invoked_in);
    // Check for early exit code.
    if (result.has_value()) {
      return result;
    }
    return Super::PostEarlyInitialization(invoked_in);
  }

  MOCK_METHOD(ContentClient*, MockCreateContentClient, ());
  ContentClient* CreateContentClient() override {
    ContentClient* result = MockCreateContentClient();
    if (result) {
      return result;
    }
    return Super::CreateContentClient();
  }

  MOCK_METHOD(ContentBrowserClient*, MockCreateContentBrowserClient, ());
  ContentBrowserClient* CreateContentBrowserClient() override {
    ContentBrowserClient* result = MockCreateContentBrowserClient();
    if (result) {
      return result;
    }
    return Super::CreateContentBrowserClient();
  }

  MOCK_METHOD(ContentGpuClient*, MockCreateContentGpuClient, ());
  ContentGpuClient* CreateContentGpuClient() override {
    ContentGpuClient* result = MockCreateContentGpuClient();
    if (result) {
      return result;
    }
    return Super::CreateContentGpuClient();
  }

  MOCK_METHOD(ContentRendererClient*, MockCreateContentRendererClient, ());
  ContentRendererClient* CreateContentRendererClient() override {
    ContentRendererClient* result = MockCreateContentRendererClient();
    if (result) {
      return result;
    }
    return Super::CreateContentRendererClient();
  }

  MOCK_METHOD(ContentUtilityClient*, MockCreateContentUtilityClient, ());
  ContentUtilityClient* CreateContentUtilityClient() override {
    ContentUtilityClient* result = MockCreateContentUtilityClient();
    if (result) {
      return result;
    }
    return Super::CreateContentUtilityClient();
  }
};

MATCHER_P(InvokedInMatcher, process_type, "") {
  // `arg` is an std::variant. Return true if the type held by the variant is
  // correct for `process_type` (empty means the browser process).
  return std::visit(base::Overloaded{
                        [&](ContentMainDelegate::InvokedInBrowserProcess) {
                          return process_type.empty();
                        },
                        [&](ContentMainDelegate::InvokedInChildProcess) {
                          return !process_type.empty();
                        },
                    },
                    arg);
}

// Tests that methods of ContentMainDelegate are called in the expected order.
class ContentMainRunnerImplBrowserTest : public ContentBrowserTest {
 protected:
  using Self = ContentMainRunnerImplBrowserTest;
  using Super = ContentBrowserTest;

  void SetUp() override {
    const std::string kBrowserProcessType = "";

    EXPECT_CALL(mock_delegate_, MockBasicStartupComplete())
        .Times(AtMost(1))
        .WillRepeatedly(DoAll(Invoke(this, &Self::TestBasicStartupComplete),
                              Return(std::nullopt)));

    EXPECT_CALL(mock_delegate_, MockCreateVariationsIdsProvider())
        .Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, MockPreSandboxStartup()).Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, MockSandboxInitialized(kBrowserProcessType))
        .Times(AtMost(1));

    EXPECT_CALL(mock_delegate_, ShouldCreateFeatureList(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_delegate_, ShouldInitializeMojo(_))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(mock_delegate_, MockPreBrowserMain())
        .Times(AtMost(1))
        .WillRepeatedly(Return(std::nullopt));

    EXPECT_CALL(mock_delegate_, MockPostEarlyInitialization(
                                    InvokedInMatcher(kBrowserProcessType)))
        .Times(AtMost(1))
        .WillRepeatedly(DoAll(Invoke(this, &Self::TestPostEarlyInitialization),
                              Return(std::nullopt)));

    EXPECT_CALL(mock_delegate_, MockRunProcess(kBrowserProcessType, _))
        .Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, MockProcessExiting(kBrowserProcessType))
        .Times(AtMost(1));

    EXPECT_CALL(mock_delegate_, MockShouldLockSchemeRegistry())
        .Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, MockCreateContentClient()).Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, MockCreateContentBrowserClient())
        .Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, MockCreateContentGpuClient()).Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, MockCreateContentRendererClient())
        .Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, MockCreateContentUtilityClient())
        .Times(AtMost(1));

    Super::SetUp();
  }

  ContentMainDelegate* GetOptionalContentMainDelegateOverride() override {
    return &mock_delegate_;
  }

  void TestBasicStartupComplete() {
    // The PostEarlyInitialization test checks that ContentMainRunnerImpl set up
    // the FeatureList.
    // In standard multi-process tests, FeatureList should not exist yet
    // (EXPECT_FALSE). However, on Starboard, we run in single-process mode
    // where TestLauncher has already initialized the global FeatureList.
    // ContentMainRunnerImpl correctly detects this and skips re-initialization,
    // but the test must expect it to be present.
    EXPECT_TRUE(base::FeatureList::GetInstance());
  }

  void TestPostEarlyInitialization() {
    // ContentMainRunnerImpl should have set up the ThreadPoolInstance and
    // FeatureList by this point.
    EXPECT_TRUE(base::ThreadPoolInstance::Get());
    EXPECT_TRUE(base::FeatureList::GetInstance());
  }

  ::testing::StrictMock<MockContentMainDelegate> mock_delegate_;
};

IN_PROC_BROWSER_TEST_F(ContentMainRunnerImplBrowserTest, StartupSequence) {
  // All of the work is done in SetUp().
}

}  // namespace

}  // namespace content
