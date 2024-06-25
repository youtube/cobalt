// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Adapted from file_util_posix.cc.

#include "base/files/file_util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stack>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "base/strings/strcat.h"
#include "starboard/system.h"

namespace base {

namespace {

// The list of portable filename characters as per POSIX. This should be the
// lowest-common-denominator of acceptable filename characters.
const char kPortableFilenameCharacters[] = {
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789"
  "_-"
};
const int kPortableFilenameCharactersLength =
    SB_ARRAY_SIZE_INT(kPortableFilenameCharacters) - 1;

// 8 characters = 65 ^ 8 possible filenames, which gives a nice wide space for
// avoiding collisions.
const char kTempSubstitution[] = "XXXXXXXX";
const int kTempSubstitutionLength = SB_ARRAY_SIZE_INT(kTempSubstitution) - 1;

std::string TempFileName() {
  return std::string(".com.youtube.Cobalt.XXXXXXXX");
}

// Takes the template defined in |in_out_template| and destructively replaces
// any 'X' characters at the end with randomized portable filename characters.
void GenerateTempFileName(FilePath::StringType *in_out_template) {
  size_t template_start = in_out_template->find(kTempSubstitution);
  if (template_start == FilePath::StringType::npos) {
    // No pattern.
    return;
  }

  for (int i = 0; i < kTempSubstitutionLength; ++i) {
    uint64_t random = SbSystemGetRandomUInt64();
    int index = random % kPortableFilenameCharactersLength;
    (*in_out_template)[template_start + i] = kPortableFilenameCharacters[index];
  }
}

// Creates a random filename based on the TempFileName() pattern, creates the
// file, leaving it open, placing the path in |out_path| and returning the open
// file.
int CreateAndOpenTemporaryFile(FilePath directory, FilePath *out_path) {
  internal::AssertBlockingAllowed();
  DCHECK(out_path);
  FilePath path = directory.Append(TempFileName());
  FilePath::StringType tmpdir_string = path.value();
  GenerateTempFileName(&tmpdir_string);
  *out_path = FilePath(tmpdir_string);
  return open(tmpdir_string.c_str(), O_CREAT | O_EXCL | O_WRONLY,
              S_IRUSR | S_IWUSR);
}

// Retries creating a temporary file until it can win the race to create a
// unique one.
int CreateAndOpenTemporaryFileSafely(FilePath directory,
                                        FilePath *out_path) {
  int file = -1;
  while (file < 0) {
    file = CreateAndOpenTemporaryFile(directory, out_path);
  }
  return file;
}

bool CreateTemporaryDirInDirImpl(const FilePath &base_dir,
                                 const FilePath::StringType &name_tmpl,
                                 FilePath *new_dir) {
  internal::AssertBlockingAllowed();
  DCHECK(name_tmpl.find(kTempSubstitution) != FilePath::StringType::npos)
      << "Directory name template must contain \"XXXXXXXX\".";

  FilePath dir_template = base_dir.Append(name_tmpl);
  std::string dir_template_string = dir_template.value();
  FilePath sub_dir;
  while (true) {
    std::string sub_dir_string = dir_template_string;
    GenerateTempFileName(&sub_dir_string);
    sub_dir = FilePath(sub_dir_string);
    if (!DirectoryExists(sub_dir)) {
      break;
    }
  }

  // NOTE: This is not as secure as mkdtemp, because it doesn't atomically
  // guarantee that there is no collision. But, with 8 X's it should be good
  // enough for our purposes.
  if (!CreateDirectory(sub_dir)) {
    DPLOG(ERROR) << "CreateDirectory";
    return false;
  }

  *new_dir = sub_dir;
  return true;
}

}  // namespace

FilePath FormatTemporaryFileName(FilePath::StringPieceType identifier) {
  StringPiece prefix = ".com.youtube.Cobalt.XXXXXXXX";
  return FilePath(StrCat({".", prefix, ".", identifier}));
}

bool AbsolutePath(FilePath* path) {
  // We don't have cross-platform tools for this, so we will just return true if
  // the path is already absolute.
  return path->IsAbsolute();
}

// Borrowed from file_util_posix.cc
bool DoDeleteFile(const FilePath& path, bool recursive) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  // Reset errno in the beginning of the funciton
  errno = 0;

  const char* path_str = path.value().c_str();
  stat_wrapper_t file_info;
  if (::stat(path_str, &file_info) != 0) {
    return (errno == ENOENT || errno == ENOTDIR);
  }
  if (!S_ISDIR(file_info.st_mode)) {
    return (unlink(path_str) == 0) || (errno == ENOENT);
  }
  if (!recursive){
    return (rmdir(path_str) == 0) || (errno == ENOENT);
  }

  bool success = true;
  stack<std::string> directories;
  directories.push(path.value());
  FileEnumerator traversal(path, true,
      FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
  for (FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    if (traversal.GetInfo().IsDirectory()) {
      directories.push(current.value());
    }
    else {
      success &= (unlink(current.value().c_str()) == 0) || (errno == ENOENT);
    }
  }

  while (!directories.empty()) {
    FilePath dir = FilePath(directories.top());
    directories.pop();
    success &= (rmdir(dir.value().c_str()) == 0) || (errno == ENOENT);
  }
  return success;
}

bool DeleteFile(const FilePath& path) {
  return DoDeleteFile(path, /*recursive=*/false);
}

bool DeletePathRecursively(const FilePath& path) {
  return DoDeleteFile(path, /*recursive=*/true);
}

bool DieFileDie(const FilePath& file, bool recurse) {
  // There is no need to workaround Windows problems on POSIX.
  // Just pass-through.
  if (recurse)
    return DeletePathRecursively(file);
  return DeleteFile(file);
}

bool EvictFileFromSystemCache(const FilePath& file) {
  NOTIMPLEMENTED();
  return false;
}

bool ReplaceFile(const FilePath& from_path,
                 const FilePath& to_path,
                 File::Error* error) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  return true;
}

// Mac has its own implementation, this is for all other Posix systems.
bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  File infile;
  infile = File(from_path, File::FLAG_OPEN | File::FLAG_READ);
  if (!infile.IsValid())
    return false;

  File outfile(to_path, File::FLAG_WRITE | File::FLAG_CREATE_ALWAYS);
  if (!outfile.IsValid())
    return false;

  return CopyFileContents(infile, outfile);
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  return nullptr;
}

bool PathExists(const FilePath &path) {
  internal::AssertBlockingAllowed();
  struct ::stat file_info;
  return ::stat(path.value().c_str(), &file_info) == 0;
}

bool PathIsReadable(const FilePath &path) {
  internal::AssertBlockingAllowed();
  return SbFileCanOpen(path.value().c_str(), kSbFileOpenAlways | kSbFileRead);
}

bool PathIsWritable(const FilePath &path) {
  internal::AssertBlockingAllowed();
  return SbFileCanOpen(path.value().c_str(), kSbFileOpenAlways | kSbFileWrite);
}

bool DirectoryExists(const FilePath& path) {
  internal::AssertBlockingAllowed();
  SbFileInfo info;
  if (!SbFileGetPathInfo(path.value().c_str(), &info)) {
    return false;
  }

  return info.is_directory;
}

bool GetTempDir(FilePath *path) {
  std::vector<char> buffer(kSbFileMaxPath + 1, 0);
  bool result =
      SbSystemGetPath(kSbSystemPathTempDirectory, buffer.data(), buffer.size());
  if (!result) {
    return false;
  }

  *path = FilePath(buffer.data());
  if (DirectoryExists(*path)) {
    return true;
  }

  return CreateDirectory(*path);
}

bool GetShmemTempDir(FilePath *path, bool executable) {
  return GetTempDir(path);
}

FilePath GetHomeDir() {
  FilePath path;
  bool result = PathService::Get(base::DIR_CACHE, &path);
  DCHECK(result);
  return path;
}

ScopedFILE CreateAndOpenTemporaryStreamInDir(const FilePath& dir,
                                             FilePath* path) {
  ScopedFILE stream;
  return stream;
}

File CreateAndOpenTemporaryFileInDir(const FilePath &dir, FilePath *temp_file) {
  internal::AssertBlockingAllowed();
  DCHECK(temp_file);
  int file = CreateAndOpenTemporaryFileSafely(dir, temp_file);
  return file >= 0 ? File(std::move(file)) : File(File::GetLastFileError());
}

bool CreateTemporaryFileInDir(const FilePath &dir, FilePath *temp_file) {
  internal::AssertBlockingAllowed();
  DCHECK(temp_file);
  int file = CreateAndOpenTemporaryFileSafely(dir, temp_file);
  return ((file >= 0) && !::close(file));
}

bool CreateTemporaryDirInDir(const FilePath &base_dir,
                             const FilePath::StringType &prefix,
                             FilePath *new_dir) {
  FilePath::StringType mkdtemp_template = prefix + kTempSubstitution;
  return CreateTemporaryDirInDirImpl(base_dir, mkdtemp_template, new_dir);
}

bool CreateNewTempDirectory(const FilePath::StringType &prefix,
                            FilePath *new_temp_path) {
  FilePath tmpdir;
  if (!GetTempDir(&tmpdir)) {
    return false;
  }

  return CreateTemporaryDirInDirImpl(tmpdir, TempFileName(), new_temp_path);
}

bool CreateDirectoryAndGetError(const FilePath &full_path, File::Error* error) {
  internal::AssertBlockingAllowed();

  // Fast-path: can the full path be resolved from the full path?
  if (mkdir(full_path.value().c_str(), 0700) == 0
      || DirectoryExists(full_path)) {
    return true;
  }

  // Slow-path: iterate through the paths and resolve from the root
  // to the leaf.
  std::vector<FilePath> subpaths;

  // Collect a list of all parent directories.
  FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (FilePath path = full_path.DirName();
       path.value() != last_path.value();
       path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (std::vector<FilePath>::reverse_iterator i = subpaths.rbegin();
       i != subpaths.rend(); ++i) {
    if (DirectoryExists(*i)) {
      continue;
    }
    
    struct ::stat info;
    if (mkdir(i->value().c_str(), 0700) != 0 &&
        !(::stat(i->value().c_str(), &info) == 0 && S_ISDIR(info.st_mode))){
      if (error)
        *error = File::OSErrorToFileError(SbSystemGetLastError());
      return false;
    }
  }

  return true;
}

bool NormalizeFilePath(const FilePath& path, FilePath* normalized_path) {
  internal::AssertBlockingAllowed();
  // Only absolute paths are supported in Starboard.
  if (!path.IsAbsolute()) {
    return false;
  }
  *normalized_path = path;
  return true;
}

bool IsLink(const FilePath &file_path) {
  internal::AssertBlockingAllowed();
  SbFileInfo info;

  // If we can't SbfileGetPathInfo on the file, it's safe to assume that the
  // file won't at least be a 'followable' link.
  if (!SbFileGetPathInfo(file_path.value().c_str(), &info)) {
    return false;
  }

  return info.is_symbolic_link;
}

bool GetFileInfo(const FilePath &file_path, File::Info *results) {
  stat_wrapper_t file_info;
    if (::stat(file_path.value().c_str(), &file_info) != 0)
      return false;

  results->FromStat(file_info);
  return true;
}

int ReadFile(const FilePath &filename, char *data, int size) {
  internal::AssertBlockingAllowed();

  base::File file(filename, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    DLOG(ERROR) << "ReadFile(" << filename.value() << "): Unable to open.";
    return -1;
  }

  // We use a best-effort read here.
  return file.ReadAtCurrentPos(data, size);
}

int WriteFile(const FilePath &filename, const char *data, int size) {
  internal::AssertBlockingAllowed();

  base::File file(
      filename, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    DLOG(ERROR) << "WriteFile(" << filename.value() << "): Unable to open.";
    return -1;
  }

  // We use a best-effort write here.
  return file.WriteAtCurrentPos(data, size);
}

bool AppendToFile(const FilePath &filename, const char *data, int size) {
  internal::AssertBlockingAllowed();
  base::File file(filename, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    DLOG(ERROR) << "AppendToFile(" << filename.value() << "): Unable to open.";
    return false;
  }

  if (file.Seek(base::File::FROM_END, 0) == -1) {
    DLOG(ERROR) << "AppendToFile(" << filename.value()
                << "): Unable to truncate.";
    return false;
  }

  return file.WriteAtCurrentPos(data, size) == size;
}

bool AppendToFile(const FilePath& filename, StringPiece data) {
  return AppendToFile(filename, data.data(), data.size());
}

bool HasFileBeenModifiedSince(const FileEnumerator::FileInfo &file_info,
                              const base::Time &cutoff_time) {
  return file_info.GetLastModifiedTime() >= cutoff_time;
}

bool GetCurrentDirectory(FilePath* dir) {
  // ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);

  // Not supported on Starboard.
  NOTREACHED();
  return false;
}

bool SetCurrentDirectory(const FilePath& path) {
  // ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);

  // Not supported on Starboard.
  NOTREACHED();
  return false;
}

FilePath MakeAbsoluteFilePath(const FilePath& input) {
  internal::AssertBlockingAllowed();
  // Only absolute paths are supported in Starboard.
  DCHECK(input.IsAbsolute());
  return input;
}

namespace internal {

bool MoveUnsafe(const FilePath& from_path, const FilePath& to_path) {
  internal::AssertBlockingAllowed();
  // Moving files is not supported in Starboard.
  NOTREACHED();
  return false;
}

}  // internal

}  // namespace base
