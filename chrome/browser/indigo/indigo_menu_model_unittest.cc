// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_menu_model.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace indigo {

namespace {

class MockIndigoPageActionController : public IndigoPageActionController {
 public:
  MockIndigoPageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller)
      : IndigoPageActionController(tab_interface, page_action_controller) {}
  MOCK_METHOD(void, OpenSettings, (), (override));
};

class IndigoMenuModelTest : public testing::Test {
 protected:
  IndigoMenuModelTest() {
    feature_list_.InitAndEnableFeature(features::kIndigo);
  }
  ~IndigoMenuModelTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    tab_interface_ =
        std::make_unique<page_actions::FakeTabInterface>(profile_.get());
    page_action_controller_ = std::make_unique<
        testing::NiceMock<page_actions::MockPageActionController>>();

    ON_CALL(*tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    controller_ =
        std::make_unique<testing::NiceMock<MockIndigoPageActionController>>(
            *tab_interface_, *page_action_controller_);
  }

  void TearDown() override {
    controller_.reset();
    page_action_controller_.reset();
    tab_interface_.reset();
    profile_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<page_actions::FakeTabInterface> tab_interface_;
  std::unique_ptr<testing::NiceMock<page_actions::MockPageActionController>>
      page_action_controller_;
  std::unique_ptr<testing::NiceMock<MockIndigoPageActionController>>
      controller_;
};

TEST_F(IndigoMenuModelTest, MenuStructureAndCommands) {
  IndigoMenuModel model(profile_.get(), controller_->GetWeakPtr());

  // Expected items:
  // 0: Dismiss (command ID 1)
  // 1: Separator (command ID -1)
  // 2: Settings (command ID 2)
  ASSERT_EQ(model.GetItemCount(), 3u);

  EXPECT_EQ(model.GetCommandIdAt(0), 1);
  EXPECT_EQ(model.GetTypeAt(0), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(model.GetLabelAt(0), l10n_util::GetStringUTF16(
                                     IDS_INDIGO_ANCHORED_MESSAGE_MENU_DISMISS));
  ui::ImageModel icon0 = model.GetIconAt(0);
  EXPECT_FALSE(icon0.IsEmpty());

  EXPECT_EQ(model.GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_EQ(model.GetCommandIdAt(2), 2);
  EXPECT_EQ(model.GetTypeAt(2), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(
      model.GetLabelAt(2),
      l10n_util::GetStringUTF16(IDS_INDIGO_ANCHORED_MESSAGE_MENU_SETTINGS));
  ui::ImageModel icon2 = model.GetIconAt(2);
  EXPECT_FALSE(icon2.IsEmpty());
}

TEST_F(IndigoMenuModelTest, ExecuteDismissCommand) {
  IndigoMenuModel model(profile_.get(), controller_->GetWeakPtr());

  // Executing the dismiss command (command ID 1) should call
  // HideAnchoredMessage on the page action controller.
  EXPECT_CALL(*page_action_controller_, HideAnchoredMessage(kActionIndigo))
      .Times(1);

  model.ExecuteCommand(1, 0);
}

TEST_F(IndigoMenuModelTest, ExecuteSettingsCommand) {
  IndigoMenuModel model(profile_.get(), controller_->GetWeakPtr());

  EXPECT_CALL(*controller_, OpenSettings()).Times(1);

  // Executing the settings command (command ID 2).
  model.ExecuteCommand(2, 0);
}

}  // namespace

}  // namespace indigo
