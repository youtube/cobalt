// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/strike_database_integrator_test_strike_database.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/strike_databases/payments/virtual_card_enrollment_strike_database.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class VirtualCardEnrollmentStrikeDatabaseTest : public ::testing::Test {
 public:
  VirtualCardEnrollmentStrikeDatabaseTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_service_ = std::make_unique<StrikeDatabase>(
        db_provider_.get(), temp_dir_.GetPath());

    strike_database_ = std::make_unique<VirtualCardEnrollmentStrikeDatabase>(
        strike_database_service_.get());
  }

  void TearDown() override {
    // The destruction of |strike_database_service_|'s components is posted
    // to a task runner, requires running the loop to complete.
    strike_database_.reset();
    strike_database_service_.reset();
    db_provider_.reset();
    task_environment_.RunUntilIdle();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<StrikeDatabase> strike_database_service_;
  std::unique_ptr<VirtualCardEnrollmentStrikeDatabase> strike_database_;
};

TEST_F(VirtualCardEnrollmentStrikeDatabaseTest, AddAndRemoveStrikes) {
  int max_strikes = strike_database_->GetMaxStrikesLimit();
  DCHECK(strike_database_->GetRequiredDelaySinceLastStrike().has_value());
  std::string test_guid = "00000000-0000-0000-0000-000000000001";
  TestAutofillClock test_autofill_clock(AutofillClock::Now());

  EXPECT_EQ(strike_database_->GetStrikes(test_guid), 0);
  // Add one strike for the card.
  strike_database_->AddStrike(test_guid);
  EXPECT_EQ(strike_database_->GetStrikes(test_guid), 1);

  // Verify at this moment, even though the strikes have not reached limit,
  // feature should still be blocked since it is still within the enforced delay
  // period.
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(test_guid));

  // Advance time and verify feature should not be blocked.
  test_autofill_clock.Advance(
      strike_database_->GetRequiredDelaySinceLastStrike().value());
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(test_guid));

  // Add strikes to reach the limit.
  strike_database_->AddStrikes(max_strikes - 1, test_guid);
  EXPECT_EQ(strike_database_->GetStrikes(test_guid), max_strikes);
  EXPECT_EQ(strike_database_->GetMaxStrikesLimit(), max_strikes);

  // Verify at this moment feature should be blocked.
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(test_guid));

  // Remove one strike.
  strike_database_->RemoveStrike(test_guid);
  EXPECT_EQ(strike_database_->GetStrikes(test_guid), max_strikes - 1);

  // Verify feature should be blocked since it is within the enforced delay
  // period.
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(test_guid));

  // Advance time and verify feature should not be blocked.
  test_autofill_clock.Advance(
      strike_database_->GetRequiredDelaySinceLastStrike().value());
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(test_guid));
}

// Test to ensure that IsLastOffer works correctly.
TEST_F(VirtualCardEnrollmentStrikeDatabaseTest, IsLastOffer) {
  int max_strikes = strike_database_->GetMaxStrikesLimit();
  std::string instrument_id = "123";
  ASSERT_EQ(strike_database_->GetStrikes(instrument_id), 0);

  // Adds one strike and check IsLastOffer.
  strike_database_->AddStrike(instrument_id);
  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), 1);
  EXPECT_FALSE(strike_database_->IsLastOffer(instrument_id));

  // Removes existing strikes.
  strike_database_->RemoveStrike(instrument_id);
  ASSERT_EQ(strike_database_->GetStrikes(instrument_id), 0);

  // Adds |max_strikes - 1| strikes to the strike database and check
  // IsLastOffer.
  strike_database_->AddStrikes(max_strikes - 1, instrument_id);
  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), max_strikes - 1);
  EXPECT_TRUE(strike_database_->IsLastOffer(instrument_id));
}

}  // namespace

}  // namespace autofill
