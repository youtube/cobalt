// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_state_manager.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/caching_permuted_entropy_provider.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class MetricsStateManagerTest : public testing::Test {
 public:
  MetricsStateManagerTest()
      : test_begin_time_(base::Time::Now().ToTimeT()),
        enabled_state_provider_(new TestEnabledStateProvider(false, false)) {
    MetricsService::RegisterPrefs(prefs_.registry());
  }

  std::unique_ptr<MetricsStateManager> CreateStateManager() {
    return MetricsStateManager::Create(
        &prefs_, enabled_state_provider_.get(), base::string16(),
        base::Bind(&MetricsStateManagerTest::MockStoreClientInfoBackup,
                   base::Unretained(this)),
        base::Bind(&MetricsStateManagerTest::LoadFakeClientInfoBackup,
                   base::Unretained(this)));
  }

  // Sets metrics reporting as enabled for testing.
  void EnableMetricsReporting() {
    enabled_state_provider_->set_consent(true);
    enabled_state_provider_->set_enabled(true);
  }

  void SetClientInfoPrefs(const ClientInfo& client_info) {
    prefs_.SetString(prefs::kMetricsClientID, client_info.client_id);
    prefs_.SetInt64(prefs::kInstallDate, client_info.installation_date);
    prefs_.SetInt64(prefs::kMetricsReportingEnabledTimestamp,
                    client_info.reporting_enabled_date);
  }

  void SetFakeClientInfoBackup(const ClientInfo& client_info) {
    fake_client_info_backup_.reset(new ClientInfo);
    fake_client_info_backup_->client_id = client_info.client_id;
    fake_client_info_backup_->installation_date = client_info.installation_date;
    fake_client_info_backup_->reporting_enabled_date =
        client_info.reporting_enabled_date;
  }

 protected:
  TestingPrefServiceSimple prefs_;

  // Last ClientInfo stored by the MetricsStateManager via
  // MockStoreClientInfoBackup.
  std::unique_ptr<ClientInfo> stored_client_info_backup_;

  // If set, will be returned via LoadFakeClientInfoBackup if requested by the
  // MetricsStateManager.
  std::unique_ptr<ClientInfo> fake_client_info_backup_;

  const int64_t test_begin_time_;

 private:
  // Stores the |client_info| in |stored_client_info_backup_| for verification
  // by the tests later.
  void MockStoreClientInfoBackup(const ClientInfo& client_info) {
    stored_client_info_backup_.reset(new ClientInfo);
    stored_client_info_backup_->client_id = client_info.client_id;
    stored_client_info_backup_->installation_date =
        client_info.installation_date;
    stored_client_info_backup_->reporting_enabled_date =
        client_info.reporting_enabled_date;

    // Respect the contract that storing an empty client_id voids the existing
    // backup (required for the last section of the ForceClientIdCreation test
    // below).
    if (client_info.client_id.empty())
      fake_client_info_backup_.reset();
  }

  // Hands out a copy of |fake_client_info_backup_| if it is set.
  std::unique_ptr<ClientInfo> LoadFakeClientInfoBackup() {
    if (!fake_client_info_backup_)
      return std::unique_ptr<ClientInfo>();

    std::unique_ptr<ClientInfo> backup_copy(new ClientInfo);
    backup_copy->client_id = fake_client_info_backup_->client_id;
    backup_copy->installation_date =
        fake_client_info_backup_->installation_date;
    backup_copy->reporting_enabled_date =
        fake_client_info_backup_->reporting_enabled_date;
    return backup_copy;
  }

  std::unique_ptr<TestEnabledStateProvider> enabled_state_provider_;

  DISALLOW_COPY_AND_ASSIGN(MetricsStateManagerTest);
};

// Ensure the ClientId is formatted as expected.
TEST_F(MetricsStateManagerTest, ClientIdCorrectlyFormatted) {
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  state_manager->ForceClientIdCreation();

  const std::string client_id = state_manager->client_id();
  EXPECT_EQ(36U, client_id.length());

  for (size_t i = 0; i < client_id.length(); ++i) {
    char current = client_id[i];
    if (i == 8 || i == 13 || i == 18 || i == 23)
      EXPECT_EQ('-', current);
    else
      EXPECT_TRUE(isxdigit(current));
  }
}

TEST_F(MetricsStateManagerTest, EntropySourceUsed_Low) {
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  state_manager->CreateDefaultEntropyProvider();
  EXPECT_EQ(MetricsStateManager::ENTROPY_SOURCE_LOW,
            state_manager->entropy_source_returned());
}

TEST_F(MetricsStateManagerTest, EntropySourceUsed_High) {
  EnableMetricsReporting();
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  state_manager->CreateDefaultEntropyProvider();
  EXPECT_EQ(MetricsStateManager::ENTROPY_SOURCE_HIGH,
            state_manager->entropy_source_returned());
}

TEST_F(MetricsStateManagerTest, LowEntropySource0NotReset) {
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

  // Get the low entropy source once, to initialize it.
  state_manager->GetLowEntropySource();

  // Now, set it to 0 and ensure it doesn't get reset.
  state_manager->low_entropy_source_ = 0;
  EXPECT_EQ(0, state_manager->GetLowEntropySource());
  // Call it another time, just to make sure.
  EXPECT_EQ(0, state_manager->GetLowEntropySource());
}

TEST_F(MetricsStateManagerTest,
       PermutedEntropyCacheClearedWhenLowEntropyReset) {
  const PrefService::Preference* low_entropy_pref =
      prefs_.FindPreference(prefs::kMetricsLowEntropySource);
  const char* kCachePrefName =
      variations::prefs::kVariationsPermutedEntropyCache;
  int low_entropy_value = -1;

  // First, generate an initial low entropy source value.
  {
    EXPECT_TRUE(low_entropy_pref->IsDefaultValue());

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->GetLowEntropySource();

    EXPECT_FALSE(low_entropy_pref->IsDefaultValue());
    EXPECT_TRUE(low_entropy_pref->GetValue()->GetAsInteger(&low_entropy_value));
  }

  // Now, set a dummy value in the permuted entropy cache pref and verify that
  // another call to GetLowEntropySource() doesn't clobber it when
  // --reset-variation-state wasn't specified.
  {
    prefs_.SetString(kCachePrefName, "test");

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->GetLowEntropySource();

    EXPECT_EQ("test", prefs_.GetString(kCachePrefName));
    EXPECT_EQ(low_entropy_value,
              prefs_.GetInteger(prefs::kMetricsLowEntropySource));
  }

  // Verify that the cache does get reset if --reset-variations-state is passed.
  {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kResetVariationState);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->GetLowEntropySource();

    EXPECT_TRUE(prefs_.GetString(kCachePrefName).empty());
  }
}

// Check that setting the kMetricsResetIds pref to true causes the client id to
// be reset. We do not check that the low entropy source is reset because we
// cannot ensure that metrics state manager won't generate the same id again.
TEST_F(MetricsStateManagerTest, ResetMetricsIDs) {
  // Set an initial client id in prefs. It should not be possible for the
  // metrics state manager to generate this id randomly.
  const std::string kInitialClientId = "initial client id";
  prefs_.SetString(prefs::kMetricsClientID, kInitialClientId);

  // Make sure the initial client id isn't reset by the metrics state manager.
  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->ForceClientIdCreation();
    EXPECT_EQ(kInitialClientId, state_manager->client_id());
    EXPECT_FALSE(state_manager->metrics_ids_were_reset_);
  }

  // Set the reset pref to cause the IDs to be reset.
  prefs_.SetBoolean(prefs::kMetricsResetIds, true);

  // Cause the actual reset to happen.
  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->ForceClientIdCreation();
    EXPECT_NE(kInitialClientId, state_manager->client_id());
    EXPECT_TRUE(state_manager->metrics_ids_were_reset_);
    EXPECT_EQ(kInitialClientId, state_manager->previous_client_id_);

    state_manager->GetLowEntropySource();

    EXPECT_FALSE(prefs_.GetBoolean(prefs::kMetricsResetIds));
  }

  EXPECT_NE(kInitialClientId, prefs_.GetString(prefs::kMetricsClientID));
}

TEST_F(MetricsStateManagerTest, ForceClientIdCreation) {
  const int64_t kFakeInstallationDate = 12345;
  prefs_.SetInt64(prefs::kInstallDate, kFakeInstallationDate);

  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

    // client_id shouldn't be auto-generated if metrics reporting is not
    // enabled.
    EXPECT_EQ(std::string(), state_manager->client_id());
    EXPECT_EQ(0, prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp));

    // Confirm that the initial ForceClientIdCreation call creates the client id
    // and backs it up via MockStoreClientInfoBackup.
    EXPECT_FALSE(stored_client_info_backup_);
    state_manager->ForceClientIdCreation();
    EXPECT_NE(std::string(), state_manager->client_id());
    EXPECT_GE(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              test_begin_time_);

    ASSERT_TRUE(stored_client_info_backup_);
    EXPECT_EQ(state_manager->client_id(),
              stored_client_info_backup_->client_id);
    EXPECT_EQ(kFakeInstallationDate,
              stored_client_info_backup_->installation_date);
    EXPECT_EQ(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              stored_client_info_backup_->reporting_enabled_date);
  }
}

TEST_F(MetricsStateManagerTest, LoadPrefs) {
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF";
  client_info.installation_date = 1112;
  client_info.reporting_enabled_date = 2223;
  SetClientInfoPrefs(client_info);

  EnableMetricsReporting();
  {
    EXPECT_FALSE(fake_client_info_backup_);
    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

    // client_id should be auto-obtained from the constructor when metrics
    // reporting is enabled.
    EXPECT_EQ(client_info.client_id, state_manager->client_id());

    // The backup should not be modified.
    ASSERT_FALSE(stored_client_info_backup_);

    // Re-forcing client id creation shouldn't cause another backup and
    // shouldn't affect the existing client id.
    state_manager->ForceClientIdCreation();
    EXPECT_FALSE(stored_client_info_backup_);
    EXPECT_EQ(client_info.client_id, state_manager->client_id());
  }
}

TEST_F(MetricsStateManagerTest, PreferPrefs) {
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF";
  client_info.installation_date = 1112;
  client_info.reporting_enabled_date = 2223;
  SetClientInfoPrefs(client_info);

  ClientInfo client_info2;
  client_info2.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info2.installation_date = 1111;
  client_info2.reporting_enabled_date = 2222;
  SetFakeClientInfoBackup(client_info2);

  EnableMetricsReporting();
  {
    // The backup should be ignored if we already have a client id.

    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    EXPECT_EQ(client_info.client_id, state_manager->client_id());

    // The backup should not be modified.
    ASSERT_FALSE(stored_client_info_backup_);
  }
}

TEST_F(MetricsStateManagerTest, RestoreBackup) {
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEF";
  client_info.installation_date = 1112;
  client_info.reporting_enabled_date = 2223;
  SetClientInfoPrefs(client_info);

  ClientInfo client_info2;
  client_info2.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info2.installation_date = 1111;
  client_info2.reporting_enabled_date = 2222;
  SetFakeClientInfoBackup(client_info2);

  prefs_.ClearPref(prefs::kMetricsClientID);
  prefs_.ClearPref(prefs::kMetricsReportingEnabledTimestamp);

  EnableMetricsReporting();
  {
    // The backup should kick in if the client id has gone missing. It should
    // replace remaining and missing dates as well.

    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    EXPECT_EQ(client_info2.client_id, state_manager->client_id());
    EXPECT_EQ(client_info2.installation_date,
              prefs_.GetInt64(prefs::kInstallDate));
    EXPECT_EQ(client_info2.reporting_enabled_date,
              prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp));

    EXPECT_TRUE(stored_client_info_backup_);
  }
}

TEST_F(MetricsStateManagerTest, ResetBackup) {
  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info.installation_date = 1111;
  client_info.reporting_enabled_date = 2222;

  SetFakeClientInfoBackup(client_info);
  SetClientInfoPrefs(client_info);

  prefs_.SetBoolean(prefs::kMetricsResetIds, true);

  EnableMetricsReporting();
  {
    // Upon request to reset metrics ids, the existing backup should not be
    // restored.

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

    // A brand new client id should have been generated.
    EXPECT_NE(std::string(), state_manager->client_id());
    EXPECT_NE(client_info.client_id, state_manager->client_id());
    EXPECT_TRUE(state_manager->metrics_ids_were_reset_);
    EXPECT_EQ(client_info.client_id, state_manager->previous_client_id_);
    EXPECT_TRUE(stored_client_info_backup_);

    // The installation date should not have been affected.
    EXPECT_EQ(client_info.installation_date,
              prefs_.GetInt64(prefs::kInstallDate));

    // The metrics-reporting-enabled date will be reset to Now().
    EXPECT_GE(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              test_begin_time_);
  }
}

TEST_F(MetricsStateManagerTest, CheckProvider) {
  int64_t kInstallDate = 1373051956;
  int64_t kInstallDateExpected = 1373050800;  // Computed from kInstallDate.
  int64_t kEnabledDate = 1373001211;
  int64_t kEnabledDateExpected = 1373000400;  // Computed from kEnabledDate.

  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info.installation_date = kInstallDate;
  client_info.reporting_enabled_date = kEnabledDate;

  SetFakeClientInfoBackup(client_info);
  SetClientInfoPrefs(client_info);

  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  std::unique_ptr<MetricsProvider> provider = state_manager->GetProvider();
  SystemProfileProto system_profile;
  provider->ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(system_profile.install_date(), kInstallDateExpected);
  EXPECT_EQ(system_profile.uma_enabled_date(), kEnabledDateExpected);

  base::HistogramTester histogram_tester;
  ChromeUserMetricsExtension uma_proto;
  provider->ProvidePreviousSessionData(&uma_proto);
  // The client_id field in the proto should not be overwritten.
  EXPECT_FALSE(uma_proto.has_client_id());
  // Nothing should have been emitted to the cloned install histogram.
  histogram_tester.ExpectTotalCount("UMA.IsClonedInstall", 0);
}

TEST_F(MetricsStateManagerTest, CheckProviderResetIds) {
  int64_t kInstallDate = 1373051956;
  int64_t kInstallDateExpected = 1373050800;  // Computed from kInstallDate.
  int64_t kEnabledDate = 1373001211;
  int64_t kEnabledDateExpected = 1373000400;  // Computed from kEnabledDate.

  ClientInfo client_info;
  client_info.client_id = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  client_info.installation_date = kInstallDate;
  client_info.reporting_enabled_date = kEnabledDate;

  SetFakeClientInfoBackup(client_info);
  SetClientInfoPrefs(client_info);

  // Set the reset pref to cause the IDs to be reset.
  prefs_.SetBoolean(prefs::kMetricsResetIds, true);
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  EXPECT_NE(client_info.client_id, state_manager->client_id());
  EXPECT_TRUE(state_manager->metrics_ids_were_reset_);
  EXPECT_EQ(client_info.client_id, state_manager->previous_client_id_);

  std::unique_ptr<MetricsProvider> provider = state_manager->GetProvider();
  SystemProfileProto system_profile;
  provider->ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(system_profile.install_date(), kInstallDateExpected);
  EXPECT_EQ(system_profile.uma_enabled_date(), kEnabledDateExpected);

  base::HistogramTester histogram_tester;
  ChromeUserMetricsExtension uma_proto;
  provider->ProvidePreviousSessionData(&uma_proto);
  EXPECT_EQ(MetricsLog::Hash(state_manager->previous_client_id_),
            uma_proto.client_id());
  histogram_tester.ExpectUniqueSample("UMA.IsClonedInstall", 1, 1);
}

}  // namespace metrics
