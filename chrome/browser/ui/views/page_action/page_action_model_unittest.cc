// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_model.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace page_actions {
namespace {

using ::actions::ActionItem;

const std::u16string kOverrideText = u"Override";
const std::u16string kTestText = u"Test";
const std::u16string kTooltipText = u"Tooltip";
const ui::ImageModel kTestImage =
    ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(/*size=*/16));

base::PassKey<PageActionController> PassKey() {
  return PageActionController::PassKeyForTesting();
}

class MockPageActionModelObserver : public PageActionModelObserver {
 public:
  MOCK_METHOD(void,
              OnPageActionModelChanged,
              (const PageActionModelInterface& model),
              (override));
  MOCK_METHOD(void,
              OnPageActionModelWillBeDeleted,
              (const PageActionModelInterface& model),
              (override));
};

class PageActionModelTest : public ::testing::Test {
 protected:
  void SetUp() override { model_.AddObserver(&observer_); }

  void TearDown() override { model_.RemoveObserver(&observer_); }

  PageActionModel model_;
  testing::NiceMock<MockPageActionModelObserver> observer_;
};

TEST_F(PageActionModelTest, DefaultVisibility) {
  EXPECT_FALSE(model_.GetVisible());
}

TEST_F(PageActionModelTest, VisibilityConditions) {
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(4);

  model_.SetShowRequested(PassKey(), true);
  EXPECT_FALSE(model_.GetVisible());

  model_.SetActionItemProperties(
      PassKey(),
      ActionItem::Builder().SetEnabled(true).SetVisible(true).Build().get());
  EXPECT_FALSE(model_.GetVisible());

  model_.SetTabActive(PassKey(), true);
  EXPECT_TRUE(model_.GetVisible());

  model_.SetHasPinnedIcon(PassKey(), true);
  EXPECT_FALSE(model_.GetVisible());
}

TEST_F(PageActionModelTest, ChipVisibility) {
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(2);

  model_.SetShowSuggestionChip(PassKey(), true);
  EXPECT_EQ(model_.GetShowSuggestionChip(), true);

  model_.SetShowSuggestionChip(PassKey(), false);
  EXPECT_EQ(model_.GetShowSuggestionChip(), false);
}

TEST_F(PageActionModelTest, OverrideText) {
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(2);

  model_.SetOverrideText(PassKey(), kOverrideText);
  EXPECT_EQ(model_.GetText(), kOverrideText);

  model_.SetOverrideText(PassKey(), std::nullopt);
  EXPECT_EQ(model_.GetText(), std::u16string());
}

TEST_F(PageActionModelTest, SetActionItemProperties) {
  // NOTE: The visibility is exercised by the test VisibilityConditions.
  EXPECT_CALL(observer_, OnPageActionModelChanged).Times(1);

  model_.SetActionItemProperties(PassKey(), ActionItem::Builder()
                                                .SetText(kTestText)
                                                .SetTooltipText(kTooltipText)
                                                .SetImage(kTestImage)
                                                .Build()
                                                .get());

  EXPECT_EQ(model_.GetText(), kTestText);
  EXPECT_EQ(model_.GetImage(), kTestImage);
  EXPECT_EQ(model_.GetTooltipText(), kTooltipText);
}

}  // namespace
}  // namespace page_actions
