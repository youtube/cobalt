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

#ifndef COBALT_SHELL_BROWSER_SHELL_TEST_SUPPORT_H_
#define COBALT_SHELL_BROWSER_SHELL_TEST_SUPPORT_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/notification_service_impl.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace content {

class MockShellPlatformDelegate : public ShellPlatformDelegate {
 public:
  void Initialize(const gfx::Size& default_window_size,
                  bool is_visible) override {
    set_is_visible(is_visible);
  }
  MOCK_METHOD(void,
              CreatePlatformWindow,
              (Shell * shell, const gfx::Size& initial_size),
              (override));
  MOCK_METHOD(void, SetContents, (Shell * shell), (override));
  MOCK_METHOD(void, LoadSplashScreenContents, (Shell * shell), (override));
  MOCK_METHOD(void, UpdateContents, (Shell * shell), (override));
  MOCK_METHOD(void,
              ResizeWebContent,
              (Shell * shell, const gfx::Size& content_size),
              (override));
  MOCK_METHOD(bool, DestroyShell, (Shell * shell), (override));
  MOCK_METHOD(void, CleanUp, (Shell * shell), (override));
  MOCK_METHOD(void,
              EnableUIControl,
              (Shell * shell, UIControl control, bool is_enabled),
              (override));
  MOCK_METHOD(void,
              SetAddressBarURL,
              (Shell * shell, const GURL& url),
              (override));
  MOCK_METHOD(void,
              SetTitle,
              (Shell * shell, const std::u16string& title),
              (override));
  MOCK_METHOD(void, DidCloseLastWindow, (), (override));
  MOCK_METHOD(void, RevealShell, (Shell * shell), (override));
};

class TestBrowserAccessibilityState : public BrowserAccessibilityStateImpl {
 public:
  TestBrowserAccessibilityState() = default;
};

class ShellTestBase : public ::testing::Test {
 public:
  ShellTestBase()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ForceInProcessNetworkService(true);
    mojo::core::Init();
    ui::DeviceDataManager::CreateInstance();
#if defined(USE_AURA)
    env_ = aura::Env::CreateInstance();
#endif
    browser_accessibility_state_ =
        std::make_unique<TestBrowserAccessibilityState>();

    notification_service_ = std::make_unique<NotificationServiceImpl>();

    SetContentClient(&test_content_client_);
    SetBrowserClientForTesting(&test_content_browser_client_);

    rvh_enabler_ = std::make_unique<RenderViewHostTestEnabler>();

    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  void TearDown() override {
    platform_ = nullptr;
    Shell::Shutdown();
    browser_context_.reset();
    rvh_enabler_.reset();
    SetBrowserClientForTesting(nullptr);
    SetContentClient(nullptr);
    notification_service_.reset();
    browser_accessibility_state_.reset();
#if defined(USE_AURA)
    env_.reset();
#endif
    ui::DeviceDataManager::DeleteInstance();
  }

  void InitializeShell(bool is_visible) {
    auto platform =
        std::make_unique<::testing::NiceMock<MockShellPlatformDelegate>>();
    platform_ = platform.get();
    Shell::Initialize(std::move(platform), is_visible);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestContentClient test_content_client_;
  TestContentBrowserClient test_content_browser_client_;
#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env_;
#endif
  std::unique_ptr<NotificationServiceImpl> notification_service_;
  std::unique_ptr<BrowserAccessibilityState> browser_accessibility_state_;
  std::unique_ptr<RenderViewHostTestEnabler> rvh_enabler_;
  raw_ptr<::testing::NiceMock<MockShellPlatformDelegate>> platform_ = nullptr;
  std::unique_ptr<TestBrowserContext> browser_context_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_TEST_SUPPORT_H_
