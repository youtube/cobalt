// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/navigation_entry_remover.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "url/gurl.h"

using history::DeletionInfo;

class NavigationEntryRemoverTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    auto path = base::FilePath(FILE_PATH_LITERAL("browsing_data"));
    url_a_ = ui_test_utils::GetTestUrl(
        path, base::FilePath(FILE_PATH_LITERAL("a.html")));
    url_b_ = ui_test_utils::GetTestUrl(
        path, base::FilePath(FILE_PATH_LITERAL("b.html")));
    url_c_ = ui_test_utils::GetTestUrl(
        path, base::FilePath(FILE_PATH_LITERAL("c.html")));
    url_d_ = ui_test_utils::GetTestUrl(
        path, base::FilePath(FILE_PATH_LITERAL("d.html")));
    about_blank_ = GURL("about:blank");
  }

  void AddNavigations(Browser* browser, const std::vector<GURL>& urls) {
    for (const GURL& url : urls) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser, url, WindowOpenDisposition::CURRENT_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
  }

  void AddTab(Browser* browser, const std::vector<GURL>& urls) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser, urls[0], WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
            ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    AddNavigations(browser, {urls.begin() + 1, urls.end()});
  }

  void AddBrowser(Browser* browser, const std::vector<GURL>& urls) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser, urls[0], WindowOpenDisposition::NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
    AddNavigations(BrowserList::GetInstance()->GetLastActive(),
                   {urls.begin() + 1, urls.end()});
  }

  void GoBack(content::WebContents* web_contents) {
    content::LoadStopObserver load_stop_observer(web_contents);
    web_contents->GetController().GoBack();
    load_stop_observer.Wait();
  }

  std::vector<GURL> GetEntries() {
    std::vector<GURL> urls;
    for (Browser* browser : *BrowserList::GetInstance()) {
      for (int j = 0; j < browser->tab_strip_model()->count(); j++) {
        content::NavigationController* controller =
            &browser->tab_strip_model()->GetWebContentsAt(j)->GetController();
        for (int i = 0; i < controller->GetEntryCount(); i++)
          urls.push_back(controller->GetEntryAtIndex(i)->GetURL());
      }
    }
    return urls;
  }

  DeletionInfo CreateDeletionInfo(base::Time from,
                                  base::Time to,
                                  std::set<GURL> restrict_urls = {}) {
    return DeletionInfo(history::DeletionTimeRange(from, to), false, {}, {},
                        restrict_urls.empty() ? absl::optional<std::set<GURL>>()
                                              : restrict_urls);
  }

  // Helper to compare vectors. The macro gets confused by EXPECT_EQ(v, {a,b}).
  void ExpectEntries(const std::vector<GURL>& expected,
                     const std::vector<GURL>& actual) {
    EXPECT_EQ(expected, actual);
  }

  GURL url_a_;
  GURL url_b_;
  GURL url_c_;
  GURL url_d_;
  GURL about_blank_;

};

// === Tests for helper functions ===
IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, AddNavigation) {
  // A new browser starts with about:blank. Add a,b,c and check.
  ExpectEntries({about_blank_}, GetEntries());
  AddNavigations(browser(), {url_a_, url_b_, url_c_});
  ExpectEntries({about_blank_, url_a_, url_b_, url_c_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, AddTab) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  AddTab(browser(), {url_a_});
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  ExpectEntries({about_blank_, url_a_}, GetEntries());

  AddTab(browser(), {url_b_, url_c_});
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  ExpectEntries({about_blank_, url_a_, url_b_, url_c_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, AddWindow) {
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  AddBrowser(browser(), {url_a_, url_b_});
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());
  ExpectEntries({about_blank_, url_a_, url_b_}, GetEntries());

  AddBrowser(browser(), {url_c_, url_d_});
  EXPECT_EQ(3U, BrowserList::GetInstance()->size());
  ExpectEntries({about_blank_, url_a_, url_b_, url_c_, url_d_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, GoBack) {
  AddNavigations(browser(), {url_a_, url_b_, url_c_});
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url_c_, web_contents->GetLastCommittedURL());
  GoBack(web_contents);
  EXPECT_EQ(url_b_, web_contents->GetLastCommittedURL());
  ExpectEntries({about_blank_, url_a_, url_b_, url_c_}, GetEntries());
}

// === The actual tests ===
IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, DeleteIndividual) {
  AddNavigations(browser(), {url_a_, url_b_, url_c_, url_d_});
  browsing_data::RemoveNavigationEntries(
      browser()->profile(),
      DeletionInfo::ForUrls({history::URLResult(url_b_, base::Time())}, {}));
  ExpectEntries({about_blank_, url_a_, url_c_, url_d_}, GetEntries());

  browsing_data::RemoveNavigationEntries(
      browser()->profile(),
      DeletionInfo::ForUrls({history::URLResult(url_c_, base::Time())}, {}));
  ExpectEntries({about_blank_, url_a_, url_d_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, DeleteAfterNavigation) {
  AddNavigations(browser(), {url_a_, url_b_});
  browsing_data::RemoveNavigationEntries(
      browser()->profile(),
      DeletionInfo::ForUrls({history::URLResult(url_b_, base::Time())}, {}));
  // The commited entry can't be removed.
  ExpectEntries({about_blank_, url_a_, url_b_}, GetEntries());

  AddNavigations(browser(), {url_c_});
  browsing_data::RemoveNavigationEntries(
      browser()->profile(),
      DeletionInfo::ForUrls({history::URLResult(url_b_, base::Time())}, {}));
  ExpectEntries({about_blank_, url_a_, url_c_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, DeleteAll) {
  AddNavigations(browser(), {url_a_, url_b_, url_c_});
  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         DeletionInfo::ForAllHistory());
  ExpectEntries({url_c_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, DeleteRestricted) {
  AddNavigations(browser(), {url_a_, url_b_, url_c_});
  browsing_data::RemoveNavigationEntries(
      browser()->profile(),
      CreateDeletionInfo(base::Time(), base::Time::Now(), {url_b_}));
  ExpectEntries({about_blank_, url_a_, url_c_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, DeleteRange) {
  base::Time t1 = base::Time::Now();
  AddNavigations(browser(), {url_a_});
  base::Time t2 = base::Time::Now();
  AddNavigations(browser(), {url_b_});
  base::Time t3 = base::Time::Now();
  AddNavigations(browser(), {url_c_, url_d_});
  ASSERT_NE(t1, t2);
  ASSERT_NE(t2, t3);

  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         CreateDeletionInfo(t2, t3));
  ExpectEntries({about_blank_, url_a_, url_c_, url_d_}, GetEntries());
  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         CreateDeletionInfo(base::Time(), t1));
  ExpectEntries({url_a_, url_c_, url_d_}, GetEntries());
  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         CreateDeletionInfo(t3, base::Time()));
  ExpectEntries({url_a_, url_d_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, DeleteRangeRestricted) {
  base::Time t1 = base::Time::Now();
  AddNavigations(browser(), {url_a_});
  base::Time t2 = base::Time::Now();
  AddNavigations(browser(), {url_b_});
  base::Time t3 = base::Time::Now();
  AddNavigations(browser(), {url_c_, url_a_});
  ASSERT_NE(t1, t2);
  ASSERT_NE(t2, t3);

  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         CreateDeletionInfo(t1, t3, {url_b_}));
  ExpectEntries({about_blank_, url_a_, url_c_, url_a_}, GetEntries());
  browsing_data::RemoveNavigationEntries(
      browser()->profile(), CreateDeletionInfo(base::Time(), t2, {url_a_}));
  ExpectEntries({about_blank_, url_c_, url_a_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, DeleteAllAfterNavigation) {
  AddNavigations(browser(), {url_a_, url_b_, url_c_});
  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         DeletionInfo::ForAllHistory());
  ExpectEntries({url_c_}, GetEntries());

  AddNavigations(browser(), {url_d_});
  ExpectEntries({url_c_, url_d_}, GetEntries());

  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         DeletionInfo::ForAllHistory());
  ExpectEntries({url_d_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, TwoTabsDeletion) {
  AddNavigations(browser(), {url_a_, url_b_});
  AddTab(browser(), {url_c_, url_d_});
  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         DeletionInfo::ForAllHistory());

  ExpectEntries({url_b_, url_d_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, TwoWindowsDeletion) {
  AddNavigations(browser(), {url_a_, url_b_});
  AddBrowser(browser(), {url_c_, url_d_});

  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         DeletionInfo::ForAllHistory());

  ExpectEntries({url_b_, url_d_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, GoBackAndDelete) {
  AddNavigations(browser(), {url_a_, url_b_, url_c_});

  GoBack(browser()->tab_strip_model()->GetActiveWebContents());
  browsing_data::RemoveNavigationEntries(browser()->profile(),
                                         DeletionInfo::ForAllHistory());

  ExpectEntries({url_b_}, GetEntries());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, RecentTabDeletion) {
  AddNavigations(browser(), {url_a_, url_b_});
  AddTab(browser(), {url_c_});
  AddTab(browser(), {url_d_});
  chrome::CloseTab(browser());
  chrome::CloseTab(browser());

  sessions::TabRestoreService* tab_service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(2U, tab_service->entries().size());

  browsing_data::RemoveNavigationEntries(
      browser()->profile(),
      DeletionInfo::ForUrls({history::URLResult(url_c_, base::Time())}, {}));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1U, tab_service->entries().size());
  auto* tab = static_cast<sessions::TabRestoreService::Tab*>(
      tab_service->entries().front().get());
  EXPECT_EQ(url_d_, tab->navigations.front().virtual_url());
  EXPECT_TRUE(tab_service->IsLoaded());

  browsing_data::RemoveNavigationEntries(
      browser()->profile(),
      DeletionInfo::ForUrls({history::URLResult(url_d_, base::Time())}, {}));
  EXPECT_EQ(0U, tab_service->entries().size());
}

IN_PROC_BROWSER_TEST_F(NavigationEntryRemoverTest, RecentTabWindowDeletion) {
  // Create a new browser with three tabs and close it.
  AddBrowser(browser(), {url_a_});
  Browser* new_browser = BrowserList::GetInstance()->GetLastActive();
  AddTab(new_browser, {url_b_, url_c_});
  AddTab(new_browser, {url_d_});
  chrome::CloseWindow(new_browser);

  sessions::TabRestoreService* tab_service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(1U, tab_service->entries().size());
  ASSERT_EQ(sessions::TabRestoreService::WINDOW,
            tab_service->entries().front()->type);
  auto* window = static_cast<sessions::TabRestoreService::Window*>(
      tab_service->entries().front().get());
  EXPECT_EQ(3U, window->tabs.size());

  // Delete b and d. The last opened tab should be removed.
  browsing_data::RemoveNavigationEntries(
      browser()->profile(),
      DeletionInfo::ForUrls({history::URLResult(url_b_, base::Time()),
                             history::URLResult(url_d_, base::Time())},
                            {}));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1U, tab_service->entries().size());
  ASSERT_EQ(sessions::TabRestoreService::WINDOW,
            tab_service->entries().front()->type);
  window = static_cast<sessions::TabRestoreService::Window*>(
      tab_service->entries().front().get());
  EXPECT_EQ(2U, window->tabs.size());
  EXPECT_EQ(2U, window->tabs.size());
  EXPECT_EQ(1, window->selected_tab_index);
  EXPECT_EQ(0, window->tabs[0]->tabstrip_index);
  EXPECT_EQ(1, window->tabs[1]->tabstrip_index);
  EXPECT_EQ(url_a_, window->tabs[0]->navigations.front().virtual_url());
  EXPECT_EQ(url_c_, window->tabs[1]->navigations.front().virtual_url());

  // Delete a. The Window should be converted to a Tab.
  browsing_data::RemoveNavigationEntries(
      browser()->profile(),
      DeletionInfo::ForUrls({history::URLResult(url_a_, base::Time())}, {}));
  EXPECT_EQ(1U, tab_service->entries().size());
  ASSERT_EQ(sessions::TabRestoreService::TAB,
            tab_service->entries().front()->type);
  auto* tab = static_cast<sessions::TabRestoreService::Tab*>(
      tab_service->entries().front().get());
  EXPECT_EQ(url_c_, tab->navigations.front().virtual_url());
  EXPECT_EQ(0, tab->tabstrip_index);
}
