// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler_v2.h"

#include <memory>

#include "base/test/test_future.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {
namespace {

class SkillsPageHandlerV2Test : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    handler_ = std::make_unique<SkillsPageHandlerV2>(
        remote_handler_.BindNewPipeAndPassReceiver(), profile(),
        identity_test_env_.identity_manager(), web_contents());
  }

  void TearDown() override {
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  signin::IdentityTestEnvironment identity_test_env_;
  mojo::Remote<::skills::mojom::SkillsPageHandler> remote_handler_;
  std::unique_ptr<SkillsPageHandlerV2> handler_;
};

TEST_F(SkillsPageHandlerV2Test, SyncCookies) {
  base::test::TestFuture<bool> future;
  remote_handler_->SyncCookies(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace
}  // namespace skills
