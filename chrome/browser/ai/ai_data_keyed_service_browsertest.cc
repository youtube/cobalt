// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_data_keyed_service.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class AiDataKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  AiDataKeyedServiceBrowserTest() = default;

  AiDataKeyedServiceBrowserTest(const AiDataKeyedServiceBrowserTest&) = delete;
  AiDataKeyedServiceBrowserTest& operator=(
      const AiDataKeyedServiceBrowserTest&) = delete;

  ~AiDataKeyedServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());

    url_ = https_server_->GetURL("/simple.html");
  }

  void SetAiData(base::OnceClosure quit_closure,
                 AiDataKeyedService::AiData ai_data) {
    ai_data_ = std::move(ai_data);
    std::move(quit_closure).Run();
  }

  GURL url() { return url_; }

  const AiDataKeyedService::AiData& ai_data() { return ai_data_; }

  void LoadSimplePageAndData() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents, url(), 1);
    AiDataKeyedService* ai_data_service =
        AiDataKeyedServiceFactory::GetAiDataKeyedService(browser()->profile());

    base::RunLoop run_loop;
    auto dom_node_id = 0;
    ai_data_service->GetAiDataWithSpecifiers(
        1, dom_node_id, web_contents, "test",
        base::BindOnce(&AiDataKeyedServiceBrowserTest::SetAiData,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    DCHECK(ai_data());
  }

 private:
  GURL url_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  AiDataKeyedService::AiData ai_data_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest,
                       AllowlistedExtensionList) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "hpkopmikdojpadgmioifjjodbmnjjjca", "nfdaijodggdcjengofmbibbkcnopmikg"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, GetsData) {
  LoadSimplePageAndData();
  EXPECT_TRUE(ai_data());
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, InnerText) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->page_context().inner_text(), "Non empty simple page");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, InnerTextOffset) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->page_context().inner_text_offset(), 0u);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Title) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->page_context().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Url) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_NE(ai_data()->page_context().url().find("simple"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, AxTreeUpdate) {
  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  // If there are nodes and the titles is correct, then the AX tree is filled
  // out.
  EXPECT_GT(ai_data()->page_context().ax_tree_data().nodes().size(), 0);
  EXPECT_EQ(ai_data()->page_context().ax_tree_data().tree_data().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabData) {
  chrome::AddTabAt(browser(), GURL("foo.com"), -1, false);
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, false);

  auto* tab_group1 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({0}));
  auto vis_data1 = *tab_group1->visual_data();
  vis_data1.SetTitle(u"ok");
  tab_group1->SetVisualData(vis_data1);

  auto* tab_group2 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({1, 2}));
  auto vis_data2 = *tab_group1->visual_data();
  vis_data2.SetTitle(u"ok");
  tab_group2->SetVisualData(vis_data2);

  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());

  EXPECT_EQ(ai_data()->active_tab_id(), 0);
  EXPECT_EQ(ai_data()->tabs().size(), 3);
  EXPECT_EQ(ai_data()->pre_existing_tab_groups().size(), 2);
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabInnerText) {
  chrome::AddTabAt(browser(), GURL("foo.com"), -1, false);
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, false);

  auto* tab_group1 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({0}));
  auto vis_data1 = *tab_group1->visual_data();
  vis_data1.SetTitle(u"ok");
  tab_group1->SetVisualData(vis_data1);

  auto* tab_group2 = browser()->GetTabStripModel()->group_model()->GetTabGroup(
      browser()->GetTabStripModel()->AddToNewGroup({1, 2}));
  auto vis_data2 = *tab_group1->visual_data();
  vis_data2.SetTitle(u"ok");
  tab_group2->SetVisualData(vis_data2);

  LoadSimplePageAndData();
  ASSERT_TRUE(ai_data());
  EXPECT_EQ(ai_data()->active_tab_id(), 0);
  for (const auto& tab_in_proto : ai_data()->tabs()) {
    if (tab_in_proto.tab_id() == 0) {
      EXPECT_EQ(tab_in_proto.title(), "OK");
      EXPECT_NE(tab_in_proto.url().find("simple"), std::string::npos);
      EXPECT_EQ(tab_in_proto.page_context().inner_text(),
                "Non empty simple page");
    }
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, TabInnerTextLimit) {
  LoadSimplePageAndData();
  chrome::AddTabAt(browser(), GURL("bar.com"), -1, true);
  LoadSimplePageAndData();
  EXPECT_EQ(ai_data()->active_tab_id(), 1);
  for (auto& tab : ai_data()->tabs()) {
    if (tab.tab_id() == 0) {
      EXPECT_EQ(tab.page_context().inner_text(), "Non empty simple page");
    }
    if (tab.tab_id() == 1) {
      EXPECT_EQ(tab.page_context().inner_text(), "");
    }
  }
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, Screenshot) {
  LoadSimplePageAndData();
  content::RequestFrame(browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(ai_data()->page_context().tab_screenshot(), "");
}

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTest, SiteEngagementScores) {
  LoadSimplePageAndData();
  EXPECT_EQ(ai_data()->site_engagement().entries().size(), 1);
  EXPECT_NE(ai_data()->site_engagement().entries()[0].url(), "");
  EXPECT_GE(ai_data()->site_engagement().entries()[0].score(), 0);
}

class AiDataKeyedServiceBrowserTestWithBlocklistedExtensions
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithBlocklistedExtensions() override = default;
  AiDataKeyedServiceBrowserTestWithBlocklistedExtensions() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"blocked_extension_ids", "hpkopmikdojpadgmioifjjodbmnjjjca"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTestWithBlocklistedExtensions,
                       BlockedExtensionList) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "nfdaijodggdcjengofmbibbkcnopmikg"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

class AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions() override =
      default;
  AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"allowlisted_extension_ids", "1234"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AiDataKeyedServiceBrowserTestWithRemotelyAllowlistedExtensions,
    RemotelyAllowlistedExtensionList) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "1234",
      "hpkopmikdojpadgmioifjjodbmnjjjca",
      "nfdaijodggdcjengofmbibbkcnopmikg",
  };

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

class AiDataKeyedServiceBrowserTestWithAllowAndBlock
    : public AiDataKeyedServiceBrowserTest {
 public:
  ~AiDataKeyedServiceBrowserTestWithAllowAndBlock() override = default;
  AiDataKeyedServiceBrowserTestWithAllowAndBlock() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"allowlisted_extension_ids", "1234"},
         {"blocked_extension_ids", "1234"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiDataKeyedServiceBrowserTestWithAllowAndBlock,
                       AllowAndBlock) {
  std::vector<std::string> expected_allowlisted_extensions = {
      "hpkopmikdojpadgmioifjjodbmnjjjca", "nfdaijodggdcjengofmbibbkcnopmikg"};

  EXPECT_EQ(AiDataKeyedService::GetAllowlistedExtensions(),
            expected_allowlisted_extensions);
}

}  // namespace
