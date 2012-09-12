// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/shortcut.h"

#include <windows.h>
#include <objbase.h>

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "base/test/test_shortcut_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const char kFileContents[] = "This is a target.";
static const char kFileContents2[] = "This is another target.";

class ShortcutTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_dir_2_.CreateUniqueTempDir());

    EXPECT_EQ(S_OK, CoInitialize(NULL));

    link_file_ = temp_dir_.path().Append(L"My Link.lnk");

    // Shortcut 1's properties
    {
      const FilePath target_file(temp_dir_.path().Append(L"Target 1.txt"));
      file_util::WriteFile(target_file, kFileContents,
                           arraysize(kFileContents));

      link_properties_.set_target(target_file);
      link_properties_.set_working_dir(temp_dir_.path());
      link_properties_.set_arguments(L"--magic --awesome");
      link_properties_.set_description(L"Chrome is awesome.");
      link_properties_.set_icon(link_properties_.target, 4);
      link_properties_.set_app_id(L"Chrome");
      link_properties_.set_dual_mode(false);
    }

    // Shortcut 2's properties (all different from properties of shortcut 1).
    {
      const FilePath target_file_2(temp_dir_.path().Append(L"Target 2.txt"));
      file_util::WriteFile(target_file_2, kFileContents2,
                           arraysize(kFileContents2));

      FilePath icon_path_2;
      file_util::CreateTemporaryFileInDir(temp_dir_.path(), &icon_path_2);

      link_properties_2_.set_target(target_file_2);
      link_properties_2_.set_working_dir(temp_dir_2_.path());
      link_properties_2_.set_arguments(L"--super --crazy");
      link_properties_2_.set_description(L"The best in the west.");
      link_properties_2_.set_icon(icon_path_2, 0);
      link_properties_2_.set_app_id(L"Chrome.UserLevelCrazySuffix");
      link_properties_2_.set_dual_mode(true);
    }
  }

  virtual void TearDown() OVERRIDE {
    CoUninitialize();
  }

  ScopedTempDir temp_dir_;
  ScopedTempDir temp_dir_2_;

  // The link file to be created/updated in the shortcut tests below.
  FilePath link_file_;

  // Properties for the created shortcut.
  base::win::ShortcutProperties link_properties_;

  // Properties for the updated shortcut.
  base::win::ShortcutProperties link_properties_2_;
};

}  // namespace

TEST_F(ShortcutTest, CreateAndResolveShortcut) {
  base::win::ShortcutProperties only_target_properties;
  only_target_properties.set_target(link_properties_.target);

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, only_target_properties, base::win::SHORTCUT_CREATE_ALWAYS));

  FilePath resolved_name;
  EXPECT_TRUE(base::win::ResolveShortcut(link_file_, &resolved_name, NULL));

  char read_contents[arraysize(kFileContents)];
  file_util::ReadFile(resolved_name, read_contents, arraysize(read_contents));
  EXPECT_STREQ(kFileContents, read_contents);
}

TEST_F(ShortcutTest, ResolveShortcutWithArgs) {
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, link_properties_, base::win::SHORTCUT_CREATE_ALWAYS));

  FilePath resolved_name;
  string16 args;
  EXPECT_TRUE(base::win::ResolveShortcut(link_file_, &resolved_name, &args));

  char read_contents[arraysize(kFileContents)];
  file_util::ReadFile(resolved_name, read_contents, arraysize(read_contents));
  EXPECT_STREQ(kFileContents, read_contents);
  EXPECT_EQ(link_properties_.arguments, args);
}

TEST_F(ShortcutTest, CreateShortcutWithOnlySomeProperties) {
  base::win::ShortcutProperties target_and_args_properties;
  target_and_args_properties.set_target(link_properties_.target);
  target_and_args_properties.set_arguments(link_properties_.arguments);

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, target_and_args_properties,
      base::win::SHORTCUT_CREATE_ALWAYS));

  ASSERT_EQ(base::win::VERIFY_SHORTCUT_SUCCESS,
            base::win::VerifyShortcut(link_file_, target_and_args_properties));
}

TEST_F(ShortcutTest, CreateShortcutVerifyProperties) {
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, link_properties_, base::win::SHORTCUT_CREATE_ALWAYS));

  ASSERT_EQ(base::win::VERIFY_SHORTCUT_SUCCESS,
            base::win::VerifyShortcut(link_file_, link_properties_));
}

TEST_F(ShortcutTest, UpdateShortcutVerifyProperties) {
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, link_properties_, base::win::SHORTCUT_CREATE_ALWAYS));

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, link_properties_2_, base::win::SHORTCUT_UPDATE_EXISTING));

  ASSERT_EQ(base::win::VERIFY_SHORTCUT_SUCCESS,
            base::win::VerifyShortcut(link_file_, link_properties_2_));
}

TEST_F(ShortcutTest, UpdateShortcutUpdateOnlyTargetAndResolve) {
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, link_properties_, base::win::SHORTCUT_CREATE_ALWAYS));

  base::win::ShortcutProperties update_only_target_properties;
  update_only_target_properties.set_target(link_properties_2_.target);

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, update_only_target_properties,
      base::win::SHORTCUT_UPDATE_EXISTING));

  base::win::ShortcutProperties expected_properties = link_properties_;
  expected_properties.set_target(link_properties_2_.target);
  ASSERT_EQ(base::win::VERIFY_SHORTCUT_SUCCESS,
            base::win::VerifyShortcut(link_file_, expected_properties));

  FilePath resolved_name;
  EXPECT_TRUE(base::win::ResolveShortcut(link_file_, &resolved_name, NULL));

  char read_contents[arraysize(kFileContents2)];
  file_util::ReadFile(resolved_name, read_contents, arraysize(read_contents));
  EXPECT_STREQ(kFileContents2, read_contents);
}

TEST_F(ShortcutTest, UpdateShortcutMakeDualMode) {
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, link_properties_, base::win::SHORTCUT_CREATE_ALWAYS));

  base::win::ShortcutProperties make_dual_mode_properties;
  make_dual_mode_properties.set_dual_mode(true);

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, make_dual_mode_properties,
      base::win::SHORTCUT_UPDATE_EXISTING));

  base::win::ShortcutProperties expected_properties = link_properties_;
  expected_properties.set_dual_mode(true);
  ASSERT_EQ(base::win::VERIFY_SHORTCUT_SUCCESS,
            base::win::VerifyShortcut(link_file_, expected_properties));
}

TEST_F(ShortcutTest, UpdateShortcutRemoveDualMode) {
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, link_properties_2_, base::win::SHORTCUT_CREATE_ALWAYS));

  base::win::ShortcutProperties remove_dual_mode_properties;
  remove_dual_mode_properties.set_dual_mode(false);

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, remove_dual_mode_properties,
      base::win::SHORTCUT_UPDATE_EXISTING));

  base::win::ShortcutProperties expected_properties = link_properties_2_;
  expected_properties.set_dual_mode(false);
  ASSERT_EQ(base::win::VERIFY_SHORTCUT_SUCCESS,
            base::win::VerifyShortcut(link_file_, expected_properties));
}

TEST_F(ShortcutTest, UpdateShortcutClearArguments) {
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, link_properties_, base::win::SHORTCUT_CREATE_ALWAYS));

  base::win::ShortcutProperties clear_arguments_properties;
  clear_arguments_properties.set_arguments(string16());

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      link_file_, clear_arguments_properties,
      base::win::SHORTCUT_UPDATE_EXISTING));

  base::win::ShortcutProperties expected_properties = link_properties_;
  expected_properties.set_arguments(string16());
  ASSERT_EQ(base::win::VERIFY_SHORTCUT_SUCCESS,
            base::win::VerifyShortcut(link_file_, expected_properties));
}
