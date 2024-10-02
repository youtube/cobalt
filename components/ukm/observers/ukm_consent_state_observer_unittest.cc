// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/ukm_consent_state_observer.h"

#include "base/observer_list.h"
#include "base/test/scoped_feature_list.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

namespace {

class MockSyncService : public syncer::TestSyncService {
 public:
  MockSyncService() {
    SetTransportState(TransportState::INITIALIZING);
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot());
  }

  MockSyncService(const MockSyncService&) = delete;
  MockSyncService& operator=(const MockSyncService&) = delete;

  ~MockSyncService() override { Shutdown(); }

  void SetStatus(bool has_passphrase, bool history_enabled, bool active) {
    SetTransportState(active ? TransportState::ACTIVE
                             : TransportState::INITIALIZING);
    SetIsUsingExplicitPassphrase(has_passphrase);

    GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/history_enabled ? syncer::UserSelectableTypeSet(
                                        syncer::UserSelectableType::kHistory)
                                  : syncer::UserSelectableTypeSet());

    // It doesn't matter what exactly we set here, it's only relevant that the
    // SyncCycleSnapshot is initialized at all.
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot(
        /*birthday=*/std::string(), /*bag_of_chips=*/std::string(),
        syncer::ModelNeutralState(), syncer::ProgressMarkerMap(), false, 0,
        true, base::Time::Now(), base::Time::Now(),
        sync_pb::SyncEnums::UNKNOWN_ORIGIN, base::Minutes(1), false));

    NotifyObserversOfStateChanged();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    SetAppSync(false);
#endif
  }

  void Shutdown() override {
    for (auto& observer : observers_) {
      observer.OnSyncShutdown(this);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetAppSync(bool enabled) {
    auto selected_os_types = GetUserSettings()->GetSelectedOsTypes();

    if (enabled)
      selected_os_types.Put(syncer::UserSelectableOsType::kOsApps);
    else
      selected_os_types.Remove(syncer::UserSelectableOsType::kOsApps);

    GetUserSettings()->SetSelectedOsTypes(false, selected_os_types);

    NotifyObserversOfStateChanged();
  }
#endif

 private:
  // syncer::TestSyncService:
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyObserversOfStateChanged() {
    for (auto& observer : observers_) {
      observer.OnStateChanged(this);
    }
  }

  // The list of observers of the SyncService state.
  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;
};

class TestUkmConsentStateObserver : public UkmConsentStateObserver {
 public:
  TestUkmConsentStateObserver() = default;

  TestUkmConsentStateObserver(const TestUkmConsentStateObserver&) = delete;
  TestUkmConsentStateObserver& operator=(const TestUkmConsentStateObserver&) =
      delete;

  ~TestUkmConsentStateObserver() override = default;

  bool ResetPurged() {
    bool was_purged = purged_;
    purged_ = false;
    return was_purged;
  }

  bool ResetNotified() {
    bool notified = notified_;
    notified_ = false;
    return notified;
  }

 private:
  // UkmConsentStateObserver:
  void OnUkmAllowedStateChanged(bool must_purge, UkmConsentState) override {
    notified_ = true;
    purged_ = purged_ || must_purge;
  }
  bool purged_ = false;
  bool notified_ = false;
};

class UkmConsentStateObserverTest : public testing::Test {
 public:
  UkmConsentStateObserverTest() = default;

  UkmConsentStateObserverTest(const UkmConsentStateObserverTest&) = delete;
  UkmConsentStateObserverTest& operator=(const UkmConsentStateObserverTest&) =
      delete;

  void RegisterUrlKeyedAnonymizedDataCollectionPref(
      sync_preferences::TestingPrefServiceSyncable& prefs) {
    unified_consent::UnifiedConsentService::RegisterPrefs(prefs.registry());
  }

  void SetUrlKeyedAnonymizedDataCollectionEnabled(PrefService* prefs,
                                                  bool enabled) {
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(UkmConsentStateObserverTest, NoProfiles) {
  TestUkmConsentStateObserver observer;
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, NotActive) {
  MockSyncService sync;
  sync.SetStatus(false, true, false);
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, OneEnabled) {
  MockSyncService sync;
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  TestUkmConsentStateObserver observer;
  observer.StartObserving(&sync, &prefs);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, MixedProfiles) {
  sync_preferences::TestingPrefServiceSyncable prefs1;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs1);
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);

  TestUkmConsentStateObserver observer;
  MockSyncService sync1;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs1, false);
  observer.StartObserving(&sync1, &prefs1);
  MockSyncService sync2;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, true);
  observer.StartObserving(&sync2, &prefs2);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, TwoEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs1;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs1);
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);

  TestUkmConsentStateObserver observer;
  MockSyncService sync1;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs1, true);
  observer.StartObserving(&sync1, &prefs1);
  EXPECT_TRUE(observer.ResetNotified());
  MockSyncService sync2;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, true);
  observer.StartObserving(&sync2, &prefs2);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, OneAddRemove) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  MockSyncService sync;
  observer.StartObserving(&sync, &prefs);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(UkmConsentStateObserverTest, PurgeOnDisable) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  MockSyncService sync;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  observer.StartObserving(&sync, &prefs);
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, false);
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_TRUE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests for when AppSync is not dependent on MSBB.
class MsbbAppOptInUkmConsentStateObserverTest
    : public UkmConsentStateObserverTest {
 public:
  MsbbAppOptInUkmConsentStateObserverTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ukm::kAppMetricsOnlyRelyOnAppSync);
  }
};

TEST_F(MsbbAppOptInUkmConsentStateObserverTest, VerifyConsentStates) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestUkmConsentStateObserver observer;
  MockSyncService sync;
  // Disable app sync consent.
  sync.SetAppSync(false);

  // Enable MSBB consent.
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, /*enabled=*/true);
  observer.StartObserving(&sync, &prefs);

  UkmConsentState state = observer.GetUkmConsentState();

  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  // MSBB and Extensions are enabled while App Sync is disabled.
  EXPECT_TRUE(state.Has(MSBB));
  EXPECT_TRUE(state.Has(EXTENSIONS));
  EXPECT_FALSE(state.Has(APPS));

  // UKM is enabled and the consent state was changed. Purge will not happen
  // because UKM was enabled not disabled.
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());

  // Turn on app sync.
  sync.SetAppSync(true);
  state = observer.GetUkmConsentState();

  // Verify that the these values remain unchanged with App-sync enablement.
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_TRUE(state.Has(MSBB));
  EXPECT_TRUE(state.Has(EXTENSIONS));
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());

  // Check for the updated consent propagated correctly.
  EXPECT_TRUE(state.Has(APPS));

  // Turn off MSBB.
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, /*enabled=*/false);

  state = observer.GetUkmConsentState();

  // UKM will remain allowed.
  EXPECT_TRUE(observer.IsUkmAllowedForAllProfiles());
  // MSBB should be off.
  EXPECT_FALSE(state.Has(MSBB));
  // Extensions should be off, implicitly.
  EXPECT_FALSE(state.Has(EXTENSIONS));
  // App sync will stay on.
  EXPECT_TRUE(state.Has(APPS));
  EXPECT_TRUE(observer.ResetNotified());
  // UKM is still allowed, total purge is not triggered.
  EXPECT_FALSE(observer.ResetPurged());

  // Finally, turn off app sync.
  sync.SetAppSync(false);

  state = observer.GetUkmConsentState();
  // All consents should be off.
  EXPECT_FALSE(state.Has(MSBB));
  EXPECT_FALSE(state.Has(EXTENSIONS));
  EXPECT_FALSE(state.Has(APPS));
  EXPECT_TRUE(observer.ResetNotified());

  // All UKM consent is turned off, total purge will be triggered.
  EXPECT_TRUE(observer.ResetPurged());
}

TEST_F(MsbbAppOptInUkmConsentStateObserverTest,
       VerifyConflictingProfilesRevokesConsent) {
  sync_preferences::TestingPrefServiceSyncable prefs1;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs1);
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);

  // Add Profile 1 with MSBB consent but not App Sync.
  TestUkmConsentStateObserver observer;
  MockSyncService sync1;
  sync1.SetAppSync(false);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs1, /*enabled=*/true);
  observer.StartObserving(&sync1, &prefs1);
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  const UkmConsentState consent_state = observer.GetUkmConsentState();
  EXPECT_TRUE(consent_state.Has(MSBB));
  EXPECT_FALSE(consent_state.Has(APPS));

  // Add Profile 2 with App Sync consent but not MSBB.
  MockSyncService sync2;
  sync2.SetAppSync(true);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, /*enabled=*/false);
  observer.StartObserving(&sync2, &prefs2);
  EXPECT_TRUE(observer.ResetNotified());

  // Consents of MSBB and App-sync for each profile conflicts with either other
  // resulting in all types of consent being false.
  // MSBB is off because Profile 1 consents but Profile 2 doesn't.
  // APP sync is off because Profile 2 consents but Profile 1 doesn't.
  UkmConsentState state = observer.GetUkmConsentState();
  EXPECT_FALSE(observer.IsUkmAllowedForAllProfiles());
  EXPECT_FALSE(state.Has(MSBB));
  EXPECT_FALSE(state.Has(EXTENSIONS));
  EXPECT_FALSE(state.Has(APPS));
  EXPECT_FALSE(observer.ResetPurged());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace ukm
