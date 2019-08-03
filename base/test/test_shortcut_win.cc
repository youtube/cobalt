// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_shortcut_win.h"

#include <windows.h>
#include <shlobj.h>
#include <propkey.h>
#include <propvarutil.h>

#include "base/file_path.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

// propsys.lib is required for PropvariantTo*().
#pragma comment(lib, "propsys.lib")

namespace base {
namespace win {

namespace {

// Validates |actual_path|'s LongPathName case-insensitively matches
// |expected_path|'s LongPathName.
void ValidatePathsAreEqual(const FilePath& expected_path,
                           const FilePath& actual_path) {
  wchar_t long_expected_path_chars[MAX_PATH] = {0};
  wchar_t long_actual_path_chars[MAX_PATH] = {0};

  // If |expected_path| is empty confirm immediately that |actual_path| is also
  // empty.
  if (expected_path.empty()) {
    EXPECT_TRUE(actual_path.empty());
    return;
  }

  // Proceed with LongPathName matching which will also confirm the paths exist.
  EXPECT_NE(0U, ::GetLongPathName(
      expected_path.value().c_str(), long_expected_path_chars, MAX_PATH))
          << "Failed to get LongPathName of " << expected_path.value();
  EXPECT_NE(0U, ::GetLongPathName(
      actual_path.value().c_str(), long_actual_path_chars, MAX_PATH))
          << "Failed to get LongPathName of " << actual_path.value();

  FilePath long_expected_path(long_expected_path_chars);
  FilePath long_actual_path(long_actual_path_chars);
  EXPECT_FALSE(long_expected_path.empty());
  EXPECT_FALSE(long_actual_path.empty());

  EXPECT_EQ(long_expected_path, long_actual_path);
}

}  // namespace

void ValidateShortcut(const FilePath& shortcut_path,
                      const ShortcutProperties& properties) {
  ScopedComPtr<IShellLink> i_shell_link;
  ScopedComPtr<IPersistFile> i_persist_file;

  wchar_t read_target[MAX_PATH] = {0};
  wchar_t read_working_dir[MAX_PATH] = {0};
  wchar_t read_arguments[MAX_PATH] = {0};
  wchar_t read_description[MAX_PATH] = {0};
  wchar_t read_icon[MAX_PATH] = {0};
  int read_icon_index = 0;

  HRESULT hr;

  // Initialize the shell interfaces.
  EXPECT_TRUE(SUCCEEDED(hr = i_shell_link.CreateInstance(
      CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER)));
  if (FAILED(hr))
    return;

  EXPECT_TRUE(SUCCEEDED(hr = i_persist_file.QueryFrom(i_shell_link)));
  if (FAILED(hr))
    return;

  // Load the shortcut.
  EXPECT_TRUE(SUCCEEDED(hr = i_persist_file->Load(
      shortcut_path.value().c_str(), 0))) << "Failed to load shortcut at "
                                          << shortcut_path.value();
  if (FAILED(hr))
    return;

  if (properties.options & ShortcutProperties::PROPERTIES_TARGET) {
    EXPECT_TRUE(SUCCEEDED(
        i_shell_link->GetPath(read_target, MAX_PATH, NULL, SLGP_SHORTPATH)));
    ValidatePathsAreEqual(properties.target, FilePath(read_target));
  }

  if (properties.options & ShortcutProperties::PROPERTIES_WORKING_DIR) {
    EXPECT_TRUE(SUCCEEDED(
        i_shell_link->GetWorkingDirectory(read_working_dir, MAX_PATH)));
    ValidatePathsAreEqual(properties.working_dir, FilePath(read_working_dir));
  }

  if (properties.options & ShortcutProperties::PROPERTIES_ARGUMENTS) {
    EXPECT_TRUE(SUCCEEDED(
        i_shell_link->GetArguments(read_arguments, MAX_PATH)));
    EXPECT_EQ(properties.arguments, read_arguments);
  }

  if (properties.options & ShortcutProperties::PROPERTIES_DESCRIPTION) {
    EXPECT_TRUE(SUCCEEDED(
        i_shell_link->GetDescription(read_description, MAX_PATH)));
    EXPECT_EQ(properties.description, read_description);
  }

  if (properties.options & ShortcutProperties::PROPERTIES_ICON) {
    EXPECT_TRUE(SUCCEEDED(
        i_shell_link->GetIconLocation(read_icon, MAX_PATH, &read_icon_index)));
    ValidatePathsAreEqual(properties.icon, FilePath(read_icon));
    EXPECT_EQ(properties.icon_index, read_icon_index);
  }

  if (GetVersion() >= VERSION_WIN7) {
    ScopedComPtr<IPropertyStore> property_store;
    // Note that, as mentioned on MSDN at http://goo.gl/M8h9g, if a property is
    // not set, GetValue will return S_OK and the PROPVARIANT will be set to
    // VT_EMPTY.
    PROPVARIANT pv_app_id, pv_dual_mode;
    EXPECT_TRUE(SUCCEEDED(hr = property_store.QueryFrom(i_shell_link)));
    if (FAILED(hr))
      return;
    EXPECT_EQ(S_OK, property_store->GetValue(PKEY_AppUserModel_ID, &pv_app_id));
    EXPECT_EQ(S_OK, property_store->GetValue(PKEY_AppUserModel_IsDualMode,
                                             &pv_dual_mode));

    // Note, as mentioned on MSDN at
    // http://msdn.microsoft.com/library/windows/desktop/bb776559.aspx, if
    // |pv_app_id| is a VT_EMPTY it is successfully converted to the empty
    // string as desired.
    wchar_t read_app_id[MAX_PATH] = {0};
    PropVariantToString(pv_app_id, read_app_id, MAX_PATH);
    if (properties.options & ShortcutProperties::PROPERTIES_APP_ID)
      EXPECT_EQ(properties.app_id, read_app_id);

    // Note, as mentioned on MSDN at
    // http://msdn.microsoft.com/library/windows/desktop/bb776531.aspx, if
    // |pv_dual_mode| is a VT_EMPTY it is successfully converted to false as
    // desired.
    BOOL read_dual_mode;
    PropVariantToBoolean(pv_dual_mode, &read_dual_mode);
    if (properties.options & ShortcutProperties::PROPERTIES_DUAL_MODE)
      EXPECT_EQ(properties.dual_mode, static_cast<bool>(read_dual_mode));
  }
}

}  // namespace win
}  // namespace base
