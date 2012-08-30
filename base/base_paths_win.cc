// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths_win.h"

#include <windows.h>
#include <shlobj.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace base {

bool PathProviderWin(int key, FilePath* result) {

  // We need to go compute the value. It would be nice to support paths with
  // names longer than MAX_PATH, but the system functions don't seem to be
  // designed for it either, with the exception of GetTempPath (but other
  // things will surely break if the temp path is too long, so we don't bother
  // handling it.
  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;

  FilePath cur;
  switch (key) {
    case base::FILE_EXE:
      GetModuleFileName(NULL, system_buffer, MAX_PATH);
      cur = FilePath(system_buffer);
      break;
    case base::FILE_MODULE: {
      // the resource containing module is assumed to be the one that
      // this code lives in, whether that's a dll or exe
      HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
      GetModuleFileName(this_module, system_buffer, MAX_PATH);
      cur = FilePath(system_buffer);
      break;
    }
    case base::DIR_WINDOWS:
      GetWindowsDirectory(system_buffer, MAX_PATH);
      cur = FilePath(system_buffer);
      break;
    case base::DIR_SYSTEM:
      GetSystemDirectory(system_buffer, MAX_PATH);
      cur = FilePath(system_buffer);
      break;
    case base::DIR_PROGRAM_FILESX86:
      if (base::win::OSInfo::GetInstance()->architecture() !=
          base::win::OSInfo::X86_ARCHITECTURE) {
        if (FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILESX86, NULL,
                                   SHGFP_TYPE_CURRENT, system_buffer)))
          return false;
        cur = FilePath(system_buffer);
        break;
      }
      // Fall through to base::DIR_PROGRAM_FILES if we're on an X86 machine.
    case base::DIR_PROGRAM_FILES:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_IE_INTERNET_CACHE:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_INTERNET_CACHE, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_COMMON_START_MENU:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_COMMON_PROGRAMS, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_START_MENU:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAMS, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_APP_DATA:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                                 system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_COMMON_APP_DATA:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_PROFILE:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT,
                                 system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_LOCAL_APP_DATA_LOW:
      if (win::GetVersion() < win::VERSION_VISTA) {
        return false;
      }
      // TODO(nsylvain): We should use SHGetKnownFolderPath instead. Bug 1281128
      if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                                 system_buffer)))
        return false;
      cur = FilePath(system_buffer).DirName().AppendASCII("LocalLow");
      break;
    case base::DIR_LOCAL_APP_DATA:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = FilePath(system_buffer);
      break;
    case base::DIR_SOURCE_ROOT: {
      FilePath executableDir;
      // On Windows, unit tests execute two levels deep from the source root.
      // For example:  chrome/{Debug|Release}/ui_tests.exe
      PathService::Get(base::DIR_EXE, &executableDir);
      cur = executableDir.DirName().DirName();
      break;
    }
    case base::DIR_APP_SHORTCUTS: {
      if (win::GetVersion() < win::VERSION_WIN8)
        return false;

      base::win::ScopedCoMem<wchar_t> path_buf;
      if (FAILED(SHGetKnownFolderPath(FOLDERID_ApplicationShortcuts, 0, NULL,
                                      &path_buf)))
        return false;

      cur = FilePath(string16(path_buf));
      break;
    }
    default:
      return false;
  }

  *result = cur;
  return true;
}

}  // namespace base
