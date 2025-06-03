// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_list_tracker.h"

#include <limits.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/mock_downloads_page.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

using download::DownloadItem;
using download::MockDownloadItem;
using downloads::mojom::SafeBrowsingState;
using DownloadVector = std::vector<DownloadItem*>;
using testing::_;
using testing::Return;
using testing::ReturnRefOfCopy;

namespace {

bool ShouldShowItem(const DownloadItem& item) {
  DownloadItemModel model(const_cast<DownloadItem*>(&item));
  return model.ShouldShowInShelf();
}

}  // namespace

// A test version of DownloadsListTracker.
class TestDownloadsListTracker : public DownloadsListTracker {
 public:
  TestDownloadsListTracker(content::DownloadManager* manager,
                           mojo::PendingRemote<downloads::mojom::Page> page)
      : DownloadsListTracker(manager,
                             std::move(page),
                             base::BindRepeating(&ShouldShowItem)) {}
  ~TestDownloadsListTracker() override {}

  using DownloadsListTracker::IsIncognito;
  using DownloadsListTracker::GetItemForTesting;
  using DownloadsListTracker::SetChunkSizeForTesting;

 protected:
  downloads::mojom::DataPtr CreateDownloadData(
      download::DownloadItem* download_item) const override {
    auto file_value = downloads::mojom::Data::New();
    file_value->id = base::NumberToString(download_item->GetId());
    return file_value;
  }
};

// A fixture to test DownloadsListTracker.
class DownloadsListTrackerTest : public testing::Test {
 public:
  DownloadsListTrackerTest() {}

  ~DownloadsListTrackerTest() override {
    for (const auto& mock_item : mock_items_)
      testing::Mock::VerifyAndClear(mock_item.get());
  }

  // testing::Test:
  void SetUp() override {
    ON_CALL(manager_, GetBrowserContext()).WillByDefault(Return(&profile_));
    ON_CALL(manager_, GetAllDownloads(_)).WillByDefault(
        testing::Invoke(this, &DownloadsListTrackerTest::GetAllDownloads));
  }

  MockDownloadItem* CreateMock(uint64_t id, const base::Time& started) {
    MockDownloadItem* new_item = new testing::NiceMock<MockDownloadItem>();
    mock_items_.push_back(base::WrapUnique(new_item));

    ON_CALL(*new_item, GetId()).WillByDefault(Return(id));
    ON_CALL(*new_item, GetStartTime()).WillByDefault(Return(started));
    ON_CALL(*new_item, IsTransient()).WillByDefault(Return(false));
    ON_CALL(*new_item, GetTargetFilePath())
        .WillByDefault(
            ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo.txt"))));
    ON_CALL(*new_item, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL("https://example.test")));
    content::DownloadItemUtils::AttachInfoForTesting(new_item, profile(),
                                                     nullptr);

    return new_item;
  }

  MockDownloadItem* CreateNextItem() {
    return CreateMock(mock_items_.size(), base::Time::UnixEpoch() +
                                              base::Hours(mock_items_.size()));
  }

  void CreateTracker() {
    tracker_ = std::make_unique<TestDownloadsListTracker>(
        manager(), page_.BindAndGetRemote());
  }

  TestingProfile* profile() { return &profile_; }
  content::DownloadManager* manager() { return &manager_; }
  TestDownloadsListTracker* tracker() { return tracker_.get(); }

 protected:
  testing::StrictMock<MockPage> page_;

 private:
  void GetAllDownloads(DownloadVector* result) {
    for (const auto& mock_item : mock_items_)
      result->push_back(mock_item.get());
  }

  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  testing::NiceMock<content::MockDownloadManager> manager_;
  std::unique_ptr<TestDownloadsListTracker> tracker_;

  std::vector<std::unique_ptr<MockDownloadItem>> mock_items_;
};

TEST_F(DownloadsListTrackerTest, SetSearchTerms) {
  CreateTracker();

  std::vector<std::string> empty_terms;
  EXPECT_FALSE(tracker()->SetSearchTerms(empty_terms));

  std::vector<std::string> search_terms;
  search_terms.push_back("search");
  EXPECT_TRUE(tracker()->SetSearchTerms(search_terms));

  EXPECT_FALSE(tracker()->SetSearchTerms(search_terms));

  EXPECT_TRUE(tracker()->SetSearchTerms(empty_terms));

  // Notifying the page is left up to the handler in this case.
}

MATCHER_P(MatchIds, expected, "") {
  if (arg.size() != expected.size())
    return false;

  for (size_t i = 0; i < arg.size(); ++i) {
    if (arg[i]->id != base::NumberToString(expected[i]))
      return false;
  }
  return true;
}

TEST_F(DownloadsListTrackerTest, StartCallsInsertItems) {
  DownloadItem* first_item = CreateNextItem();

  CreateTracker();
  ASSERT_TRUE(tracker()->GetItemForTesting(0));

  tracker()->StartAndSendChunk();
  std::vector<uint64_t> expected = {first_item->GetId()};
  EXPECT_CALL(page_, InsertItems(0, MatchIds(expected)));
}

// The page is in a loading state until it gets an insertItems call. Ensure that
// happens even without downloads.
TEST_F(DownloadsListTrackerTest, EmptyGetAllItemsStillCallsInsertItems) {
  CreateTracker();

  ASSERT_FALSE(tracker()->GetItemForTesting(0));
  tracker()->StartAndSendChunk();

  std::vector<uint64_t> expected;
  EXPECT_CALL(page_, InsertItems(0, MatchIds(expected)));
}

TEST_F(DownloadsListTrackerTest, OnDownloadCreatedCallsInsertItems) {
  CreateTracker();
  EXPECT_CALL(page_, InsertItems(_, _));
  tracker()->StartAndSendChunk();

  ASSERT_FALSE(tracker()->GetItemForTesting(0));
  DownloadItem* first_item = CreateNextItem();
  tracker()->OnDownloadCreated(manager(), first_item);

  std::vector<uint64_t> expected = {first_item->GetId()};
  EXPECT_CALL(page_, InsertItems(0, MatchIds(expected)));
}

TEST_F(DownloadsListTrackerTest, OnDownloadRemovedCallsRemoveItem) {
  DownloadItem* first_item = CreateNextItem();

  CreateTracker();
  EXPECT_CALL(page_, InsertItems(_, _));
  tracker()->StartAndSendChunk();

  EXPECT_TRUE(tracker()->GetItemForTesting(0));
  tracker()->OnDownloadRemoved(manager(), first_item);
  EXPECT_FALSE(tracker()->GetItemForTesting(0));

  EXPECT_CALL(page_, RemoveItem(0));
}

TEST_F(DownloadsListTrackerTest, OnDownloadUpdatedCallsRemoveItem) {
  DownloadItem* first_item = CreateNextItem();

  CreateTracker();
  EXPECT_CALL(page_, InsertItems(_, _));
  tracker()->StartAndSendChunk();

  EXPECT_TRUE(tracker()->GetItemForTesting(0));

  DownloadItemModel(first_item).SetShouldShowInShelf(false);
  tracker()->OnDownloadUpdated(manager(), first_item);

  EXPECT_FALSE(tracker()->GetItemForTesting(0));

  EXPECT_CALL(page_, RemoveItem(0));
}

TEST_F(DownloadsListTrackerTest, StartExcludesHiddenItems) {
  DownloadItem* first_item = CreateNextItem();
  DownloadItemModel(first_item).SetShouldShowInShelf(false);

  CreateTracker();
  tracker()->StartAndSendChunk();

  std::vector<uint64_t> expected;
  EXPECT_CALL(page_, InsertItems(0, MatchIds(expected)));
}

TEST_F(DownloadsListTrackerTest, Incognito) {
  testing::NiceMock<content::MockDownloadManager> incognito_manager;
  ON_CALL(incognito_manager, GetBrowserContext()).WillByDefault(Return(
      TestingProfile::Builder().BuildIncognito(profile())));

  MockDownloadItem item;
  EXPECT_CALL(item, GetId()).WillRepeatedly(Return(0));

  ON_CALL(incognito_manager, GetDownload(0)).WillByDefault(Return(&item));

  testing::StrictMock<MockPage> page;
  TestDownloadsListTracker tracker(&incognito_manager, page.BindAndGetRemote());
  EXPECT_TRUE(tracker.IsIncognito(item));
}

TEST_F(DownloadsListTrackerTest, OnlySendSomeItems) {
  CreateNextItem();
  CreateNextItem();
  CreateNextItem();
  CreateNextItem();
  CreateNextItem();

  CreateTracker();
  tracker()->SetChunkSizeForTesting(3);
  {
    tracker()->StartAndSendChunk();
    std::vector<uint64_t> expected = {4, 3, 2};
    EXPECT_CALL(page_, InsertItems(0, MatchIds(expected)));
  }
  {
    tracker()->StartAndSendChunk();
    std::vector<uint64_t> expected = {1, 0};
    EXPECT_CALL(page_, InsertItems(3, MatchIds(expected)));
  }
}

TEST_F(DownloadsListTrackerTest, IgnoreUnsentItemUpdates) {
  DownloadItem* unsent_item = CreateNextItem();
  CreateNextItem();

  CreateTracker();
  tracker()->SetChunkSizeForTesting(1);
  tracker()->StartAndSendChunk();

  std::vector<uint64_t> expected = {1};
  EXPECT_CALL(page_, InsertItems(0, MatchIds(expected)));

  // Does not send an update. StrictMock ensures no methods called on |page_|.
  tracker()->OnDownloadUpdated(manager(), unsent_item);
}

TEST_F(DownloadsListTrackerTest, IgnoreUnsentItemRemovals) {
  DownloadItem* unsent_item = CreateNextItem();
  CreateNextItem();

  CreateTracker();
  tracker()->SetChunkSizeForTesting(1);
  tracker()->StartAndSendChunk();

  std::vector<uint64_t> expected = {1};
  EXPECT_CALL(page_, InsertItems(0, MatchIds(expected)));

  // Does not send an update. StrictMock ensures no methods called on |page_|.
  DownloadItemModel(unsent_item).SetShouldShowInShelf(false);
  tracker()->OnDownloadUpdated(manager(), unsent_item);

  // Does not send an update. StrictMock ensures no methods called on |page_|.
  DownloadItemModel(unsent_item).SetShouldShowInShelf(true);
  tracker()->OnDownloadUpdated(manager(), unsent_item);
}

TEST_F(DownloadsListTrackerTest, IgnoreTransientDownloads) {
  MockDownloadItem* transient_item = CreateNextItem();
  ON_CALL(*transient_item, IsTransient()).WillByDefault(Return(true));

  CreateTracker();
  tracker()->StartAndSendChunk();

  std::vector<uint64_t> expected;
  EXPECT_CALL(page_, InsertItems(0, MatchIds(expected)));
}

TEST_F(DownloadsListTrackerTest,
       CreateDownloadData_UrlFormatting_OmitUserPass) {
  MockDownloadItem* item = CreateNextItem();
  ON_CALL(*item, GetURL())
      .WillByDefault(ReturnRefOfCopy(GURL("https://user:pass@example.test")));

  auto tracker = std::make_unique<DownloadsListTracker>(
      manager(), page_.BindAndGetRemote());

  downloads::mojom::DataPtr data = tracker->CreateDownloadData(item);
  EXPECT_TRUE(data->url);
  EXPECT_EQ(*data->url, "https://user:pass@example.test/");
  EXPECT_EQ(data->display_url, u"https://example.test");
}

TEST_F(DownloadsListTrackerTest, CreateDownloadData_UrlFormatting_Idn) {
  MockDownloadItem* item = CreateNextItem();
  ON_CALL(*item, GetURL())
      .WillByDefault(ReturnRefOfCopy(GURL("https://xn--6qqa088eba.test")));

  auto tracker = std::make_unique<DownloadsListTracker>(
      manager(), page_.BindAndGetRemote());

  downloads::mojom::DataPtr data = tracker->CreateDownloadData(item);
  EXPECT_TRUE(data->url);
  EXPECT_EQ(*data->url, "https://xn--6qqa088eba.test/");
  EXPECT_EQ(data->display_url, u"https://\u4f60\u597d\u4f60\u597d.test");
}

TEST_F(DownloadsListTrackerTest, CreateDownloadData_UrlFormatting_VeryLong) {
  std::string url = "https://" + std::string(2 * 1024 * 1024, 'a') + ".test";
  std::u16string expected =
      u"https://" + std::u16string(2 * 1024 * 1024 - 8, 'a');

  MockDownloadItem* item = CreateNextItem();
  ON_CALL(*item, GetURL()).WillByDefault(ReturnRefOfCopy(GURL(url)));

  auto tracker = std::make_unique<DownloadsListTracker>(
      manager(), page_.BindAndGetRemote());

  downloads::mojom::DataPtr data = tracker->CreateDownloadData(item);
  EXPECT_FALSE(data->url);
  EXPECT_EQ(data->display_url, expected);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
TEST_F(DownloadsListTrackerTest, CreateDownloadData_SafeBrowsing) {
  auto tracker = std::make_unique<DownloadsListTracker>(
      manager(), page_.BindAndGetRemote());

  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  {
    MockDownloadItem* item = CreateNextItem();

    downloads::mojom::DataPtr data = tracker->CreateDownloadData(item);
    EXPECT_EQ(data->safe_browsing_state,
              SafeBrowsingState::kStandardProtection);
    EXPECT_FALSE(data->has_safe_browsing_verdict);
  }

  // Add a Safe Browsing verdict.
  {
    MockDownloadItem* item = CreateNextItem();
    safe_browsing::DownloadProtectionService::SetDownloadProtectionData(
        item, "token", safe_browsing::ClientDownloadResponse::Verdict(),
        safe_browsing::ClientDownloadResponse::TailoredVerdict());

    downloads::mojom::DataPtr data = tracker->CreateDownloadData(item);
    EXPECT_EQ(data->safe_browsing_state,
              SafeBrowsingState::kStandardProtection);
    EXPECT_TRUE(data->has_safe_browsing_verdict);

    // Now turn off Safe Browsing on the profile. The DownloadsListTracker
    // should not assume that there's no verdict. (crbug.com/1499703)
    profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
    data = tracker->CreateDownloadData(item);
    EXPECT_EQ(data->safe_browsing_state, SafeBrowsingState::kNoSafeBrowsing);
    EXPECT_TRUE(data->has_safe_browsing_verdict);
  }

  // Enable Enhanced Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  {
    MockDownloadItem* item = CreateNextItem();

    downloads::mojom::DataPtr data = tracker->CreateDownloadData(item);
    EXPECT_EQ(data->safe_browsing_state,
              SafeBrowsingState::kStandardProtection);
    EXPECT_FALSE(data->has_safe_browsing_verdict);
  }

  // Disable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  {
    MockDownloadItem* item = CreateNextItem();

    downloads::mojom::DataPtr data = tracker->CreateDownloadData(item);
    EXPECT_EQ(data->safe_browsing_state, SafeBrowsingState::kNoSafeBrowsing);
    EXPECT_FALSE(data->has_safe_browsing_verdict);
  }

  // Make Safe Browsing disabled by policy.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled,
      base::Value::ToUniquePtrValue(base::Value(false)));
  {
    MockDownloadItem* item = CreateNextItem();

    downloads::mojom::DataPtr data = tracker->CreateDownloadData(item);
    EXPECT_EQ(data->safe_browsing_state, SafeBrowsingState::kNoSafeBrowsing);
    EXPECT_FALSE(data->has_safe_browsing_verdict);
  }
}
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
