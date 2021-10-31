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
// SbFile.
SbFile CreateAndOpenTemporaryFile(FilePath directory, FilePath *out_path) {
  AssertBlockingAllowed();
  DCHECK(out_path);
  FilePath path = directory.Append(TempFileName());
  FilePath::StringType tmpdir_string = path.value();
  GenerateTempFileName(&tmpdir_string);
  *out_path = FilePath(tmpdir_string);
  return SbFileOpen(tmpdir_string.c_str(), kSbFileCreateOnly | kSbFileWrite,
                    NULL, NULL);
}

// Retries creating a temporary file until it can win the race to create a
// unique one.
SbFile CreateAndOpenTemporaryFileSafely(FilePath directory,
                                        FilePath *out_path) {
  SbFile file = kSbFileInvalid;
  while (!SbFileIsValid(file)) {
    file = CreateAndOpenTemporaryFile(directory, out_path);
  }
  return file;
}

bool CreateTemporaryDirInDirImpl(const FilePath &base_dir,
                                 const FilePath::StringType &name_tmpl,
                                 FilePath *new_dir) {
  AssertBlockingAllowed();
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

bool AbsolutePath(FilePath* path) {
  // We don't have cross-platform tools for this, so we will just return true if
  // the path is already absolute.
  return path->IsAbsolute();
}

bool DeleteFile(const FilePath &path, bool recursive) {
  AssertBlockingAllowed();
  const char *path_str = path.value().c_str();

  bool directory = SbDirectoryCanOpen(path_str);
  if (!recursive || !directory) {
    return SbFileDelete(path_str);
  }

  bool success = true;
  std::stack<std::string> directories;
  directories.push(path.value());

  // NOTE: Right now, for Linux, SbFileGetInfo does not follow
  // symlinks. This is good for avoiding deleting through symlinks, but makes it
  // hard to use symlinks on platforms where they are supported. This seems
  // safest for the lowest-common-denominator approach of pretending symlinks
  // don't exist.
  FileEnumerator traversal(
      path, true, FileEnumerator::FILES | FileEnumerator::DIRECTORIES);

  // Delete all files and push all directories in depth order onto the stack.
  for (FilePath current = traversal.Next();
       success && !current.empty();
       current = traversal.Next()) {
    FileEnumerator::FileInfo info(traversal.GetInfo());

    if (info.IsDirectory()) {
      directories.push(current.value());
    } else {
      success = SbFileDelete(current.value().c_str());
    }
  }

  // Delete all directories in reverse-depth order, now that they have no more
  // regular files.
  while (success && !directories.empty()) {
    success = SbFileDelete(directories.top().c_str());
    directories.pop();
  }

  return success;
}

bool CopyFile(const FilePath &from_path, const FilePath &to_path) {
  AssertBlockingAllowed();

  base::File source_file(from_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!source_file.IsValid()) {
    DPLOG(ERROR) << "CopyFile(): Unable to open source file: "
                 << from_path.value();
    return false;
  }

  base::File destination_file(
      to_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!destination_file.IsValid()) {
    DPLOG(ERROR) << "CopyFile(): Unable to open destination file: "
                 << to_path.value();
    return false;
  }

  const size_t kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);
  bool result = true;

  while (result) {
    int bytes_read = source_file.ReadAtCurrentPos(&buffer[0], buffer.size());
    if (bytes_read < 0) {
      result = false;
      break;
    }

    if (bytes_read == 0) {
      break;
    }

    int bytes_written =
        destination_file.WriteAtCurrentPos(&buffer[0], bytes_read);
    if (bytes_written < bytes_read) {
      DLOG(ERROR) << "CopyFile(): bytes_read (" << bytes_read
                  << ") > bytes_written (" << bytes_written << ")";
      // Because we use a best-effort write, if we wrote less than what was
      // available, something went wrong.
      result = false;
      break;
    }
  }

  return result;
}

bool PathExists(const FilePath &path) {
  AssertBlockingAllowed();
  return SbFileExists(path.value().c_str());
}

bool PathIsWritable(const FilePath &path) {
  AssertBlockingAllowed();
  return SbFileCanOpen(path.value().c_str(), kSbFileOpenAlways | kSbFileWrite);
}

bool DirectoryExists(const FilePath& path) {
  AssertBlockingAllowed();
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

bool CreateTemporaryFile(FilePath *path) {
  AssertBlockingAllowed();
  DCHECK(path);

  FilePath directory;
  if (!GetTempDir(&directory)) {
    return false;
  }

  return CreateTemporaryFileInDir(directory, path);
}

bool CreateTemporaryFileInDir(const FilePath &dir, FilePath *temp_file) {
  AssertBlockingAllowed();
  DCHECK(temp_file);
  SbFile file = CreateAndOpenTemporaryFileSafely(dir, temp_file);
  return (SbFileIsValid(file) && SbFileClose(file));
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
  AssertBlockingAllowed();

  // Fast-path: can the full path be resolved from the full path?
  if (DirectoryExists(full_path) ||
      SbDirectoryCreate(full_path.value().c_str())) {
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

    if (!SbDirectoryCreate(i->value().c_str())) {
      if (error)
        *error = File::OSErrorToFileError(SbSystemGetLastError());
      return false;
    }
  }

  return true;
}

bool IsLink(const FilePath &file_path) {
  AssertBlockingAllowed();
  SbFileInfo info;

  // If we can't SbfileGetPathInfo on the file, it's safe to assume that the
  // file won't at least be a 'followable' link.
  if (!SbFileGetPathInfo(file_path.value().c_str(), &info)) {
    return false;
  }

  return info.is_symbolic_link;
}

bool GetFileInfo(const FilePath &file_path, File::Info *results) {
  AssertBlockingAllowed();
  SbFileInfo info;
  if (!SbFileGetPathInfo(file_path.value().c_str(), &info)) {
    return false;
  }

  results->is_directory = info.is_directory;
  results->size = info.size;
  results->last_modified = base::Time::FromSbTime(info.last_modified);
  results->last_accessed = base::Time::FromSbTime(info.last_accessed);
  results->creation_time = base::Time::FromSbTime(info.creation_time);
  return true;
}

int ReadFile(const FilePath &filename, char *data, int size) {
  AssertBlockingAllowed();

  base::File file(filename, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    DLOG(ERROR) << "ReadFile(" << filename.value() << "): Unable to open.";
    return -1;
  }

  // We use a best-effort read here.
  return file.ReadAtCurrentPos(data, size);
}

int WriteFile(const FilePath &filename, const char *data, int size) {
  AssertBlockingAllowed();

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
  AssertBlockingAllowed();
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

bool HasFileBeenModifiedSince(const FileEnumerator::FileInfo &file_info,
                              const base::Time &cutoff_time) {
  return file_info.GetLastModifiedTime() >= cutoff_time;
}

bool GetCurrentDirectory(FilePath* dir) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);

  // Not supported on Starboard.
  NOTREACHED();
  return false;
}

bool SetCurrentDirectory(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);

  // Not supported on Starboard.
  NOTREACHED();
  return false;
}

FilePath MakeAbsoluteFilePath(const FilePath& input) {
  AssertBlockingAllowed();
  // Only absolute paths are supported in Starboard.
  DCHECK(input.IsAbsolute());
  return input;
}

namespace internal {

bool MoveUnsafe(const FilePath& from_path, const FilePath& to_path) {
  AssertBlockingAllowed();
  // Moving files is not supported in Starboard.
  NOTREACHED();
  return false;
}

}  // internal

}  // namespace base
