// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_PROTOCOL_BROWSERTEST_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_PROTOCOL_BROWSERTEST_H_

#include <string>

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/headless/headless_mode_devtooled_browsertest.h"

namespace headless {

class HeadlessModeProtocolBrowserTest
    : public HeadlessModeDevTooledBrowserTest {
 public:
  HeadlessModeProtocolBrowserTest();
  ~HeadlessModeProtocolBrowserTest() override;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Implement this for tests that need to pass extra parameters to
  // JavaScript code.
  virtual base::Value::Dict GetPageUrlExtraParams();

 private:
  // HeadlessModeDevTooledBrowserTest overrides.
  void RunDevTooledTest() override;

  void OnLoadEventFired(const base::Value::Dict& params);
  void OnEvaluateResult(base::Value::Dict params);
  void OnConsoleAPICalled(const base::Value::Dict& params);

  void ProcessTestResult(const std::string& test_result);

 protected:
  std::string test_folder_;
  std::string script_name_;
};

#define HEADLESS_MODE_PROTOCOL_TEST(TEST_NAME, SCRIPT_NAME)            \
  IN_PROC_BROWSER_TEST_F(HeadlessModeProtocolBrowserTest, TEST_NAME) { \
    test_folder_ = "/protocol/";                                       \
    script_name_ = SCRIPT_NAME;                                        \
    RunTest();                                                         \
  }

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_PROTOCOL_BROWSERTEST_H_
