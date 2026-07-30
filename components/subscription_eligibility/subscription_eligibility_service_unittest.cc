// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/subscription_eligibility_service.h"

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_command_line.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subscription_eligibility {

class SubscriptionEligibilityServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    auto registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
    prefs::RegisterProfilePrefs(registry.get());
    pref_service_.registry()->RegisterIntegerPref(prefs::kAiSubscriptionTier,
                                                  0);
  }

  TestingPrefServiceSimple pref_service_;
};

// Tests that by default (no command-line flag), the service returns the pref
// value.
TEST_F(SubscriptionEligibilityServiceTest, GetAiSubscriptionTier_Default) {
  pref_service_.SetInteger(prefs::kAiSubscriptionTier, 42);
  SubscriptionEligibilityService service(&pref_service_);
  EXPECT_EQ(service.GetAiSubscriptionTier(), 42);
}

// Tests that when the command-line flag is set, it overrides the pref value.
TEST_F(SubscriptionEligibilityServiceTest, GetAiSubscriptionTier_Forced) {
  pref_service_.SetInteger(prefs::kAiSubscriptionTier, 42);

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kForceAiSubscriptionTier, "100");

  SubscriptionEligibilityService service(&pref_service_);
  EXPECT_EQ(service.GetAiSubscriptionTier(), 100);
}

// Tests that when the command-line flag is set to an invalid integer, it falls
// back to the pref value.
TEST_F(SubscriptionEligibilityServiceTest, GetAiSubscriptionTier_Invalid) {
  pref_service_.SetInteger(prefs::kAiSubscriptionTier, 42);

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kForceAiSubscriptionTier, "invalid_value");

  SubscriptionEligibilityService service(&pref_service_);
  EXPECT_EQ(service.GetAiSubscriptionTier(), 42);
}

}  // namespace subscription_eligibility
