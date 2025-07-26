// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FUZZER_FUZZER_SUPPORT_H_
#define CONTENT_TEST_FUZZER_FUZZER_SUPPORT_H_

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "content/public/test/render_view_test.h"

namespace content {

// Adapter to GUnit's test case.
class RenderViewTestAdapter : public RenderViewTest {
 public:
  RenderViewTestAdapter();

  RenderViewTestAdapter(const RenderViewTestAdapter&) = delete;
  RenderViewTestAdapter& operator=(const RenderViewTestAdapter&) = delete;

  void TestBody() override {}
  // make SetUp visible.
  void SetUp() override;

  void LoadHTML(const std::string& html, const std::string& url) {
    RenderViewTest::LoadHTMLWithUrlOverride(html.c_str(), url.c_str());
  }

  using RenderViewTest::GetMainFrame;

 private:
  const base::test::ScopedRunLoopTimeout increased_timeout_;
};

// Static environment. Initialized only once.
struct Env {
  Env();
  ~Env();

  base::AtExitManager at_exit;
  std::unique_ptr<RenderViewTestAdapter> adapter;
};
}  // namespace content

#endif  // CONTENT_TEST_FUZZER_FUZZER_SUPPORT_H_
