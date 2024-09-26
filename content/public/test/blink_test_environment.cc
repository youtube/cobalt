// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/blink_test_environment.h"

#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_tokenizer.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "build/build_config.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "mojo/core/embedder/embedder.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/blink.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/dpi.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace content {

namespace {

class TestEnvironment {
 public:
  TestEnvironment() {
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
    ContentTestSuiteBase::InitializeResourceBundle();

    // TestBlinkWebUnitTestSupport construction needs Mojo to be initialized
    // first.
    mojo::core::Init(mojo::core::Configuration{.is_broker_process = true});

    // Depends on resource bundle initialization so has to happen after.
    blink_test_support_ = std::make_unique<TestBlinkWebUnitTestSupport>(
        TestBlinkWebUnitTestSupport::SchedulerType::kRealScheduler);
  }

  ~TestEnvironment() {}

  // This returns when both the main thread and the TaskSchedules queues are
  // empty.
  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

 private:
  std::unique_ptr<TestBlinkWebUnitTestSupport> blink_test_support_;
  TestContentClientInitializer content_initializer_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

TestEnvironment* test_environment;

}  // namespace

void SetUpBlinkTestEnvironment() {
  blink::WebRuntimeFeatures::EnableExperimentalFeatures(true);
  blink::WebRuntimeFeatures::EnableTestOnlyFeatures(true);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  for (const std::string& feature : content::FeaturesFromSwitch(
           command_line, switches::kEnableBlinkFeatures)) {
    blink::WebRuntimeFeatures::EnableFeatureFromString(feature, true);
  }
  for (const std::string& feature : content::FeaturesFromSwitch(
           command_line, switches::kDisableBlinkFeatures)) {
    blink::WebRuntimeFeatures::EnableFeatureFromString(feature, false);
  }

#if BUILDFLAG(IS_MAC)
  mock_cr_app::RegisterMockCrApp();
#endif

#if BUILDFLAG(IS_WIN)
  display::win::SetDefaultDeviceScaleFactor(1.0f);
#endif

  test_environment = new TestEnvironment;
}

void TearDownBlinkTestEnvironment() {
  // Flush any remaining messages before we kill ourselves.
  // http://code.google.com/p/chromium/issues/detail?id=9500
  test_environment->RunUntilIdle();

  delete test_environment;
  test_environment = nullptr;
}

}  // namespace content
