// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/shortcut.h"

#include <shellapi.h>
#include <shlobj.h>

#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

bool CreateOrUpdateShortcutLink(const FilePath& shortcut_path,
                                const ShortcutProperties& properties,
                                ShortcutOperation operation) {
  base::ThreadRestrictions::AssertIOAllowed();

  // A target is required when |operation| is SHORTCUT_CREATE_ALWAYS.
  if (operation == SHORTCUT_CREATE_ALWAYS &&
      !(properties.options & ShortcutProperties::PROPERTIES_TARGET)) {
    NOTREACHED();
    return false;
  }

  ScopedComPtr<IShellLink> i_shell_link;
  ScopedComPtr<IPersistFile> i_persist_file;
  if (FAILED(i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                         CLSCTX_INPROC_SERVER)) ||
      FAILED(i_persist_file.QueryFrom(i_shell_link))) {
    return false;
  }

  if (operation == SHORTCUT_UPDATE_EXISTING &&
      FAILED(i_persist_file->Load(shortcut_path.value().c_str(),
                                  STGM_READWRITE))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_TARGET) &&
      FAILED(i_shell_link->SetPath(properties.target.value().c_str()))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_WORKING_DIR) &&
      FAILED(i_shell_link->SetWorkingDirectory(
          properties.working_dir.value().c_str()))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_ARGUMENTS) &&
      FAILED(i_shell_link->SetArguments(properties.arguments.c_str()))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_DESCRIPTION) &&
      FAILED(i_shell_link->SetDescription(properties.description.c_str()))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_ICON) &&
      FAILED(i_shell_link->SetIconLocation(properties.icon.value().c_str(),
                                           properties.icon_index))) {
    return false;
  }

  bool has_app_id =
      (properties.options & ShortcutProperties::PROPERTIES_APP_ID) != 0;
  bool has_dual_mode =
      (properties.options & ShortcutProperties::PROPERTIES_DUAL_MODE) != 0;
  if ((has_app_id || has_dual_mode) &&
      GetVersion() >= VERSION_WIN7) {
    ScopedComPtr<IPropertyStore> property_store;
    if (FAILED(property_store.QueryFrom(i_shell_link)) || !property_store.get())
      return false;

    if (has_app_id &&
        !SetAppIdForPropertyStore(property_store, properties.app_id.c_str())) {
      return false;
    }
    if (has_dual_mode &&
        !SetDualModeForPropertyStore(property_store, properties.dual_mode)) {
      return false;
    }
  }

  HRESULT result = i_persist_file->Save(shortcut_path.value().c_str(), TRUE);

  // If we successfully updated the icon, notify the shell that we have done so.
  if (operation == SHORTCUT_UPDATE_EXISTING && SUCCEEDED(result)) {
    // Release the interfaces in case the SHChangeNotify call below depends on
    // the operations above being fully completed.
    i_persist_file.Release();
    i_shell_link.Release();

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  }

  return SUCCEEDED(result);
}

bool ResolveShortcut(const FilePath& shortcut_path,
                     FilePath* target_path,
                     string16* args) {
  base::ThreadRestrictions::AssertIOAllowed();

  HRESULT result;
  ScopedComPtr<IShellLink> i_shell_link;

  // Get pointer to the IShellLink interface.
  result = i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                       CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;

  ScopedComPtr<IPersistFile> persist;
  // Query IShellLink for the IPersistFile interface.
  result = persist.QueryFrom(i_shell_link);
  if (FAILED(result))
    return false;

  // Load the shell link.
  result = persist->Load(shortcut_path.value().c_str(), STGM_READ);
  if (FAILED(result))
    return false;

  WCHAR temp[MAX_PATH];
  if (target_path) {
    // Try to find the target of a shortcut.
    result = i_shell_link->Resolve(0, SLR_NO_UI);
    if (FAILED(result))
      return false;

    result = i_shell_link->GetPath(temp, MAX_PATH, NULL, SLGP_UNCPRIORITY);
    if (FAILED(result))
      return false;

    *target_path = FilePath(temp);
  }

  if (args) {
    result = i_shell_link->GetArguments(temp, MAX_PATH);
    if (FAILED(result))
      return false;

    *args = string16(temp);
  }
  return true;
}

bool TaskbarPinShortcutLink(const wchar_t* shortcut) {
  base::ThreadRestrictions::AssertIOAllowed();

  // "Pin to taskbar" is only supported after Win7.
  if (GetVersion() < VERSION_WIN7)
    return false;

  int result = reinterpret_cast<int>(ShellExecute(NULL, L"taskbarpin", shortcut,
      NULL, NULL, 0));
  return result > 32;
}

bool TaskbarUnpinShortcutLink(const wchar_t* shortcut) {
  base::ThreadRestrictions::AssertIOAllowed();

  // "Unpin from taskbar" is only supported after Win7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  int result = reinterpret_cast<int>(ShellExecute(NULL, L"taskbarunpin",
      shortcut, NULL, NULL, 0));
  return result > 32;
}

}  // namespace win
}  // namespace base
