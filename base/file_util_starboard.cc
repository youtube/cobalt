// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/file_util.h"

#include <stack>
#include <string>

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/system.h"

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
  base::ThreadRestrictions::AssertIOAllowed();
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
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(name_tmpl.find(kTempSubstitution) != FilePath::StringType::npos)
      << "Directory name template must contain \"XXXXXXXX\".";

  FilePath dir_template = base_dir.Append(name_tmpl);
  std::string dir_template_string = dir_template.value();
  FilePath sub_dir;
  while (true) {
    std::string sub_dir_string = dir_template_string;
    GenerateTempFileName(&sub_dir_string);
    sub_dir = FilePath(sub_dir_string);
    if (!file_util::DirectoryExists(sub_dir)) {
      break;
    }
  }

  // NOTE: This is not as secure as mkdtemp, because it doesn't atomically
  // guarantee that there is no collision. But, with 8 X's it should be good
  // enough for our purposes.
  if (!file_util::CreateDirectory(sub_dir)) {
    DPLOG(ERROR) << "CreateDirectory";
    return false;
  }

  *new_dir = sub_dir;
  return true;
}

}  // namespace

namespace file_util {

bool AbsolutePath(FilePath* path) {
  // We don't have cross-platform tools for this, so we will just return true if
  // the path is already absolute.
  return path->IsAbsolute();
}

bool Delete(const FilePath &path, bool recursive) {
  base::ThreadRestrictions::AssertIOAllowed();
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
    FileEnumerator::FindInfo info;
    traversal.GetFindInfo(&info);

    if (FileEnumerator::IsDirectory(info)) {
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
  base::ThreadRestrictions::AssertIOAllowed();

  base::PlatformFile source_file = base::CreatePlatformFile(
      from_path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ, NULL,
      NULL);
  if (source_file == base::kInvalidPlatformFileValue) {
    DPLOG(ERROR) << "CopyFile(): Unable to open source file: "
                 << from_path.value();
    return false;
  }

  base::PlatformFile destination_file = base::CreatePlatformFileUnsafe(
      to_path, base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE,
      NULL, NULL);
  if (destination_file == base::kInvalidPlatformFileValue) {
    DPLOG(ERROR) << "CopyFile(): Unable to open destination file: "
                 << to_path.value();
    ignore_result(base::ClosePlatformFile(destination_file));
    return false;
  }

  const size_t kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);
  bool result = true;

  while (result) {
    int bytes_read = base::ReadPlatformFileAtCurrentPos(source_file, &buffer[0],
                                                        buffer.size());
    if (bytes_read < 0) {
      result = false;
      break;
    }

    if (bytes_read == 0) {
      break;
    }

    int bytes_written = base::WritePlatformFileAtCurrentPos(
        destination_file, &buffer[0], bytes_read);
    if (bytes_written < bytes_read) {
      DLOG(ERROR) << "CopyFile(): bytes_read (" << bytes_read
                  << ") > bytes_written (" << bytes_written << ")";
      // Because we use a best-effort write, if we wrote less than what was
      // available, something went wrong.
      result = false;
      break;
    }
  }

  if (!base::ClosePlatformFile(source_file)) {
    result = false;
  }

  if (!base::ClosePlatformFile(destination_file)) {
    result = false;
  }

  return result;
}

bool PathExists(const FilePath &path) {
  base::ThreadRestrictions::AssertIOAllowed();
  return SbFileExists(path.value().c_str());
}

bool PathIsWritable(const FilePath &path) {
  base::ThreadRestrictions::AssertIOAllowed();
  return SbFileCanOpen(path.value().c_str(), kSbFileOpenAlways | kSbFileWrite);
}

bool DirectoryExists(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();
  SbFileInfo info;
  if (!SbFileGetPathInfo(path.value().c_str(), &info)) {
    return false;
  }

  return info.is_directory;
}

bool GetTempDir(FilePath *path) {
  char buffer[SB_FILE_MAX_PATH + 1] = {0};
  bool result = SbSystemGetPath(kSbSystemPathTempDirectory, buffer,
                                SB_ARRAY_SIZE_INT(buffer));
  if (!result) {
    return false;
  }

  *path = FilePath(buffer);
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
  bool result = PathService::Get(base::DIR_HOME, &path);
  DCHECK(result);
  return path;
}

bool CreateTemporaryFile(FilePath *path) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(path);

  FilePath directory;
  if (!GetTempDir(&directory)) {
    return false;
  }

  return CreateTemporaryFileInDir(directory, path);
}

bool CreateTemporaryFileInDir(const FilePath &dir, FilePath *temp_file) {
  base::ThreadRestrictions::AssertIOAllowed();
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

bool CreateDirectory(const FilePath &full_path) {
  base::ThreadRestrictions::AssertIOAllowed();
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
      return false;
    }
  }

  return true;
}

bool IsLink(const FilePath &file_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  SbFileInfo info;

  // If we can't SbfileGetPathInfo on the file, it's safe to assume that the
  // file won't at least be a 'followable' link.
  if (!SbFileGetPathInfo(file_path.value().c_str(), &info)) {
    return false;
  }

  return info.is_symbolic_link;
}

bool GetFileInfo(const FilePath &file_path, base::PlatformFileInfo *results) {
  base::ThreadRestrictions::AssertIOAllowed();
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
  base::ThreadRestrictions::AssertIOAllowed();

  base::PlatformFile file = base::CreatePlatformFile(
      filename, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ, NULL,
      NULL);
  if (file == base::kInvalidPlatformFileValue) {
    DLOG(ERROR) << "ReadFile(" << filename.value() << "): Unable to open.";
    return -1;
  }

  // We use a best-effort read here.
  int bytes_read = base::ReadPlatformFileAtCurrentPos(file, data, size);
  if (!base::ClosePlatformFile(file)) {
    DLOG(ERROR) << "ReadFile(" << filename.value() << "): Unable to close.";
    return -1;
  }

  return bytes_read;
}

int WriteFile(const FilePath &filename, const char *data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();

  base::PlatformFile file = base::CreatePlatformFile(
      filename, base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE,
      NULL, NULL);
  if (file == base::kInvalidPlatformFileValue) {
    DLOG(ERROR) << "WriteFile(" << filename.value() << "): Unable to open.";
    return -1;
  }

  // We use a best-effort write here.
  int bytes_written = base::WritePlatformFileAtCurrentPos(file, data, size);
  if (!base::ClosePlatformFile(file)) {
    DLOG(ERROR) << "WriteFile(" << filename.value() << "): Unable to close.";
    return -1;
  }

  return bytes_written;
}

int AppendToFile(const FilePath &filename, const char *data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::PlatformFile file = base::CreatePlatformFile(
      filename, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE, NULL,
      NULL);
  if (file == base::kInvalidPlatformFileValue) {
    DLOG(ERROR) << "AppendToFile(" << filename.value() << "): Unable to open.";
    return -1;
  }

  if (!base::SeekPlatformFile(file, base::PLATFORM_FILE_FROM_END, 0)) {
    DLOG(ERROR) << "AppendToFile(" << filename.value()
                << "): Unable to truncate.";
    return -1;
  }

  int bytes_written = base::WritePlatformFileAtCurrentPos(file, data, size);
  if (!base::ClosePlatformFile(file)) {
    DLOG(ERROR) << "AppendToFile(" << filename.value() << "): Unable to close.";
    return -1;
  }

  return bytes_written;
}

bool HasFileBeenModifiedSince(const FileEnumerator::FindInfo &find_info,
                              const base::Time &cutoff_time) {
  return FileEnumerator::GetLastModifiedTime(find_info) >= cutoff_time;
}


///////////////////////////////////////////////
// FileEnumerator

FileEnumerator::FileEnumerator(const FilePath &root_path,
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

FileEnumerator::FileEnumerator(const FilePath &root_path,
                               bool recursive,
                               int file_type,
                               const FilePath::StringType &pattern)
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
  if (pattern.empty()) {
    pattern_ = FilePath::StringType();
  }
  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() {
}

FilePath FileEnumerator::Next() {
  ++current_directory_entry_;

  // While we've exhausted the entries in the current directory, do the next
  while (current_directory_entry_ >= directory_entries_.size()) {
    if (pending_paths_.empty()) {
      return FilePath();
    }

    root_path_ = pending_paths_.top();
    root_path_ = root_path_.StripTrailingSeparators();
    pending_paths_.pop();

    std::vector<DirectoryEntryInfo> entries;
    if (!ReadDirectory(&entries, root_path_)) {
      continue;
    }

    directory_entries_.clear();
    current_directory_entry_ = 0;
    for (std::vector<DirectoryEntryInfo>::const_iterator i = entries.begin();
         i != entries.end(); ++i) {
      FilePath full_path = root_path_.Append(i->filename);
      if (ShouldSkip(full_path)) {
        continue;
      }

      if (pattern_.size()) {
        NOTREACHED() << "Patterns not supported in Starboard.";
        continue;
      }

      if (recursive_ && i->sb_info.is_directory) {
        pending_paths_.push(full_path);
      }

      if ((i->sb_info.is_directory && (file_type_ & DIRECTORIES)) ||
          (!i->sb_info.is_directory && (file_type_ & FILES))) {
        directory_entries_.push_back(*i);
      }
    }
  }

  return
      root_path_.Append(directory_entries_[current_directory_entry_].filename);
}

void FileEnumerator::GetFindInfo(FindInfo *info) {
  DCHECK(info);

  if (current_directory_entry_ >= directory_entries_.size()) {
    return;
  }

  DirectoryEntryInfo *cur_entry = &directory_entries_[current_directory_entry_];
  memcpy(&(info->sb_info), &(cur_entry->sb_info), sizeof(info->sb_info));
  info->filename.assign(cur_entry->filename.value());
}

// static
bool FileEnumerator::IsDirectory(const FindInfo &info) {
  return info.sb_info.is_directory;
}

// static
FilePath FileEnumerator::GetFilename(const FindInfo &find_info) {
  return FilePath(find_info.filename);
}

// static
int64 FileEnumerator::GetFilesize(const FindInfo &find_info) {
  return find_info.sb_info.size;
}

// static
base::Time FileEnumerator::GetLastModifiedTime(const FindInfo &find_info) {
  return base::Time::FromSbTime(find_info.sb_info.last_modified);
}

// static
bool FileEnumerator::ReadDirectory(std::vector<DirectoryEntryInfo> *entries,
                                   const FilePath &source) {
  base::ThreadRestrictions::AssertIOAllowed();
  SbDirectory dir = SbDirectoryOpen(source.value().c_str(), NULL);
  if (!SbDirectoryIsValid(dir)) {
    return false;
  }

  SbDirectoryEntry entry;
  while (SbDirectoryGetNext(dir, &entry)) {
    DirectoryEntryInfo info;
    info.filename = FilePath(entry.name);

    FilePath full_name = source.Append(entry.name);
    // TODO: Make sure this follows symlinks on relevant platforms.
    if (!SbFileGetPathInfo(full_name.value().c_str(), &info.sb_info)) {
      DPLOG(ERROR) << "Couldn't SbFileGetInfo on " << full_name.value();
      memset(&info.sb_info, 0, sizeof(info.sb_info));
    }

    entries->push_back(info);
  }

  ignore_result(SbDirectoryClose(dir));
  return true;
}

}  // namespace file_util
