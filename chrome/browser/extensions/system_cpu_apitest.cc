// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test.h"
#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/extension_platform_apitest.h"
#else
#include "chrome/browser/extensions/extension_apitest.h"
#endif

namespace extensions {

using api::system_cpu::CpuInfo;

class MockCpuInfoProviderImpl : public CpuInfoProvider {
 public:
  MockCpuInfoProviderImpl() = default;
  MockCpuInfoProviderImpl(const MockCpuInfoProviderImpl&) = delete;
  MockCpuInfoProviderImpl& operator=(const MockCpuInfoProviderImpl&) = delete;

  bool QueryInfo() override {
    info_.num_of_processors = 4;
    info_.arch_name = "x86";
    info_.model_name = "unknown";

    info_.features.clear();
    info_.features.push_back("mmx");
    info_.features.push_back("avx");

    info_.processors.clear();
    info_.processors.emplace_back(api::system_cpu::ProcessorInfo());
    info_.processors[0].usage.kernel = 1;
    info_.processors[0].usage.user = 2;
    info_.processors[0].usage.idle = 3;
    info_.processors[0].usage.total = 6;

    // The fractional part of these values should be exactly represented as
    // floating points to avoid rounding errors.
    info_.temperatures = {30.125, 40.0625};
    return true;
  }

 private:
  ~MockCpuInfoProviderImpl() override = default;
};

#if BUILDFLAG(IS_ANDROID)
using SystemCpuApiTest = ExtensionPlatformApiTest;
#else
using SystemCpuApiTest = ExtensionApiTest;
#endif

IN_PROC_BROWSER_TEST_F(SystemCpuApiTest, Cpu) {
  scoped_refptr<CpuInfoProvider> provider = new MockCpuInfoProviderImpl;
  // The provider is owned by the single CpuInfoProvider instance.
  CpuInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("system_cpu")) << message_;
}

}  // namespace extensions
