// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_TEST_MOJO_TEST_SUITE_BASE_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_TEST_MOJO_TEST_SUITE_BASE_H_

#include <memory>

#include "base/test/test_suite.h"
#include "build/build_config.h"

namespace base::test {
class ScopedFeatureList;
}

namespace mojo_legacy::core::test {

class ScopedMojoSupport;

class MojoTestSuiteBase : public base::TestSuite {
 public:
  MojoTestSuiteBase(int argc, char** argv);
#if BUILDFLAG(IS_WIN)
  MojoTestSuiteBase(int argc, wchar_t** argv);
#endif  // BUILDFLAG(IS_WIN)

  MojoTestSuiteBase(const MojoTestSuiteBase&) = delete;
  MojoTestSuiteBase& operator=(const MojoTestSuiteBase&) = delete;

  ~MojoTestSuiteBase() override;

 protected:
  void Initialize() override;
  void Shutdown() override;

 private:
  // Test child processes for multiprocess tests and death tests do not run
  // GTest listener hooks used by this TestSuite to initialize and teardown
  // Mojo (and by the base TestSuite to initialize FeatureList). In these cases
  // we manually initialize both Mojo and FeatureList.
  void MaybeInitializeChildProcessEnvironment();

  std::unique_ptr<base::test::ScopedFeatureList> child_feature_list_;
  std::unique_ptr<ScopedMojoSupport> child_mojo_support_;
};

}  // namespace mojo_legacy::core::test

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_TEST_MOJO_TEST_SUITE_BASE_H_
