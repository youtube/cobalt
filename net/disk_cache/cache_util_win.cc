// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include <windows.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/win/scoped_handle.h"

namespace {

// Deletes all the files on path that match search_name pattern.
void DeleteFiles(const wchar_t* path, const wchar_t* search_name) {
  std::wstring name(path);
  file_util::AppendToPath(&name, search_name);

  WIN32_FIND_DATA data;
  HANDLE handle = FindFirstFile(name.c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE)
    return;

  std::wstring adjusted_path(path);
  adjusted_path += L'\\';
  do {
    if (data.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY ||
        data.dwFileAttributes == FILE_ATTRIBUTE_REPARSE_POINT)
      continue;
    std::wstring current(adjusted_path);
    current += data.cFileName;
    DeleteFile(current.c_str());
  } while (FindNextFile(handle, &data));

  FindClose(handle);
}

}  // namespace

namespace disk_cache {

bool MoveCache(const FilePath& from_path, const FilePath& to_path) {
  // I don't want to use the shell version of move because if something goes
  // wrong, that version will attempt to move file by file and fail at the end.
  if (!MoveFileEx(from_path.value().c_str(), to_path.value().c_str(), 0)) {
    LOG(ERROR) << "Unable to move the cache: " << GetLastError();
    return false;
  }
  return true;
}

void DeleteCache(const FilePath& path, bool remove_folder) {
  DeleteFiles(path.value().c_str(), L"*");
  if (remove_folder)
    RemoveDirectory(path.value().c_str());
}

bool DeleteCacheFile(const FilePath& name) {
  // We do a simple delete, without ever falling back to SHFileOperation, as the
  // version from base does.
  if (!DeleteFile(name.value().c_str())) {
    // There is an error, but we share delete access so let's see if there is a
    // file to open. Note that this code assumes that we have a handle to the
    // file at all times (even now), so nobody can have a handle that prevents
    // us from opening the file again (unless it was deleted).
    DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD access = SYNCHRONIZE;
    base::win::ScopedHandle file(CreateFile(
        name.value().c_str(), access, sharing, NULL, OPEN_EXISTING, 0, NULL));
    if (file.IsValid())
      return false;

    // Most likely there is no file to open... and that's what we wanted.
  }
  return true;
}

}  // namespace disk_cache
