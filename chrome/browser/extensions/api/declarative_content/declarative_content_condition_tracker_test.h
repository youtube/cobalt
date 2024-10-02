// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_TRACKER_TEST_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_TRACKER_TEST_H_

#include <memory>

#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class MockRenderProcessHost;
class WebContents;
}  // namespace content

namespace extensions {

// Creates a TestWebContents browser context and mocks out RenderViewHosts. The
// latter is done to avoid having to run renderer processes and because the
// actual RenderViewHost implementation depends on things not available in this
// configuration.
class DeclarativeContentConditionTrackerTest : public testing::Test {
 public:
  DeclarativeContentConditionTrackerTest();

  DeclarativeContentConditionTrackerTest(
      const DeclarativeContentConditionTrackerTest&) = delete;
  DeclarativeContentConditionTrackerTest& operator=(
      const DeclarativeContentConditionTrackerTest&) = delete;

  ~DeclarativeContentConditionTrackerTest() override;

 protected:
  // Creates a new WebContents and retains ownership.
  std::unique_ptr<content::WebContents> MakeTab();

  // Gets the MockRenderProcessHost associated with a WebContents.
  content::MockRenderProcessHost* GetMockRenderProcessHost(
      content::WebContents* contents);

  // Can only be used before calling profile().
  TestingProfile::Builder* profile_builder() { return &profile_builder_; }

  // Returns a TestingProfile constructed lazily (upon first call).
  TestingProfile* profile();

  const void* GeneratePredicateGroupID();

  // Returns a list of factories to use when creating the TestingProfile.
  // Can be overridden by sub-classes if needed.
  virtual TestingProfile::TestingFactories GetTestingFactories() const;

 private:
  content::BrowserTaskEnvironment task_environment_;

  // Enables MockRenderProcessHosts.
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  TestingProfile::Builder profile_builder_;
  std::unique_ptr<TestingProfile> profile_;

  uintptr_t next_predicate_group_id_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_TRACKER_TEST_H_
