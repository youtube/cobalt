// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tchar.h>
#include <winioctl.h>
#endif

#if !defined(__LB_PS3__)
// These headers are not available on the PS3
#include <algorithm>
#include <fstream>
#endif
#include <set>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/test_file_util.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)

#if defined(COBALT) || defined(OS_STARBOARD)
#define MAYBE_CopyDirectoryRecursivelyNew DISABLED_CopyDirectoryRecursivelyNew
#define MAYBE_CopyDirectoryRecursivelyExists \
  DISABLED_CopyDirectoryRecursivelyExists
#define MAYBE_CopyDirectoryNew DISABLED_CopyDirectoryNew
#define MAYBE_CopyDirectoryExists DISABLED_CopyDirectoryExists
#define MAYBE_CopyFileWithCopyDirectoryRecursiveToNew \
  DISABLED_CopyFileWithCopyDirectoryRecursiveToNew
#define MAYBE_CopyFileWithCopyDirectoryRecursiveToExisting \
  DISABLED_CopyFileWithCopyDirectoryRecursiveToExisting
#define MAYBE_CopyFileWithCopyDirectoryRecursiveToExistingDirectory \
  DISABLED_CopyFileWithCopyDirectoryRecursiveToExistingDirectory
#define MAYBE_CopyDirectoryWithTrailingSeparators \
  DISABLED_CopyDirectoryWithTrailingSeparators
#else
#define MAYBE_CopyDirectoryRecursivelyNew CopyDirectoryRecursivelyNew
#define MAYBE_CopyDirectoryRecursivelyExists CopyDirectoryRecursivelyExists
#define MAYBE_CopyDirectoryNew CopyDirectoryNew
#define MAYBE_CopyDirectoryExists CopyDirectoryExists
#define MAYBE_CopyFileWithCopyDirectoryRecursiveToNew \
  CopyFileWithCopyDirectoryRecursiveToNew
#define MAYBE_CopyFileWithCopyDirectoryRecursiveToExisting \
  CopyFileWithCopyDirectoryRecursiveToExisting
#define MAYBE_CopyFileWithCopyDirectoryRecursiveToExistingDirectory \
  CopyFileWithCopyDirectoryRecursiveToExistingDirectory
#define MAYBE_CopyDirectoryWithTrailingSeparators \
  CopyDirectoryWithTrailingSeparators
#endif

namespace {

// To test that file_util::Normalize FilePath() deals with NTFS reparse points
// correctly, we need functions to create and delete reparse points.
#if defined(OS_WIN)
typedef struct _REPARSE_DATA_BUFFER {
  ULONG  ReparseTag;
  USHORT  ReparseDataLength;
  USHORT  Reserved;
  union {
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG Flags;
      WCHAR PathBuffer[1];
    } SymbolicLinkReparseBuffer;
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR PathBuffer[1];
    } MountPointReparseBuffer;
    struct {
      UCHAR DataBuffer[1];
    } GenericReparseBuffer;
  };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

// Sets a reparse point. |source| will now point to |target|. Returns true if
// the call succeeds, false otherwise.
bool SetReparsePoint(HANDLE source, const FilePath& target_path) {
  std::wstring kPathPrefix = L"\\??\\";
  std::wstring target_str;
  // The juction will not work if the target path does not start with \??\ .
  if (kPathPrefix != target_path.value().substr(0, kPathPrefix.size()))
    target_str += kPathPrefix;
  target_str += target_path.value();
  const wchar_t* target = target_str.c_str();
  USHORT size_target = static_cast<USHORT>(wcslen(target)) * sizeof(target[0]);
  char buffer[2000] = {0};
  DWORD returned;

  REPARSE_DATA_BUFFER* data = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer);

  data->ReparseTag = 0xa0000003;
  memcpy(data->MountPointReparseBuffer.PathBuffer, target, size_target + 2);

  data->MountPointReparseBuffer.SubstituteNameLength = size_target;
  data->MountPointReparseBuffer.PrintNameOffset = size_target + 2;
  data->ReparseDataLength = size_target + 4 + 8;

  int data_size = data->ReparseDataLength + 8;

  if (!DeviceIoControl(source, FSCTL_SET_REPARSE_POINT, &buffer, data_size,
                       NULL, 0, &returned, NULL)) {
    return false;
  }
  return true;
}

// Delete the reparse point referenced by |source|. Returns true if the call
// succeeds, false otherwise.
bool DeleteReparsePoint(HANDLE source) {
  DWORD returned;
  REPARSE_DATA_BUFFER data = {0};
  data.ReparseTag = 0xa0000003;
  if (!DeviceIoControl(source, FSCTL_DELETE_REPARSE_POINT, &data, 8, NULL, 0,
                       &returned, NULL)) {
    return false;
  }
  return true;
}
#endif

#if defined(OS_POSIX)
// Provide a simple way to change the permissions bits on |path| in tests.
// ASSERT failures will return, but not stop the test.  Caller should wrap
// calls to this function in ASSERT_NO_FATAL_FAILURE().
void ChangePosixFilePermissions(const FilePath& path,
                                int mode_bits_to_set,
                                int mode_bits_to_clear) {
  ASSERT_FALSE(mode_bits_to_set & mode_bits_to_clear)
      << "Can't set and clear the same bits.";

  int mode = 0;
  ASSERT_TRUE(file_util::GetPosixFilePermissions(path, &mode));
  mode |= mode_bits_to_set;
  mode &= ~mode_bits_to_clear;
  ASSERT_TRUE(file_util::SetPosixFilePermissions(path, mode));
}
#endif  // defined(OS_POSIX)

const wchar_t bogus_content[] = L"I'm cannon fodder.";

const int FILES_AND_DIRECTORIES =
    file_util::FileEnumerator::FILES | file_util::FileEnumerator::DIRECTORIES;

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest
class FileUtilTest : public PlatformTest {
 protected:
  virtual void SetUp() OVERRIDE {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::ScopedTempDir temp_dir_;
};

// Collects all the results from the given file enumerator, and provides an
// interface to query whether a given file is present.
class FindResultCollector {
 public:
  explicit FindResultCollector(file_util::FileEnumerator& enumerator) {
    FilePath cur_file;
    while (!(cur_file = enumerator.Next()).value().empty()) {
      FilePath::StringType path = cur_file.value();
      // The file should not be returned twice.
      EXPECT_TRUE(files_.end() == files_.find(path))
          << "Same file returned twice";

      // Save for later.
      files_.insert(path);
    }
  }

  // Returns true if the enumerator found the file.
  bool HasFile(const FilePath& file) const {
    return files_.find(file.value()) != files_.end();
  }

  int size() {
    return static_cast<int>(files_.size());
  }

 private:
  std::set<FilePath::StringType> files_;
};

// Simple function to dump some text into a new file.
void CreateTextFile(const FilePath& filename,
                    const std::wstring& contents) {
#if defined(OS_STARBOARD)
  SbFile file =
      SbFileOpen(filename.value().c_str(), kSbFileCreateAlways | kSbFileWrite,
                 NULL /*out_created*/, NULL /*out_error*/);
  EXPECT_TRUE(file != kSbFileInvalid);
  std::string utf8 = WideToUTF8(contents);
  int bytes_written = SbFileWrite(file, utf8.c_str(), utf8.length());
  EXPECT_TRUE(bytes_written == utf8.length());
  SbFileClose(file);
#elif defined(__LB_PS3__)
  FILE *file = fopen(filename.value().c_str(), "w");
  ASSERT_TRUE(file != NULL);
  fputws(contents.c_str(), file);
  fclose(file);
#elif defined(__LB_WIIU__)
  int fd = open(filename.value().c_str(), O_WRONLY | O_CREAT | O_TRUNC);
  ASSERT_TRUE(fd >= 0);
  char mb_sequence[64];
  const wchar_t* wc_string = contents.c_str();
  size_t nbytes = wcsrtombs(mb_sequence, &wc_string, arraysize(mb_sequence), NULL);
  ssize_t bytes_written = write(fd, mb_sequence, nbytes);
  ASSERT_TRUE(bytes_written == nbytes);
  ASSERT_TRUE(wc_string == NULL);
  close(fd);
#else
  std::wofstream file;
  file.open(filename.value().c_str());
  ASSERT_TRUE(file.is_open());
  file << contents;
  file.close();
#endif  // defined(OS_STARBOARD)
}

// Simple function to take out some text from a file.
std::wstring ReadTextFile(const FilePath& filename) {
#if defined(OS_STARBOARD)
  SbFile file =
      SbFileOpen(filename.value().c_str(), kSbFileOpenOnly | kSbFileRead,
                 NULL /*out_created*/, NULL /*out_error*/);
  EXPECT_TRUE(file != kSbFileInvalid);
  char utf8_buffer[64] = {0};
  int bytes_read =
      SbFileRead(file, utf8_buffer, SB_ARRAY_SIZE_INT(utf8_buffer));
  EXPECT_TRUE(bytes_read >= 0);
  SbFileClose(file);
  std::wstring result;
  bool did_convert =
      UTF8ToWide(utf8_buffer, SbStringGetLength(utf8_buffer), &result);
  EXPECT_TRUE(did_convert);
  return result;
#elif defined(__LB_PS3__)
  wchar_t contents[64];
  FILE *file = fopen(filename.value().c_str(), "r");
  EXPECT_TRUE(file != NULL);
  fgetws(contents, arraysize(contents), file);
  fclose(file);
  return std::wstring(contents);
#elif defined(__LB_WIIU__)
  wchar_t contents[64];
  char mb_sequence_buffer[64];
  int fd = open(filename.value().c_str(), O_RDONLY);
  EXPECT_TRUE(fd >= 0);
  ssize_t bytes_read = read(fd, mb_sequence_buffer, arraysize(mb_sequence_buffer));
  EXPECT_TRUE(bytes_read >= 0);
  close(fd);
  const char * mb_sequence = mb_sequence_buffer;
  size_t nbytes = mbsrtowcs(contents, &mb_sequence, sizeof(contents), NULL);
  EXPECT_TRUE(mb_sequence == NULL);
  return std::wstring(contents);
#else
  wchar_t contents[64];
  std::wifstream file;
  file.open(filename.value().c_str());
  EXPECT_TRUE(file.is_open());
  file.getline(contents, arraysize(contents));
  file.close();
  return std::wstring(contents);
#endif
}

#if defined(OS_WIN)
uint64 FileTimeAsUint64(const FILETIME& ft) {
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return u.QuadPart;
}
#endif


TEST_F(FileUtilTest, FileAndDirectorySize) {
  // Create three files of 20, 30 and 3 chars (utf8). ComputeDirectorySize
  // should return 53 bytes.
  FilePath file_01 = temp_dir_.path().Append(FPL("The file 01.txt"));
  CreateTextFile(file_01, L"12345678901234567890");
  int64 size_f1 = 0;
  ASSERT_TRUE(file_util::GetFileSize(file_01, &size_f1));
  EXPECT_EQ(20ll, size_f1);

  FilePath subdir_path = temp_dir_.path().Append(FPL("Level2"));
  file_util::CreateDirectory(subdir_path);

  FilePath file_02 = subdir_path.Append(FPL("The file 02.txt"));
  CreateTextFile(file_02, L"123456789012345678901234567890");
  int64 size_f2 = 0;
  ASSERT_TRUE(file_util::GetFileSize(file_02, &size_f2));
  EXPECT_EQ(30ll, size_f2);

  FilePath subsubdir_path = subdir_path.Append(FPL("Level3"));
  file_util::CreateDirectory(subsubdir_path);

  FilePath file_03 = subsubdir_path.Append(FPL("The file 03.txt"));
  CreateTextFile(file_03, L"123");

  int64 computed_size = file_util::ComputeDirectorySize(temp_dir_.path());
  EXPECT_EQ(size_f1 + size_f2 + 3, computed_size);

#if !defined(OS_STARBOARD)
  // No pattern support in Starboard.
  computed_size =
      file_util::ComputeFilesSize(temp_dir_.path(), FPL("The file*"));
  EXPECT_EQ(size_f1, computed_size);

  computed_size = file_util::ComputeFilesSize(temp_dir_.path(), FPL("bla*"));
  EXPECT_EQ(0, computed_size);
#endif
}

#if !defined(OS_STARBOARD)
TEST_F(FileUtilTest, NormalizeFilePathBasic) {
  // Create a directory under the test dir.  Because we create it,
  // we know it is not a link.
  FilePath file_a_path = temp_dir_.path().Append(FPL("file_a"));
  FilePath dir_path = temp_dir_.path().Append(FPL("dir"));
  FilePath file_b_path = dir_path.Append(FPL("file_b"));
  file_util::CreateDirectory(dir_path);

  FilePath normalized_file_a_path, normalized_file_b_path;
  ASSERT_FALSE(file_util::PathExists(file_a_path));
  ASSERT_FALSE(file_util::NormalizeFilePath(file_a_path,
                                            &normalized_file_a_path))
    << "NormalizeFilePath() should fail on nonexistent paths.";

  CreateTextFile(file_a_path, bogus_content);
  ASSERT_TRUE(file_util::PathExists(file_a_path));
  ASSERT_TRUE(file_util::NormalizeFilePath(file_a_path,
                                           &normalized_file_a_path));

  CreateTextFile(file_b_path, bogus_content);
  ASSERT_TRUE(file_util::PathExists(file_b_path));
  ASSERT_TRUE(file_util::NormalizeFilePath(file_b_path,
                                           &normalized_file_b_path));

  // Beacuse this test created |dir_path|, we know it is not a link
  // or junction.  So, the real path of the directory holding file a
  // must be the parent of the path holding file b.
  ASSERT_TRUE(normalized_file_a_path.DirName()
      .IsParent(normalized_file_b_path.DirName()));
}
#endif

#if defined(OS_WIN)

TEST_F(FileUtilTest, NormalizeFilePathReparsePoints) {
  // Build the following directory structure:
  //
  // temp_dir
  // |-> base_a
  // |   |-> sub_a
  // |       |-> file.txt
  // |       |-> long_name___... (Very long name.)
  // |           |-> sub_long
  // |              |-> deep.txt
  // |-> base_b
  //     |-> to_sub_a (reparse point to temp_dir\base_a\sub_a)
  //     |-> to_base_b (reparse point to temp_dir\base_b)
  //     |-> to_sub_long (reparse point to temp_dir\sub_a\long_name_\sub_long)

  FilePath base_a = temp_dir_.path().Append(FPL("base_a"));
  ASSERT_TRUE(file_util::CreateDirectory(base_a));

  FilePath sub_a = base_a.Append(FPL("sub_a"));
  ASSERT_TRUE(file_util::CreateDirectory(sub_a));

  FilePath file_txt = sub_a.Append(FPL("file.txt"));
  CreateTextFile(file_txt, bogus_content);

  // Want a directory whose name is long enough to make the path to the file
  // inside just under MAX_PATH chars.  This will be used to test that when
  // a junction expands to a path over MAX_PATH chars in length,
  // NormalizeFilePath() fails without crashing.
  FilePath sub_long_rel(FPL("sub_long"));
  FilePath deep_txt(FPL("deep.txt"));

  int target_length = MAX_PATH;
  target_length -= (sub_a.value().length() + 1);  // +1 for the sepperator '\'.
  target_length -= (sub_long_rel.Append(deep_txt).value().length() + 1);
  // Without making the path a bit shorter, CreateDirectory() fails.
  // the resulting path is still long enough to hit the failing case in
  // NormalizePath().
  const int kCreateDirLimit = 4;
  target_length -= kCreateDirLimit;
  FilePath::StringType long_name_str = FPL("long_name_");
  long_name_str.resize(target_length, '_');

  FilePath long_name = sub_a.Append(FilePath(long_name_str));
  FilePath deep_file = long_name.Append(sub_long_rel).Append(deep_txt);
  ASSERT_EQ(MAX_PATH - kCreateDirLimit, deep_file.value().length());

  FilePath sub_long = deep_file.DirName();
  ASSERT_TRUE(file_util::CreateDirectory(sub_long));
  CreateTextFile(deep_file, bogus_content);

  FilePath base_b = temp_dir_.path().Append(FPL("base_b"));
  ASSERT_TRUE(file_util::CreateDirectory(base_b));

  FilePath to_sub_a = base_b.Append(FPL("to_sub_a"));
  ASSERT_TRUE(file_util::CreateDirectory(to_sub_a));
  base::win::ScopedHandle reparse_to_sub_a(
      ::CreateFile(to_sub_a.value().c_str(),
                   FILE_ALL_ACCESS,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   NULL,
                   OPEN_EXISTING,
                   FILE_FLAG_BACKUP_SEMANTICS,  // Needed to open a directory.
                   NULL));
  ASSERT_TRUE(reparse_to_sub_a.IsValid());
  ASSERT_TRUE(SetReparsePoint(reparse_to_sub_a, sub_a));

  FilePath to_base_b = base_b.Append(FPL("to_base_b"));
  ASSERT_TRUE(file_util::CreateDirectory(to_base_b));
  base::win::ScopedHandle reparse_to_base_b(
      ::CreateFile(to_base_b.value().c_str(),
                   FILE_ALL_ACCESS,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   NULL,
                   OPEN_EXISTING,
                   FILE_FLAG_BACKUP_SEMANTICS,  // Needed to open a directory.
                   NULL));
  ASSERT_TRUE(reparse_to_base_b.IsValid());
  ASSERT_TRUE(SetReparsePoint(reparse_to_base_b, base_b));

  FilePath to_sub_long = base_b.Append(FPL("to_sub_long"));
  ASSERT_TRUE(file_util::CreateDirectory(to_sub_long));
  base::win::ScopedHandle reparse_to_sub_long(
      ::CreateFile(to_sub_long.value().c_str(),
                   FILE_ALL_ACCESS,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   NULL,
                   OPEN_EXISTING,
                   FILE_FLAG_BACKUP_SEMANTICS,  // Needed to open a directory.
                   NULL));
  ASSERT_TRUE(reparse_to_sub_long.IsValid());
  ASSERT_TRUE(SetReparsePoint(reparse_to_sub_long, sub_long));

  // Normalize a junction free path: base_a\sub_a\file.txt .
  FilePath normalized_path;
  ASSERT_TRUE(file_util::NormalizeFilePath(file_txt, &normalized_path));
  ASSERT_STREQ(file_txt.value().c_str(), normalized_path.value().c_str());

  // Check that the path base_b\to_sub_a\file.txt can be normalized to exclude
  // the junction to_sub_a.
  ASSERT_TRUE(file_util::NormalizeFilePath(to_sub_a.Append(FPL("file.txt")),
                                           &normalized_path));
  ASSERT_STREQ(file_txt.value().c_str(), normalized_path.value().c_str());

  // Check that the path base_b\to_base_b\to_base_b\to_sub_a\file.txt can be
  // normalized to exclude junctions to_base_b and to_sub_a .
  ASSERT_TRUE(file_util::NormalizeFilePath(base_b.Append(FPL("to_base_b"))
                                                 .Append(FPL("to_base_b"))
                                                 .Append(FPL("to_sub_a"))
                                                 .Append(FPL("file.txt")),
                                           &normalized_path));
  ASSERT_STREQ(file_txt.value().c_str(), normalized_path.value().c_str());

  // A long enough path will cause NormalizeFilePath() to fail.  Make a long
  // path using to_base_b many times, and check that paths long enough to fail
  // do not cause a crash.
  FilePath long_path = base_b;
  const int kLengthLimit = MAX_PATH + 200;
  while (long_path.value().length() <= kLengthLimit) {
    long_path = long_path.Append(FPL("to_base_b"));
  }
  long_path = long_path.Append(FPL("to_sub_a"))
                       .Append(FPL("file.txt"));

  ASSERT_FALSE(file_util::NormalizeFilePath(long_path, &normalized_path));

  // Normalizing the junction to deep.txt should fail, because the expanded
  // path to deep.txt is longer than MAX_PATH.
  ASSERT_FALSE(file_util::NormalizeFilePath(to_sub_long.Append(deep_txt),
                                            &normalized_path));

  // Delete the reparse points, and see that NormalizeFilePath() fails
  // to traverse them.
  ASSERT_TRUE(DeleteReparsePoint(reparse_to_sub_a));
  ASSERT_TRUE(DeleteReparsePoint(reparse_to_base_b));
  ASSERT_TRUE(DeleteReparsePoint(reparse_to_sub_long));

  ASSERT_FALSE(file_util::NormalizeFilePath(to_sub_a.Append(FPL("file.txt")),
                                            &normalized_path));
}

TEST_F(FileUtilTest, DevicePathToDriveLetter) {
  // Get a drive letter.
  std::wstring real_drive_letter = temp_dir_.path().value().substr(0, 2);
  if (!isalpha(real_drive_letter[0]) || ':' != real_drive_letter[1]) {
    LOG(ERROR) << "Can't get a drive letter to test with.";
    return;
  }

  // Get the NT style path to that drive.
  wchar_t device_path[MAX_PATH] = {'\0'};
  ASSERT_TRUE(
      ::QueryDosDevice(real_drive_letter.c_str(), device_path, MAX_PATH));
  FilePath actual_device_path(device_path);
  FilePath win32_path;

  // Run DevicePathToDriveLetterPath() on the NT style path we got from
  // QueryDosDevice().  Expect the drive letter we started with.
  ASSERT_TRUE(file_util::DevicePathToDriveLetterPath(actual_device_path,
                                                     &win32_path));
  ASSERT_EQ(real_drive_letter, win32_path.value());

  // Add some directories to the path.  Expect those extra path componenets
  // to be preserved.
  FilePath kRelativePath(FPL("dir1\\dir2\\file.txt"));
  ASSERT_TRUE(file_util::DevicePathToDriveLetterPath(
      actual_device_path.Append(kRelativePath),
      &win32_path));
  EXPECT_EQ(FilePath(real_drive_letter + L"\\").Append(kRelativePath).value(),
            win32_path.value());

  // Deform the real path so that it is invalid by removing the last four
  // characters.  The way windows names devices that are hard disks
  // (\Device\HardDiskVolume${NUMBER}) guarantees that the string is longer
  // than three characters.  The only way the truncated string could be a
  // real drive is if more than 10^3 disks are mounted:
  // \Device\HardDiskVolume10000 would be truncated to \Device\HardDiskVolume1
  // Check that DevicePathToDriveLetterPath fails.
  int path_length = actual_device_path.value().length();
  int new_length = path_length - 4;
  ASSERT_LT(0, new_length);
  FilePath prefix_of_real_device_path(
      actual_device_path.value().substr(0, new_length));
  ASSERT_FALSE(file_util::DevicePathToDriveLetterPath(
      prefix_of_real_device_path,
      &win32_path));

  ASSERT_FALSE(file_util::DevicePathToDriveLetterPath(
      prefix_of_real_device_path.Append(kRelativePath),
      &win32_path));

  // Deform the real path so that it is invalid by adding some characters. For
  // example, if C: maps to \Device\HardDiskVolume8, then we simulate a
  // request for the drive letter whose native path is
  // \Device\HardDiskVolume812345 .  We assume such a device does not exist,
  // because drives are numbered in order and mounting 112345 hard disks will
  // never happen.
  const FilePath::StringType kExtraChars = FPL("12345");

  FilePath real_device_path_plus_numbers(
      actual_device_path.value() + kExtraChars);

  ASSERT_FALSE(file_util::DevicePathToDriveLetterPath(
      real_device_path_plus_numbers,
      &win32_path));

  ASSERT_FALSE(file_util::DevicePathToDriveLetterPath(
      real_device_path_plus_numbers.Append(kRelativePath),
      &win32_path));
}

TEST_F(FileUtilTest, GetPlatformFileInfoForDirectory) {
  FilePath empty_dir = temp_dir_.path().Append(FPL("gpfi_test"));
  ASSERT_TRUE(file_util::CreateDirectory(empty_dir));
  base::win::ScopedHandle dir(
      ::CreateFile(empty_dir.value().c_str(),
                   FILE_ALL_ACCESS,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   NULL,
                   OPEN_EXISTING,
                   FILE_FLAG_BACKUP_SEMANTICS,  // Needed to open a directory.
                   NULL));
  ASSERT_TRUE(dir.IsValid());
  base::PlatformFileInfo info;
  EXPECT_TRUE(base::GetPlatformFileInfo(dir.Get(), &info));
  EXPECT_TRUE(info.is_directory);
  EXPECT_FALSE(info.is_symbolic_link);
  EXPECT_EQ(0, info.size);
}

TEST_F(FileUtilTest, CreateTemporaryFileInDirLongPathTest) {
  // Test that CreateTemporaryFileInDir() creates a path and returns a long path
  // if it is available. This test requires that:
  // - the filesystem at |temp_dir_| supports long filenames.
  // - the account has FILE_LIST_DIRECTORY permission for all ancestor
  //   directories of |temp_dir_|.
  const FilePath::CharType kLongDirName[] = FPL("A long path");
  const FilePath::CharType kTestSubDirName[] = FPL("test");
  FilePath long_test_dir = temp_dir_.path().Append(kLongDirName);
  ASSERT_TRUE(file_util::CreateDirectory(long_test_dir));

  // kLongDirName is not a 8.3 component. So GetShortName() should give us a
  // different short name.
  WCHAR path_buffer[MAX_PATH];
  DWORD path_buffer_length = GetShortPathName(long_test_dir.value().c_str(),
                                              path_buffer, MAX_PATH);
  ASSERT_LT(path_buffer_length, DWORD(MAX_PATH));
  ASSERT_NE(DWORD(0), path_buffer_length);
  FilePath short_test_dir(path_buffer);
  ASSERT_STRNE(kLongDirName, short_test_dir.BaseName().value().c_str());

  FilePath temp_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(short_test_dir, &temp_file));
  EXPECT_STREQ(kLongDirName, temp_file.DirName().BaseName().value().c_str());
  EXPECT_TRUE(file_util::PathExists(temp_file));

  // Create a subdirectory of |long_test_dir| and make |long_test_dir|
  // unreadable. We should still be able to create a temp file in the
  // subdirectory, but we won't be able to determine the long path for it. This
  // mimics the environment that some users run where their user profiles reside
  // in a location where the don't have full access to the higher level
  // directories. (Note that this assumption is true for NTFS, but not for some
  // network file systems. E.g. AFS).
  FilePath access_test_dir = long_test_dir.Append(kTestSubDirName);
  ASSERT_TRUE(file_util::CreateDirectory(access_test_dir));
  file_util::PermissionRestorer long_test_dir_restorer(long_test_dir);
  ASSERT_TRUE(file_util::MakeFileUnreadable(long_test_dir));

  // Use the short form of the directory to create a temporary filename.
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(
      short_test_dir.Append(kTestSubDirName), &temp_file));
  EXPECT_TRUE(file_util::PathExists(temp_file));
  EXPECT_TRUE(short_test_dir.IsParent(temp_file.DirName()));

  // Check that the long path can't be determined for |temp_file|.
  path_buffer_length = GetLongPathName(temp_file.value().c_str(),
                                       path_buffer, MAX_PATH);
  EXPECT_EQ(DWORD(0), path_buffer_length);
}

#endif  // defined(OS_WIN)

#if defined(OS_POSIX) && !defined(__LB_SHELL__)
// Symbolic links not supported in lbshell

TEST_F(FileUtilTest, CreateAndReadSymlinks) {
  FilePath link_from = temp_dir_.path().Append(FPL("from_file"));
  FilePath link_to = temp_dir_.path().Append(FPL("to_file"));
  CreateTextFile(link_to, bogus_content);

  ASSERT_TRUE(file_util::CreateSymbolicLink(link_to, link_from))
    << "Failed to create file symlink.";

  // If we created the link properly, we should be able to read the
  // contents through it.
  std::wstring contents = ReadTextFile(link_from);
  ASSERT_EQ(contents, bogus_content);

  FilePath result;
  ASSERT_TRUE(file_util::ReadSymbolicLink(link_from, &result));
  ASSERT_EQ(link_to.value(), result.value());

  // Link to a directory.
  link_from = temp_dir_.path().Append(FPL("from_dir"));
  link_to = temp_dir_.path().Append(FPL("to_dir"));
  file_util::CreateDirectory(link_to);

  ASSERT_TRUE(file_util::CreateSymbolicLink(link_to, link_from))
    << "Failed to create directory symlink.";

  // Test failures.
  ASSERT_FALSE(file_util::CreateSymbolicLink(link_to, link_to));
  ASSERT_FALSE(file_util::ReadSymbolicLink(link_to, &result));
  FilePath missing = temp_dir_.path().Append(FPL("missing"));
  ASSERT_FALSE(file_util::ReadSymbolicLink(missing, &result));
}

// The following test of NormalizeFilePath() require that we create a symlink.
// This can not be done on Windows before Vista.  On Vista, creating a symlink
// requires privilege "SeCreateSymbolicLinkPrivilege".
// TODO(skerner): Investigate the possibility of giving base_unittests the
// privileges required to create a symlink.
TEST_F(FileUtilTest, NormalizeFilePathSymlinks) {
  FilePath normalized_path;

  // Link one file to another.
  FilePath link_from = temp_dir_.path().Append(FPL("from_file"));
  FilePath link_to = temp_dir_.path().Append(FPL("to_file"));
  CreateTextFile(link_to, bogus_content);

  ASSERT_TRUE(file_util::CreateSymbolicLink(link_to, link_from))
    << "Failed to create file symlink.";

  // Check that NormalizeFilePath sees the link.
  ASSERT_TRUE(file_util::NormalizeFilePath(link_from, &normalized_path));
  ASSERT_TRUE(link_to != link_from);
  ASSERT_EQ(link_to.BaseName().value(), normalized_path.BaseName().value());
  ASSERT_EQ(link_to.BaseName().value(), normalized_path.BaseName().value());

  // Link to a directory.
  link_from = temp_dir_.path().Append(FPL("from_dir"));
  link_to = temp_dir_.path().Append(FPL("to_dir"));
  file_util::CreateDirectory(link_to);

  ASSERT_TRUE(file_util::CreateSymbolicLink(link_to, link_from))
    << "Failed to create directory symlink.";

  ASSERT_FALSE(file_util::NormalizeFilePath(link_from, &normalized_path))
    << "Links to directories should return false.";

  // Test that a loop in the links causes NormalizeFilePath() to return false.
  link_from = temp_dir_.path().Append(FPL("link_a"));
  link_to = temp_dir_.path().Append(FPL("link_b"));
  ASSERT_TRUE(file_util::CreateSymbolicLink(link_to, link_from))
    << "Failed to create loop symlink a.";
  ASSERT_TRUE(file_util::CreateSymbolicLink(link_from, link_to))
    << "Failed to create loop symlink b.";

  // Infinite loop!
  ASSERT_FALSE(file_util::NormalizeFilePath(link_from, &normalized_path));
}
#endif  // defined(OS_POSIX)

TEST_F(FileUtilTest, DeleteNonExistent) {
  FilePath non_existent = temp_dir_.path().AppendASCII("bogus_file_dne.foobar");
  ASSERT_FALSE(file_util::PathExists(non_existent));

  EXPECT_TRUE(file_util::Delete(non_existent, false));
  ASSERT_FALSE(file_util::PathExists(non_existent));
  EXPECT_TRUE(file_util::Delete(non_existent, true));
  ASSERT_FALSE(file_util::PathExists(non_existent));
}

TEST_F(FileUtilTest, DeleteFile) {
  // Create a file
  FilePath file_name = temp_dir_.path().Append(FPL("Test DeleteFile 1.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(file_util::PathExists(file_name));

  // Make sure it's deleted
  EXPECT_TRUE(file_util::Delete(file_name, false));
  EXPECT_FALSE(file_util::PathExists(file_name));

  // Test recursive case, create a new file
  file_name = temp_dir_.path().Append(FPL("Test DeleteFile 2.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(file_util::PathExists(file_name));

  // Make sure it's deleted
  EXPECT_TRUE(file_util::Delete(file_name, true));
  EXPECT_FALSE(file_util::PathExists(file_name));
}

#if defined(OS_POSIX) && !defined(__LB_SHELL__)
// Symbolic links not supported in lbshell

TEST_F(FileUtilTest, DeleteSymlinkToExistentFile) {
  // Create a file.
  FilePath file_name = temp_dir_.path().Append(FPL("Test DeleteFile 2.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(file_util::PathExists(file_name));

  // Create a symlink to the file.
  FilePath file_link = temp_dir_.path().Append("file_link_2");
  ASSERT_TRUE(file_util::CreateSymbolicLink(file_name, file_link))
      << "Failed to create symlink.";

  // Delete the symbolic link.
  EXPECT_TRUE(file_util::Delete(file_link, false));

  // Make sure original file is not deleted.
  EXPECT_FALSE(file_util::PathExists(file_link));
  EXPECT_TRUE(file_util::PathExists(file_name));
}

TEST_F(FileUtilTest, DeleteSymlinkToNonExistentFile) {
  // Create a non-existent file path.
  FilePath non_existent = temp_dir_.path().Append(FPL("Test DeleteFile 3.txt"));
  EXPECT_FALSE(file_util::PathExists(non_existent));

  // Create a symlink to the non-existent file.
  FilePath file_link = temp_dir_.path().Append("file_link_3");
  ASSERT_TRUE(file_util::CreateSymbolicLink(non_existent, file_link))
      << "Failed to create symlink.";

  // Make sure the symbolic link is exist.
  EXPECT_TRUE(file_util::IsLink(file_link));
  EXPECT_FALSE(file_util::PathExists(file_link));

  // Delete the symbolic link.
  EXPECT_TRUE(file_util::Delete(file_link, false));

  // Make sure the symbolic link is deleted.
  EXPECT_FALSE(file_util::IsLink(file_link));
}

TEST_F(FileUtilTest, ChangeFilePermissionsAndRead) {
  // Create a file path.
  FilePath file_name = temp_dir_.path().Append(FPL("Test Readable File.txt"));
  EXPECT_FALSE(file_util::PathExists(file_name));

  const std::string kData("hello");

  int buffer_size = kData.length();
  char* buffer = new char[buffer_size];

  // Write file.
  EXPECT_EQ(static_cast<int>(kData.length()),
            file_util::WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_TRUE(file_util::PathExists(file_name));

  // Make sure the file is readable.
  int32 mode = 0;
  EXPECT_TRUE(file_util::GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & file_util::FILE_PERMISSION_READ_BY_USER);

  // Get rid of the read permission.
  EXPECT_TRUE(file_util::SetPosixFilePermissions(file_name, 0u));
  EXPECT_TRUE(file_util::GetPosixFilePermissions(file_name, &mode));
  EXPECT_FALSE(mode & file_util::FILE_PERMISSION_READ_BY_USER);
  // Make sure the file can't be read.
  EXPECT_EQ(-1, file_util::ReadFile(file_name, buffer, buffer_size));

  // Give the read permission.
  EXPECT_TRUE(file_util::SetPosixFilePermissions(
      file_name,
      file_util::FILE_PERMISSION_READ_BY_USER));
  EXPECT_TRUE(file_util::GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & file_util::FILE_PERMISSION_READ_BY_USER);
  // Make sure the file can be read.
  EXPECT_EQ(static_cast<int>(kData.length()),
            file_util::ReadFile(file_name, buffer, buffer_size));

  // Delete the file.
  EXPECT_TRUE(file_util::Delete(file_name, false));
  EXPECT_FALSE(file_util::PathExists(file_name));

  delete[] buffer;
}

TEST_F(FileUtilTest, ChangeFilePermissionsAndWrite) {
  // Create a file path.
  FilePath file_name = temp_dir_.path().Append(FPL("Test Readable File.txt"));
  EXPECT_FALSE(file_util::PathExists(file_name));

  const std::string kData("hello");

  // Write file.
  EXPECT_EQ(static_cast<int>(kData.length()),
            file_util::WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_TRUE(file_util::PathExists(file_name));

  // Make sure the file is writable.
  int mode = 0;
  EXPECT_TRUE(file_util::GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & file_util::FILE_PERMISSION_WRITE_BY_USER);
  EXPECT_TRUE(file_util::PathIsWritable(file_name));

  // Get rid of the write permission.
  EXPECT_TRUE(file_util::SetPosixFilePermissions(file_name, 0u));
  EXPECT_TRUE(file_util::GetPosixFilePermissions(file_name, &mode));
  EXPECT_FALSE(mode & file_util::FILE_PERMISSION_WRITE_BY_USER);
  // Make sure the file can't be write.
  EXPECT_EQ(-1,
            file_util::WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_FALSE(file_util::PathIsWritable(file_name));

  // Give read permission.
  EXPECT_TRUE(file_util::SetPosixFilePermissions(
      file_name,
      file_util::FILE_PERMISSION_WRITE_BY_USER));
  EXPECT_TRUE(file_util::GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & file_util::FILE_PERMISSION_WRITE_BY_USER);
  // Make sure the file can be write.
  EXPECT_EQ(static_cast<int>(kData.length()),
            file_util::WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_TRUE(file_util::PathIsWritable(file_name));

  // Delete the file.
  EXPECT_TRUE(file_util::Delete(file_name, false));
  EXPECT_FALSE(file_util::PathExists(file_name));
}

TEST_F(FileUtilTest, ChangeDirectoryPermissionsAndEnumerate) {
  // Create a directory path.
  FilePath subdir_path =
      temp_dir_.path().Append(FPL("PermissionTest1"));
  file_util::CreateDirectory(subdir_path);
  ASSERT_TRUE(file_util::PathExists(subdir_path));

  // Create a dummy file to enumerate.
  FilePath file_name = subdir_path.Append(FPL("Test Readable File.txt"));
  EXPECT_FALSE(file_util::PathExists(file_name));
  const std::string kData("hello");
  EXPECT_EQ(static_cast<int>(kData.length()),
            file_util::WriteFile(file_name, kData.data(), kData.length()));
  EXPECT_TRUE(file_util::PathExists(file_name));

  // Make sure the directory has the all permissions.
  int mode = 0;
  EXPECT_TRUE(file_util::GetPosixFilePermissions(subdir_path, &mode));
  EXPECT_EQ(file_util::FILE_PERMISSION_USER_MASK,
            mode & file_util::FILE_PERMISSION_USER_MASK);

  // Get rid of the permissions from the directory.
  EXPECT_TRUE(file_util::SetPosixFilePermissions(subdir_path, 0u));
  EXPECT_TRUE(file_util::GetPosixFilePermissions(subdir_path, &mode));
  EXPECT_FALSE(mode & file_util::FILE_PERMISSION_USER_MASK);

  // Make sure the file in the directory can't be enumerated.
  file_util::FileEnumerator f1(subdir_path, true,
                               file_util::FileEnumerator::FILES);
  EXPECT_TRUE(file_util::PathExists(subdir_path));
  FindResultCollector c1(f1);
  EXPECT_EQ(c1.size(), 0);
  EXPECT_FALSE(file_util::GetPosixFilePermissions(file_name, &mode));

  // Give the permissions to the directory.
  EXPECT_TRUE(file_util::SetPosixFilePermissions(
      subdir_path,
      file_util::FILE_PERMISSION_USER_MASK));
  EXPECT_TRUE(file_util::GetPosixFilePermissions(subdir_path, &mode));
  EXPECT_EQ(file_util::FILE_PERMISSION_USER_MASK,
            mode & file_util::FILE_PERMISSION_USER_MASK);

  // Make sure the file in the directory can be enumerated.
  file_util::FileEnumerator f2(subdir_path, true,
                               file_util::FileEnumerator::FILES);
  FindResultCollector c2(f2);
  EXPECT_TRUE(c2.HasFile(file_name));
  EXPECT_EQ(c2.size(), 1);

  // Delete the file.
  EXPECT_TRUE(file_util::Delete(subdir_path, true));
  EXPECT_FALSE(file_util::PathExists(subdir_path));
}

#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
// Tests that the Delete function works for wild cards, especially
// with the recursion flag.  Also coincidentally tests PathExists.
// TODO(erikkay): see if anyone's actually using this feature of the API
TEST_F(FileUtilTest, DeleteWildCard) {
  // Create a file and a directory
  FilePath file_name = temp_dir_.path().Append(FPL("Test DeleteWildCard.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(file_util::PathExists(file_name));

  FilePath subdir_path = temp_dir_.path().Append(FPL("DeleteWildCardDir"));
  file_util::CreateDirectory(subdir_path);
  ASSERT_TRUE(file_util::PathExists(subdir_path));

  // Create the wildcard path
  FilePath directory_contents = temp_dir_.path();
  directory_contents = directory_contents.Append(FPL("*"));

  // Delete non-recursively and check that only the file is deleted
  EXPECT_TRUE(file_util::Delete(directory_contents, false));
  EXPECT_FALSE(file_util::PathExists(file_name));
  EXPECT_TRUE(file_util::PathExists(subdir_path));

  // Delete recursively and make sure all contents are deleted
  EXPECT_TRUE(file_util::Delete(directory_contents, true));
  EXPECT_FALSE(file_util::PathExists(file_name));
  EXPECT_FALSE(file_util::PathExists(subdir_path));
}

// TODO(erikkay): see if anyone's actually using this feature of the API
TEST_F(FileUtilTest, DeleteNonExistantWildCard) {
  // Create a file and a directory
  FilePath subdir_path =
      temp_dir_.path().Append(FPL("DeleteNonExistantWildCard"));
  file_util::CreateDirectory(subdir_path);
  ASSERT_TRUE(file_util::PathExists(subdir_path));

  // Create the wildcard path
  FilePath directory_contents = subdir_path;
  directory_contents = directory_contents.Append(FPL("*"));

  // Delete non-recursively and check nothing got deleted
  EXPECT_TRUE(file_util::Delete(directory_contents, false));
  EXPECT_TRUE(file_util::PathExists(subdir_path));

  // Delete recursively and check nothing got deleted
  EXPECT_TRUE(file_util::Delete(directory_contents, true));
  EXPECT_TRUE(file_util::PathExists(subdir_path));
}
#endif

// Tests non-recursive Delete() for a directory.
TEST_F(FileUtilTest, DeleteDirNonRecursive) {
  // Create a subdirectory and put a file and two directories inside.
  FilePath test_subdir = temp_dir_.path().Append(FPL("DeleteDirNonRecursive"));
  file_util::CreateDirectory(test_subdir);
  ASSERT_TRUE(file_util::PathExists(test_subdir));

  FilePath file_name = test_subdir.Append(FPL("Test DeleteDir.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(file_util::PathExists(file_name));

  FilePath subdir_path1 = test_subdir.Append(FPL("TestSubDir1"));
  file_util::CreateDirectory(subdir_path1);
  ASSERT_TRUE(file_util::PathExists(subdir_path1));

  FilePath subdir_path2 = test_subdir.Append(FPL("TestSubDir2"));
  file_util::CreateDirectory(subdir_path2);
  ASSERT_TRUE(file_util::PathExists(subdir_path2));

  // Delete non-recursively and check that the empty dir got deleted
  EXPECT_TRUE(file_util::Delete(subdir_path2, false));
  EXPECT_FALSE(file_util::PathExists(subdir_path2));

  // Delete non-recursively and check that nothing got deleted
  EXPECT_FALSE(file_util::Delete(test_subdir, false));
  EXPECT_TRUE(file_util::PathExists(test_subdir));
  EXPECT_TRUE(file_util::PathExists(file_name));
  EXPECT_TRUE(file_util::PathExists(subdir_path1));
}

// Tests recursive Delete() for a directory.
TEST_F(FileUtilTest, DeleteDirRecursive) {
  // Create a subdirectory and put a file and two directories inside.
  FilePath test_subdir = temp_dir_.path().Append(FPL("DeleteDirRecursive"));
  file_util::CreateDirectory(test_subdir);
  ASSERT_TRUE(file_util::PathExists(test_subdir));

  FilePath file_name = test_subdir.Append(FPL("Test DeleteDirRecursive.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(file_util::PathExists(file_name));

  FilePath subdir_path1 = test_subdir.Append(FPL("TestSubDir1"));
  file_util::CreateDirectory(subdir_path1);
  ASSERT_TRUE(file_util::PathExists(subdir_path1));

  FilePath subdir_path2 = test_subdir.Append(FPL("TestSubDir2"));
  file_util::CreateDirectory(subdir_path2);
  ASSERT_TRUE(file_util::PathExists(subdir_path2));

  // Delete recursively and check that the empty dir got deleted
  EXPECT_TRUE(file_util::Delete(subdir_path2, true));
  EXPECT_FALSE(file_util::PathExists(subdir_path2));

  // Delete recursively and check that everything got deleted
  EXPECT_TRUE(file_util::Delete(test_subdir, true));
  EXPECT_FALSE(file_util::PathExists(file_name));
  EXPECT_FALSE(file_util::PathExists(subdir_path1));
  EXPECT_FALSE(file_util::PathExists(test_subdir));
}

#if !defined(OS_STARBOARD)
TEST_F(FileUtilTest, MoveFileNew) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // The destination.
  FilePath file_name_to = temp_dir_.path().Append(
      FILE_PATH_LITERAL("Move_Test_File_Destination.txt"));
  ASSERT_FALSE(file_util::PathExists(file_name_to));

  EXPECT_TRUE(file_util::Move(file_name_from, file_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}

TEST_F(FileUtilTest, MoveFileExists) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // The destination name.
  FilePath file_name_to = temp_dir_.path().Append(
      FILE_PATH_LITERAL("Move_Test_File_Destination.txt"));
  CreateTextFile(file_name_to, L"Old file content");
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  EXPECT_TRUE(file_util::Move(file_name_from, file_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(L"Gooooooooooooooooooooogle" == ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, MoveFileDirExists) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // The destination directory
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Destination"));
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  EXPECT_FALSE(file_util::Move(file_name_from, dir_name_to));
}


TEST_F(FileUtilTest, MoveNew) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Move the directory.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::Move(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(file_util::PathExists(dir_name_from));
  EXPECT_FALSE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}

TEST_F(FileUtilTest, MoveExist) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Move_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Move the directory
  FilePath dir_name_exists =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Destination"));

  FilePath dir_name_to =
      dir_name_exists.Append(FILE_PATH_LITERAL("Move_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));

  // Create the destination directory.
  file_util::CreateDirectory(dir_name_exists);
  ASSERT_TRUE(file_util::PathExists(dir_name_exists));

  EXPECT_TRUE(file_util::Move(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(file_util::PathExists(dir_name_from));
  EXPECT_FALSE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}
#endif

#if !defined(OS_STARBOARD)
TEST_F(FileUtilTest, MAYBE_CopyDirectoryRecursivelyNew) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  file_util::CreateDirectory(subdir_name_from);
  ASSERT_TRUE(file_util::PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name2_from));

  // Copy the directory recursively.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));
  FilePath file_name2_to =
      subdir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::CopyDirectory(dir_name_from, dir_name_to, true));

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(dir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(subdir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name2_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(file_util::PathExists(subdir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name2_to));
}

TEST_F(FileUtilTest, MAYBE_CopyDirectoryRecursivelyExists) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  file_util::CreateDirectory(subdir_name_from);
  ASSERT_TRUE(file_util::PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name2_from));

  // Copy the directory recursively.
  FilePath dir_name_exists =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Destination"));

  FilePath dir_name_to =
      dir_name_exists.Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));
  FilePath file_name2_to =
      subdir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  // Create the destination directory.
  file_util::CreateDirectory(dir_name_exists);
  ASSERT_TRUE(file_util::PathExists(dir_name_exists));

  EXPECT_TRUE(file_util::CopyDirectory(dir_name_from, dir_name_exists, true));

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(dir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(subdir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name2_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(file_util::PathExists(subdir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name2_to));
}

TEST_F(FileUtilTest, MAYBE_CopyDirectoryNew) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  file_util::CreateDirectory(subdir_name_from);
  ASSERT_TRUE(file_util::PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name2_from));

  // Copy the directory not recursively.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::CopyDirectory(dir_name_from, dir_name_to, false));

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(dir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(subdir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name2_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_FALSE(file_util::PathExists(subdir_name_to));
}

TEST_F(FileUtilTest, MAYBE_CopyDirectoryExists) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  file_util::CreateDirectory(subdir_name_from);
  ASSERT_TRUE(file_util::PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name2_from));

  // Copy the directory not recursively.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));

  // Create the destination directory.
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::CopyDirectory(dir_name_from, dir_name_to, false));

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(dir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(subdir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name2_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_FALSE(file_util::PathExists(subdir_name_to));
}

TEST_F(FileUtilTest, MAYBE_CopyFileWithCopyDirectoryRecursiveToNew) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // The destination name
  FilePath file_name_to = temp_dir_.path().Append(
      FILE_PATH_LITERAL("Copy_Test_File_Destination.txt"));
  ASSERT_FALSE(file_util::PathExists(file_name_to));

  EXPECT_TRUE(file_util::CopyDirectory(file_name_from, file_name_to, true));

  // Check the has been copied
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}

TEST_F(FileUtilTest, MAYBE_CopyFileWithCopyDirectoryRecursiveToExisting) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // The destination name
  FilePath file_name_to = temp_dir_.path().Append(
      FILE_PATH_LITERAL("Copy_Test_File_Destination.txt"));
  CreateTextFile(file_name_to, L"Old file content");
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  EXPECT_TRUE(file_util::CopyDirectory(file_name_from, file_name_to, true));

  // Check the has been copied
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(L"Gooooooooooooooooooooogle" == ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest,
       MAYBE_CopyFileWithCopyDirectoryRecursiveToExistingDirectory) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // The destination
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Destination"));
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  EXPECT_TRUE(file_util::CopyDirectory(file_name_from, dir_name_to, true));

  // Check the has been copied
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}

TEST_F(FileUtilTest, MAYBE_CopyDirectoryWithTrailingSeparators) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Copy the directory recursively.
  FilePath dir_name_to =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  // Create from path with trailing separators.
#if defined(OS_WIN)
  FilePath from_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir\\\\\\"));
#elif defined (OS_POSIX)
  FilePath from_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir///"));
#endif

  EXPECT_TRUE(file_util::CopyDirectory(from_path, dir_name_to, true));

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(dir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}
#endif  // defined(OS_STARBOARD)

TEST_F(FileUtilTest, CopyFile) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  const std::wstring file_contents(L"Gooooooooooooooooooooogle");
  CreateTextFile(file_name_from, file_contents);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Copy the file.
  FilePath dest_file = dir_name_from.Append(FILE_PATH_LITERAL("DestFile.txt"));
  ASSERT_TRUE(file_util::CopyFile(file_name_from, dest_file));

#if defined(OS_STARBOARD)
  // Why should we ensure CopyFile is unsafe? On Starboard, it won't open files
  // with a ".." in the path.
  // Copy the file to another location using '..' in the path.
  FilePath dest_file2(temp_dir_.path());
  dest_file2 = dest_file2.AppendASCII("DestFile.txt");
  ASSERT_TRUE(file_util::CopyFile(file_name_from, dest_file2));
#else
  // Copy the file to another location using '..' in the path.
  FilePath dest_file2(dir_name_from);
  dest_file2 = dest_file2.AppendASCII("..");
  dest_file2 = dest_file2.AppendASCII("DestFile.txt");
  ASSERT_TRUE(file_util::CopyFile(file_name_from, dest_file2));
#endif

  FilePath dest_file2_test(dir_name_from);
  dest_file2_test = dest_file2_test.DirName();
  dest_file2_test = dest_file2_test.AppendASCII("DestFile.txt");

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dest_file));
  const std::wstring read_contents = ReadTextFile(dest_file);
  EXPECT_EQ(file_contents, read_contents);
  EXPECT_TRUE(file_util::PathExists(dest_file2_test));
  EXPECT_TRUE(file_util::PathExists(dest_file2));
}

// TODO(erikkay): implement
#if defined(OS_WIN)
TEST_F(FileUtilTest, GetFileCreationLocalTime) {
  FilePath file_name = temp_dir_.path().Append(L"Test File.txt");

  SYSTEMTIME start_time;
  GetLocalTime(&start_time);
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  CreateTextFile(file_name, L"New file!");
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  SYSTEMTIME end_time;
  GetLocalTime(&end_time);

  SYSTEMTIME file_creation_time;
  file_util::GetFileCreationLocalTime(file_name.value(), &file_creation_time);

  FILETIME start_filetime;
  SystemTimeToFileTime(&start_time, &start_filetime);
  FILETIME end_filetime;
  SystemTimeToFileTime(&end_time, &end_filetime);
  FILETIME file_creation_filetime;
  SystemTimeToFileTime(&file_creation_time, &file_creation_filetime);

  EXPECT_EQ(-1, CompareFileTime(&start_filetime, &file_creation_filetime)) <<
    "start time: " << FileTimeAsUint64(start_filetime) << ", " <<
    "creation time: " << FileTimeAsUint64(file_creation_filetime);

  EXPECT_EQ(-1, CompareFileTime(&file_creation_filetime, &end_filetime)) <<
    "creation time: " << FileTimeAsUint64(file_creation_filetime) << ", " <<
    "end time: " << FileTimeAsUint64(end_filetime);

  ASSERT_TRUE(DeleteFile(file_name.value().c_str()));
}
#endif

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest.
typedef PlatformTest ReadOnlyFileUtilTest;

#if !defined(__LB_WIIU__) && !defined(OS_STARBOARD)
TEST_F(ReadOnlyFileUtilTest, ContentsEqual) {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("base"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(FILE_PATH_LITERAL("file_util_unittest"));
  ASSERT_TRUE(file_util::PathExists(data_dir));

  FilePath original_file =
      data_dir.Append(FILE_PATH_LITERAL("original.txt"));
  FilePath same_file =
      data_dir.Append(FILE_PATH_LITERAL("same.txt"));
  FilePath same_length_file =
      data_dir.Append(FILE_PATH_LITERAL("same_length.txt"));
  FilePath different_file =
      data_dir.Append(FILE_PATH_LITERAL("different.txt"));
  FilePath different_first_file =
      data_dir.Append(FILE_PATH_LITERAL("different_first.txt"));
  FilePath different_last_file =
      data_dir.Append(FILE_PATH_LITERAL("different_last.txt"));
  FilePath empty1_file =
      data_dir.Append(FILE_PATH_LITERAL("empty1.txt"));
  FilePath empty2_file =
      data_dir.Append(FILE_PATH_LITERAL("empty2.txt"));
  FilePath shortened_file =
      data_dir.Append(FILE_PATH_LITERAL("shortened.txt"));
  FilePath binary_file =
      data_dir.Append(FILE_PATH_LITERAL("binary_file.bin"));
  FilePath binary_file_same =
      data_dir.Append(FILE_PATH_LITERAL("binary_file_same.bin"));
  FilePath binary_file_diff =
      data_dir.Append(FILE_PATH_LITERAL("binary_file_diff.bin"));

  EXPECT_TRUE(file_util::ContentsEqual(original_file, original_file));
  EXPECT_TRUE(file_util::ContentsEqual(original_file, same_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, same_length_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, different_file));
  EXPECT_FALSE(file_util::ContentsEqual(
      FilePath(FILE_PATH_LITERAL("bogusname")),
      FilePath(FILE_PATH_LITERAL("bogusname"))));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, different_first_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, different_last_file));
  EXPECT_TRUE(file_util::ContentsEqual(empty1_file, empty2_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, shortened_file));
  EXPECT_FALSE(file_util::ContentsEqual(shortened_file, original_file));
  EXPECT_TRUE(file_util::ContentsEqual(binary_file, binary_file_same));
  EXPECT_FALSE(file_util::ContentsEqual(binary_file, binary_file_diff));
}
#endif

#if !defined(__LB_SHELL__) && !defined(OS_STARBOARD)
TEST_F(ReadOnlyFileUtilTest, TextContentsEqual) {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("base"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(FILE_PATH_LITERAL("file_util_unittest"));
  ASSERT_TRUE(file_util::PathExists(data_dir));

  FilePath original_file =
      data_dir.Append(FILE_PATH_LITERAL("original.txt"));
  FilePath same_file =
      data_dir.Append(FILE_PATH_LITERAL("same.txt"));
  FilePath crlf_file =
      data_dir.Append(FILE_PATH_LITERAL("crlf.txt"));
  FilePath shortened_file =
      data_dir.Append(FILE_PATH_LITERAL("shortened.txt"));
  FilePath different_file =
      data_dir.Append(FILE_PATH_LITERAL("different.txt"));
  FilePath different_first_file =
      data_dir.Append(FILE_PATH_LITERAL("different_first.txt"));
  FilePath different_last_file =
      data_dir.Append(FILE_PATH_LITERAL("different_last.txt"));
  FilePath first1_file =
      data_dir.Append(FILE_PATH_LITERAL("first1.txt"));
  FilePath first2_file =
      data_dir.Append(FILE_PATH_LITERAL("first2.txt"));
  FilePath empty1_file =
      data_dir.Append(FILE_PATH_LITERAL("empty1.txt"));
  FilePath empty2_file =
      data_dir.Append(FILE_PATH_LITERAL("empty2.txt"));
  FilePath blank_line_file =
      data_dir.Append(FILE_PATH_LITERAL("blank_line.txt"));
  FilePath blank_line_crlf_file =
      data_dir.Append(FILE_PATH_LITERAL("blank_line_crlf.txt"));

  EXPECT_TRUE(file_util::TextContentsEqual(original_file, same_file));
  EXPECT_TRUE(file_util::TextContentsEqual(original_file, crlf_file));
  EXPECT_FALSE(file_util::TextContentsEqual(original_file, shortened_file));
  EXPECT_FALSE(file_util::TextContentsEqual(original_file, different_file));
  EXPECT_FALSE(file_util::TextContentsEqual(original_file,
                                            different_first_file));
  EXPECT_FALSE(file_util::TextContentsEqual(original_file,
                                            different_last_file));
  EXPECT_FALSE(file_util::TextContentsEqual(first1_file, first2_file));
  EXPECT_TRUE(file_util::TextContentsEqual(empty1_file, empty2_file));
  EXPECT_FALSE(file_util::TextContentsEqual(original_file, empty1_file));
  EXPECT_TRUE(file_util::TextContentsEqual(blank_line_file,
                                           blank_line_crlf_file));
}
#endif // __LB_SHELL__

// We don't need equivalent functionality outside of Windows.
#if defined(OS_WIN)
TEST_F(FileUtilTest, CopyAndDeleteDirectoryTest) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.path().Append(FILE_PATH_LITERAL("CopyAndDelete_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("CopyAndDelete_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Move the directory by using CopyAndDeleteDirectory
  FilePath dir_name_to = temp_dir_.path().Append(
      FILE_PATH_LITERAL("CopyAndDelete_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("CopyAndDelete_Test_File.txt"));

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::CopyAndDeleteDirectory(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(file_util::PathExists(dir_name_from));
  EXPECT_FALSE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}

TEST_F(FileUtilTest, GetTempDirTest) {
  static const TCHAR* kTmpKey = _T("TMP");
  static const TCHAR* kTmpValues[] = {
    _T(""), _T("C:"), _T("C:\\"), _T("C:\\tmp"), _T("C:\\tmp\\")
  };
  // Save the original $TMP.
  size_t original_tmp_size;
  TCHAR* original_tmp;
  ASSERT_EQ(0, ::_tdupenv_s(&original_tmp, &original_tmp_size, kTmpKey));
  // original_tmp may be NULL.

  for (unsigned int i = 0; i < arraysize(kTmpValues); ++i) {
    FilePath path;
    ::_tputenv_s(kTmpKey, kTmpValues[i]);
    file_util::GetTempDir(&path);
    EXPECT_TRUE(path.IsAbsolute()) << "$TMP=" << kTmpValues[i] <<
        " result=" << path.value();
  }

  // Restore the original $TMP.
  if (original_tmp) {
    ::_tputenv_s(kTmpKey, original_tmp);
    free(original_tmp);
  } else {
    ::_tputenv_s(kTmpKey, _T(""));
  }
}
#endif  // OS_WIN

TEST_F(FileUtilTest, CreateTemporaryFileTest) {
  FilePath temp_files[3];
  for (int i = 0; i < 3; i++) {
    ASSERT_TRUE(file_util::CreateTemporaryFile(&(temp_files[i])));
    EXPECT_TRUE(file_util::PathExists(temp_files[i]));
    EXPECT_FALSE(file_util::DirectoryExists(temp_files[i]));
  }
  for (int i = 0; i < 3; i++)
    EXPECT_FALSE(temp_files[i] == temp_files[(i+1)%3]);
  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(file_util::Delete(temp_files[i], false));
}

#if !defined(OS_STARBOARD)
TEST_F(FileUtilTest, CreateAndOpenTemporaryFileTest) {
  FilePath names[3];
  FILE* fps[3];
  int i;

  // Create; make sure they are open and exist.
  for (i = 0; i < 3; ++i) {
    fps[i] = file_util::CreateAndOpenTemporaryFile(&(names[i]));
    ASSERT_TRUE(fps[i]);
    EXPECT_TRUE(file_util::PathExists(names[i]));
  }

  // Make sure all names are unique.
  for (i = 0; i < 3; ++i) {
    EXPECT_FALSE(names[i] == names[(i+1)%3]);
  }

  // Close and delete.
  for (i = 0; i < 3; ++i) {
    EXPECT_TRUE(file_util::CloseFile(fps[i]));
    EXPECT_TRUE(file_util::Delete(names[i], false));
  }
}
#endif

TEST_F(FileUtilTest, CreateNewTempDirectoryTest) {
  FilePath temp_dir;
  ASSERT_TRUE(file_util::CreateNewTempDirectory(FilePath::StringType(),
                                                &temp_dir));
  EXPECT_TRUE(file_util::PathExists(temp_dir));
  EXPECT_TRUE(file_util::Delete(temp_dir, false));
}

TEST_F(FileUtilTest, CreateNewTemporaryDirInDirTest) {
  FilePath new_dir;
  ASSERT_TRUE(file_util::CreateTemporaryDirInDir(
                  temp_dir_.path(),
                  FILE_PATH_LITERAL("CreateNewTemporaryDirInDirTest"),
                  &new_dir));
  EXPECT_TRUE(file_util::PathExists(new_dir));
  EXPECT_TRUE(temp_dir_.path().IsParent(new_dir));
  EXPECT_TRUE(file_util::Delete(new_dir, false));
}

#if !defined(__LB_SHELL__)
TEST_F(FileUtilTest, GetShmemTempDirTest) {
  FilePath dir;
  EXPECT_TRUE(file_util::GetShmemTempDir(&dir, false));
  EXPECT_TRUE(file_util::DirectoryExists(dir));
}
#endif

TEST_F(FileUtilTest, CreateDirectoryTest) {
  FilePath test_root =
      temp_dir_.path().Append(FILE_PATH_LITERAL("create_directory_test"));
#if defined(OS_WIN)
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("dir\\tree\\likely\\doesnt\\exist\\"));
#elif defined(OS_POSIX)
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("dir/tree/likely/doesnt/exist/"));
#elif defined(OS_STARBOARD)
  // Directory depth is limited on certain platforms.
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("tree/likely/doesnt/exist/"));
#endif

  EXPECT_FALSE(file_util::PathExists(test_path));
  EXPECT_TRUE(file_util::CreateDirectory(test_path));
  EXPECT_TRUE(file_util::PathExists(test_path));
  // CreateDirectory returns true if the DirectoryExists returns true.
  EXPECT_TRUE(file_util::CreateDirectory(test_path));

  // Doesn't work to create it on top of a non-dir
  test_path = test_path.Append(FILE_PATH_LITERAL("foobar.txt"));
  EXPECT_FALSE(file_util::PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(file_util::PathExists(test_path));
  EXPECT_FALSE(file_util::CreateDirectory(test_path));

  EXPECT_TRUE(file_util::Delete(test_root, true));
  EXPECT_FALSE(file_util::PathExists(test_root));
  EXPECT_FALSE(file_util::PathExists(test_path));

#if !defined(__LB_PS3__) && !defined(OS_STARBOARD)
  // Starboard doesn't support relative paths.

  // Verify assumptions made by the Windows implementation:
  // 1. The current directory always exists.
  // 2. The root directory always exists.
  ASSERT_TRUE(file_util::DirectoryExists(
      FilePath(FilePath::kCurrentDirectory)));
#endif

  FilePath top_level = test_root;
  while (top_level != top_level.DirName()) {
    top_level = top_level.DirName();
  }
  ASSERT_TRUE(file_util::DirectoryExists(top_level));

  // Given these assumptions hold, it should be safe to
  // test that "creating" these directories succeeds.
#if !defined(OS_STARBOARD)
  EXPECT_TRUE(file_util::CreateDirectory(
      FilePath(FilePath::kCurrentDirectory)));
#endif

  EXPECT_TRUE(file_util::CreateDirectory(top_level));

#if defined(OS_WIN)
  FilePath invalid_drive(FILE_PATH_LITERAL("o:\\"));
  FilePath invalid_path =
      invalid_drive.Append(FILE_PATH_LITERAL("some\\inaccessible\\dir"));
  if (!file_util::PathExists(invalid_drive)) {
    EXPECT_FALSE(file_util::CreateDirectory(invalid_path));
  }
#endif
}

TEST_F(FileUtilTest, DetectDirectoryTest) {
  // Check a directory
  FilePath test_root =
      temp_dir_.path().Append(FILE_PATH_LITERAL("detect_directory_test"));
  EXPECT_FALSE(file_util::PathExists(test_root));
  EXPECT_TRUE(file_util::CreateDirectory(test_root));
  EXPECT_TRUE(file_util::PathExists(test_root));
  EXPECT_TRUE(file_util::DirectoryExists(test_root));
  // Check a file
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("foobar.txt"));
  EXPECT_FALSE(file_util::PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(file_util::PathExists(test_path));
  EXPECT_FALSE(file_util::DirectoryExists(test_path));
  EXPECT_TRUE(file_util::Delete(test_path, false));

  EXPECT_TRUE(file_util::Delete(test_root, true));
}

TEST_F(FileUtilTest, FileEnumeratorTest) {
  // Test an empty directory.
  file_util::FileEnumerator f0(temp_dir_.path(), true, FILES_AND_DIRECTORIES);
  EXPECT_EQ(f0.Next().value(), FILE_PATH_LITERAL(""));
  EXPECT_EQ(f0.Next().value(), FILE_PATH_LITERAL(""));

  // Test an empty directory, non-recursively, including "..".
#if !defined(OS_STARBOARD)
  // Starboard doesn't support relative paths.
  file_util::FileEnumerator f0_dotdot(temp_dir_.path(), false,
      FILES_AND_DIRECTORIES | file_util::FileEnumerator::INCLUDE_DOT_DOT);
  EXPECT_EQ(temp_dir_.path().Append(FILE_PATH_LITERAL("..")).value(),
            f0_dotdot.Next().value());
  EXPECT_EQ(FILE_PATH_LITERAL(""),
            f0_dotdot.Next().value());
#endif

  // create the directories
  FilePath dir1 = temp_dir_.path().Append(FILE_PATH_LITERAL("dir1"));
  EXPECT_TRUE(file_util::CreateDirectory(dir1));
  FilePath dir2 = temp_dir_.path().Append(FILE_PATH_LITERAL("dir2"));
  EXPECT_TRUE(file_util::CreateDirectory(dir2));
  FilePath dir2inner = dir2.Append(FILE_PATH_LITERAL("inner"));
  EXPECT_TRUE(file_util::CreateDirectory(dir2inner));

  // create the files
  FilePath dir2file = dir2.Append(FILE_PATH_LITERAL("dir2file.txt"));
  CreateTextFile(dir2file, L"");
  FilePath dir2innerfile = dir2inner.Append(FILE_PATH_LITERAL("innerfile.txt"));
  CreateTextFile(dir2innerfile, L"");
  FilePath file1 = temp_dir_.path().Append(FILE_PATH_LITERAL("file1.txt"));
  CreateTextFile(file1, L"");
  FilePath file2_rel =
      dir2.Append(FilePath::kParentDirectory)
          .Append(FILE_PATH_LITERAL("file2.txt"));
  CreateTextFile(file2_rel, L"");
  FilePath file2_abs = temp_dir_.path().Append(FILE_PATH_LITERAL("file2.txt"));

  // Only enumerate files.
  file_util::FileEnumerator f1(temp_dir_.path(), true,
                               file_util::FileEnumerator::FILES);
  FindResultCollector c1(f1);
  EXPECT_TRUE(c1.HasFile(file1));
  EXPECT_TRUE(c1.HasFile(file2_abs));
  EXPECT_TRUE(c1.HasFile(dir2file));
  EXPECT_TRUE(c1.HasFile(dir2innerfile));
  EXPECT_EQ(c1.size(), 4);

  // Only enumerate directories.
  file_util::FileEnumerator f2(temp_dir_.path(), true,
                               file_util::FileEnumerator::DIRECTORIES);
  FindResultCollector c2(f2);
  EXPECT_TRUE(c2.HasFile(dir1));
  EXPECT_TRUE(c2.HasFile(dir2));
  EXPECT_TRUE(c2.HasFile(dir2inner));
  EXPECT_EQ(c2.size(), 3);

  // Only enumerate directories non-recursively.
  file_util::FileEnumerator f2_non_recursive(
      temp_dir_.path(), false, file_util::FileEnumerator::DIRECTORIES);
  FindResultCollector c2_non_recursive(f2_non_recursive);
  EXPECT_TRUE(c2_non_recursive.HasFile(dir1));
  EXPECT_TRUE(c2_non_recursive.HasFile(dir2));
  EXPECT_EQ(c2_non_recursive.size(), 2);

#if !defined(__LB_XB1__) && !defined(OS_STARBOARD)
// As all file access is absolute, this is not an issue
  // Only enumerate directories, non-recursively, including "..".
  file_util::FileEnumerator f2_dotdot(temp_dir_.path(), false,
      file_util::FileEnumerator::DIRECTORIES |
      file_util::FileEnumerator::INCLUDE_DOT_DOT);
  FindResultCollector c2_dotdot(f2_dotdot);
  EXPECT_TRUE(c2_dotdot.HasFile(dir1));
  EXPECT_TRUE(c2_dotdot.HasFile(dir2));
  EXPECT_TRUE(c2_dotdot.HasFile(
      temp_dir_.path().Append(FILE_PATH_LITERAL(".."))));
  EXPECT_EQ(c2_dotdot.size(), 3);
#endif

  // Enumerate files and directories.
  file_util::FileEnumerator f3(temp_dir_.path(), true, FILES_AND_DIRECTORIES);
  FindResultCollector c3(f3);
  EXPECT_TRUE(c3.HasFile(dir1));
  EXPECT_TRUE(c3.HasFile(dir2));
  EXPECT_TRUE(c3.HasFile(file1));
  EXPECT_TRUE(c3.HasFile(file2_abs));
  EXPECT_TRUE(c3.HasFile(dir2file));
  EXPECT_TRUE(c3.HasFile(dir2inner));
  EXPECT_TRUE(c3.HasFile(dir2innerfile));
  EXPECT_EQ(c3.size(), 7);

  // Non-recursive operation.
  file_util::FileEnumerator f4(temp_dir_.path(), false, FILES_AND_DIRECTORIES);
  FindResultCollector c4(f4);
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(file1));
  EXPECT_TRUE(c4.HasFile(file2_abs));
  EXPECT_EQ(c4.size(), 4);

#if !defined(OS_STARBOARD)
  // Enumerate with a pattern.
  file_util::FileEnumerator f5(temp_dir_.path(), true, FILES_AND_DIRECTORIES,
      FILE_PATH_LITERAL("dir*"));
  FindResultCollector c5(f5);
  EXPECT_TRUE(c5.HasFile(dir1));
  EXPECT_TRUE(c5.HasFile(dir2));
  EXPECT_TRUE(c5.HasFile(dir2file));
  EXPECT_TRUE(c5.HasFile(dir2inner));
  EXPECT_TRUE(c5.HasFile(dir2innerfile));
  EXPECT_EQ(c5.size(), 5);
#endif

  // Make sure the destructor closes the find handle while in the middle of a
  // query to allow TearDown to delete the directory.
  file_util::FileEnumerator f6(temp_dir_.path(), true, FILES_AND_DIRECTORIES);
  EXPECT_FALSE(f6.Next().value().empty());  // Should have found something
                                            // (we don't care what).
}

TEST_F(FileUtilTest, AppendToFile) {
  FilePath data_dir =
      temp_dir_.path().Append(FILE_PATH_LITERAL("FilePathTest"));

  // Create a fresh, empty copy of this directory.
  if (file_util::PathExists(data_dir)) {
    ASSERT_TRUE(file_util::Delete(data_dir, true));
  }
  ASSERT_TRUE(file_util::CreateDirectory(data_dir));

  // Create a fresh, empty copy of this directory.
  if (file_util::PathExists(data_dir)) {
    ASSERT_TRUE(file_util::Delete(data_dir, true));
  }
  ASSERT_TRUE(file_util::CreateDirectory(data_dir));
  FilePath foobar(data_dir.Append(FILE_PATH_LITERAL("foobar.txt")));

  std::string data("hello");
  EXPECT_EQ(-1, file_util::AppendToFile(foobar, data.c_str(), data.length()));
  EXPECT_EQ(static_cast<int>(data.length()),
            file_util::WriteFile(foobar, data.c_str(), data.length()));
  EXPECT_EQ(static_cast<int>(data.length()),
            file_util::AppendToFile(foobar, data.c_str(), data.length()));

  const std::wstring read_content = ReadTextFile(foobar);
  EXPECT_EQ(L"hellohello", read_content);
}

TEST_F(FileUtilTest, Contains) {
  FilePath data_dir =
      temp_dir_.path().Append(FILE_PATH_LITERAL("FilePathTest"));

  // Create a fresh, empty copy of this directory.
  if (file_util::PathExists(data_dir)) {
    ASSERT_TRUE(file_util::Delete(data_dir, true));
  }
  ASSERT_TRUE(file_util::CreateDirectory(data_dir));

  FilePath foo(data_dir.Append(FILE_PATH_LITERAL("foo")));
  FilePath bar(foo.Append(FILE_PATH_LITERAL("bar.txt")));
  FilePath baz(data_dir.Append(FILE_PATH_LITERAL("baz.txt")));
  FilePath foobar(data_dir.Append(FILE_PATH_LITERAL("foobar.txt")));

  // Annoyingly, the directories must actually exist in order for realpath(),
  // which Contains() relies on in posix, to work.
  ASSERT_TRUE(file_util::CreateDirectory(foo));
  std::string data("hello");
  ASSERT_TRUE(file_util::WriteFile(bar, data.c_str(), data.length()));
  ASSERT_TRUE(file_util::WriteFile(baz, data.c_str(), data.length()));
  ASSERT_TRUE(file_util::WriteFile(foobar, data.c_str(), data.length()));

  EXPECT_TRUE(file_util::ContainsPath(foo, bar));
  EXPECT_FALSE(file_util::ContainsPath(foo, baz));
  EXPECT_FALSE(file_util::ContainsPath(foo, foobar));
  EXPECT_FALSE(file_util::ContainsPath(foo, foo));

  // Platform-specific concerns.
  FilePath foo_caps(data_dir.Append(FILE_PATH_LITERAL("FOO")));
#if defined(OS_WIN)
  EXPECT_TRUE(file_util::ContainsPath(foo,
      foo_caps.Append(FILE_PATH_LITERAL("bar.txt"))));
  EXPECT_TRUE(file_util::ContainsPath(foo,
      FilePath(foo.value() + FILE_PATH_LITERAL("/bar.txt"))));
#elif defined(OS_MACOSX)
  // We can't really do this test on OS X since the case-sensitivity of the
  // filesystem is configurable.
#elif defined(OS_POSIX)
  EXPECT_FALSE(file_util::ContainsPath(foo,
      foo_caps.Append(FILE_PATH_LITERAL("bar.txt"))));
#endif
}

#if !defined(OS_STARBOARD)
TEST_F(FileUtilTest, TouchFile) {
  FilePath data_dir =
      temp_dir_.path().Append(FILE_PATH_LITERAL("FilePathTest"));

  // Create a fresh, empty copy of this directory.
  if (file_util::PathExists(data_dir)) {
    ASSERT_TRUE(file_util::Delete(data_dir, true));
  }
  ASSERT_TRUE(file_util::CreateDirectory(data_dir));

  FilePath foobar(data_dir.Append(FILE_PATH_LITERAL("foobar.txt")));
  std::string data("hello");
  ASSERT_TRUE(file_util::WriteFile(foobar, data.c_str(), data.length()));

  base::Time access_time;
  // This timestamp is divisible by one day (in local timezone),
  // to make it work on FAT too.
  ASSERT_TRUE(base::Time::FromString("Wed, 16 Nov 1994, 00:00:00",
                                     &access_time));

  base::Time modification_time;
  // Note that this timestamp is divisible by two (seconds) - FAT stores
  // modification times with 2s resolution.
  ASSERT_TRUE(base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT",
              &modification_time));

#if !defined(__LB_PS3__)
  // file_util::TouchFile creates a platform file and passes its descriptor to
  // base::TouchPlatformFile().
  // Unfortunately, PS3 only implements utime, not futime, which means it
  // can only touch a file path, not a file descriptor.

  ASSERT_TRUE(file_util::TouchFile(foobar, access_time, modification_time));
  base::PlatformFileInfo file_info;
  ASSERT_TRUE(file_util::GetFileInfo(foobar, &file_info));
  EXPECT_EQ(file_info.last_accessed.ToInternalValue(),
            access_time.ToInternalValue());
  EXPECT_EQ(file_info.last_modified.ToInternalValue(),
            modification_time.ToInternalValue());
#endif
}
#endif

TEST_F(FileUtilTest, IsDirectoryEmpty) {
  FilePath empty_dir = temp_dir_.path().Append(FILE_PATH_LITERAL("EmptyDir"));

  ASSERT_FALSE(file_util::PathExists(empty_dir));

  ASSERT_TRUE(file_util::CreateDirectory(empty_dir));

  EXPECT_TRUE(file_util::IsDirectoryEmpty(empty_dir));

  FilePath foo(empty_dir.Append(FILE_PATH_LITERAL("foo.txt")));
  std::string bar("baz");
  ASSERT_TRUE(file_util::WriteFile(foo, bar.c_str(), bar.length()));

  EXPECT_FALSE(file_util::IsDirectoryEmpty(empty_dir));
}

#if defined(OS_POSIX) && !defined(__LB_SHELL__)
// We don't support uids or symlinks in lbshell

// Testing VerifyPathControlledByAdmin() is hard, because there is no
// way a test can make a file owned by root, or change file paths
// at the root of the file system.  VerifyPathControlledByAdmin()
// is implemented as a call to VerifyPathControlledByUser, which gives
// us the ability to test with paths under the test's temp directory,
// using a user id we control.
// Pull tests of VerifyPathControlledByUserTest() into a separate test class
// with a common SetUp() method.
class VerifyPathControlledByUserTest : public FileUtilTest {
 protected:
  virtual void SetUp() OVERRIDE {
    FileUtilTest::SetUp();

    // Create a basic structure used by each test.
    // base_dir_
    //  |-> sub_dir_
    //       |-> text_file_

    base_dir_ = temp_dir_.path().AppendASCII("base_dir");
    ASSERT_TRUE(file_util::CreateDirectory(base_dir_));

    sub_dir_ = base_dir_.AppendASCII("sub_dir");
    ASSERT_TRUE(file_util::CreateDirectory(sub_dir_));

    text_file_ = sub_dir_.AppendASCII("file.txt");
    CreateTextFile(text_file_, L"This text file has some text in it.");

    // Get the user and group files are created with from |base_dir_|.
    struct stat stat_buf;
    ASSERT_EQ(0, stat(base_dir_.value().c_str(), &stat_buf));
    uid_ = stat_buf.st_uid;
    ok_gids_.insert(stat_buf.st_gid);
    bad_gids_.insert(stat_buf.st_gid + 1);

    ASSERT_EQ(uid_, getuid());  // This process should be the owner.

    // To ensure that umask settings do not cause the initial state
    // of permissions to be different from what we expect, explicitly
    // set permissions on the directories we create.
    // Make all files and directories non-world-writable.

    // Users and group can read, write, traverse
    int enabled_permissions =
        file_util::FILE_PERMISSION_USER_MASK |
        file_util::FILE_PERMISSION_GROUP_MASK;
    // Other users can't read, write, traverse
    int disabled_permissions =
        file_util::FILE_PERMISSION_OTHERS_MASK;

    ASSERT_NO_FATAL_FAILURE(
        ChangePosixFilePermissions(
            base_dir_, enabled_permissions, disabled_permissions));
    ASSERT_NO_FATAL_FAILURE(
        ChangePosixFilePermissions(
            sub_dir_, enabled_permissions, disabled_permissions));
  }

  FilePath base_dir_;
  FilePath sub_dir_;
  FilePath text_file_;
  uid_t uid_;

  std::set<gid_t> ok_gids_;
  std::set<gid_t> bad_gids_;
};

TEST_F(VerifyPathControlledByUserTest, BadPaths) {
  // File does not exist.
  FilePath does_not_exist = base_dir_.AppendASCII("does")
                                     .AppendASCII("not")
                                     .AppendASCII("exist");
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, does_not_exist, uid_, ok_gids_));

  // |base| not a subpath of |path|.
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, base_dir_, uid_, ok_gids_));

  // An empty base path will fail to be a prefix for any path.
  FilePath empty;
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          empty, base_dir_, uid_, ok_gids_));

  // Finding that a bad call fails proves nothing unless a good call succeeds.
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
}

#if !defined(__LB_SHELL__)
// We don't support uids or symlinks in lbshell

TEST_F(VerifyPathControlledByUserTest, Symlinks) {
  // Symlinks in the path should cause failure.

  // Symlink to the file at the end of the path.
  FilePath file_link =  base_dir_.AppendASCII("file_link");
  ASSERT_TRUE(file_util::CreateSymbolicLink(text_file_, file_link))
      << "Failed to create symlink.";

  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, file_link, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          file_link, file_link, uid_, ok_gids_));

  // Symlink from one directory to another within the path.
  FilePath link_to_sub_dir =  base_dir_.AppendASCII("link_to_sub_dir");
  ASSERT_TRUE(file_util::CreateSymbolicLink(sub_dir_, link_to_sub_dir))
    << "Failed to create symlink.";

  FilePath file_path_with_link = link_to_sub_dir.AppendASCII("file.txt");
  ASSERT_TRUE(file_util::PathExists(file_path_with_link));

  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, file_path_with_link, uid_, ok_gids_));

  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          link_to_sub_dir, file_path_with_link, uid_, ok_gids_));

  // Symlinks in parents of base path are allowed.
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          file_path_with_link, file_path_with_link, uid_, ok_gids_));
}

TEST_F(VerifyPathControlledByUserTest, OwnershipChecks) {
  // Get a uid that is not the uid of files we create.
  uid_t bad_uid = uid_ + 1;

  // Make all files and directories non-world-writable.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(base_dir_, 0u, S_IWOTH));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(sub_dir_, 0u, S_IWOTH));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(text_file_, 0u, S_IWOTH));

  // We control these paths.
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));

  // Another user does not control these paths.
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, bad_uid, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, bad_uid, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, bad_uid, ok_gids_));

  // Another group does not control the paths.
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, bad_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, bad_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, bad_gids_));
}

TEST_F(VerifyPathControlledByUserTest, GroupWriteTest) {
  // Make all files and directories writable only by their owner.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(base_dir_, 0u, S_IWOTH|S_IWGRP));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(sub_dir_, 0u, S_IWOTH|S_IWGRP));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(text_file_, 0u, S_IWOTH|S_IWGRP));

  // Any group is okay because the path is not group-writable.
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));

  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, bad_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, bad_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, bad_gids_));

  // No group is okay, because we don't check the group
  // if no group can write.
  std::set<gid_t> no_gids;  // Empty set of gids.
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, no_gids));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, no_gids));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, no_gids));


  // Make all files and directories writable by their group.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(base_dir_, S_IWGRP, 0u));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(sub_dir_, S_IWGRP, 0u));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(text_file_, S_IWGRP, 0u));

  // Now |ok_gids_| works, but |bad_gids_| fails.
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));

  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, bad_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, bad_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, bad_gids_));

  // Because any group in the group set is allowed,
  // the union of good and bad gids passes.

  std::set<gid_t> multiple_gids;
  std::set_union(
      ok_gids_.begin(), ok_gids_.end(),
      bad_gids_.begin(), bad_gids_.end(),
      std::inserter(multiple_gids, multiple_gids.begin()));

  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, multiple_gids));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, multiple_gids));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, multiple_gids));
}

TEST_F(VerifyPathControlledByUserTest, WriteBitChecks) {
  // Make all files and directories non-world-writable.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(base_dir_, 0u, S_IWOTH));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(sub_dir_, 0u, S_IWOTH));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(text_file_, 0u, S_IWOTH));

  // Initialy, we control all parts of the path.
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));

  // Make base_dir_ world-writable.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(base_dir_, S_IWOTH, 0u));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));

  // Make sub_dir_ world writable.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(sub_dir_, S_IWOTH, 0u));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));

  // Make text_file_ world writable.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(text_file_, S_IWOTH, 0u));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));

  // Make sub_dir_ non-world writable.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(sub_dir_, 0u, S_IWOTH));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));

  // Make base_dir_ non-world-writable.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(base_dir_, 0u, S_IWOTH));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_FALSE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));

  // Back to the initial state: Nothing is writable, so every path
  // should pass.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(text_file_, 0u, S_IWOTH));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(
      file_util::VerifyPathControlledByUser(
          sub_dir_, text_file_, uid_, ok_gids_));
}
#endif

#endif  // defined(OS_POSIX)

}  // namespace
