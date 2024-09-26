// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_BROWSERTEST_HARNESS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_BROWSERTEST_HARNESS_H_

#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/test/content_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class Graph;

// Like PerformanceManagerTestHarness, but for browser tests. Full process
// trees and live RFHs, etc, are created. Meant to be used from
// components_browsertests and browser_tests.
class PerformanceManagerBrowserTestHarness
    : public content::ContentBrowserTest {
  using Super = content::ContentBrowserTest;

 public:
  PerformanceManagerBrowserTestHarness();
  PerformanceManagerBrowserTestHarness(
      const PerformanceManagerBrowserTestHarness&) = delete;
  PerformanceManagerBrowserTestHarness& operator=(
      const PerformanceManagerBrowserTestHarness&) = delete;
  ~PerformanceManagerBrowserTestHarness() override;

  void SetUp() override;

  // content::BrowserTestBase:
  void PreRunTestOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // An additional seam that gets invoked as part of the PM initialization. This
  // will be invoked on the PM sequence. This will be called after graph
  // features have been configured (see "GetGraphFeatures").
  virtual void OnGraphCreated(Graph* graph);

  // Creates a content shell with its own window, hosting a single tab that is
  // navigated to about:blank. The WebContents will have the PM helpers
  // attached. Ownership of the shell rests with this object. Note that such a
  // shell is created by default by the fixture, accessible via "shell()". Only
  // call this if you need multiple independent WebContents.
  content::Shell* CreateShell();

  // Starts a navigation for the given |contents|.
  void StartNavigation(content::WebContents* contents, const GURL& url);

  // Navigates the given |contents| to |url|, and waits for |console_pattern|
  // to appear on the console. This can be used with test files like
  // components/test/data/performance_manager/a_embeds_a.html, to wait for both
  // the main frame and the embedded subframe to load. Returns
  // ::testing::AssertionFailure if the navigation fails. If the navigation
  // succeeds, will wait (potentially forever) for |console_pattern| to appear
  // and then return ::testing::AssertionSuccess.
  ::testing::AssertionResult NavigateAndWaitForConsoleMessage(
      content::WebContents* contents,
      const GURL& url,
      base::StringPiece console_pattern);

  // Waits for an ongoing navigation to terminate on the given |contents|.
  void WaitForLoad(content::WebContents* contents);

  // Helper function for running a task on the graph, and waiting for it to
  // complete. The signature of OnGraphCallback is expected to be void(Graph*).
  template <typename OnGraphCallback>
  void RunInGraph(OnGraphCallback on_graph_callback) {
    base::RunLoop run_loop;
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindLambdaForTesting([quit_loop = run_loop.QuitClosure(),
                                    &on_graph_callback](Graph* graph) {
          on_graph_callback(graph);
          quit_loop.Run();
        }));
    run_loop.Run();
  }

  // Allows configuring which Graph features are initialized during "SetUp".
  // This defaults to initializing no features. Features will be initialized
  // before "OnGraphCreated" is called.
  GraphFeatures& GetGraphFeatures() { return graph_features_; }

 private:
  // This is called during "SetUp". It installs features on the graph and then
  // delegates to "OnGraphCreated".
  void OnGraphCreatedImpl(Graph* graph);

  GraphFeatures graph_features_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_BROWSERTEST_HARNESS_H_
