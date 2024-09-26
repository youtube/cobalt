// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/bookmark_counter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BookmarkCounterTest : public testing::Test {
 public:
  BookmarkCounterTest() {
    run_loop_ = std::make_unique<base::RunLoop>();

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    model_ =
        BookmarkModelFactory::GetInstance()->GetForBrowserContext(profile());
  }

  Profile* profile() { return profile_.get(); }

  bookmarks::BookmarkModel* model() { return model_; }

  browsing_data::BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

  void AddNodes(const std::string& model_string) {
    bookmarks::test::AddNodesFromModelString(
        model(), model()->bookmark_bar_node(), model_string);
  }

  void Callback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    DCHECK(result->Finished());
    finished_ = result->Finished();
    result_ = static_cast<browsing_data::BrowsingDataCounter::FinishedResult*>(
                  result.get())
                  ->Value();
    run_loop_->Quit();
  }

  void WaitForResult() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<base::RunLoop> run_loop_;

  raw_ptr<bookmarks::BookmarkModel> model_;
  bool finished_ = false;
  browsing_data::BrowsingDataCounter::ResultInt result_ = 0;
};

TEST_F(BookmarkCounterTest, CountUnloaded) {
  ASSERT_FALSE(model()->loaded());
  browsing_data::BookmarkCounter counter(model());
  counter.InitWithoutPref(base::Time(),
                          base::BindRepeating(&BookmarkCounterTest::Callback,
                                              base::Unretained(this)));
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(0, GetResult());
}

TEST_F(BookmarkCounterTest, Count) {
  bookmarks::test::WaitForBookmarkModelToLoad(model());
  ASSERT_TRUE(model()->loaded());
  AddNodes("1 2 3 ");
  browsing_data::BookmarkCounter counter(model());
  counter.InitWithoutPref(base::Time(),
                          base::BindRepeating(&BookmarkCounterTest::Callback,
                                              base::Unretained(this)));
  counter.Restart();
  EXPECT_EQ(3, GetResult());
  AddNodes("5 6 ");
  counter.Restart();
  EXPECT_EQ(5, GetResult());
}

TEST_F(BookmarkCounterTest, CountWithPeriod) {
  bookmarks::test::WaitForBookmarkModelToLoad(model());
  base::Time now = base::Time::Now();
  AddNodes("1 2 3 ");
  GURL url("https://google.com");
  const bookmarks::BookmarkNode* node1 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"4", url);
  model()->SetDateAdded(node1, now - base::Minutes(30));
  const bookmarks::BookmarkNode* node2 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"5", url);
  model()->SetDateAdded(node2, now - base::Minutes(90));

  browsing_data::BookmarkCounter counter(model());
  counter.InitWithoutPref(now - base::Minutes(60),
                          base::BindRepeating(&BookmarkCounterTest::Callback,
                                              base::Unretained(this)));
  counter.Restart();
  // 1,2,3 and 4 should be counted. 5 is too old, so it will be skipped.
  EXPECT_EQ(4, GetResult());
}

TEST_F(BookmarkCounterTest, CountWithFolders) {
  bookmarks::test::WaitForBookmarkModelToLoad(model());
  AddNodes("1 2 3 f1:[ 4 5 f2:[ 6 ] ] ");
  browsing_data::BookmarkCounter counter(model());
  counter.InitWithoutPref(base::Time(),
                          base::BindRepeating(&BookmarkCounterTest::Callback,
                                              base::Unretained(this)));
  counter.Restart();
  EXPECT_EQ(6, GetResult());
}

}  // namespace
