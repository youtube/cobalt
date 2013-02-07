// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if defined(OS_MACOSX)
#include <AvailabilityMacros.h>
#include "base/mac/foundation_util.h"
#elif !defined(OS_ANDROID)
#include <glib.h>
#endif

#include <fstream>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

#if defined(OS_ANDROID)
#include "base/os_compat_android.h"
#endif

#if !defined(OS_IOS)
#include <grp.h>
#endif

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace file_util {

namespace {

#if defined(OS_BSD) || defined(OS_MACOSX)
typedef struct stat stat_wrapper_t;
static int CallStat(const char *path, stat_wrapper_t *sb) {
  base::ThreadRestrictions::AssertIOAllowed();
  return stat(path, sb);
}
static int CallLstat(const char *path, stat_wrapper_t *sb) {
  base::ThreadRestrictions::AssertIOAllowed();
  return lstat(path, sb);
}
#else
typedef struct stat64 stat_wrapper_t;
static int CallStat(const char *path, stat_wrapper_t *sb) {
  base::ThreadRestrictions::AssertIOAllowed();
  return stat64(path, sb);
}
static int CallLstat(const char *path, stat_wrapper_t *sb) {
  base::ThreadRestrictions::AssertIOAllowed();
  return lstat64(path, sb);
}
#endif

// Helper for NormalizeFilePath(), defined below.
bool RealPath(const FilePath& path, FilePath* real_path) {
  base::ThreadRestrictions::AssertIOAllowed();  // For realpath().
  FilePath::CharType buf[PATH_MAX];
  if (!realpath(path.value().c_str(), buf))
    return false;

  *real_path = FilePath(buf);
  return true;
}

// Helper for VerifyPathControlledByUser.
bool VerifySpecificPathControlledByUser(const FilePath& path,
                                        uid_t owner_uid,
                                        const std::set<gid_t>& group_gids) {
  stat_wrapper_t stat_info;
  if (CallLstat(path.value().c_str(), &stat_info) != 0) {
    DPLOG(ERROR) << "Failed to get information on path "
                 << path.value();
    return false;
  }

  if (S_ISLNK(stat_info.st_mode)) {
    DLOG(ERROR) << "Path " << path.value()
               << " is a symbolic link.";
    return false;
  }

  if (stat_info.st_uid != owner_uid) {
    DLOG(ERROR) << "Path " << path.value()
                << " is owned by the wrong user.";
    return false;
  }

  if ((stat_info.st_mode & S_IWGRP) &&
      !ContainsKey(group_gids, stat_info.st_gid)) {
    DLOG(ERROR) << "Path " << path.value()
                << " is writable by an unprivileged group.";
    return false;
  }

  if (stat_info.st_mode & S_IWOTH) {
    DLOG(ERROR) << "Path " << path.value()
                << " is writable by any user.";
    return false;
  }

  return true;
}

}  // namespace

static std::string TempFileName() {
#if defined(OS_MACOSX)
  return StringPrintf(".%s.XXXXXX", base::mac::BaseBundleID());
#endif

#if defined(GOOGLE_CHROME_BUILD)
  return std::string(".com.google.Chrome.XXXXXX");
#else
  return std::string(".org.chromium.Chromium.XXXXXX");
#endif
}

bool AbsolutePath(FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();  // For realpath().
  char full_path[PATH_MAX];
  if (realpath(path->value().c_str(), full_path) == NULL)
    return false;
  *path = FilePath(full_path);
  return true;
}

int CountFilesCreatedAfter(const FilePath& path,
                           const base::Time& comparison_time) {
  base::ThreadRestrictions::AssertIOAllowed();
  int file_count = 0;

  DIR* dir = opendir(path.value().c_str());
  if (dir) {
#if !defined(OS_LINUX) && !defined(OS_MACOSX) && !defined(OS_BSD) && \
    !defined(OS_SOLARIS) && !defined(OS_ANDROID)
  #error Port warning: depending on the definition of struct dirent, \
         additional space for pathname may be needed
#endif
    struct dirent ent_buf;
    struct dirent* ent;
    while (readdir_r(dir, &ent_buf, &ent) == 0 && ent) {
      if ((strcmp(ent->d_name, ".") == 0) ||
          (strcmp(ent->d_name, "..") == 0))
        continue;

      stat_wrapper_t st;
      int test = CallStat(path.Append(ent->d_name).value().c_str(), &st);
      if (test != 0) {
        DPLOG(ERROR) << "stat64 failed";
        continue;
      }
      // Here, we use Time::TimeT(), which discards microseconds. This
      // means that files which are newer than |comparison_time| may
      // be considered older. If we don't discard microseconds, it
      // introduces another issue. Suppose the following case:
      //
      // 1. Get |comparison_time| by Time::Now() and the value is 10.1 (secs).
      // 2. Create a file and the current time is 10.3 (secs).
      //
      // As POSIX doesn't have microsecond precision for |st_ctime|,
      // the creation time of the file created in the step 2 is 10 and
      // the file is considered older than |comparison_time|. After
      // all, we may have to accept either of the two issues: 1. files
      // which are older than |comparison_time| are considered newer
      // (current implementation) 2. files newer than
      // |comparison_time| are considered older.
      if (static_cast<time_t>(st.st_ctime) >= comparison_time.ToTimeT())
        ++file_count;
    }
    closedir(dir);
  }
  return file_count;
}

// TODO(erikkay): The Windows version of this accepts paths like "foo/bar/*"
// which works both with and without the recursive flag.  I'm not sure we need
// that functionality. If not, remove from file_util_win.cc, otherwise add it
// here.
bool Delete(const FilePath& path, bool recursive) {
  base::ThreadRestrictions::AssertIOAllowed();
  const char* path_str = path.value().c_str();
  stat_wrapper_t file_info;
  int test = CallLstat(path_str, &file_info);
  if (test != 0) {
    // The Windows version defines this condition as success.
    bool ret = (errno == ENOENT || errno == ENOTDIR);
    return ret;
  }
  if (!S_ISDIR(file_info.st_mode))
    return (unlink(path_str) == 0);
  if (!recursive)
    return (rmdir(path_str) == 0);

  bool success = true;
  std::stack<std::string> directories;
  directories.push(path.value());
  FileEnumerator traversal(path, true,
      FileEnumerator::FILES | FileEnumerator::DIRECTORIES |
      FileEnumerator::SHOW_SYM_LINKS);
  for (FilePath current = traversal.Next(); success && !current.empty();
       current = traversal.Next()) {
    FileEnumerator::FindInfo info;
    traversal.GetFindInfo(&info);

    if (S_ISDIR(info.stat.st_mode))
      directories.push(current.value());
    else
      success = (unlink(current.value().c_str()) == 0);
  }

  while (success && !directories.empty()) {
    FilePath dir = FilePath(directories.top());
    directories.pop();
    success = (rmdir(dir.value().c_str()) == 0);
  }
  return success;
}

bool Move(const FilePath& from_path, const FilePath& to_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  // Windows compatibility: if to_path exists, from_path and to_path
  // must be the same type, either both files, or both directories.
  stat_wrapper_t to_file_info;
  if (CallStat(to_path.value().c_str(), &to_file_info) == 0) {
    stat_wrapper_t from_file_info;
    if (CallStat(from_path.value().c_str(), &from_file_info) == 0) {
      if (S_ISDIR(to_file_info.st_mode) != S_ISDIR(from_file_info.st_mode))
        return false;
    } else {
      return false;
    }
  }

  if (rename(from_path.value().c_str(), to_path.value().c_str()) == 0)
    return true;

  if (!CopyDirectory(from_path, to_path, true))
    return false;

  Delete(from_path, true);
  return true;
}

bool ReplaceFile(const FilePath& from_path, const FilePath& to_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  return (rename(from_path.value().c_str(), to_path.value().c_str()) == 0);
}

bool CopyDirectory(const FilePath& from_path,
                   const FilePath& to_path,
                   bool recursive) {
  base::ThreadRestrictions::AssertIOAllowed();
  // Some old callers of CopyDirectory want it to support wildcards.
  // After some discussion, we decided to fix those callers.
  // Break loudly here if anyone tries to do this.
  // TODO(evanm): remove this once we're sure it's ok.
  DCHECK(to_path.value().find('*') == std::string::npos);
  DCHECK(from_path.value().find('*') == std::string::npos);

  char top_dir[PATH_MAX];
  if (base::strlcpy(top_dir, from_path.value().c_str(),
                    arraysize(top_dir)) >= arraysize(top_dir)) {
    return false;
  }

  // This function does not properly handle destinations within the source
  FilePath real_to_path = to_path;
  if (PathExists(real_to_path)) {
    if (!AbsolutePath(&real_to_path))
      return false;
  } else {
    real_to_path = real_to_path.DirName();
    if (!AbsolutePath(&real_to_path))
      return false;
  }
  FilePath real_from_path = from_path;
  if (!AbsolutePath(&real_from_path))
    return false;
  if (real_to_path.value().size() >= real_from_path.value().size() &&
      real_to_path.value().compare(0, real_from_path.value().size(),
      real_from_path.value()) == 0)
    return false;

  bool success = true;
  int traverse_type = FileEnumerator::FILES | FileEnumerator::SHOW_SYM_LINKS;
  if (recursive)
    traverse_type |= FileEnumerator::DIRECTORIES;
  FileEnumerator traversal(from_path, recursive, traverse_type);

  // We have to mimic windows behavior here. |to_path| may not exist yet,
  // start the loop with |to_path|.
  FileEnumerator::FindInfo info;
  FilePath current = from_path;
  if (stat(from_path.value().c_str(), &info.stat) < 0) {
    DLOG(ERROR) << "CopyDirectory() couldn't stat source directory: "
                << from_path.value() << " errno = " << errno;
    success = false;
  }
  struct stat to_path_stat;
  FilePath from_path_base = from_path;
  if (recursive && stat(to_path.value().c_str(), &to_path_stat) == 0 &&
      S_ISDIR(to_path_stat.st_mode)) {
    // If the destination already exists and is a directory, then the
    // top level of source needs to be copied.
    from_path_base = from_path.DirName();
  }

  // The Windows version of this function assumes that non-recursive calls
  // will always have a directory for from_path.
  DCHECK(recursive || S_ISDIR(info.stat.st_mode));

  while (success && !current.empty()) {
    // current is the source path, including from_path, so append
    // the suffix after from_path to to_path to create the target_path.
    FilePath target_path(to_path);
    if (from_path_base != current) {
      if (!from_path_base.AppendRelativePath(current, &target_path)) {
        success = false;
        break;
      }
    }

    if (S_ISDIR(info.stat.st_mode)) {
      if (mkdir(target_path.value().c_str(), info.stat.st_mode & 01777) != 0 &&
          errno != EEXIST) {
        DLOG(ERROR) << "CopyDirectory() couldn't create directory: "
                    << target_path.value() << " errno = " << errno;
        success = false;
      }
    } else if (S_ISREG(info.stat.st_mode)) {
      if (!CopyFile(current, target_path)) {
        DLOG(ERROR) << "CopyDirectory() couldn't create file: "
                    << target_path.value();
        success = false;
      }
    } else {
      DLOG(WARNING) << "CopyDirectory() skipping non-regular file: "
                    << current.value();
    }

    current = traversal.Next();
    traversal.GetFindInfo(&info);
  }

  return success;
}

bool PathExists(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();
  return access(path.value().c_str(), F_OK) == 0;
}

bool PathIsWritable(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();
  return access(path.value().c_str(), W_OK) == 0;
}

bool DirectoryExists(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();
  stat_wrapper_t file_info;
  if (CallStat(path.value().c_str(), &file_info) == 0)
    return S_ISDIR(file_info.st_mode);
  return false;
}

// TODO(erikkay): implement
#if 0
bool GetFileCreationLocalTimeFromHandle(int fd,
                                        LPSYSTEMTIME creation_time) {
  if (!file_handle)
    return false;

  FILETIME utc_filetime;
  if (!GetFileTime(file_handle, &utc_filetime, NULL, NULL))
    return false;

  FILETIME local_filetime;
  if (!FileTimeToLocalFileTime(&utc_filetime, &local_filetime))
    return false;

  return !!FileTimeToSystemTime(&local_filetime, creation_time);
}

bool GetFileCreationLocalTime(const std::string& filename,
                              LPSYSTEMTIME creation_time) {
  ScopedHandle file_handle(
      CreateFile(filename.c_str(), GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  return GetFileCreationLocalTimeFromHandle(file_handle.Get(), creation_time);
}
#endif

bool ReadFromFD(int fd, char* buffer, size_t bytes) {
  size_t total_read = 0;
  while (total_read < bytes) {
    ssize_t bytes_read =
        HANDLE_EINTR(read(fd, buffer + total_read, bytes - total_read));
    if (bytes_read <= 0)
      break;
    total_read += bytes_read;
  }
  return total_read == bytes;
}

bool CreateSymbolicLink(const FilePath& target_path,
                        const FilePath& symlink_path) {
  DCHECK(!symlink_path.empty());
  DCHECK(!target_path.empty());
  return ::symlink(target_path.value().c_str(),
                   symlink_path.value().c_str()) != -1;
}

bool ReadSymbolicLink(const FilePath& symlink_path,
                      FilePath* target_path) {
  DCHECK(!symlink_path.empty());
  DCHECK(target_path);
  char buf[PATH_MAX];
  ssize_t count = ::readlink(symlink_path.value().c_str(), buf, arraysize(buf));

  if (count <= 0) {
    target_path->clear();
    return false;
  }

  *target_path = FilePath(FilePath::StringType(buf, count));
  return true;
}

bool GetPosixFilePermissions(const FilePath& path, int* mode) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(mode);

  stat_wrapper_t file_info;
  // Uses stat(), because on symbolic link, lstat() does not return valid
  // permission bits in st_mode
  if (CallStat(path.value().c_str(), &file_info) != 0)
    return false;

  *mode = file_info.st_mode & FILE_PERMISSION_MASK;
  return true;
}

bool SetPosixFilePermissions(const FilePath& path,
                             int mode) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK((mode & ~FILE_PERMISSION_MASK) == 0);

  // Calls stat() so that we can preserve the higher bits like S_ISGID.
  stat_wrapper_t stat_buf;
  if (CallStat(path.value().c_str(), &stat_buf) != 0)
    return false;

  // Clears the existing permission bits, and adds the new ones.
  mode_t updated_mode_bits = stat_buf.st_mode & ~FILE_PERMISSION_MASK;
  updated_mode_bits |= mode & FILE_PERMISSION_MASK;

  if (HANDLE_EINTR(chmod(path.value().c_str(), updated_mode_bits)) != 0)
    return false;

  return true;
}

// Creates and opens a temporary file in |directory|, returning the
// file descriptor. |path| is set to the temporary file path.
// This function does NOT unlink() the file.
int CreateAndOpenFdForTemporaryFile(FilePath directory, FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();  // For call to mkstemp().
  *path = directory.Append(TempFileName());
  const std::string& tmpdir_string = path->value();
  // this should be OK since mkstemp just replaces characters in place
  char* buffer = const_cast<char*>(tmpdir_string.c_str());

  return HANDLE_EINTR(mkstemp(buffer));
}

bool CreateTemporaryFile(FilePath* path) {
  base::ThreadRestrictions::AssertIOAllowed();  // For call to close().
  FilePath directory;
  if (!GetTempDir(&directory))
    return false;
  int fd = CreateAndOpenFdForTemporaryFile(directory, path);
  if (fd < 0)
    return false;
  ignore_result(HANDLE_EINTR(close(fd)));
  return true;
}

FILE* CreateAndOpenTemporaryShmemFile(FilePath* path, bool executable) {
  FilePath directory;
  if (!GetShmemTempDir(&directory, executable))
    return NULL;

  return CreateAndOpenTemporaryFileInDir(directory, path);
}

FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path) {
  int fd = CreateAndOpenFdForTemporaryFile(dir, path);
  if (fd < 0)
    return NULL;

  FILE* file = fdopen(fd, "a+");
  if (!file)
    ignore_result(HANDLE_EINTR(close(fd)));
  return file;
}

bool CreateTemporaryFileInDir(const FilePath& dir, FilePath* temp_file) {
  base::ThreadRestrictions::AssertIOAllowed();  // For call to close().
  int fd = CreateAndOpenFdForTemporaryFile(dir, temp_file);
  return ((fd >= 0) && !HANDLE_EINTR(close(fd)));
}

static bool CreateTemporaryDirInDirImpl(const FilePath& base_dir,
                                        const FilePath::StringType& name_tmpl,
                                        FilePath* new_dir) {
  base::ThreadRestrictions::AssertIOAllowed();  // For call to mkdtemp().
  DCHECK(name_tmpl.find("XXXXXX") != FilePath::StringType::npos)
      << "Directory name template must contain \"XXXXXX\".";

  FilePath sub_dir = base_dir.Append(name_tmpl);
  std::string sub_dir_string = sub_dir.value();

  // this should be OK since mkdtemp just replaces characters in place
  char* buffer = const_cast<char*>(sub_dir_string.c_str());
  char* dtemp = mkdtemp(buffer);
  if (!dtemp) {
    DPLOG(ERROR) << "mkdtemp";
    return false;
  }
  *new_dir = FilePath(dtemp);
  return true;
}

bool CreateTemporaryDirInDir(const FilePath& base_dir,
                             const FilePath::StringType& prefix,
                             FilePath* new_dir) {
  FilePath::StringType mkdtemp_template = prefix;
  mkdtemp_template.append(FILE_PATH_LITERAL("XXXXXX"));
  return CreateTemporaryDirInDirImpl(base_dir, mkdtemp_template, new_dir);
}

bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  FilePath tmpdir;
  if (!GetTempDir(&tmpdir))
    return false;

  return CreateTemporaryDirInDirImpl(tmpdir, TempFileName(), new_temp_path);
}

bool CreateDirectory(const FilePath& full_path) {
  base::ThreadRestrictions::AssertIOAllowed();  // For call to mkdir().
  std::vector<FilePath> subpaths;

  // Collect a list of all parent directories.
  FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (std::vector<FilePath>::reverse_iterator i = subpaths.rbegin();
       i != subpaths.rend(); ++i) {
    if (DirectoryExists(*i))
      continue;
    if (mkdir(i->value().c_str(), 0700) == 0)
      continue;
    // Mkdir failed, but it might have failed with EEXIST, or some other error
    // due to the the directory appearing out of thin air. This can occur if
    // two processes are trying to create the same file system tree at the same
    // time. Check to see if it exists and make sure it is a directory.
    if (!DirectoryExists(*i))
      return false;
  }
  return true;
}

// TODO(rkc): Refactor GetFileInfo and FileEnumerator to handle symlinks
// correctly. http://code.google.com/p/chromium-os/issues/detail?id=15948
bool IsLink(const FilePath& file_path) {
  stat_wrapper_t st;
  // If we can't lstat the file, it's safe to assume that the file won't at
  // least be a 'followable' link.
  if (CallLstat(file_path.value().c_str(), &st) != 0)
    return false;

  if (S_ISLNK(st.st_mode))
    return true;
  else
    return false;
}

bool GetFileInfo(const FilePath& file_path, base::PlatformFileInfo* results) {
  stat_wrapper_t file_info;
  if (CallStat(file_path.value().c_str(), &file_info) != 0)
    return false;
  results->is_directory = S_ISDIR(file_info.st_mode);
  results->size = file_info.st_size;
  results->last_modified = base::Time::FromTimeT(file_info.st_mtime);
  results->last_accessed = base::Time::FromTimeT(file_info.st_atime);
  results->creation_time = base::Time::FromTimeT(file_info.st_ctime);
  return true;
}

bool GetInode(const FilePath& path, ino_t* inode) {
  base::ThreadRestrictions::AssertIOAllowed();  // For call to stat().
  struct stat buffer;
  int result = stat(path.value().c_str(), &buffer);
  if (result < 0)
    return false;

  *inode = buffer.st_ino;
  return true;
}

FILE* OpenFile(const std::string& filename, const char* mode) {
  return OpenFile(FilePath(filename), mode);
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  base::ThreadRestrictions::AssertIOAllowed();
  FILE* result = NULL;
  do {
    result = fopen(filename.value().c_str(), mode);
  } while (!result && errno == EINTR);
  return result;
}

int ReadFile(const FilePath& filename, char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  int fd = HANDLE_EINTR(open(filename.value().c_str(), O_RDONLY));
  if (fd < 0)
    return -1;

  ssize_t bytes_read = HANDLE_EINTR(read(fd, data, size));
  if (int ret = HANDLE_EINTR(close(fd)) < 0)
    return ret;
  return bytes_read;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  int fd = HANDLE_EINTR(creat(filename.value().c_str(), 0666));
  if (fd < 0)
    return -1;

  int bytes_written = WriteFileDescriptor(fd, data, size);
  if (int ret = HANDLE_EINTR(close(fd)) < 0)
    return ret;
  return bytes_written;
}

int WriteFileDescriptor(const int fd, const char* data, int size) {
  // Allow for partial writes.
  ssize_t bytes_written_total = 0;
  for (ssize_t bytes_written_partial = 0; bytes_written_total < size;
       bytes_written_total += bytes_written_partial) {
    bytes_written_partial =
        HANDLE_EINTR(write(fd, data + bytes_written_total,
                           size - bytes_written_total));
    if (bytes_written_partial < 0)
      return -1;
  }

  return bytes_written_total;
}

int AppendToFile(const FilePath& filename, const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  int fd = HANDLE_EINTR(open(filename.value().c_str(), O_WRONLY | O_APPEND));
  if (fd < 0)
    return -1;

  int bytes_written = WriteFileDescriptor(fd, data, size);
  if (int ret = HANDLE_EINTR(close(fd)) < 0)
    return ret;
  return bytes_written;
}

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* dir) {
  // getcwd can return ENOENT, which implies it checks against the disk.
  base::ThreadRestrictions::AssertIOAllowed();

  char system_buffer[PATH_MAX] = "";
  if (!getcwd(system_buffer, sizeof(system_buffer))) {
    NOTREACHED();
    return false;
  }
  *dir = FilePath(system_buffer);
  return true;
}

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();
  int ret = chdir(path.value().c_str());
  return !ret;
}

///////////////////////////////////////////////
// FileEnumerator

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               int file_type)
    : current_directory_entry_(0),
      root_path_(root_path),
      recursive_(recursive),
      file_type_(file_type) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  pending_paths_.push(root_path);
}

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               int file_type,
                               const FilePath::StringType& pattern)
    : current_directory_entry_(0),
      root_path_(root_path),
      recursive_(recursive),
      file_type_(file_type),
      pattern_(root_path.Append(pattern).value()) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  // The Windows version of this code appends the pattern to the root_path,
  // potentially only matching against items in the top-most directory.
  // Do the same here.
  if (pattern.empty())
    pattern_ = FilePath::StringType();
  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() {
}

FilePath FileEnumerator::Next() {
  ++current_directory_entry_;

  // While we've exhausted the entries in the current directory, do the next
  while (current_directory_entry_ >= directory_entries_.size()) {
    if (pending_paths_.empty())
      return FilePath();

    root_path_ = pending_paths_.top();
    root_path_ = root_path_.StripTrailingSeparators();
    pending_paths_.pop();

    std::vector<DirectoryEntryInfo> entries;
    if (!ReadDirectory(&entries, root_path_, file_type_ & SHOW_SYM_LINKS))
      continue;

    directory_entries_.clear();
    current_directory_entry_ = 0;
    for (std::vector<DirectoryEntryInfo>::const_iterator
        i = entries.begin(); i != entries.end(); ++i) {
      FilePath full_path = root_path_.Append(i->filename);
      if (ShouldSkip(full_path))
        continue;

      if (pattern_.size() &&
          fnmatch(pattern_.c_str(), full_path.value().c_str(), FNM_NOESCAPE))
        continue;

      if (recursive_ && S_ISDIR(i->stat.st_mode))
        pending_paths_.push(full_path);

      if ((S_ISDIR(i->stat.st_mode) && (file_type_ & DIRECTORIES)) ||
          (!S_ISDIR(i->stat.st_mode) && (file_type_ & FILES)))
        directory_entries_.push_back(*i);
    }
  }

  return root_path_.Append(directory_entries_[current_directory_entry_
      ].filename);
}

void FileEnumerator::GetFindInfo(FindInfo* info) {
  DCHECK(info);

  if (current_directory_entry_ >= directory_entries_.size())
    return;

  DirectoryEntryInfo* cur_entry = &directory_entries_[current_directory_entry_];
  memcpy(&(info->stat), &(cur_entry->stat), sizeof(info->stat));
  info->filename.assign(cur_entry->filename.value());
}

// static
bool FileEnumerator::IsDirectory(const FindInfo& info) {
  return S_ISDIR(info.stat.st_mode);
}

// static
FilePath FileEnumerator::GetFilename(const FindInfo& find_info) {
  return FilePath(find_info.filename);
}

// static
int64 FileEnumerator::GetFilesize(const FindInfo& find_info) {
  return find_info.stat.st_size;
}

// static
base::Time FileEnumerator::GetLastModifiedTime(const FindInfo& find_info) {
  return base::Time::FromTimeT(find_info.stat.st_mtime);
}

bool FileEnumerator::ReadDirectory(std::vector<DirectoryEntryInfo>* entries,
                                   const FilePath& source, bool show_links) {
  base::ThreadRestrictions::AssertIOAllowed();
  DIR* dir = opendir(source.value().c_str());
  if (!dir)
    return false;

#if !defined(OS_LINUX) && !defined(OS_MACOSX) && !defined(OS_BSD) && \
    !defined(OS_SOLARIS) && !defined(OS_ANDROID)
  #error Port warning: depending on the definition of struct dirent, \
         additional space for pathname may be needed
#endif

  struct dirent dent_buf;
  struct dirent* dent;
  while (readdir_r(dir, &dent_buf, &dent) == 0 && dent) {
    DirectoryEntryInfo info;
    info.filename = FilePath(dent->d_name);

    FilePath full_name = source.Append(dent->d_name);
    int ret;
    if (show_links)
      ret = lstat(full_name.value().c_str(), &info.stat);
    else
      ret = stat(full_name.value().c_str(), &info.stat);
    if (ret < 0) {
      // Print the stat() error message unless it was ENOENT and we're
      // following symlinks.
      if (!(errno == ENOENT && !show_links)) {
        DPLOG(ERROR) << "Couldn't stat "
                     << source.Append(dent->d_name).value();
      }
      memset(&info.stat, 0, sizeof(info.stat));
    }
    entries->push_back(info);
  }

  closedir(dir);
  return true;
}

///////////////////////////////////////////////
// MemoryMappedFile

MemoryMappedFile::MemoryMappedFile()
    : file_(base::kInvalidPlatformFileValue),
      data_(NULL),
      length_(0) {
}

bool MemoryMappedFile::MapFileToMemoryInternal() {
  base::ThreadRestrictions::AssertIOAllowed();

  struct stat file_stat;
  if (fstat(file_, &file_stat) == base::kInvalidPlatformFileValue) {
    DLOG(ERROR) << "Couldn't fstat " << file_ << ", errno " << errno;
    return false;
  }
  length_ = file_stat.st_size;

  data_ = static_cast<uint8*>(
      mmap(NULL, length_, PROT_READ, MAP_SHARED, file_, 0));
  if (data_ == MAP_FAILED)
    DLOG(ERROR) << "Couldn't mmap " << file_ << ", errno " << errno;

  return data_ != MAP_FAILED;
}

void MemoryMappedFile::CloseHandles() {
  base::ThreadRestrictions::AssertIOAllowed();

  if (data_ != NULL)
    munmap(data_, length_);
  if (file_ != base::kInvalidPlatformFileValue)
    ignore_result(HANDLE_EINTR(close(file_)));

  data_ = NULL;
  length_ = 0;
  file_ = base::kInvalidPlatformFileValue;
}

bool HasFileBeenModifiedSince(const FileEnumerator::FindInfo& find_info,
                              const base::Time& cutoff_time) {
  return static_cast<time_t>(find_info.stat.st_mtime) >= cutoff_time.ToTimeT();
}

bool NormalizeFilePath(const FilePath& path, FilePath* normalized_path) {
  FilePath real_path_result;
  if (!RealPath(path, &real_path_result))
    return false;

  // To be consistant with windows, fail if |real_path_result| is a
  // directory.
  stat_wrapper_t file_info;
  if (CallStat(real_path_result.value().c_str(), &file_info) != 0 ||
      S_ISDIR(file_info.st_mode))
    return false;

  *normalized_path = real_path_result;
  return true;
}

#if !defined(OS_MACOSX)
bool GetTempDir(FilePath* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp)
    *path = FilePath(tmp);
  else
#if defined(OS_ANDROID)
    return PathService::Get(base::DIR_CACHE, path);
#else
    *path = FilePath("/tmp");
#endif
  return true;
}

#if !defined(OS_ANDROID)

#if defined(OS_LINUX)
// Determine if /dev/shm files can be mapped and then mprotect'd PROT_EXEC.
// This depends on the mount options used for /dev/shm, which vary among
// different Linux distributions and possibly local configuration.  It also
// depends on details of kernel--ChromeOS uses the noexec option for /dev/shm
// but its kernel allows mprotect with PROT_EXEC anyway.

namespace {

bool DetermineDevShmExecutable() {
  bool result = false;
  FilePath path;
  int fd = CreateAndOpenFdForTemporaryFile(FilePath("/dev/shm"), &path);
  if (fd >= 0) {
    ScopedFD shm_fd_closer(&fd);
    Delete(path, false);
    long sysconf_result = sysconf(_SC_PAGESIZE);
    CHECK_GE(sysconf_result, 0);
    size_t pagesize = static_cast<size_t>(sysconf_result);
    CHECK_GE(sizeof(pagesize), sizeof(sysconf_result));
    void *mapping = mmap(NULL, pagesize, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping != MAP_FAILED) {
      if (mprotect(mapping, pagesize, PROT_READ | PROT_EXEC) == 0)
        result = true;
      munmap(mapping, pagesize);
    }
  }
  return result;
}

};  // namespace
#endif  // defined(OS_LINUX)

bool GetShmemTempDir(FilePath* path, bool executable) {
#if defined(OS_LINUX)
  bool use_dev_shm = true;
  if (executable) {
    static const bool s_dev_shm_executable = DetermineDevShmExecutable();
    use_dev_shm = s_dev_shm_executable;
  }
  if (use_dev_shm) {
    *path = FilePath("/dev/shm");
    return true;
  }
#endif
  return GetTempDir(path);
}
#endif  // !defined(OS_ANDROID)

FilePath GetHomeDir() {
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS())
    return FilePath("/home/chronos/user");
#endif

  const char* home_dir = getenv("HOME");
  if (home_dir && home_dir[0])
    return FilePath(home_dir);

#if defined(OS_ANDROID)
  DLOG(WARNING) << "OS_ANDROID: Home directory lookup not yet implemented.";
#else
  // g_get_home_dir calls getpwent, which can fall through to LDAP calls.
  base::ThreadRestrictions::AssertIOAllowed();

  home_dir = g_get_home_dir();
  if (home_dir && home_dir[0])
    return FilePath(home_dir);
#endif

  FilePath rv;
  if (file_util::GetTempDir(&rv))
    return rv;

  // Last resort.
  return FilePath("/tmp");
}

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  int infile = HANDLE_EINTR(open(from_path.value().c_str(), O_RDONLY));
  if (infile < 0)
    return false;

  int outfile = HANDLE_EINTR(creat(to_path.value().c_str(), 0666));
  if (outfile < 0) {
    ignore_result(HANDLE_EINTR(close(infile)));
    return false;
  }

  const size_t kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);
  bool result = true;

  while (result) {
    ssize_t bytes_read = HANDLE_EINTR(read(infile, &buffer[0], buffer.size()));
    if (bytes_read < 0) {
      result = false;
      break;
    }
    if (bytes_read == 0)
      break;
    // Allow for partial writes
    ssize_t bytes_written_per_read = 0;
    do {
      ssize_t bytes_written_partial = HANDLE_EINTR(write(
          outfile,
          &buffer[bytes_written_per_read],
          bytes_read - bytes_written_per_read));
      if (bytes_written_partial < 0) {
        result = false;
        break;
      }
      bytes_written_per_read += bytes_written_partial;
    } while (bytes_written_per_read < bytes_read);
  }

  if (HANDLE_EINTR(close(infile)) < 0)
    result = false;
  if (HANDLE_EINTR(close(outfile)) < 0)
    result = false;

  return result;
}
#endif  // !defined(OS_MACOSX)

bool VerifyPathControlledByUser(const FilePath& base,
                                const FilePath& path,
                                uid_t owner_uid,
                                const std::set<gid_t>& group_gids) {
  if (base != path && !base.IsParent(path)) {
     DLOG(ERROR) << "|base| must be a subdirectory of |path|.  base = \""
                 << base.value() << "\", path = \"" << path.value() << "\"";
     return false;
  }

  std::vector<FilePath::StringType> base_components;
  std::vector<FilePath::StringType> path_components;

  base.GetComponents(&base_components);
  path.GetComponents(&path_components);

  std::vector<FilePath::StringType>::const_iterator ib, ip;
  for (ib = base_components.begin(), ip = path_components.begin();
       ib != base_components.end(); ++ib, ++ip) {
    // |base| must be a subpath of |path|, so all components should match.
    // If these CHECKs fail, look at the test that base is a parent of
    // path at the top of this function.
    DCHECK(ip != path_components.end());
    DCHECK(*ip == *ib);
  }

  FilePath current_path = base;
  if (!VerifySpecificPathControlledByUser(current_path, owner_uid, group_gids))
    return false;

  for (; ip != path_components.end(); ++ip) {
    current_path = current_path.Append(*ip);
    if (!VerifySpecificPathControlledByUser(
            current_path, owner_uid, group_gids))
      return false;
  }
  return true;
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
bool VerifyPathControlledByAdmin(const FilePath& path) {
  const unsigned kRootUid = 0;
  const FilePath kFileSystemRoot("/");

  // The name of the administrator group on mac os.
  const char* const kAdminGroupNames[] = {
    "admin",
    "wheel"
  };

  // Reading the groups database may touch the file system.
  base::ThreadRestrictions::AssertIOAllowed();

  std::set<gid_t> allowed_group_ids;
  for (int i = 0, ie = arraysize(kAdminGroupNames); i < ie; ++i) {
    struct group *group_record = getgrnam(kAdminGroupNames[i]);
    if (!group_record) {
      DPLOG(ERROR) << "Could not get the group ID of group \""
                   << kAdminGroupNames[i] << "\".";
      continue;
    }

    allowed_group_ids.insert(group_record->gr_gid);
  }

  return VerifyPathControlledByUser(
      kFileSystemRoot, path, kRootUid, allowed_group_ids);
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

}  // namespace file_util
