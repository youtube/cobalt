// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/sync_disable_observer.h"

#include "base/observer_list.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

namespace {

class MockSyncService : public syncer::FakeSyncService {
 public:
  MockSyncService() {}
  ~MockSyncService() override { Shutdown(); }

  void SetStatus(bool has_passphrase, bool history_enabled) {
    initialized_ = true;
    has_passphrase_ = has_passphrase;
    preferred_data_types_ =
        history_enabled
            ? syncer::ModelTypeSet(syncer::HISTORY_DELETE_DIRECTIVES)
            : syncer::ModelTypeSet();
    NotifyObserversOfStateChanged();
  }

  void SetConnectionStatus(syncer::ConnectionStatus status) {
    connection_status_ = status;
    NotifyObserversOfStateChanged();
  }

  void Shutdown() override {
    for (auto& observer : observers_) {
      observer.OnSyncShutdown(this);
    }
  }

  void NotifyObserversOfStateChanged() {
    for (auto& observer : observers_) {
      observer.OnStateChanged(this);
    }
  }

 private:
  // syncer::FakeSyncService:
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    observers_.RemoveObserver(observer);
  }
  TransportState GetTransportState() const override {
    return initialized_ ? TransportState::ACTIVE : TransportState::INITIALIZING;
  }
  bool IsUsingSecondaryPassphrase() const override { return has_passphrase_; }
  syncer::ModelTypeSet GetPreferredDataTypes() const override {
    return preferred_data_types_;
  }
  syncer::SyncTokenStatus GetSyncTokenStatus() const override {
    syncer::SyncTokenStatus status;
    status.connection_status = connection_status_;
    return status;
  }

  bool initialized_ = false;
  bool has_passphrase_ = false;
  syncer::ConnectionStatus connection_status_ = syncer::CONNECTION_OK;
  syncer::ModelTypeSet preferred_data_types_;

  // The list of observers of the SyncService state.
  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(MockSyncService);
};

class TestSyncDisableObserver : public SyncDisableObserver {
 public:
  TestSyncDisableObserver() : purged_(false), notified_(false) {}
  ~TestSyncDisableObserver() override {}

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
  // SyncDisableObserver:
  void OnSyncPrefsChanged(bool must_purge) override {
    notified_ = true;
    purged_ = purged_ || must_purge;
  }
  bool purged_;
  bool notified_;
  DISALLOW_COPY_AND_ASSIGN(TestSyncDisableObserver);
};

class SyncDisableObserverTest : public testing::Test {
 public:
  SyncDisableObserverTest() {}
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

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncDisableObserverTest);
};

}  // namespace

TEST_F(SyncDisableObserverTest, NoProfiles) {
  TestSyncDisableObserver observer;
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, OneEnabled_UnifiedConsentDisabled) {
  TestSyncDisableObserver observer;
  MockSyncService sync;
  sync.SetStatus(false, true);
  observer.ObserveServiceForSyncDisables(&sync, nullptr, false);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, OneEnabled_UnifiedConsentEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  MockSyncService sync;
  for (bool has_passphrase : {true, false}) {
    for (bool history_enabled : {true, false}) {
      TestSyncDisableObserver observer;
      sync.SetStatus(has_passphrase, history_enabled);
      observer.ObserveServiceForSyncDisables(&sync, &prefs, true);
      EXPECT_TRUE(observer.SyncStateAllowsUkm());
      EXPECT_TRUE(observer.ResetNotified());
      EXPECT_FALSE(observer.ResetPurged());
    }
  }
}

TEST_F(SyncDisableObserverTest, Passphrased_UnifiedConsentDisabled) {
  MockSyncService sync;
  sync.SetStatus(true, true);
  TestSyncDisableObserver observer;
  observer.ObserveServiceForSyncDisables(&sync, nullptr, false);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, HistoryDisabled_UnifiedConsentDisabled) {
  TestSyncDisableObserver observer;
  MockSyncService sync;
  sync.SetStatus(false, false);
  observer.ObserveServiceForSyncDisables(&sync, nullptr, false);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, AuthError_UnifiedConsentDisabled) {
  TestSyncDisableObserver observer;
  MockSyncService sync;
  sync.SetStatus(false, true);
  observer.ObserveServiceForSyncDisables(&sync, nullptr, false);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  sync.SetConnectionStatus(syncer::CONNECTION_AUTH_ERROR);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  sync.SetConnectionStatus(syncer::CONNECTION_OK);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
}

TEST_F(SyncDisableObserverTest, MixedProfiles1_UnifiedConsentDisabled) {
  TestSyncDisableObserver observer;
  MockSyncService sync1;
  sync1.SetStatus(false, false);
  observer.ObserveServiceForSyncDisables(&sync1, nullptr, false);
  MockSyncService sync2;
  sync2.SetStatus(false, true);
  observer.ObserveServiceForSyncDisables(&sync2, nullptr, false);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, MixedProfiles2_UnifiedConsentDisabled) {
  TestSyncDisableObserver observer;
  MockSyncService sync1;
  sync1.SetStatus(false, true);
  observer.ObserveServiceForSyncDisables(&sync1, nullptr, false);
  EXPECT_TRUE(observer.ResetNotified());

  MockSyncService sync2;
  sync2.SetStatus(false, false);
  observer.ObserveServiceForSyncDisables(&sync2, nullptr, false);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync2.Shutdown();
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, TwoEnabled_UnifiedConsentDisabled) {
  TestSyncDisableObserver observer;
  MockSyncService sync1;
  sync1.SetStatus(false, true);
  observer.ObserveServiceForSyncDisables(&sync1, nullptr, false);
  EXPECT_TRUE(observer.ResetNotified());
  MockSyncService sync2;
  sync2.SetStatus(false, true);
  observer.ObserveServiceForSyncDisables(&sync2, nullptr, false);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, TwoEnabled_UnifiedConsentEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);
  TestSyncDisableObserver observer;

  // First profile has sync enabled.
  MockSyncService sync1;
  sync1.SetStatus(false, true);
  observer.ObserveServiceForSyncDisables(&sync1, nullptr, false);
  EXPECT_TRUE(observer.ResetNotified());

  // Second profile has URL-keyed anonymized data collection enabled.
  MockSyncService sync2;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, true);
  observer.ObserveServiceForSyncDisables(&sync2, &prefs2, true);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, OneAddRemove_UnifiedConsentDisabled) {
  TestSyncDisableObserver observer;
  MockSyncService sync;
  observer.ObserveServiceForSyncDisables(&sync, nullptr, false);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync.SetStatus(false, true);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, OneAddRemove_UnifiedConsentEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestSyncDisableObserver observer;
  MockSyncService sync;
  observer.ObserveServiceForSyncDisables(&sync, &prefs, true);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, PurgeOnDisable_UnifiedConsentDisabled) {
  TestSyncDisableObserver observer;
  MockSyncService sync;
  sync.SetStatus(false, true);
  observer.ObserveServiceForSyncDisables(&sync, nullptr, false);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync.SetStatus(false, false);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_TRUE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, PurgeOnDisable_UnifiedConsentEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestSyncDisableObserver observer;
  MockSyncService sync;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  observer.ObserveServiceForSyncDisables(&sync, &prefs, true);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, false);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_TRUE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

}  // namespace ukm
