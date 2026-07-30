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

#ifndef COBALT_TESTING_BROWSER_TESTS_SITE_PER_PROCESS_BROWSERTEST_H_
#define COBALT_TESTING_BROWSER_TESTS_SITE_PER_PROCESS_BROWSERTEST_H_

#include <string>

#include "base/test/scoped_feature_list.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils_internal.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;

class SitePerProcessBrowserTestBase : public ContentBrowserTest {
 public:
  SitePerProcessBrowserTestBase();

  SitePerProcessBrowserTestBase(const SitePerProcessBrowserTestBase&) = delete;
  SitePerProcessBrowserTestBase& operator=(
      const SitePerProcessBrowserTestBase&) = delete;

 protected:
  std::string DepictFrameTree(FrameTreeNode* node);

  std::string WaitForMessageScript(const std::string& result_expression);

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  static void ForceUpdateViewportIntersection(
      FrameTreeNode* frame_tree_node,
      const blink::mojom::ViewportIntersectionState& intersection_state);

  void RunPostedTasks();

 private:
  FrameTreeVisualizer visualizer_;
  base::test::ScopedFeatureList feature_list_;
};

class SitePerProcessBrowserTest
    : public SitePerProcessBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 public:
  SitePerProcessBrowserTest();

  SitePerProcessBrowserTest(const SitePerProcessBrowserTest&) = delete;
  SitePerProcessBrowserTest& operator=(const SitePerProcessBrowserTest&) =
      delete;

  std::string GetExpectedOrigin(const std::string& host);

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SitePerProcessIgnoreCertErrorsBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessIgnoreCertErrorsBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_SITE_PER_PROCESS_BROWSERTEST_H_
