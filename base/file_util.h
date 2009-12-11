// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef BASE_FILE_UTIL_H_
#define BASE_FILE_UTIL_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <sys/stat.h>
#endif

#include <stdio.h>

#include <stack>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace base {
class Time;
}

namespace file_util {

//-----------------------------------------------------------------------------
// Functions that operate purely on a path string w/o touching the filesystem:

// Returns true if the given path ends with a path separator character.
bool EndsWithSeparator(const FilePath& path);
// These two versions are both deprecated. TODO(estade): remove them.
bool EndsWithSeparator(std::wstring* path);
bool EndsWithSeparator(const std::wstring& path);

// Makes sure that |path| ends with a separator IFF path is a directory that
// exists. Returns true if |path| is an existing directory, false otherwise.
bool EnsureEndsWithSeparator(FilePath* path);

// Strips the topmost directory from the end of 'dir'.  Assumes 'dir' does not
// refer to a file.
// If 'dir' is a root directory, return without change.
// Deprecated. Use FilePath::DirName instead.
void UpOneDirectory(std::wstring* dir);
// Strips the topmost directory from the end of 'dir'.  Assumes 'dir' does not
// refer to a file.
// If 'dir' is a root directory, the result becomes empty string.
// Deprecated. Use FilePath::DirName instead.
void UpOneDirectoryOrEmpty(std::wstring* dir);

// Returns the filename portion of 'path', without any leading \'s or /'s.
// Deprecated. Use FilePath::BaseName instead.
std::wstring GetFilenameFromPath(const std::wstring& path);

// Deprecated compatibility function.  Use FilePath::Extension.
FilePath::StringType GetFileExtensionFromPath(const FilePath& path);
// Deprecated temporary compatibility function.
std::wstring GetFileExtensionFromPath(const std::wstring& path);

// Returns the directory component of a path, without the trailing
// path separator, or an empty string on error. The function does not
// check for the existence of the path, so if it is passed a directory
// without the trailing \, it will interpret the last component of the
// path as a file and chomp it. This does not support relative paths.
// Examples:
// path == "C:\pics\jojo.jpg",     returns "C:\pics"
// path == "C:\Windows\system32\", returns "C:\Windows\system32"
// path == "C:\Windows\system32",  returns "C:\Windows"
std::wstring GetDirectoryFromPath(const std::wstring& path);

// Appends new_ending to path, adding a separator between the two if necessary.
void AppendToPath(std::wstring* path, const std::wstring& new_ending);

// Convert provided relative path into an absolute path.  Returns false on
// error. On POSIX, this function fails if the path does not exist.
bool AbsolutePath(FilePath* path);
// Deprecated temporary compatibility function.
bool AbsolutePath(std::wstring* path);

// Returns true if |parent| contains |child|. Both paths are converted to
// absolute paths before doing the comparison.
bool ContainsPath(const FilePath& parent, const FilePath& child);

// Deprecated compatibility function.  Use FilePath::InsertBeforeExtension.
void InsertBeforeExtension(FilePath* path, const FilePath::StringType& suffix);

// Deprecated compatibility function.  Use FilePath::ReplaceExtension.
void ReplaceExtension(FilePath* file_name,
                      const FilePath::StringType& extension);

//-----------------------------------------------------------------------------
// Functions that involve filesystem access or modification:

// created on or after the given |file_time|.  Doesn't count ".." or ".".
//
// Note for POSIX environments: a file created before |file_time|
// can be mis-detected as a newer file due to low precision of
// timestmap of file creation time. If you need to avoid such
// mis-detection perfectly, you should wait one second before
// obtaining |file_time|.
int CountFilesCreatedAfter(const FilePath& path,
                           const base::Time& file_time);

// Deletes the given path, whether it's a file or a directory.
// If it's a directory, it's perfectly happy to delete all of the
// directory's contents.  Passing true to recursive deletes
// subdirectories and their contents as well.
// Returns true if successful, false otherwise.
//
// WARNING: USING THIS WITH recursive==true IS EQUIVALENT
//          TO "rm -rf", SO USE WITH CAUTION.
bool Delete(const FilePath& path, bool recursive);
// Deprecated temporary compatibility function.
bool Delete(const std::wstring& path, bool recursive);

// Moves the given path, whether it's a file or a directory.
// If a simple rename is not possible, such as in the case where the paths are
// on different volumes, this will attempt to copy and delete. Returns
// true for success.
bool Move(const FilePath& from_path, const FilePath& to_path);

// Renames file |from_path| to |to_path|. Both paths must be on the same
// volume, or the function will fail. Destination file will be created
// if it doesn't exist. Prefer this function over Move when dealing with
// temporary files. On Windows it preserves attributes of the target file.
// Returns true on success.
bool ReplaceFile(const FilePath& from_path, const FilePath& to_path);

// Copies a single file. Use CopyDirectory to copy directories.
bool CopyFile(const FilePath& from_path, const FilePath& to_path);

// Copies the given path, and optionally all subdirectories and their contents
// as well.
// If there are files existing under to_path, always overwrite.
// Returns true if successful, false otherwise.
// Don't use wildcards on the names, it may stop working without notice.
//
// If you only need to copy a file use CopyFile, it's faster.
bool CopyDirectory(const FilePath& from_path, const FilePath& to_path,
                   bool recursive);
// Deprecated temporary compatibility function.
bool CopyDirectory(const std::wstring& from_path, const std::wstring& to_path,
                   bool recursive);

// Returns true if the given path exists on the local filesystem,
// false otherwise.
bool PathExists(const FilePath& path);

// Returns true if the given path is writable by the user, false otherwise.
bool PathIsWritable(const FilePath& path);

// Returns true if the given path exists and is a directory, false otherwise.
bool DirectoryExists(const FilePath& path);

#if defined(OS_WIN)
// Gets the creation time of the given file (expressed in the local timezone),
// and returns it via the creation_time parameter.  Returns true if successful,
// false otherwise.
bool GetFileCreationLocalTime(const std::wstring& filename,
                              LPSYSTEMTIME creation_time);

// Same as above, but takes a previously-opened file handle instead of a name.
bool GetFileCreationLocalTimeFromHandle(HANDLE file_handle,
                                        LPSYSTEMTIME creation_time);
#endif  // defined(OS_WIN)

// Returns true if the contents of the two files given are equal, false
// otherwise.  If either file can't be read, returns false.
bool ContentsEqual(const FilePath& filename1,
                   const FilePath& filename2);

// Returns true if the contents of the two text files given are equal, false
// otherwise.  This routine treats "\r\n" and "\n" as equivalent.
bool TextContentsEqual(const FilePath& filename1, const FilePath& filename2);

// Read the file at |path| into |contents|, returning true on success.
// Useful for unit tests.
bool ReadFileToString(const FilePath& path, std::string* contents);
// Deprecated version.
bool ReadFileToString(const std::wstring& path, std::string* contents);

#if defined(OS_POSIX)
// Read exactly |bytes| bytes from file descriptor |fd|, storing the result
// in |buffer|. This function is protected against EINTR and partial reads.
// Returns true iff |bytes| bytes have been successfuly read from |fd|.
bool ReadFromFD(int fd, char* buffer, size_t bytes);
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
// Resolve Windows shortcut (.LNK file)
// This methods tries to resolve a shortcut .LNK file. If the |path| is valid
// returns true and puts the target into the |path|, otherwise returns
// false leaving the path as it is.
bool ResolveShortcut(FilePath* path);

// Create a Windows shortcut (.LNK file)
// This method creates a shortcut link using the information given. Ensure
// you have initialized COM before calling into this function. 'source'
// and 'destination' parameters are required, everything else can be NULL.
// 'source' is the existing file, 'destination' is the new link file to be
// created; for best results pass the filename with the .lnk extension.
// The 'icon' can specify a dll or exe in which case the icon index is the
// resource id. 'app_id' is the app model id for the shortcut on Win7.
// Note that if the shortcut exists it will overwrite it.
bool CreateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index, const wchar_t* app_id);

// Update a Windows shortcut (.LNK file). This method assumes the shortcut
// link already exists (otherwise false is returned). Ensure you have
// initialized COM before calling into this function. Only 'destination'
// parameter is required, everything else can be NULL (but if everything else
// is NULL no changes are made to the shortcut). 'destination' is the link
// file to be updated. 'app_id' is the app model id for the shortcut on Win7.
// For best results pass the filename with the .lnk extension.
bool UpdateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index, const wchar_t* app_id);

// Pins a shortcut to the Windows 7 taskbar. The shortcut file must already
// exist and be a shortcut that points to an executable.
bool TaskbarPinShortcutLink(const wchar_t* shortcut);

// Unpins a shortcut from the Windows 7 taskbar. The shortcut must exist and
// already be pinned to the taskbar.
bool TaskbarUnpinShortcutLink(const wchar_t* shortcut);

// Return true if the given directory is empty
bool IsDirectoryEmpty(const std::wstring& dir_path);

// Copy from_path to to_path recursively and then delete from_path recursively.
// Returns true if all operations succeed.
// This function simulates Move(), but unlike Move() it works across volumes.
// This fuction is not transactional.
bool CopyAndDeleteDirectory(const FilePath& from_path,
                            const FilePath& to_path);
#endif

// Get the temporary directory provided by the system.
bool GetTempDir(FilePath* path);
// Deprecated temporary compatibility function.
bool GetTempDir(std::wstring* path);
// Get a temporary directory for shared memory files.
// Only useful on POSIX; redirects to GetTempDir() on Windows.
bool GetShmemTempDir(FilePath* path);

// Creates a temporary file. The full path is placed in |path|, and the
// function returns true if was successful in creating the file. The file will
// be empty and all handles closed after this function returns.
bool CreateTemporaryFile(FilePath* path);

// Create and open a temporary file.  File is opened for read/write.
// The full path is placed in |path|, and the function returns true if
// was successful in creating and opening the file.
FILE* CreateAndOpenTemporaryFile(FilePath* path);
// Like above but for shmem files.  Only useful for POSIX.
FILE* CreateAndOpenTemporaryShmemFile(FilePath* path);

// Similar to CreateAndOpenTemporaryFile, but the file is created in |dir|.
FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path);

// Same as CreateTemporaryFile but the file is created in |dir|.
bool CreateTemporaryFileInDir(const FilePath& dir,
                              FilePath* temp_file);

// Create a new directory under TempPath. If prefix is provided, the new
// directory name is in the format of prefixyyyy.
// NOTE: prefix is ignored in the POSIX implementation.
// TODO(erikkay): is this OK?
// If success, return true and output the full path of the directory created.
bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path);

// Creates a directory, as well as creating any parent directories, if they
// don't exist. Returns 'true' on successful creation, or if the directory
// already exists.  The directory is only readable by the current user.
bool CreateDirectory(const FilePath& full_path);

// Returns the file size. Returns true on success.
bool GetFileSize(const FilePath& file_path, int64* file_size);
// Deprecated temporary compatibility function.
bool GetFileSize(const std::wstring& file_path, int64* file_size);

// Used to hold information about a given file path.  See GetFileInfo below.
struct FileInfo {
  // The size of the file in bytes.  Undefined when is_directory is true.
  int64 size;

  // True if the file corresponds to a directory.
  bool is_directory;

  // The last modified time of a file.
  base::Time last_modified;

  // Add additional fields here as needed.
};

// Returns information about the given file path.
bool GetFileInfo(const FilePath& file_path, FileInfo* info);
// Deprecated temporary compatibility function.
bool GetFileInfo(const std::wstring& file_path, FileInfo* info);

#if defined(OS_POSIX)
// Store inode number of |path| in |inode|. Return true on success.
bool GetInode(const FilePath& path, ino_t* inode);
#endif

// Wrapper for fopen-like calls. Returns non-NULL FILE* on success.
FILE* OpenFile(const FilePath& filename, const char* mode);
// Deprecated temporary compatibility functions.
FILE* OpenFile(const std::string& filename, const char* mode);
FILE* OpenFile(const std::wstring& filename, const char* mode);

// Closes file opened by OpenFile. Returns true on success.
bool CloseFile(FILE* file);

// Truncates an open file to end at the location of the current file pointer.
// This is a cross-platform analog to Windows' SetEndOfFile() function.
bool TruncateFile(FILE* file);

// Reads the given number of bytes from the file into the buffer.  Returns
// the number of read bytes, or -1 on error.
int ReadFile(const FilePath& filename, char* data, int size);
// Deprecated temporary compatibility function.
int ReadFile(const std::wstring& filename, char* data, int size);

// Writes the given buffer into the file, overwriting any data that was
// previously there.  Returns the number of bytes written, or -1 on error.
int WriteFile(const FilePath& filename, const char* data, int size);
// Deprecated temporary compatibility function.
int WriteFile(const std::wstring& filename, const char* data, int size);
#if defined(OS_POSIX)
// Append the data to |fd|. Does not close |fd| when done.
int WriteFileDescriptor(const int fd, const char* data, int size);
#endif

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* path);
// Deprecated temporary compatibility function.
bool GetCurrentDirectory(std::wstring* path);

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& path);

// A class to handle auto-closing of FILE*'s.
class ScopedFILEClose {
 public:
  inline void operator()(FILE* x) const {
    if (x) {
      fclose(x);
    }
  }
};

typedef scoped_ptr_malloc<FILE, ScopedFILEClose> ScopedFILE;

// A class for enumerating the files in a provided path. The order of the
// results is not guaranteed.
//
// DO NOT USE FROM THE MAIN THREAD of your application unless it is a test
// program where latency does not matter. This class is blocking.
class FileEnumerator {
 public:
#if defined(OS_WIN)
  typedef WIN32_FIND_DATA FindInfo;
#elif defined(OS_POSIX)
  typedef struct {
    struct stat stat;
    std::string filename;
  } FindInfo;
#endif

  enum FILE_TYPE {
    FILES                 = 1 << 0,
    DIRECTORIES           = 1 << 1,
    INCLUDE_DOT_DOT       = 1 << 2,
#if defined(OS_POSIX)
    SHOW_SYM_LINKS        = 1 << 4,
#endif
  };

  // |root_path| is the starting directory to search for. It may or may not end
  // in a slash.
  //
  // If |recursive| is true, this will enumerate all matches in any
  // subdirectories matched as well. It does a breadth-first search, so all
  // files in one directory will be returned before any files in a
  // subdirectory.
  //
  // |file_type| specifies whether the enumerator should match files,
  // directories, or both.
  //
  // |pattern| is an optional pattern for which files to match. This
  // works like shell globbing. For example, "*.txt" or "Foo???.doc".
  // However, be careful in specifying patterns that aren't cross platform
  // since the underlying code uses OS-specific matching routines.  In general,
  // Windows matching is less featureful than others, so test there first.
  // If unspecified, this will match all files.
  // NOTE: the pattern only matches the contents of root_path, not files in
  // recursive subdirectories.
  // TODO(erikkay): Fix the pattern matching to work at all levels.
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 FileEnumerator::FILE_TYPE file_type);
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 FileEnumerator::FILE_TYPE file_type,
                 const FilePath::StringType& pattern);
  ~FileEnumerator();

  // Returns an empty string if there are no more results.
  FilePath Next();

  // Write the file info into |info|.
  void GetFindInfo(FindInfo* info);

  // Looks inside a FindInfo and determines if it's a directory.
  static bool IsDirectory(const FindInfo& info);

 private:
  // Returns true if the given path should be skipped in enumeration.
  bool ShouldSkip(const FilePath& path);

  // Returns true if the given path's base name is ".".
  bool IsDot(const FilePath& path);

  // Returns true if the given path's base name is "..".
  bool IsDotDot(const FilePath& path);

#if defined(OS_WIN)
  WIN32_FIND_DATA find_data_;
  HANDLE find_handle_;
#elif defined(OS_POSIX)
  typedef struct {
    FilePath filename;
    struct stat stat;
  } DirectoryEntryInfo;

  // Read the filenames in source into the vector of DirectoryEntryInfo's
  static bool ReadDirectory(std::vector<DirectoryEntryInfo>* entries,
                            const FilePath& source, bool show_links);

  // The files in the current directory
  std::vector<DirectoryEntryInfo> directory_entries_;

  // The next entry to use from the directory_entries_ vector
  size_t current_directory_entry_;
#endif

  FilePath root_path_;
  bool recursive_;
  FILE_TYPE file_type_;
  FilePath::StringType pattern_;  // Empty when we want to find everything.

  // Set to true when there is a find operation open. This way, we can lazily
  // start the operations when the caller calls Next().
  bool is_in_find_op_;

  // A stack that keeps track of which subdirectories we still need to
  // enumerate in the breadth-first search.
  std::stack<FilePath> pending_paths_;

  DISALLOW_COPY_AND_ASSIGN(FileEnumerator);
};

class MemoryMappedFile {
 public:
  // The default constructor sets all members to invalid/null values.
  MemoryMappedFile();
  ~MemoryMappedFile();

  // Opens an existing file and maps it into memory. Access is restricted to
  // read only. If this object already points to a valid memory mapped file
  // then this method will fail and return false. If it cannot open the file,
  // the file does not exist, or the memory mapping fails, it will return false.
  // Later we may want to allow the user to specify access.
  bool Initialize(const FilePath& file_name);
  // As above, but works with an already-opened file. MemoryMappedFile will take
  // ownership of |file| and close it when done.
  bool Initialize(base::PlatformFile file);

  const uint8* data() const { return data_; }
  size_t length() const { return length_; }

  // Is file_ a valid file handle that points to an open, memory mapped file?
  bool IsValid();

 private:
  // Open the given file and pass it to MapFileToMemoryInternal().
  bool MapFileToMemory(const FilePath& file_name);

  // Map the file to memory, set data_ to that memory address. Return true on
  // success, false on any kind of failure. This is a helper for Initialize().
  bool MapFileToMemoryInternal();

  // Closes all open handles. Later we may want to make this public.
  void CloseHandles();

  base::PlatformFile file_;
#if defined(OS_WIN)
  HANDLE file_mapping_;
#endif
  uint8* data_;
  size_t length_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMappedFile);
};

// Renames a file using the SHFileOperation API to ensure that the target file
// gets the correct default security descriptor in the new path.
bool RenameFileAndResetSecurityDescriptor(
    const FilePath& source_file_path,
    const FilePath& target_file_path);

// Returns whether the file has been modified since a particular date.
bool HasFileBeenModifiedSince(const FileEnumerator::FindInfo& find_info,
                              const base::Time& cutoff_time);

}  // namespace file_util

#endif  // BASE_FILE_UTIL_H_
