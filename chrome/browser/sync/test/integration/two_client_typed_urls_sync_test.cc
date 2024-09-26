// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/big_endian.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

using base::ASCIIToUTF16;
using bookmarks::BookmarkNode;
using typed_urls_helper::AddUrlToHistory;
using typed_urls_helper::AddUrlToHistoryWithTimestamp;
using typed_urls_helper::AddUrlToHistoryWithTransition;
using typed_urls_helper::AreVisitsEqual;
using typed_urls_helper::AreVisitsUnique;
using typed_urls_helper::CheckSyncHasMetadataForURLID;
using typed_urls_helper::CheckSyncHasURLMetadata;
using typed_urls_helper::CheckURLRowVectorsAreEqualForTypedURLs;
using typed_urls_helper::DeleteUrlFromHistory;
using typed_urls_helper::ExpireHistoryBefore;
using typed_urls_helper::ExpireHistoryBetween;
using typed_urls_helper::GetAllSyncMetadata;
using typed_urls_helper::GetTypedUrlsFromClient;
using typed_urls_helper::GetUrlFromClient;
using typed_urls_helper::GetVisitsFromClient;
using typed_urls_helper::WriteMetadataToClient;

namespace {
const char kDummyUrl[] = "http://dummy-history.google.com/";
}  // namespace

// TODO(crbug.com/1365291): Evaluate which of these tests should be kept after
// kSyncEnableHistoryDataType is enabled and HISTORY has replaced TYPED_URLS.

class TwoClientTypedUrlsSyncTest : public SyncTest {
 public:
  TwoClientTypedUrlsSyncTest() : SyncTest(TWO_CLIENT) {}

  TwoClientTypedUrlsSyncTest(const TwoClientTypedUrlsSyncTest&) = delete;
  TwoClientTypedUrlsSyncTest& operator=(const TwoClientTypedUrlsSyncTest&) =
      delete;

  ~TwoClientTypedUrlsSyncTest() override = default;

  ::testing::AssertionResult CheckClientsEqual() {
    history::URLRows urls = GetTypedUrlsFromClient(0);
    history::URLRows urls2 = GetTypedUrlsFromClient(1);
    if (!CheckURLRowVectorsAreEqualForTypedURLs(urls, urls2)) {
      return ::testing::AssertionFailure() << "URLVectors are not equal";
    }
    // Now check the visits.
    for (size_t i = 0; i < urls.size() && i < urls2.size(); i++) {
      history::VisitVector visit1 = GetVisitsFromClient(0, urls[i].id());
      history::VisitVector visit2 = GetVisitsFromClient(1, urls2[i].id());
      if (!AreVisitsEqual(visit1, visit2)) {
        return ::testing::AssertionFailure() << "Visits are not equal";
      }
    }
    return ::testing::AssertionSuccess();
  }

  bool CheckNoDuplicateVisits() {
    for (int i = 0; i < num_clients(); ++i) {
      history::URLRows urls = GetTypedUrlsFromClient(i);
      for (const history::URLRow& url : urls) {
        history::VisitVector visits = GetVisitsFromClient(i, url.id());
        if (!AreVisitsUnique(visits)) {
          return false;
        }
      }
    }
    return true;
  }

  int GetVisitCountForFirstURL(int index) {
    history::URLRows urls = GetTypedUrlsFromClient(index);
    if (urls.empty()) {
      return 0;
    } else {
      return urls[0].visit_count();
    }
  }
};

class TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest
    : public TwoClientTypedUrlsSyncTest {
 public:
  TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest() {
    features_.InitAndDisableFeature(syncer::kSyncEnableHistoryDataType);
  }

  ~TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest() override = default;

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsSyncTest, E2E_ENABLED(Add)) {
  ResetSyncForPrimaryAccount();
  // Use a randomized URL to prevent test collisions.
  const std::u16string kHistoryUrl = ASCIIToUTF16(base::StringPrintf(
      "http://www.add-history.google.com/%s",
      base::Uuid::GenerateRandomV4().AsLowercaseString().c_str()));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  size_t initial_count = GetTypedUrlsFromClient(0).size();

  // Populate one client with a URL, wait for it to sync to the other.
  GURL new_url(kHistoryUrl);
  AddUrlToHistory(0, new_url);
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  // Assert that the second client has the correct new URL.
  history::URLRows urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(initial_count + 1, urls.size());
  ASSERT_EQ(new_url, urls.back().url());
}

// Doesn't work with HISTORY because of CheckSyncHasURLMetadata().
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       AddExpired) {
  const std::u16string kHistoryUrl(u"http://www.add-one-history.google.com/");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate one client with a URL, should sync to the other.
  GURL new_url(kHistoryUrl);
  // Create a URL with a timestamp 1 year before today.
  base::Time timestamp = base::Time::Now() - base::Days(365);
  AddUrlToHistoryWithTimestamp(0, new_url, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, timestamp);
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());

  // Let sync finish.
  // Add a dummy url and sync it.
  AddUrlToHistory(0, GURL(kDummyUrl));
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(2U, urls.size());
  ASSERT_TRUE(TypedURLChecker(1, kDummyUrl).Wait());

  // Second client should only have dummy URL since kHistoryUrl is expired.
  urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(GURL(kDummyUrl), urls.back().url());
  EXPECT_TRUE(CheckSyncHasURLMetadata(1, GURL(kDummyUrl)));

  // Sync on both clients should not receive expired visits.
  EXPECT_FALSE(CheckSyncHasURLMetadata(0, new_url));
  EXPECT_FALSE(CheckSyncHasURLMetadata(1, new_url));
}

// Doesn't work with HISTORY because of CheckSyncHasURLMetadata().
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       AddExpiredThenUpdate) {
  const std::u16string kHistoryUrl(u"http://www.add-one-history.google.com/");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate one client with a URL, should sync to the other.
  GURL new_url(kHistoryUrl);
  // Create a URL with a timestamp 1 year before today.
  base::Time timestamp = base::Time::Now() - base::Days(365);
  AddUrlToHistoryWithTimestamp(0, new_url, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, timestamp);
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());

  // Let sync finish.
  // Add a dummy url and sync it.
  AddUrlToHistory(0, GURL(kDummyUrl));
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(2U, urls.size());
  ASSERT_TRUE(TypedURLChecker(1, kDummyUrl).Wait());

  // Second client should only have dummy URL since kHistoryUrl is expired.
  urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(1U, urls.size());
  EXPECT_TRUE(CheckSyncHasURLMetadata(0, GURL(kDummyUrl)));

  // Sync should not receive expired visits.
  EXPECT_FALSE(CheckSyncHasURLMetadata(0, new_url));

  // Now drive an update on the first client.
  AddUrlToHistory(0, new_url);

  // Let sync finish again.
  ASSERT_TRUE(TypedURLChecker(1, new_url.spec()).Wait());

  // Second client should have kHistoryUrl now.
  urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(2U, urls.size());

  // Sync should receive the new visit.
  EXPECT_TRUE(CheckSyncHasURLMetadata(0, new_url));
  EXPECT_TRUE(CheckSyncHasURLMetadata(1, new_url));
}

// Doesn't work with HISTORY because of CheckSyncHasURLMetadata().
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       AddThenExpireOnSecondClient) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  base::Time now = base::Time::Now();

  // Populate one client with a URL, should sync to the other.
  GURL url("http://www.add-one-history.google.com/");
  base::Time insertion_time = now - base::Days(1);
  AddUrlToHistoryWithTimestamp(0, url, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, insertion_time);
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(url, urls[0].url());
  history::URLID url_id_on_first_client = urls[0].id();

  // Wait for sync to finish.
  ASSERT_TRUE(TypedURLChecker(1, url.spec()).Wait());

  // Second client should have the url.
  ASSERT_EQ(1U, GetTypedUrlsFromClient(1).size());
  EXPECT_TRUE(CheckSyncHasURLMetadata(1, url));

  // Expire the url on the second client.
  ExpireHistoryBefore(1, insertion_time + base::Seconds(1));

  // The data and the metadata should be gone on the second client.
  ASSERT_EQ(0U, GetTypedUrlsFromClient(1).size());
  EXPECT_FALSE(CheckSyncHasURLMetadata(1, url));

  // Let sync finish; Add a dummy url to the second client and sync it.
  AddUrlToHistory(1, GURL(kDummyUrl));
  ASSERT_EQ(1U, GetTypedUrlsFromClient(1).size());
  ASSERT_TRUE(TypedURLChecker(0, kDummyUrl).Wait());

  // The expiration should not get synced up, the first client still has the
  // URL (and also the dummy URL)
  ASSERT_EQ(2U, GetTypedUrlsFromClient(0).size());
  EXPECT_TRUE(CheckSyncHasMetadataForURLID(0, url_id_on_first_client));
}

// Doesn't work with HISTORY because of CheckSyncHasURLMetadata().
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       AddThenExpireThenAddAgain) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  base::Time now = base::Time::Now();

  // Populate one client with a URL, should sync to the other.
  GURL url("http://www.add-one-history.google.com/");
  base::Time insertion_time = now - base::Days(1);
  AddUrlToHistoryWithTimestamp(0, url, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, insertion_time);
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(url, urls[0].url());
  history::URLID url_id_on_first_client = urls[0].id();

  // Wait for sync to finish.
  ASSERT_TRUE(TypedURLChecker(1, url.spec()).Wait());

  // Second client should have the url.
  urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(1U, urls.size());
  EXPECT_TRUE(CheckSyncHasURLMetadata(0, url));

  // Expire the url on the first client.
  ExpireHistoryBefore(0, insertion_time + base::Seconds(1));

  // The data and the metadata should be gone on the first client.
  ASSERT_EQ(0U, GetTypedUrlsFromClient(0).size());
  EXPECT_FALSE(CheckSyncHasMetadataForURLID(0, url_id_on_first_client));

  // Let sync finish.
  // Add a dummy url and sync it.
  AddUrlToHistory(0, GURL(kDummyUrl));
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_TRUE(TypedURLChecker(1, kDummyUrl).Wait());

  // The expiration should not get synced up, the second client still has the
  // URL.
  urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(2U, urls.size());
  EXPECT_TRUE(CheckSyncHasURLMetadata(1, url));

  // The first client can add the URL again (regression test for
  // https://crbug.com/827111).
  AddUrlToHistoryWithTransition(0, url, ui::PAGE_TRANSITION_TYPED,
                                history::SOURCE_BROWSED);
  urls = GetTypedUrlsFromClient(0);
  EXPECT_EQ(2U, urls.size());
  EXPECT_TRUE(CheckSyncHasURLMetadata(0, url));
}

// Doesn't work with HISTORY because of CheckSyncHasURLMetadata().
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       AddThenExpireVisitByVisit) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  base::Time now = base::Time::Now();

  // Populate one client with a URL (with three visits), should sync to the
  // other. First non-typed, then typed, then non-typed again.
  GURL url("http://www.add-one-history.google.com/");
  base::Time insertion_time = now - base::Days(6);
  base::Time second_typed_visit_time = now - base::Days(5);
  base::Time third_link_visit_time = now - base::Days(4);
  base::Time dummy_visit_1 = now - base::Days(3);
  base::Time dummy_visit_2 = now - base::Days(2);
  base::Time dummy_visit_3 = now - base::Days(1);
  AddUrlToHistoryWithTimestamp(0, url, ui::PAGE_TRANSITION_LINK,
                               history::SOURCE_BROWSED, insertion_time);
  AddUrlToHistoryWithTimestamp(0, url, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED,
                               second_typed_visit_time);
  AddUrlToHistoryWithTimestamp(0, url, ui::PAGE_TRANSITION_LINK,
                               history::SOURCE_BROWSED, third_link_visit_time);
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(url, urls[0].url());
  history::URLID url_id_on_first_client = urls[0].id();

  // Wait for sync to finish.
  ASSERT_TRUE(TypedURLChecker(1, url.spec()).Wait());

  // Second client should have the url.
  ASSERT_EQ(1U, GetTypedUrlsFromClient(1).size());
  EXPECT_TRUE(CheckSyncHasURLMetadata(1, url));

  // Expire the first (non-typed) visit on the first client and assert both data
  // and metadata are intact.
  ExpireHistoryBefore(0, insertion_time + base::Seconds(1));
  ASSERT_EQ(1U, GetTypedUrlsFromClient(0).size());
  EXPECT_TRUE(CheckSyncHasMetadataForURLID(0, url_id_on_first_client));

  // Force a sync cycle (add a dummy typed url and sync it) and check the second
  // client still has the original URL (plus the dummy one).
  AddUrlToHistoryWithTimestamp(0, GURL(kDummyUrl), ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, dummy_visit_1);
  ASSERT_EQ(2U, GetTypedUrlsFromClient(0).size());
  ASSERT_TRUE(TypedURLChecker(1, kDummyUrl).Wait());
  ASSERT_EQ(2U, GetTypedUrlsFromClient(1).size());
  EXPECT_TRUE(CheckSyncHasURLMetadata(1, url));

  // Expire the second (typed) visit on the first client and assert both data
  // and metadata for the URL are gone.
  ExpireHistoryBefore(0, second_typed_visit_time + base::Seconds(1));
  std::vector<history::URLRow> pruned_urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, pruned_urls.size());
  ASSERT_EQ(GURL(kDummyUrl), pruned_urls[0].url());
  EXPECT_FALSE(CheckSyncHasMetadataForURLID(0, url_id_on_first_client));

  // Force a sync cycle (add another visit to the dummy url and sync it).
  AddUrlToHistoryWithTimestamp(0, GURL(kDummyUrl), ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, dummy_visit_2);
  ASSERT_TRUE(TypedURLChecker(1, kDummyUrl).Wait());

  // The expiration should not get synced up, the second client still has the
  // URL (and also the dummy one).
  ASSERT_EQ(2U, GetTypedUrlsFromClient(1).size());
  EXPECT_TRUE(CheckSyncHasURLMetadata(1, url));

  // Now expire also the last non-typed visit (make sure it has no impact).
  ExpireHistoryBefore(0, third_link_visit_time + base::Seconds(1));
  ASSERT_EQ(1U, GetTypedUrlsFromClient(0).size());
  EXPECT_FALSE(CheckSyncHasMetadataForURLID(0, url_id_on_first_client));

  // Force a sync cycle (add another visit to the dummy url and sync it) and
  // check the second client still has the same state.
  AddUrlToHistoryWithTimestamp(0, GURL(kDummyUrl), ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, dummy_visit_3);
  ASSERT_TRUE(TypedURLChecker(1, kDummyUrl).Wait());
  ASSERT_EQ(2U, GetTypedUrlsFromClient(1).size());
  EXPECT_TRUE(CheckSyncHasURLMetadata(1, url));
}

// Doesn't work with HISTORY because DeleteUrlFromHistory() deletes only
// locally (doesn't send a delete directive).
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       E2E_ENABLED(AddThenDelete)) {
  ResetSyncForPrimaryAccount();
  // Use a randomized URL to prevent test collisions.
  const std::u16string kHistoryUrl = ASCIIToUTF16(base::StringPrintf(
      "http://www.add-history.google.com/%s",
      base::Uuid::GenerateRandomV4().AsLowercaseString().c_str()));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  size_t initial_count = GetTypedUrlsFromClient(0).size();

  // Populate one client with a URL, wait for it to sync to the other.
  GURL new_url(kHistoryUrl);
  AddUrlToHistory(0, new_url);
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  // Assert that the second client has the correct new URL.
  history::URLRows urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(initial_count + 1, urls.size());
  ASSERT_EQ(new_url, urls.back().url());

  // Delete from first client, and wait for them to sync.
  DeleteUrlFromHistory(0, new_url);
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  // Assert that it's deleted from the second client.
  ASSERT_EQ(initial_count, GetTypedUrlsFromClient(1).size());
}

// Doesn't work with HISTORY because it uses ExpireHistoryBetween() (rather than
// DeleteLocalAndRemoteHistoryBetween()).
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       AddMultipleVisitsThenDeleteAllTypedVisits) {
  const std::u16string kHistoryUrl(u"http://history1.google.com/");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  base::Time now = base::Time::Now();
  base::Time insertion_time = now - base::Days(2);
  base::Time visit_time = now - base::Days(1);

  // Populate one client with a URL with multiple visits, wait for it to sync to
  // the other.
  GURL new_url(kHistoryUrl);
  AddUrlToHistoryWithTimestamp(0, new_url, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, insertion_time);
  AddUrlToHistoryWithTimestamp(0, new_url, ui::PAGE_TRANSITION_LINK,
                               history::SOURCE_BROWSED, visit_time);
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  // Assert that the second client has the correct new URL.
  history::URLRows urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(new_url, urls[0].url());

  // Delete the only typed visit from the first client, and wait for them to
  // sync.
  ExpireHistoryBetween(0, insertion_time - base::Seconds(1),
                       insertion_time + base::Seconds(1));
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  // Assert that it's deleted from the second client.
  ASSERT_EQ(0u, GetTypedUrlsFromClient(1).size());
}

// Doesn't work with HISTORY because that doesn't sync retroactively.
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       DisableEnableSync) {
  ResetSyncForPrimaryAccount();
  const std::u16string kUrl1(u"http://history1.google.com/");
  const std::u16string kUrl2(u"http://history2.google.com/");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Disable history sync for one client, leave it active for the other.
  GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kHistory);

  // Add one URL to non-syncing client, add a different URL to the other,
  // wait for sync cycle to complete. No data should be exchanged.
  GURL url1(kUrl1);
  GURL url2(kUrl2);
  AddUrlToHistory(0, url1);
  AddUrlToHistory(1, url2);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(1)).Wait());

  // Make sure that no data was exchanged.
  history::URLRows post_sync_urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, post_sync_urls.size());
  ASSERT_EQ(url1, post_sync_urls[0].url());
  post_sync_urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(1U, post_sync_urls.size());
  ASSERT_EQ(url2, post_sync_urls[0].url());

  // Enable history sync, make both URLs are synced to each client.
  GetClient(0)->EnableSyncForType(syncer::UserSelectableType::kHistory);

  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());
}

// Doesn't work with HISTORY because DeleteUrlFromHistory() deletes only
// locally (doesn't send a delete directive).
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       AddOneDeleteOther) {
  const std::u16string kHistoryUrl(
      u"http://www.add-one-delete-history.google.com/");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate one client with a URL, should sync to the other.
  GURL new_url(kHistoryUrl);
  AddUrlToHistory(0, new_url);
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());

  // Both clients should have this URL.
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  // Now, delete the URL from the second client.
  DeleteUrlFromHistory(1, new_url);
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());

  // Both clients should have this URL removed.
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());
}

// Doesn't work with HISTORY because DeleteUrlFromHistory() deletes only
// locally (doesn't send a delete directive).
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       AddOneDeleteOtherAddAgain) {
  const std::u16string kHistoryUrl(
      u"http://www.add-delete-add-history.google.com/");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate one client with a URL, should sync to the other.
  GURL new_url(kHistoryUrl);
  AddUrlToHistory(0, new_url);
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());

  // Both clients should have this URL.
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  // Now, delete the URL from the second client.
  DeleteUrlFromHistory(1, new_url);
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());

  // Both clients should have this URL removed.
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  // Add it to the first client again, should succeed (tests that the deletion
  // properly disassociates that URL).
  AddUrlToHistory(0, new_url);

  // Both clients should have this URL added again.
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());
}

// Doesn't work with HISTORY because that doesn't sync retroactively.
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       MergeTypedWithNonTypedDuringAssociation) {
  ASSERT_TRUE(SetupClients());
  GURL new_url("http://history.com");
  base::Time timestamp = base::Time::Now();
  // Put a non-typed URL in both clients with an identical timestamp.
  // Then add a typed URL to the second client - this test makes sure that
  // we properly merge both sets of visits together to end up with the same
  // set of visits on both ends.
  AddUrlToHistoryWithTimestamp(0, new_url, ui::PAGE_TRANSITION_LINK,
                               history::SOURCE_BROWSED, timestamp);
  AddUrlToHistoryWithTimestamp(1, new_url, ui::PAGE_TRANSITION_LINK,
                               history::SOURCE_BROWSED, timestamp);
  AddUrlToHistoryWithTimestamp(1, new_url, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED,
                               timestamp + base::Seconds(1));

  // Now start up sync - URLs should get merged. Fully sync client 1 first,
  // before syncing client 0, so we have both of client 1's URLs in the sync DB
  // at the time that client 0 does model association.
  ASSERT_TRUE(GetClient(1)->SetupSync()) << "SetupSync() failed";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(1)).Wait());
  ASSERT_TRUE(GetClient(0)->SetupSync()) << "SetupSync() failed";
  ASSERT_TRUE(TypedURLChecker(1, new_url.spec()).Wait());

  ASSERT_TRUE(CheckClientsEqual());
  // At this point, we should have no duplicates (total visit count should be
  // 2). We only need to check client 0 since we already verified that both
  // clients are identical above.
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());
  ASSERT_TRUE(CheckNoDuplicateVisits());
  ASSERT_EQ(2, GetVisitCountForFirstURL(0));
}

// Tests transitioning a URL from non-typed to typed when both clients
// have already seen that URL (so a merge is required).
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsSyncTest,
                       MergeTypedWithNonTypedDuringChangeProcessing) {
  ASSERT_TRUE(SetupClients());
  GURL new_url("http://history.com");
  base::Time timestamp = base::Time::Now();

  AddUrlToHistoryWithTimestamp(0, new_url, ui::PAGE_TRANSITION_LINK,
                               history::SOURCE_BROWSED, timestamp);
  AddUrlToHistoryWithTimestamp(1, new_url, ui::PAGE_TRANSITION_LINK,
                               history::SOURCE_BROWSED, timestamp);

  // Now start up sync. Neither URL should get synced as they do not look like
  // typed URLs.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckClientsEqual());
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());

  // Now, add a typed visit to the first client.
  AddUrlToHistoryWithTimestamp(0, new_url, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED,
                               timestamp + base::Seconds(1));

  ASSERT_TRUE(TypedURLChecker(1, new_url.spec()).Wait());
  ASSERT_TRUE(CheckClientsEqual());
  ASSERT_TRUE(CheckNoDuplicateVisits());
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(2, GetVisitCountForFirstURL(0));
  ASSERT_EQ(2, GetVisitCountForFirstURL(1));
}

// Tests transitioning a URL from non-typed to typed when one of the clients
// has never seen that URL before (so no merge is necessary).
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsSyncTest, UpdateToNonTypedURL) {
  const std::u16string kHistoryUrl(
      u"http://www.add-delete-add-history.google.com/");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate one client with a non-typed URL, should not be synced.
  GURL new_url(kHistoryUrl);
  AddUrlToHistoryWithTransition(0, new_url, ui::PAGE_TRANSITION_LINK,
                                history::SOURCE_BROWSED);
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());

  // Both clients should have 0 typed URLs.
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());

  // Now, add a typed visit to this URL.
  AddUrlToHistory(0, new_url);

  // Let sync finish.
  ASSERT_TRUE(TypedURLChecker(1, new_url.spec()).Wait());

  // Both clients should have this URL as typed and have two visits synced up.
  ASSERT_TRUE(CheckClientsEqual());
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());
  ASSERT_EQ(2, GetVisitCountForFirstURL(0));
}

// Doesn't work with HISTORY because that *does* sync non-typed visits.
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       E2E_ENABLED(DontSyncUpdatedNonTypedURLs)) {
  ResetSyncForPrimaryAccount();
  // Checks if a non-typed URL that has been updated (modified) doesn't get
  // synced. This is a regression test after fixing a bug where adding a
  // non-typed URL was guarded against but later modifying it was not. Since
  // "update" is "update or create if missing", non-typed URLs were being
  // created.
  const GURL kNonTypedURL("http://link.google.com/");
  const GURL kTypedURL("http://typed.google.com/");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  AddUrlToHistoryWithTransition(0, kNonTypedURL, ui::PAGE_TRANSITION_LINK,
                                history::SOURCE_BROWSED);
  AddUrlToHistoryWithTransition(0, kTypedURL, ui::PAGE_TRANSITION_TYPED,
                                history::SOURCE_BROWSED);

  // Modify the non-typed URL. It should not get synced.
  typed_urls_helper::SetPageTitle(0, kNonTypedURL, "Welcome to Non-Typed URL");
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  history::VisitVector visits;
  // First client has both visits.
  visits = typed_urls_helper::GetVisitsForURLFromClient(0, kNonTypedURL);
  ASSERT_EQ(1U, visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(visits[0].transition,
                                           ui::PAGE_TRANSITION_LINK));
  visits = typed_urls_helper::GetVisitsForURLFromClient(0, kTypedURL);
  ASSERT_EQ(1U, visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(visits[0].transition,
                                           ui::PAGE_TRANSITION_TYPED));
  // Second client has only the typed visit.
  visits = typed_urls_helper::GetVisitsForURLFromClient(1, kNonTypedURL);
  ASSERT_EQ(0U, visits.size());
  visits = typed_urls_helper::GetVisitsForURLFromClient(1, kTypedURL);
  ASSERT_EQ(1U, visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(visits[0].transition,
                                           ui::PAGE_TRANSITION_TYPED));
}

IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsSyncTest,
                       E2E_ENABLED(SyncTypedRedirects)) {
  ResetSyncForPrimaryAccount();
  const std::u16string kHistoryUrl(u"http://typed.google.com/");
  const std::u16string kRedirectedHistoryUrl(u"http://www.typed.google.com/");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Simulate a typed address that gets redirected by the server to a different
  // address.
  GURL initial_url(kHistoryUrl);
  const ui::PageTransition initial_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_CHAIN_START);
  AddUrlToHistoryWithTransition(0, initial_url, initial_transition,
                                history::SOURCE_BROWSED);

  GURL redirected_url(kRedirectedHistoryUrl);
  const ui::PageTransition redirected_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_CHAIN_END |
      ui::PAGE_TRANSITION_SERVER_REDIRECT);
  // This address will have a typed_count == 0 because it's a redirection.
  // It should still be synced.
  AddUrlToHistoryWithTransition(0, redirected_url, redirected_transition,
                                history::SOURCE_BROWSED);

  // Both clients should have both URLs.
  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());

  history::VisitVector visits =
      typed_urls_helper::GetVisitsForURLFromClient(0, initial_url);
  ASSERT_EQ(1U, visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(visits[0].transition,
                                           ui::PAGE_TRANSITION_TYPED));
  visits = typed_urls_helper::GetVisitsForURLFromClient(0, redirected_url);
  ASSERT_EQ(1U, visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(visits[0].transition,
                                           ui::PAGE_TRANSITION_TYPED));

  visits = typed_urls_helper::GetVisitsForURLFromClient(1, initial_url);
  ASSERT_EQ(1U, visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(visits[0].transition,
                                           ui::PAGE_TRANSITION_TYPED));
  visits = typed_urls_helper::GetVisitsForURLFromClient(1, redirected_url);
  ASSERT_EQ(1U, visits.size());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(visits[0].transition,
                                           ui::PAGE_TRANSITION_TYPED));
}

// Doesn't work with HISTORY because that doesn't sync retroactively.
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest,
                       SkipImportedVisits) {
  GURL imported_url("http://imported_url.com");
  GURL browsed_url("http://browsed_url.com");
  GURL browsed_and_imported_url("http://browsed_and_imported_url.com");
  ASSERT_TRUE(SetupClients());

  // Create 3 items in our first client - 1 imported, one browsed, one with
  // both imported and browsed entries.
  AddUrlToHistoryWithTransition(0, imported_url, ui::PAGE_TRANSITION_TYPED,
                                history::SOURCE_FIREFOX_IMPORTED);
  AddUrlToHistoryWithTransition(0, browsed_url, ui::PAGE_TRANSITION_TYPED,
                                history::SOURCE_BROWSED);
  AddUrlToHistoryWithTransition(0, browsed_and_imported_url,
                                ui::PAGE_TRANSITION_TYPED,
                                history::SOURCE_FIREFOX_IMPORTED);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(TypedURLChecker(1, browsed_url.spec()).Wait());
  history::URLRows urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(browsed_url, urls[0].url());

  // Now browse to 3rd URL - this should cause it to be synced, even though it
  // was initially imported.
  AddUrlToHistoryWithTransition(0, browsed_and_imported_url,
                                ui::PAGE_TRANSITION_TYPED,
                                history::SOURCE_BROWSED);
  ASSERT_TRUE(TypedURLChecker(1, browsed_and_imported_url.spec()).Wait());
  urls = GetTypedUrlsFromClient(1);
  ASSERT_EQ(2U, urls.size());

  // Make sure the imported URL didn't make it over.
  for (const history::URLRow& url : urls) {
    ASSERT_NE(imported_url, url.url());
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsSyncTest, BookmarksWithTypedVisit) {
  GURL bookmark_url("http://www.bookmark.google.com/");
  GURL bookmark_icon_url("http://www.bookmark.google.com/favicon.ico");
  ASSERT_TRUE(SetupClients());
  // Create a bookmark.
  const BookmarkNode* node = bookmarks_helper::AddURL(
      0, bookmarks_helper::IndexedURLTitle(0), bookmark_url);
  bookmarks_helper::SetFavicon(0, node, bookmark_icon_url,
                               bookmarks_helper::CreateFavicon(SK_ColorWHITE),
                               bookmarks_helper::FROM_UI);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // A row in the DB for client 1 should have been created as a result of the
  // sync.
  history::URLRow row;
  ASSERT_TRUE(GetUrlFromClient(1, bookmark_url, &row));

  // Now, add a typed visit for client 0 to the bookmark URL and sync it over
  // - this should not cause a crash.
  AddUrlToHistory(0, bookmark_url);

  ASSERT_TRUE(ProfilesHaveSameTypedURLsChecker().Wait());
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(bookmark_url, urls[0].url());
  ASSERT_EQ(1, GetVisitCountForFirstURL(0));
}

// ResetWithDuplicateMetadata doesn't work with HISTORY because it manually
// writes to the TypedURL metadata DB.
class TwoClientTypedUrlsSyncTestWithoutLacrosSupport
    : public TwoClientTypedUrlsWithoutNewHistoryTypeSyncTest {
 public:
  TwoClientTypedUrlsSyncTestWithoutLacrosSupport() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // TODO(crbug.com/1263014): Update test to pass with Lacros enabled.
    feature_list_.InitAndDisableFeature(ash::features::kLacrosSupport);
#endif
  }
  ~TwoClientTypedUrlsSyncTestWithoutLacrosSupport() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for one part crbug.com/1075573. The fix for the issue was
// general, so typed_urls is somewhat arbitrary choice (typed_urls were the most
// affected by the issue).
IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsSyncTestWithoutLacrosSupport,
                       PRE_ResetWithDuplicateMetadata) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate one client with a URL, should sync to the other.
  const GURL url(kDummyUrl);
  AddUrlToHistory(0, url);
  EXPECT_TRUE(TypedURLChecker(1, kDummyUrl).Wait());
  EXPECT_TRUE(CheckSyncHasURLMetadata(1, GURL(kDummyUrl)));

  // Write a duplicate metadata entity. Duplicates appear on clients due to
  // different reasons / bugs, this test is not realistic in this sense. It is
  // just the simplest way to get to the desired state.
  sync_pb::EntityMetadata duplicate_metadata;
  duplicate_metadata.set_client_tag_hash(
      syncer::ClientTagHash::FromUnhashed(syncer::TYPED_URLS, kDummyUrl)
          .value());
  duplicate_metadata.set_creation_time(0);
  history::URLID arbitrary_id(5438392);
  std::string storage_key(sizeof(arbitrary_id), 0);
  base::WriteBigEndian<history::URLID>(&storage_key[0], arbitrary_id);
  WriteMetadataToClient(1, storage_key, duplicate_metadata);
}

IN_PROC_BROWSER_TEST_F(TwoClientTypedUrlsSyncTestWithoutLacrosSupport,
                       ResetWithDuplicateMetadata) {
  base::HistogramTester histogram_tester;

  // SetupSync() can't be used since it relies on an additional sync cycle (via
  // UpdateProgressMarkerChecker which checks non-empty progress markers).
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_TRUE(GetClient(1)->AwaitSyncSetupCompletion());

  // On startup, client 1 should reset its metadata and perform initial sync
  // once again. Sync one more url across the clients to be sure client 1
  // finishes its initial sync.
  const std::string kFurtherURL("http://www.typed.google.com/");
  AddUrlToHistory(0, GURL(kFurtherURL));
  EXPECT_TRUE(TypedURLChecker(1, kFurtherURL).Wait());

  // Check that client 1 has all the metadata without the duplicate.
  syncer::MetadataBatch batch = GetAllSyncMetadata(1);
  syncer::EntityMetadataMap metadata_map(batch.TakeAllMetadata());
  size_t count_for_dummy = 0;
  const syncer::ClientTagHash kClientTagHash =
      syncer::ClientTagHash::FromUnhashed(syncer::TYPED_URLS, kDummyUrl);
  for (const auto& [storage_key, metadata] : metadata_map) {
    if (metadata->client_tag_hash() == kClientTagHash.value()) {
      ++count_for_dummy;
    }
  }
  EXPECT_EQ(count_for_dummy, 1u);
  histogram_tester.ExpectBucketCount(
      "Sync.ModelTypeOrphanMetadata.ModelReadyToSync",
      /*sample=*/ModelTypeHistogramValue(syncer::TYPED_URLS),
      /*expected_count=*/1);
}
