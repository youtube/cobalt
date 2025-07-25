// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_index_provider.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {
namespace {

class ContentIndexTest : public ContentBrowserTest {
 public:
  ContentIndexTest() = default;
  ~ContentIndexTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    shell_ = CreateBrowser();

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_->Start());
    ASSERT_TRUE(NavigateToURL(
        shell_, https_server_->GetURL("/content_index/test.html")));

    provider_ = static_cast<ShellContentIndexProvider*>(
        shell_->web_contents()->GetBrowserContext()->GetContentIndexProvider());
    ASSERT_TRUE(provider_);

    auto* storage_partition =
        shell_->web_contents()->GetBrowserContext()->GetStoragePartition(
            shell_->web_contents()->GetSiteInstance());
    context_ = storage_partition->GetContentIndexContext();
    ASSERT_TRUE(context_);
  }

  void TearDownOnMainThread() override {
    context_ = nullptr;
    provider_ = nullptr;
    shell_ = nullptr;
    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  std::string RunScriptWithResult(const std::string& script) {
    return EvalJs(shell_->web_contents(), script).ExtractString();
  }

  // Runs |script| and expects it to complete successfully.
  void RunScript(const std::string& script) {
    ASSERT_EQ(RunScriptWithResult(script), "ok");
  }

  std::vector<SkBitmap> GetIcons(int64_t service_worker_registration_id,
                                 const std::string& description_id) {
    std::vector<SkBitmap> out_icons;
    base::RunLoop run_loop;
    context_->GetIcons(
        service_worker_registration_id, description_id,
        base::BindLambdaForTesting([&](std::vector<SkBitmap> icons) {
          out_icons = std::move(icons);
          run_loop.Quit();
        }));
    run_loop.Run();
    return out_icons;
  }

  ShellContentIndexProvider* provider() { return provider_; }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  raw_ptr<ShellContentIndexProvider> provider_ = nullptr;
  raw_ptr<ContentIndexContext> context_ = nullptr;
  raw_ptr<Shell> shell_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ContentIndexTest, GetIcons) {
  {
    // Don't load any icons.
    provider()->set_icon_sizes({});
    RunScript("addContent('id1', [{src: '/single_face.jpg'}])");
    base::RunLoop().RunUntilIdle();

    auto registration_data = provider()->GetRegistrationDataFromId("id1");
    ASSERT_NE(registration_data.first, -1);
    EXPECT_TRUE(GetIcons(registration_data.first, "id1").empty());
  }

  {
    // Load one icon.
    provider()->set_icon_sizes({{42, 42}});
    RunScript("addContent('id2', [{src: '/single_face.jpg'}])");
    base::RunLoop().RunUntilIdle();

    auto registration_data = provider()->GetRegistrationDataFromId("id2");
    ASSERT_NE(registration_data.first, -1);
    auto icons = GetIcons(registration_data.first, "id2");
    ASSERT_EQ(icons.size(), 1u);
    ASSERT_FALSE(icons[0].isNull());
    EXPECT_EQ(icons[0].width(), 42);
    EXPECT_EQ(icons[0].height(), 42);
  }

  {
    // Load two icons.
    provider()->set_icon_sizes({{42, 42}, {24, 24}});
    RunScript("addContent('id3', [{src: '/single_face.jpg'}])");
    base::RunLoop().RunUntilIdle();

    auto registration_data = provider()->GetRegistrationDataFromId("id3");
    ASSERT_NE(registration_data.first, -1);
    auto icons = GetIcons(registration_data.first, "id3");
    ASSERT_EQ(icons.size(), 2u);
    if (icons[0].height() > icons[1].height()) {
      std::swap(icons[0], icons[1]);
    }

    ASSERT_FALSE(icons[0].isNull());
    EXPECT_EQ(icons[0].width(), 24);
    EXPECT_EQ(icons[0].height(), 24);

    ASSERT_FALSE(icons[1].isNull());
    EXPECT_EQ(icons[1].width(), 42);
    EXPECT_EQ(icons[1].height(), 42);
  }
}

IN_PROC_BROWSER_TEST_F(ContentIndexTest, RegistrationWithoutIcons) {
  // Don't load any icons.
  provider()->set_icon_sizes({});
  // Registering without icons is ok since the browser doesn't need any.
  RunScript("addContent('id1')");
}

IN_PROC_BROWSER_TEST_F(ContentIndexTest, BestIconIsChosen) {
  // Load one icon.
  provider()->set_icon_sizes({{42, 42}});

  // If the first resource is chosen, the registration would fail since the
  // resource does not exist.
  RunScript(R"(
    addContent('id3', [
      {
        src: '/MISSING_IMAGE.png',
        sizes: '2x2',
        type: 'image/png',
      },
      {
        src: '/single_face.jpg',
        sizes: '42x42',
        type: 'image/jpg',
      },
    ]))");
}

class ContentIndexOfflineCapabilityTest : public ContentIndexTest {
  void SetUp() override {
    feature_list_.InitFromCommandLine("ContentIndexCheckOffline", "");
    ContentIndexTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContentIndexOfflineCapabilityTest,
                       CheckOfflineCapability) {
  // Registering content should still work if the url is offline-capable.
  RunScript("addContent('id1', [{src: '/single_face.jpg'}], 'forcesuccess')");

  // Registering content should fail if the url is not offline-capable.
  std::string result = RunScriptWithResult(
      "addContent('id2', [{src: '/single_face.jpg'}], 'forcefail')");
  EXPECT_EQ(result,
            "TypeError - The provided launch URL is not offline-capable.");
}

}  // namespace
}  // namespace content
