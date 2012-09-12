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
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"

// propsys.lib is required for PropvariantTo*().
#pragma comment(lib, "propsys.lib")

namespace base {
namespace win {

namespace {

// Returns true if |actual_path|'s LongPathName case-insensitively matches
// |expected_path|'s LongPathName.
bool PathsAreEqual(const FilePath& expected_path, const FilePath& actual_path) {
  wchar_t long_expected_path_chars[MAX_PATH] = {0};
  wchar_t long_actual_path_chars[MAX_PATH] = {0};

  if (::GetLongPathName(
          expected_path.value().c_str(), long_expected_path_chars,
          MAX_PATH) == 0 ||
      ::GetLongPathName(
          actual_path.value().c_str(), long_actual_path_chars,
          MAX_PATH) == 0) {
    return false;
  }

  FilePath long_expected_path(long_expected_path_chars);
  FilePath long_actual_path(long_actual_path_chars);
  if(long_expected_path.empty() || long_actual_path.empty())
    return false;

  return long_expected_path == long_actual_path;
}

}  // namespace

VerifyShortcutStatus VerifyShortcut(const FilePath& shortcut_path,
                                    const ShortcutProperties& properties) {
  ScopedComPtr<IShellLink> i_shell_link;
  ScopedComPtr<IPersistFile> i_persist_file;

  wchar_t read_target[MAX_PATH] = {0};
  wchar_t read_working_dir[MAX_PATH] = {0};
  wchar_t read_arguments[MAX_PATH] = {0};
  wchar_t read_description[MAX_PATH] = {0};
  wchar_t read_icon[MAX_PATH] = {0};
  int read_icon_index = 0;

  // Initialize the shell interfaces.
  if (FAILED(i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                         CLSCTX_INPROC_SERVER)) ||
      FAILED(i_persist_file.QueryFrom(i_shell_link))) {
    return VERIFY_SHORTCUT_FAILURE_UNEXPECTED;
  }

  // Load the shortcut.
  if (FAILED(i_persist_file->Load(shortcut_path.value().c_str(), 0)))
    return VERIFY_SHORTCUT_FAILURE_FILE_NOT_FOUND;

  if ((properties.options & ShortcutProperties::PROPERTIES_TARGET) &&
       (FAILED(i_shell_link->GetPath(
            read_target, MAX_PATH, NULL, SLGP_SHORTPATH)) ||
        !PathsAreEqual(properties.target, FilePath(read_target)))) {
    return VERIFY_SHORTCUT_FAILURE_TARGET;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_WORKING_DIR) &&
      (FAILED(i_shell_link->GetWorkingDirectory(read_working_dir, MAX_PATH)) ||
       FilePath(read_working_dir) != properties.working_dir)) {
    return VERIFY_SHORTCUT_FAILURE_WORKING_DIR;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_ARGUMENTS) &&
      (FAILED(i_shell_link->GetArguments(read_arguments, MAX_PATH)) ||
       string16(read_arguments) != properties.arguments)) {
    return VERIFY_SHORTCUT_FAILURE_ARGUMENTS;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_DESCRIPTION) &&
      (FAILED(i_shell_link->GetDescription(read_description, MAX_PATH)) ||
       string16(read_description) != properties.description)) {
    return VERIFY_SHORTCUT_FAILURE_DESCRIPTION;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_ICON) &&
      (FAILED(i_shell_link->GetIconLocation(read_icon, MAX_PATH,
                                            &read_icon_index)) ||
       read_icon_index != properties.icon_index ||
       !PathsAreEqual(properties.icon, FilePath(read_icon)))) {
    return VERIFY_SHORTCUT_FAILURE_ICON;
  }

  if(GetVersion() >= VERSION_WIN7) {
    ScopedComPtr<IPropertyStore> property_store;
    // Note that, as mentioned on MSDN at http://goo.gl/M8h9g, if a property is
    // not set, GetValue will return S_OK and the PROPVARIANT will be set to
    // VT_EMPTY.
    PROPVARIANT pv_app_id, pv_dual_mode;
    if (FAILED(property_store.QueryFrom(i_shell_link)) ||
        property_store->GetValue(PKEY_AppUserModel_ID, &pv_app_id) != S_OK ||
        property_store->GetValue(PKEY_AppUserModel_IsDualMode,
                                 &pv_dual_mode) != S_OK) {
      return VERIFY_SHORTCUT_FAILURE_UNEXPECTED;
    }

    // Note, as mentioned on MSDN at http://goo.gl/hZ3sO, if |pv_app_id| is a
    // VT_EMPTY it is successfully converted to the empty string.
    wchar_t read_app_id[MAX_PATH] = {0};
    PropVariantToString(pv_app_id, read_app_id, MAX_PATH);
    if((properties.options & ShortcutProperties::PROPERTIES_APP_ID) &&
       string16(read_app_id) != properties.app_id) {
      return VERIFY_SHORTCUT_FAILURE_APP_ID;
    }

    // Note, as mentioned on MSDN at http://goo.gl/9mBHB, if |pv_dual_mode| is a
    // VT_EMPTY it is successfully converted to false.
    BOOL read_dual_mode;
    PropVariantToBoolean(pv_dual_mode, &read_dual_mode);
    if((properties.options & ShortcutProperties::PROPERTIES_DUAL_MODE) &&
       static_cast<bool>(read_dual_mode) != properties.dual_mode) {
      return VERIFY_SHORTCUT_FAILURE_DUAL_MODE;
    }
  }

  return VERIFY_SHORTCUT_SUCCESS;
}

}  // namespace win
}  // namespace base
