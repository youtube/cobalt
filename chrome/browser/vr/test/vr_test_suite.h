// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_VR_TEST_SUITE_H_
#define CHROME_BROWSER_VR_TEST_VR_TEST_SUITE_H_

#include "base/test/test_suite.h"

namespace content {
class BrowserTaskEnvironment;
}  // namespace content

namespace vr {

class VrTestSuite : public base::TestSuite {
 public:
  VrTestSuite(int argc, char** argv);

  VrTestSuite(const VrTestSuite&) = delete;
  VrTestSuite& operator=(const VrTestSuite&) = delete;

  ~VrTestSuite() override;

 protected:
  void Initialize() override;
  void Shutdown() override;

 private:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_VR_TEST_SUITE_H_
