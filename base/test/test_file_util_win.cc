// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <shlwapi.h>
#include <windows.h>

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "base/threading/platform_thread.h"

namespace file_util {

static const ptrdiff_t kOneMB = 1024 * 1024;

bool DieFileDie(const FilePath& file, bool recurse) {
  // It turns out that to not induce flakiness a long timeout is needed.
  const int kTimeoutMs = 10000;

  if (!file_util::PathExists(file))
    return true;

  // Sometimes Delete fails, so try a few more times. Divide the timeout
  // into short chunks, so that if a try succeeds, we won't delay the test
  // for too long.
  for (int i = 0; i < 25; ++i) {
    if (file_util::Delete(file, recurse))
      return true;
    base::PlatformThread::Sleep(kTimeoutMs / 25);
  }
  return false;
}

bool EvictFileFromSystemCache(const FilePath& file) {
  // Request exclusive access to the file and overwrite it with no buffering.
  base::win::ScopedHandle file_handle(
      CreateFile(file.value().c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                 OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL));
  if (!file_handle)
    return false;

  // Get some attributes to restore later.
  BY_HANDLE_FILE_INFORMATION bhi = {0};
  CHECK(::GetFileInformationByHandle(file_handle, &bhi));

  // Execute in chunks. It could be optimized. We want to do few of these since
  // these operations will be slow without the cache.

  // Allocate a buffer for the reads and the writes.
  char* buffer = reinterpret_cast<char*>(VirtualAlloc(NULL,
                                                      kOneMB,
                                                      MEM_COMMIT | MEM_RESERVE,
                                                      PAGE_READWRITE));

  // If the file size isn't a multiple of kOneMB, we'll need special
  // processing.
  bool file_is_aligned = true;
  int total_bytes = 0;
  DWORD bytes_read, bytes_written;
  for (;;) {
    bytes_read = 0;
    ::ReadFile(file_handle, buffer, kOneMB, &bytes_read, NULL);
    if (bytes_read == 0)
      break;

    if (bytes_read < kOneMB) {
      // Zero out the remaining part of the buffer.
      // WriteFile will fail if we provide a buffer size that isn't a
      // sector multiple, so we'll have to write the entire buffer with
      // padded zeros and then use SetEndOfFile to truncate the file.
      ZeroMemory(buffer + bytes_read, kOneMB - bytes_read);
      file_is_aligned = false;
    }

    // Move back to the position we just read from.
    // Note that SetFilePointer will also fail if total_bytes isn't sector
    // aligned, but that shouldn't happen here.
    DCHECK((total_bytes % kOneMB) == 0);
    SetFilePointer(file_handle, total_bytes, NULL, FILE_BEGIN);
    if (!::WriteFile(file_handle, buffer, kOneMB, &bytes_written, NULL) ||
        bytes_written != kOneMB) {
      BOOL freed = VirtualFree(buffer, 0, MEM_RELEASE);
      DCHECK(freed);
      NOTREACHED();
      return false;
    }

    total_bytes += bytes_read;

    // If this is false, then we just processed the last portion of the file.
    if (!file_is_aligned)
      break;
  }

  BOOL freed = VirtualFree(buffer, 0, MEM_RELEASE);
  DCHECK(freed);

  if (!file_is_aligned) {
    // The size of the file isn't a multiple of 1 MB, so we'll have
    // to open the file again, this time without the FILE_FLAG_NO_BUFFERING
    // flag and use SetEndOfFile to mark EOF.
    file_handle.Set(NULL);
    file_handle.Set(CreateFile(file.value().c_str(), GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL));
    CHECK_NE(SetFilePointer(file_handle, total_bytes, NULL, FILE_BEGIN),
             INVALID_SET_FILE_POINTER);
    CHECK(::SetEndOfFile(file_handle));
  }

  // Restore the file attributes.
  CHECK(::SetFileTime(file_handle, &bhi.ftCreationTime, &bhi.ftLastAccessTime,
                      &bhi.ftLastWriteTime));

  return true;
}

// Like CopyFileNoCache but recursively copies all files and subdirectories
// in the given input directory to the output directory.
bool CopyRecursiveDirNoCache(const FilePath& source_dir,
                             const FilePath& dest_dir) {
  // Try to create the directory if it doesn't already exist.
  if (!CreateDirectory(dest_dir)) {
    if (GetLastError() != ERROR_ALREADY_EXISTS)
      return false;
  }

  std::vector<std::wstring> files_copied;

  std::wstring src(source_dir.value());
  file_util::AppendToPath(&src, L"*");

  WIN32_FIND_DATA fd;
  HANDLE fh = FindFirstFile(src.c_str(), &fd);
  if (fh == INVALID_HANDLE_VALUE)
    return false;

  do {
    std::wstring cur_file(fd.cFileName);
    if (cur_file == L"." || cur_file == L"..")
      continue;  // Skip these special entries.

    FilePath cur_source_path = source_dir.Append(cur_file);
    FilePath cur_dest_path = dest_dir.Append(cur_file);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // Recursively copy a subdirectory. We stripped "." and ".." already.
      if (!CopyRecursiveDirNoCache(cur_source_path, cur_dest_path)) {
        FindClose(fh);
        return false;
      }
    } else {
      // Copy the file.
      if (!::CopyFile(cur_source_path.value().c_str(),
                      cur_dest_path.value().c_str(), false)) {
        FindClose(fh);
        return false;
      }

      // We don't check for errors from this function, often, we are copying
      // files that are in the repository, and they will have read-only set.
      // This will prevent us from evicting from the cache, but these don't
      // matter anyway.
      EvictFileFromSystemCache(cur_dest_path);
    }
  } while (FindNextFile(fh, &fd));

  FindClose(fh);
  return true;
}

// Checks if the volume supports Alternate Data Streams. This is required for
// the Zone Identifier implementation.
bool VolumeSupportsADS(const FilePath& path) {
  wchar_t drive[MAX_PATH] = {0};
  wcscpy_s(drive, MAX_PATH, path.value().c_str());

  if (!PathStripToRootW(drive))
    return false;

  DWORD fs_flags = 0;
  if (!GetVolumeInformationW(drive, NULL, 0, 0, NULL, &fs_flags, NULL, 0))
    return false;

  if (fs_flags & FILE_NAMED_STREAMS)
    return true;

  return false;
}

// Return whether the ZoneIdentifier is correctly set to "Internet" (3)
// Only returns a valid result when called from same process as the
// one that (was supposed to have) set the zone identifier.
bool HasInternetZoneIdentifier(const FilePath& full_path) {
  FilePath zone_path(full_path.value() + L":Zone.Identifier");
  std::string zone_path_contents;
  if (!file_util::ReadFileToString(zone_path, &zone_path_contents))
    return false;

  static const char kInternetIdentifier[] = "[ZoneTransfer]\nZoneId=3";
  static const size_t kInternetIdentifierSize =
      // Don't include null byte in size of identifier.
      arraysize(kInternetIdentifier) - 1;

  // Our test is that the initial characters match the above, and that
  // the character after the end of the string is eof, null, or newline; any
  // of those three will invoke the Window Finder cautionary dialog.
  return ((zone_path_contents.compare(0, kInternetIdentifierSize,
                                      kInternetIdentifier) == 0) &&
          (kInternetIdentifierSize == zone_path_contents.length() ||
           zone_path_contents[kInternetIdentifierSize] == '\0' ||
           zone_path_contents[kInternetIdentifierSize] == '\n'));
}

}  // namespace file_util
