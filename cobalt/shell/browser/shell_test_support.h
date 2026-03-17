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
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/views_delegate.h"

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
  MOCK_METHOD(void, ConcealShell, (Shell * shell), (override));
  MOCK_METHOD(void, OnBlur, (), (override));
  MOCK_METHOD(void, OnFocus, (), (override));
  MOCK_METHOD(void, OnConceal, (), (override));
  MOCK_METHOD(void, OnReveal, (), (override));
  MOCK_METHOD(void, OnFreeze, (), (override));
  MOCK_METHOD(void, OnUnfreeze, (), (override));
  MOCK_METHOD(void, OnStop, (), (override));
};

class TestBrowserAccessibilityState : public BrowserAccessibilityStateImpl {
 public:
  TestBrowserAccessibilityState() = default;
};

class MojoInitializer {
 public:
  MojoInitializer() {
    if (!aura::Env::HasInstance()) {
      aura_env_ = aura::Env::CreateInstance();
    }
    mojo::core::Init();
  }

 private:
  std::unique_ptr<aura::Env> aura_env_;
};

class CobaltShellTestBase : public ::testing::Test {
 public:
  CobaltShellTestBase()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~CobaltShellTestBase() override = default;

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(switches::kSingleProcess)) {
      command_line->AppendSwitch(switches::kSingleProcess);
    }
    content_initializer_ = std::make_unique<TestContentClientInitializer>();
    browser_context_ = std::make_unique<TestBrowserContext>();
    rvh_enabler_ = std::make_unique<RenderViewHostTestEnabler>();
    if (!ui::ResourceBundle::HasSharedInstance()) {
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          "en-US", nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
    }
    browser_accessibility_state_ =
        std::make_unique<TestBrowserAccessibilityState>();
  }

  void TearDown() override {
    Shell::Shutdown();
    rvh_enabler_.reset();
    browser_context_.reset();
    browser_accessibility_state_.reset();
    content_initializer_.reset();
  }

  void InitializeShell(bool is_visible) {
    auto platform =
        std::make_unique<::testing::NiceMock<MockShellPlatformDelegate>>();
    platform_ = platform.get();
    ON_CALL(*platform_, OnBlur()).WillByDefault([this]() {
      platform_->ShellPlatformDelegate::OnBlur();
    });
    ON_CALL(*platform_, OnFocus()).WillByDefault([this]() {
      platform_->ShellPlatformDelegate::OnFocus();
    });
    ON_CALL(*platform_, OnConceal()).WillByDefault([this]() {
      platform_->ShellPlatformDelegate::OnConceal();
    });
    ON_CALL(*platform_, OnReveal()).WillByDefault([this]() {
      platform_->ShellPlatformDelegate::OnReveal();
    });
    ON_CALL(*platform_, OnFreeze()).WillByDefault([this]() {
      platform_->ShellPlatformDelegate::OnFreeze();
    });
    ON_CALL(*platform_, OnUnfreeze()).WillByDefault([this]() {
      platform_->ShellPlatformDelegate::OnUnfreeze();
    });
    ON_CALL(*platform_, OnStop()).WillByDefault([this]() {
      platform_->ShellPlatformDelegate::OnStop();
    });
    ON_CALL(*platform_, DidCloseLastWindow()).WillByDefault([this]() {
      platform_->ShellPlatformDelegate::DidCloseLastWindow();
    });

    Shell::Initialize(std::move(platform), is_visible);
  }

  TestBrowserContext* browser_context() { return browser_context_.get(); }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 protected:
  MojoInitializer mojo_initializer_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestContentClientInitializer> content_initializer_;
  std::unique_ptr<TestBrowserAccessibilityState> browser_accessibility_state_;
  std::unique_ptr<RenderViewHostTestEnabler> rvh_enabler_;
  raw_ptr<::testing::NiceMock<MockShellPlatformDelegate>> platform_ = nullptr;
  std::unique_ptr<TestBrowserContext> browser_context_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_TEST_SUPPORT_H_
