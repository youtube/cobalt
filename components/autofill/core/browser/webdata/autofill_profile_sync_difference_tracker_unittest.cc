// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_sync_difference_tracker.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/model/model_error.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ASCIIToUTF16;
using base::MockCallback;
using testing::ElementsAre;
using testing::IsEmpty;

// Some guids for testing.
const char kSmallerGuid[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kBiggerGuid[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
const char kHttpOrigin[] = "http://www.example.com/";
const char kHttpsOrigin[] = "https://www.example.com/";
const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

struct UpdatesToSync {
  std::vector<AutofillProfile> profiles_to_upload_to_sync;
  std::vector<std::string> profiles_to_delete_from_sync;
};

}  // namespace

class AutofillProfileSyncDifferenceTrackerTestBase : public testing::Test {
 public:
  AutofillProfileSyncDifferenceTrackerTestBase() {}

  AutofillProfileSyncDifferenceTrackerTestBase(
      const AutofillProfileSyncDifferenceTrackerTestBase&) = delete;
  AutofillProfileSyncDifferenceTrackerTestBase& operator=(
      const AutofillProfileSyncDifferenceTrackerTestBase&) = delete;

  ~AutofillProfileSyncDifferenceTrackerTestBase() override {}

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
  }

  void AddAutofillProfilesToTable(
      const std::vector<AutofillProfile>& profile_list) {
    for (const auto& profile : profile_list) {
      table_.AddAutofillProfile(profile);
    }
  }

  void IncorporateRemoteProfile(const AutofillProfile& profile) {
    EXPECT_EQ(absl::nullopt, tracker()->IncorporateRemoteProfile(
                                 std::make_unique<AutofillProfile>(profile)));
  }

  UpdatesToSync FlushToSync() {
    EXPECT_EQ(absl::nullopt,
              tracker()->FlushToLocal(
                  /*autofill_changes_callback=*/base::DoNothing()));

    UpdatesToSync updates;
    std::vector<std::unique_ptr<AutofillProfile>> vector_of_unique_ptrs;
    EXPECT_EQ(absl::nullopt,
              tracker()->FlushToSync(
                  /*profiles_to_upload_to_sync=*/&vector_of_unique_ptrs,
                  /*profiles_to_delete_from_sync=*/&updates
                      .profiles_to_delete_from_sync));

    // Copy all the elements by value so that we have a vector that is easier to
    // work with in the test.
    for (const std::unique_ptr<AutofillProfile>& entry :
         vector_of_unique_ptrs) {
      updates.profiles_to_upload_to_sync.push_back(*entry);
    }
    return updates;
  }

  std::vector<AutofillProfile> GetAllLocalData() {
    std::vector<std::unique_ptr<AutofillProfile>> vector_of_unique_ptrs;
    // Meant as an assertion but I cannot use ASSERT_TRUE in non-void function.
    EXPECT_TRUE(table()->GetAutofillProfiles(
        &vector_of_unique_ptrs, AutofillProfile::Source::kLocalOrSyncable));

    // Copy all the elements by value so that we have a vector that is easier to
    // work with in the test.
    std::vector<AutofillProfile> local_data;
    for (const std::unique_ptr<AutofillProfile>& entry :
         vector_of_unique_ptrs) {
      local_data.push_back(*entry);
    }
    return local_data;
  }

  virtual AutofillProfileSyncDifferenceTracker* tracker() = 0;

  AutofillTable* table() { return &table_; }

 private:
  autofill::TestAutofillClock test_clock_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  AutofillTable table_;
  WebDatabase db_;
};

class AutofillProfileSyncDifferenceTrackerTest
    : public AutofillProfileSyncDifferenceTrackerTestBase {
 public:
  AutofillProfileSyncDifferenceTrackerTest() : tracker_(table()) {}

  AutofillProfileSyncDifferenceTrackerTest(
      const AutofillProfileSyncDifferenceTrackerTest&) = delete;
  AutofillProfileSyncDifferenceTrackerTest& operator=(
      const AutofillProfileSyncDifferenceTrackerTest&) = delete;

  ~AutofillProfileSyncDifferenceTrackerTest() override {}

  AutofillProfileSyncDifferenceTracker* tracker() override { return &tracker_; }

 private:
  AutofillProfileSyncDifferenceTracker tracker_;
};

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       IncorporateRemoteProfileShouldOverwriteProfileWithSameKey) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kHttpOrigin);
  local.SetRawInfo(NAME_FIRST, u"John");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile is completely different but it has the same key.
  AutofillProfile remote = AutofillProfile(kSmallerGuid, kHttpsOrigin);
  remote.SetRawInfo(NAME_FIRST, u"Tom");
  remote.FinalizeAfterImport();

  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, IsEmpty());
  EXPECT_THAT(updates.profiles_to_delete_from_sync, IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       IncorporateRemoteProfileShouldOverwriteUnverifiedProfileByVerified) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kHttpsOrigin);
  local.SetRawInfo(NAME_FIRST, u"John");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile has the same key but it is not verified.
  AutofillProfile remote = AutofillProfile(kSmallerGuid, kSettingsOrigin);
  remote.SetRawInfo(NAME_FIRST, u"Tom");
  remote.FinalizeAfterImport();

  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the local profile wins.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, IsEmpty());
  EXPECT_THAT(updates.profiles_to_delete_from_sync, IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       IncorporateRemoteProfileShouldNotOverwriteVerifiedProfileByUnverified) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kSettingsOrigin);
  local.SetRawInfo(NAME_FIRST, u"John");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile has the same key but it is not verified.
  AutofillProfile remote = AutofillProfile(kSmallerGuid, kHttpsOrigin);
  remote.SetRawInfo(NAME_FIRST, u"Tom");
  remote.FinalizeAfterImport();
  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the local profile wins.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, IsEmpty());
  EXPECT_THAT(updates.profiles_to_delete_from_sync, IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       IncorporateRemoteProfileShouldNotOverwriteFullNameByEmptyString) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kHttpOrigin);
  local.SetRawInfo(NAME_FULL, u"John");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile has the same key.
  AutofillProfile remote = AutofillProfile(kSmallerGuid, kHttpsOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"2 2st st");

  AutofillProfile merged(remote);
  merged.SetRawInfo(NAME_FULL, u"John");
  merged.FinalizeAfterImport();
  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins except for the
  // full name.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, IsEmpty());
  EXPECT_THAT(updates.profiles_to_delete_from_sync, IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

TEST_F(
    AutofillProfileSyncDifferenceTrackerTest,
    IncorporateRemoteProfileShouldKeepRemoteKeyWhenMergingDuplicateProfileWithBiggerKey) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kHttpOrigin);
  local.SetRawInfo(NAME_FIRST, u"John");
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile is identical to the local one, except that the guids and
  // origins are different.
  AutofillProfile remote = AutofillProfile(kBiggerGuid, kHttpsOrigin);
  remote.SetRawInfo(NAME_FIRST, u"John");
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  remote.FinalizeAfterImport();
  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, IsEmpty());
  EXPECT_THAT(updates.profiles_to_delete_from_sync,
              ElementsAre(std::string(kSmallerGuid)));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(
    AutofillProfileSyncDifferenceTrackerTest,
    IncorporateRemoteProfileShouldKeepRemoteKeyAndLocalOriginWhenMergingDuplicateProfileWithBiggerKey) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kSettingsOrigin);
  local.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                         VerificationStatus::kObserved);
  local.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"1 1st st", VerificationStatus::kObserved);
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile has the same key.
  AutofillProfile remote = AutofillProfile(kBiggerGuid, kHttpsOrigin);
  remote.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                          VerificationStatus::kObserved);
  remote.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"1 1st st", VerificationStatus::kObserved);
  remote.FinalizeAfterImport();

  AutofillProfile merged(remote);
  merged.set_origin(kSettingsOrigin);

  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins except for the
  // full name.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, ElementsAre(merged));
  EXPECT_THAT(updates.profiles_to_delete_from_sync,
              ElementsAre(std::string(kSmallerGuid)));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

TEST_F(
    AutofillProfileSyncDifferenceTrackerTest,
    IncorporateRemoteProfileShouldKeepLocalKeyWhenMergingDuplicateProfileWithSmallerKey) {
  AutofillProfile local = AutofillProfile(kBiggerGuid, kHttpOrigin);
  local.SetRawInfo(NAME_FIRST, u"John");
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile is identical to the local one, except that the guids and
  // origins are different.
  AutofillProfile remote = AutofillProfile(kSmallerGuid, kHttpsOrigin);
  remote.SetRawInfo(NAME_FIRST, u"John");
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  remote.FinalizeAfterImport();
  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, IsEmpty());
  EXPECT_THAT(updates.profiles_to_delete_from_sync,
              ElementsAre(std::string(kSmallerGuid)));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local));
}

TEST_F(
    AutofillProfileSyncDifferenceTrackerTest,
    IncorporateRemoteProfileShouldKeepLocalKeyAndRemoteOriginWhenMergingDuplicateProfileWithSmallerKey) {
  AutofillProfile local = AutofillProfile(kBiggerGuid, kHttpsOrigin);
  local.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                         VerificationStatus::kUserVerified);
  local.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                         u"1 1st st",
                                         VerificationStatus::kUserVerified);
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile has the same key.
  AutofillProfile remote = AutofillProfile(kSmallerGuid, kSettingsOrigin);
  remote.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                          VerificationStatus::kUserVerified);
  remote.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                          u"1 1st st",
                                          VerificationStatus::kUserVerified);
  remote.FinalizeAfterImport();

  AutofillProfile merged(local);
  merged.set_origin(kSettingsOrigin);
  IncorporateRemoteProfile(remote);

  // Nothing gets uploaded to sync and the remote profile wins except for the
  // full name.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, ElementsAre(merged));
  EXPECT_THAT(updates.profiles_to_delete_from_sync,
              ElementsAre(std::string(kSmallerGuid)));
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       FlushToLocalShouldNotCallbackWhenNotNeeded) {
  MockCallback<base::OnceClosure> autofill_changes_callback;

  EXPECT_CALL(autofill_changes_callback, Run()).Times(0);
  EXPECT_EQ(absl::nullopt,
            tracker()->FlushToLocal(autofill_changes_callback.Get()));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       FlushToLocalShouldCallbackWhenProfileDeleted) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kSettingsOrigin);
  AddAutofillProfilesToTable({local});

  EXPECT_EQ(absl::nullopt, tracker()->IncorporateRemoteDelete(kSmallerGuid));

  MockCallback<base::OnceClosure> autofill_changes_callback;
  EXPECT_CALL(autofill_changes_callback, Run()).Times(1);
  EXPECT_EQ(absl::nullopt,
            tracker()->FlushToLocal(autofill_changes_callback.Get()));

  // On top of that, the profile should also get deleted.
  EXPECT_THAT(GetAllLocalData(), IsEmpty());
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       FlushToLocalShouldCallbackWhenProfileAdded) {
  AutofillProfile remote = AutofillProfile(kSmallerGuid, kSettingsOrigin);
  IncorporateRemoteProfile(remote);

  MockCallback<base::OnceClosure> autofill_changes_callback;
  EXPECT_CALL(autofill_changes_callback, Run()).Times(1);
  EXPECT_EQ(absl::nullopt,
            tracker()->FlushToLocal(autofill_changes_callback.Get()));

  // On top of that, the profile should also get added.
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(AutofillProfileSyncDifferenceTrackerTest,
       FlushToLocalShouldCallbackWhenProfileUpdated) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kHttpsOrigin);
  AddAutofillProfilesToTable({local});

  AutofillProfile remote = AutofillProfile(kSmallerGuid, kHttpsOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  remote.FinalizeAfterImport();
  IncorporateRemoteProfile(remote);

  MockCallback<base::OnceClosure> autofill_changes_callback;
  EXPECT_CALL(autofill_changes_callback, Run()).Times(1);
  EXPECT_EQ(absl::nullopt,
            tracker()->FlushToLocal(autofill_changes_callback.Get()));

  // On top of that, the profile with key kSmallerGuid should also get updated.
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

class AutofillProfileInitialSyncDifferenceTrackerTest
    : public AutofillProfileSyncDifferenceTrackerTestBase {
 public:
  AutofillProfileInitialSyncDifferenceTrackerTest()
      : initial_tracker_(table()) {}

  AutofillProfileInitialSyncDifferenceTrackerTest(
      const AutofillProfileInitialSyncDifferenceTrackerTest&) = delete;
  AutofillProfileInitialSyncDifferenceTrackerTest& operator=(
      const AutofillProfileInitialSyncDifferenceTrackerTest&) = delete;

  ~AutofillProfileInitialSyncDifferenceTrackerTest() override {}

  [[nodiscard]] absl::optional<syncer::ModelError>
  MergeSimilarEntriesForInitialSync() {
    return initial_tracker_.MergeSimilarEntriesForInitialSync(kLocaleString);
  }

  AutofillProfileSyncDifferenceTracker* tracker() override {
    return &initial_tracker_;
  }

 private:
  AutofillProfileInitialSyncDifferenceTracker initial_tracker_;
};

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncShouldSyncUpChanges) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kHttpOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local.set_use_count(27);
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile matches the local one (except for origin and use count).
  AutofillProfile remote = AutofillProfile(kBiggerGuid, kHttpsOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  remote.SetRawInfo(COMPANY_NAME, u"Frobbers, Inc.");
  remote.set_use_count(13);
  remote.FinalizeAfterImport();
  // The remote profile wins (as regards the storage key).
  AutofillProfile merged(remote);
  // The local origin wins when merging.
  merged.set_origin(kHttpOrigin);
  // Merging two profile takes their max use count.
  merged.set_use_count(27);

  IncorporateRemoteProfile(remote);
  EXPECT_EQ(absl::nullopt, MergeSimilarEntriesForInitialSync());

  // The merged profile needs to get uploaded back to sync and stored locally.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, ElementsAre(merged));
  EXPECT_THAT(updates.profiles_to_delete_from_sync, IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(merged));
}

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncShouldNotSyncUpWhenNotNeeded) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kHttpOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local.set_use_count(13);
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile matches the local one and has some additional data.
  AutofillProfile remote = AutofillProfile(kBiggerGuid, kHttpOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  remote.SetRawInfo(COMPANY_NAME, u"Frobbers, Inc.");
  // Merging two profile takes their max use count, so use count of 27 is taken.
  remote.set_use_count(27);
  remote.FinalizeAfterImport();
  IncorporateRemoteProfile(remote);
  EXPECT_EQ(absl::nullopt, MergeSimilarEntriesForInitialSync());

  // Nothing gets uploaded to sync and the remote profile wins.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, IsEmpty());
  EXPECT_THAT(updates.profiles_to_delete_from_sync, IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(remote));
}

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncNotMatchNonsimilarEntries) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kHttpOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local.SetRawInfo(COMPANY_NAME, u"Frobbers, Inc.");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile has a different street address.
  AutofillProfile remote = AutofillProfile(kBiggerGuid, kHttpOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"2 2st st");
  remote.SetRawInfo(COMPANY_NAME, u"Frobbers, Inc.");
  remote.FinalizeAfterImport();
  IncorporateRemoteProfile(remote);
  EXPECT_EQ(absl::nullopt, MergeSimilarEntriesForInitialSync());

  // The local profile gets uploaded (due to initial sync) and the remote
  // profile gets stored locally.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, ElementsAre(local));
  EXPECT_THAT(updates.profiles_to_delete_from_sync, IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local, remote));
}

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncDoesNotMatchLocalVerifiedEntry) {
  // The local entry is verified, should not get merged.
  AutofillProfile local = AutofillProfile(kSmallerGuid, kSettingsOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile is similar to the local one.
  AutofillProfile remote = AutofillProfile(kBiggerGuid, kHttpOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  remote.SetRawInfo(COMPANY_NAME, u"Frobbers, Inc.");
  remote.FinalizeAfterImport();
  IncorporateRemoteProfile(remote);
  EXPECT_EQ(absl::nullopt, MergeSimilarEntriesForInitialSync());

  // The local profile gets uploaded (due to initial sync) and the remote
  // profile gets stored locally.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, ElementsAre(local));
  EXPECT_THAT(updates.profiles_to_delete_from_sync, IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local, remote));
}

TEST_F(AutofillProfileInitialSyncDifferenceTrackerTest,
       MergeSimilarEntriesForInitialSyncDoesNotMatchRemoteVerifiedEntry) {
  AutofillProfile local = AutofillProfile(kSmallerGuid, kHttpOrigin);
  local.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  local.FinalizeAfterImport();
  AddAutofillProfilesToTable({local});

  // The remote profile is similar to the local one but is verified and thus it
  // should not get merged.
  AutofillProfile remote = AutofillProfile(kBiggerGuid, kSettingsOrigin);
  remote.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"1 1st st");
  remote.SetRawInfo(COMPANY_NAME, u"Frobbers, Inc.");
  remote.FinalizeAfterImport();
  IncorporateRemoteProfile(remote);
  EXPECT_EQ(absl::nullopt, MergeSimilarEntriesForInitialSync());

  // The local profile gets uploaded (due to initial sync) and the remote
  // profile gets stored locally.
  UpdatesToSync updates = FlushToSync();
  EXPECT_THAT(updates.profiles_to_upload_to_sync, ElementsAre(local));
  EXPECT_THAT(updates.profiles_to_delete_from_sync, IsEmpty());
  EXPECT_THAT(GetAllLocalData(), ElementsAre(local, remote));
}

}  // namespace autofill
