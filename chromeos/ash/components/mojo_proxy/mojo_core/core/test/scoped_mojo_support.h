// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_TEST_SCOPED_MOJO_SUPPORT_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_TEST_SCOPED_MOJO_SUPPORT_H_

#include "base/test/test_io_thread.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/embedder/scoped_ipc_support.h"

namespace mojo_legacy::core::test {

// Brings up and cleanly tears down a Mojo Core instance in the current process,
// including a dedicated IO thread and ScopedIPCSupport. In order for Mojo to
// have its features properly configured, this object must be constructed AFTER
// base::FeatureList initialization.
//
// Test suites should generally initialize and tear this down around each
// individual test. MojoTestSuiteBase does exactly that when used.
class ScopedMojoSupport {
 public:
  ScopedMojoSupport();
  ScopedMojoSupport(const ScopedMojoSupport&) = delete;
  ScopedMojoSupport& operator=(const ScopedMojoSupport&) = delete;
  ~ScopedMojoSupport();

 private:
  class CoreInstance;

  std::unique_ptr<CoreInstance> core_;
  base::TestIOThread test_io_thread_{base::TestIOThread::kAutoStart};
  mojo_legacy::core::ScopedIPCSupport ipc_support_{
      test_io_thread_.task_runner(),
      mojo_legacy::core::ScopedIPCSupport::ShutdownPolicy::CLEAN};
};

}  // namespace mojo_legacy::core::test

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_TEST_SCOPED_MOJO_SUPPORT_H_
