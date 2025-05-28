// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_icon_menu_model.h"

#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using base::ASCIIToUTF16;

class StatusIconMenuModelTest : public testing::Test,
                                public StatusIconMenuModel::Observer {
 public:
  void SetUp() override {
    menu_ = std::make_unique<StatusIconMenuModel>(nullptr);
    menu_->AddObserver(this);
  }

  void TearDown() override { menu_->RemoveObserver(this); }

  int changed_count() const { return changed_count_; }

  StatusIconMenuModel* menu_model() { return menu_.get(); }

 private:
  void OnMenuStateChanged() override { ++changed_count_; }

  std::unique_ptr<StatusIconMenuModel> menu_;
  int changed_count_ = 0;
};

TEST_F(StatusIconMenuModelTest, ToggleBooleanProperties) {
  menu_model()->AddItem(0, u"foo");

  menu_model()->SetCommandIdChecked(0, true);
  EXPECT_TRUE(menu_model()->IsCommandIdChecked(0));
  menu_model()->SetCommandIdChecked(0, false);
  EXPECT_FALSE(menu_model()->IsCommandIdChecked(0));

  menu_model()->SetCommandIdEnabled(0, true);
  EXPECT_TRUE(menu_model()->IsCommandIdEnabled(0));
  menu_model()->SetCommandIdEnabled(0, false);
  EXPECT_FALSE(menu_model()->IsCommandIdEnabled(0));

  menu_model()->SetCommandIdVisible(0, true);
  EXPECT_TRUE(menu_model()->IsCommandIdVisible(0));
  menu_model()->SetCommandIdVisible(0, false);
  EXPECT_FALSE(menu_model()->IsCommandIdVisible(0));

  // Menu state should have changed 7 times in this test.
  EXPECT_EQ(7, changed_count());
}

TEST_F(StatusIconMenuModelTest, SetProperties) {
  menu_model()->AddItem(0, u"foo1");
  menu_model()->AddItem(1, u"foo2");

  ui::Accelerator test_accel(ui::VKEY_A, ui::EF_NONE);
  gfx::Image test_image1 = gfx::test::CreateImage(16, 16);
  ui::Accelerator accel_arg;
  ui::ImageModel image_arg;

  EXPECT_FALSE(menu_model()->GetAcceleratorForCommandId(0, &accel_arg));
  EXPECT_TRUE(menu_model()->GetIconForCommandId(0).IsEmpty());
  EXPECT_FALSE(menu_model()->IsItemForCommandIdDynamic(0));

  // Set the accelerator and label for the first menu item.
  menu_model()->SetAcceleratorForCommandId(0, &test_accel);
  EXPECT_TRUE(menu_model()->GetAcceleratorForCommandId(0, &accel_arg));
  EXPECT_EQ(test_accel, accel_arg);

  // Try setting label and changing it. Also ensure that menu item is marked
  // dynamic since the label has changed.
  menu_model()->ChangeLabelForCommandId(0, u"label1");
  EXPECT_TRUE(menu_model()->IsItemForCommandIdDynamic(0));
  EXPECT_EQ(u"label1", menu_model()->GetLabelForCommandId(0));
  menu_model()->ChangeLabelForCommandId(0, u"label2");
  EXPECT_EQ(u"label2", menu_model()->GetLabelForCommandId(0));

  // Try setting icon image and changing it.
  menu_model()->ChangeIconForCommandId(1, test_image1);
  image_arg = menu_model()->GetIconForCommandId(1);
  EXPECT_FALSE(image_arg.IsEmpty());
  EXPECT_TRUE(image_arg.IsImage());
  EXPECT_EQ(image_arg.GetImage().ToImageSkia(), test_image1.ToImageSkia());

  // Ensure changes to one menu item does not affect the other menu item.
  EXPECT_FALSE(menu_model()->GetAcceleratorForCommandId(1, &accel_arg));
  EXPECT_EQ(std::u16string(), menu_model()->GetLabelForCommandId(1));
  EXPECT_TRUE(menu_model()->GetIconForCommandId(0).IsEmpty());

  // Menu state should have changed 6 times in this test.
  EXPECT_EQ(6, changed_count());
}
