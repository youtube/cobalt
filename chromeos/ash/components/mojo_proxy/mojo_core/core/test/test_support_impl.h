// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_TEST_TEST_SUPPORT_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_TEST_TEST_SUPPORT_IMPL_H_

#include <stdio.h>

#include "chromeos/ash/components/mojo_proxy/mojo_core/public/tests/test_support_private.h"

namespace mojo_legacy {
namespace core {
namespace test {

class TestSupportImpl : public mojo_legacy::test::TestSupport {
 public:
  TestSupportImpl();

  TestSupportImpl(const TestSupportImpl&) = delete;
  TestSupportImpl& operator=(const TestSupportImpl&) = delete;

  ~TestSupportImpl() override;

  void LogPerfResult(const char* test_name,
                     const char* sub_test_name,
                     double value,
                     const char* units) override;
  FILE* OpenSourceRootRelativeFile(const char* relative_path) override;
  char** EnumerateSourceRootRelativeDirectory(
      const char* relative_path) override;
};

}  // namespace test
}  // namespace core
}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_TEST_TEST_SUPPORT_IMPL_H_
