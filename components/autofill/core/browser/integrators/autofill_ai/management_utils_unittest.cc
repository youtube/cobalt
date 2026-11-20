// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// Tests that `GetEntityInstancesForSettings()` drops pContext entities.
TEST(ManagementUtilsTest, GetEntityInstancesForSettings) {
  EntityInstance local_passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
  EntityInstance wallet_vehicle = test::GetVehicleEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance pcontext_order = test::GetOrderEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  EXPECT_THAT(GetEntityInstancesForSettings(
                  {local_passport, pcontext_order, wallet_vehicle}),
              testing::ElementsAre(local_passport, wallet_vehicle));
}

}  // namespace
}  // namespace autofill
