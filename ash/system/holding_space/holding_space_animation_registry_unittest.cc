// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_animation_registry.h"

#include <array>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/system/progress_indicator/progress_icon_animation.h"
#include "ash/system/progress_indicator/progress_indicator_animation.h"
#include "ash/system/progress_indicator/progress_ring_animation.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

constexpr char kTestUser[] = "user@test";

// HoldingSpaceAnimationRegistryTest -------------------------------------------

class HoldingSpaceAnimationRegistryTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Initialize holding space for `kTestUser`.
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        user_account, client(), model());
    GetSessionControllerClient()->AddUserSession(kTestUser);
    holding_space_prefs::MarkTimeOfFirstAvailability(
        GetSessionControllerClient()->GetUserPrefService(user_account));
  }

  HoldingSpaceItem* AddItem(
      HoldingSpaceItem::Type type,
      const base::FilePath& path,
      const HoldingSpaceProgress& progress = HoldingSpaceProgress()) {
    GURL file_system_url(
        base::StrCat({"filesystem:", path.BaseName().value()}));
    std::unique_ptr<HoldingSpaceItem> item =
        HoldingSpaceItem::CreateFileBackedItem(
            type, path, file_system_url, progress,
            base::BindOnce(
                [](HoldingSpaceItem::Type type, const base::FilePath& path) {
                  return std::make_unique<HoldingSpaceImage>(
                      holding_space_util::GetMaxImageSizeForType(type), path,
                      /*async_bitmap_resolver=*/base::DoNothing());
                }));
    HoldingSpaceItem* item_ptr = item.get();
    model()->AddItem(std::move(item));
    return item_ptr;
  }

  void ExpectProgressIconAnimationExistsForKey(const void* key, bool exists) {
    auto* animation = registry()->GetProgressIconAnimationForKey(key);
    EXPECT_EQ(!!animation, exists);
  }

  void ExpectProgressIconAnimationHasAnimatedForKey(const void* key,
                                                    bool has_animated) {
    auto* animation = registry()->GetProgressIconAnimationForKey(key);
    EXPECT_EQ(animation && animation->HasAnimated(), has_animated);
  }

  void ExpectProgressRingAnimationOfTypeForKey(
      const void* key,
      const absl::optional<ProgressRingAnimation::Type>& type) {
    auto* animation = registry()->GetProgressRingAnimationForKey(key);
    EXPECT_EQ(!!animation, type.has_value());
    if (animation && type.has_value())
      EXPECT_EQ(animation->type(), type.value());
  }

  void StartSession() {
    AccountId user_account = AccountId::FromUserEmail(kTestUser);
    GetSessionControllerClient()->SwitchActiveUser(user_account);
  }

  void EnableTrayIconPreviews() {
    AccountId account_id = AccountId::FromUserEmail(kTestUser);
    auto* prefs = GetSessionControllerClient()->GetUserPrefService(account_id);
    ASSERT_TRUE(prefs);
    holding_space_prefs::SetPreviewsEnabled(prefs, true);
  }

  HoldingSpaceController* controller() { return HoldingSpaceController::Get(); }

  testing::NiceMock<MockHoldingSpaceClient>* client() {
    return &holding_space_client_;
  }

  HoldingSpaceModel* model() { return &holding_space_model_; }

  HoldingSpaceAnimationRegistry* registry() {
    return HoldingSpaceAnimationRegistry::GetInstance();
  }

 private:
  testing::NiceMock<MockHoldingSpaceClient> holding_space_client_;
  HoldingSpaceModel holding_space_model_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_F(HoldingSpaceAnimationRegistryTest, ProgressIndicatorAnimations) {
  using Type = ProgressRingAnimation::Type;

  StartSession();
  EnableTrayIconPreviews();

  // Verify initial animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), false);
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);

  // Add a completed item to the `model()`.
  HoldingSpaceItem* item_0 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/0"));

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), false);
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);

  // Add an indeterminately in-progress item to the `model()`.
  HoldingSpaceItem* item_1 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/1"),
              HoldingSpaceProgress(0, absl::nullopt));

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), true);
  ExpectProgressIconAnimationHasAnimatedForKey(controller(), true);
  ExpectProgressRingAnimationOfTypeForKey(controller(), Type::kIndeterminate);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, true);
  ExpectProgressIconAnimationHasAnimatedForKey(item_1, false);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kIndeterminate);

  // Add a determinately in-progress item to the `model()`.
  HoldingSpaceItem* item_2 =
      AddItem(HoldingSpaceItem::Type::kDownload, base::FilePath("/tmp/2"),
              HoldingSpaceProgress(0, 10));

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), true);
  ExpectProgressIconAnimationHasAnimatedForKey(controller(), true);
  ExpectProgressRingAnimationOfTypeForKey(controller(), Type::kIndeterminate);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, true);
  ExpectProgressIconAnimationHasAnimatedForKey(item_1, false);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kIndeterminate);
  ExpectProgressIconAnimationExistsForKey(item_2, true);
  ExpectProgressIconAnimationHasAnimatedForKey(item_2, false);
  ExpectProgressRingAnimationOfTypeForKey(item_2, absl::nullopt);

  // Complete the first in-progress item.
  model()->UpdateItem(item_1->id())->SetProgress(HoldingSpaceProgress(10, 10));

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), true);
  ExpectProgressIconAnimationHasAnimatedForKey(controller(), true);
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, false);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kPulse);
  ExpectProgressIconAnimationExistsForKey(item_2, true);
  ExpectProgressIconAnimationHasAnimatedForKey(item_2, false);
  ExpectProgressRingAnimationOfTypeForKey(item_2, absl::nullopt);

  // Complete the second in-progress item.
  model()->UpdateItem(item_2->id())->SetProgress(HoldingSpaceProgress(10, 10));

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), false);
  ExpectProgressRingAnimationOfTypeForKey(controller(), Type::kPulse);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, false);
  ExpectProgressRingAnimationOfTypeForKey(item_1, Type::kPulse);
  ExpectProgressIconAnimationExistsForKey(item_2, false);
  ExpectProgressRingAnimationOfTypeForKey(item_2, Type::kPulse);

  // Wait for `kPulse` animations to complete.
  base::test::RepeatingTestFuture<ProgressRingAnimation*> future;
  std::array<base::CallbackListSubscription, 3u> subscriptions = {
      registry()->AddProgressRingAnimationChangedCallbackForKey(
          controller(), future.GetCallback()),
      registry()->AddProgressRingAnimationChangedCallbackForKey(
          item_1, future.GetCallback()),
      registry()->AddProgressRingAnimationChangedCallbackForKey(
          item_2, future.GetCallback())};
  for (size_t i = 0u; i < std::size(subscriptions); ++i)
    EXPECT_EQ(future.Take(), nullptr);

  // Verify animation `registry()` state.
  ExpectProgressIconAnimationExistsForKey(controller(), false);
  ExpectProgressRingAnimationOfTypeForKey(controller(), absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_0, false);
  ExpectProgressRingAnimationOfTypeForKey(item_0, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_1, false);
  ExpectProgressRingAnimationOfTypeForKey(item_1, absl::nullopt);
  ExpectProgressIconAnimationExistsForKey(item_2, false);
  ExpectProgressRingAnimationOfTypeForKey(item_2, absl::nullopt);
}

}  // namespace ash
