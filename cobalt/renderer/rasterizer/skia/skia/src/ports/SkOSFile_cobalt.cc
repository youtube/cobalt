// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "third_party/skia/include/core/SkOSFile.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"

// Implement functionality declared in SkOSFile.h via primitives provided
// by Chromium.  In doing this, we need only ensure that support for Chromium
// file utilities is working and then Skia file utilities will also work.

namespace {
base::PlatformFile ToPlatformFile(SkFILE* sk_file) {
#if defined(OS_STARBOARD)
  // PlatformFile is a pointer type in Starboard, so we cannot use static_cast
  // from intptr_t.
  return reinterpret_cast<base::PlatformFile>(sk_file);
#else
  // PlatformFile is an int in POSIX Emulation, so we must use a static_cast
  // from intptr_t.
  return static_cast<base::PlatformFile>(reinterpret_cast<intptr_t>(sk_file));
#endif
}

SkFILE* ToSkFILE(base::PlatformFile platform_file) {
  return reinterpret_cast<SkFILE*>(platform_file);
}

int ToPlatformFlags(SkFILE_Flags sk_flags) {
  int flags = 0;
  if (sk_flags & kRead_SkFILE_Flag) {
    if (sk_flags & kWrite_SkFILE_Flag) {
      flags |= base::PLATFORM_FILE_WRITE;
    }
    flags |= base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
  } else if (sk_flags & kWrite_SkFILE_Flag) {
    flags |= base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  }

  return flags;
}

}  // namespace

SkFILE* sk_fopen(const char path[], SkFILE_Flags sk_flags) {
  base::PlatformFile platform_file = base::CreatePlatformFile(
      FilePath(path), ToPlatformFlags(sk_flags), NULL, NULL);
  if (platform_file == base::kInvalidPlatformFileValue) {
    return NULL;
  }

  return ToSkFILE(platform_file);
}

void sk_fclose(SkFILE* sk_file) {
  SkASSERT(sk_file);
  base::ClosePlatformFile(ToPlatformFile(sk_file));
}

size_t sk_fgetsize(SkFILE* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);

  // Save current position so we can restore it.
  int64_t current_position =
      SeekPlatformFile(file, base::PLATFORM_FILE_FROM_CURRENT, 0);
  if (current_position < 0) {
    return 0;
  }

  // Find the file size by seeking to the end.
  int64_t size = base::SeekPlatformFile(file, base::PLATFORM_FILE_FROM_END, 0);
  if (size < 0) {
    size = 0;
  }

  // Restore original file position.
  SeekPlatformFile(file, base::PLATFORM_FILE_FROM_BEGIN, current_position);
  return size;
}

bool sk_frewind(SkFILE* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  int64_t position = SeekPlatformFile(file, base::PLATFORM_FILE_FROM_BEGIN, 0);
  return position == 0;
}

size_t sk_fread(void* buffer, size_t byteCount, SkFILE* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  return base::ReadPlatformFileAtCurrentPos(
      file, reinterpret_cast<char*>(buffer), byteCount);
}

size_t sk_fwrite(const void* buffer, size_t byteCount, SkFILE* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  return base::WritePlatformFileAtCurrentPos(
      file, reinterpret_cast<const char*>(buffer), byteCount);
}

char* sk_fgets(char* str, int size, SkFILE* sk_file) {
  SkASSERT(sk_file);
  char* p = str;
  size--;
  while (size > 0) {
    if (1 != sk_fread(p, 1, sk_file)) {
      break;
    }
    size--;
    char c = *p++;
    if (c == '\n') {
      break;
    }
  }
  if (p == str) {
    // Didn't read any characters.
    return NULL;
  }
  *p = '\0';
  return str;
}

void sk_fflush(SkFILE* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  base::FlushPlatformFile(file);
}

bool sk_fseek(SkFILE* sk_file, size_t position) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  int64_t new_position =
      SeekPlatformFile(file, base::PLATFORM_FILE_FROM_BEGIN, position);
  return new_position == position;
}

bool sk_fmove(SkFILE* sk_file, long position) {  // NOLINT[runtime/int]
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  int64_t new_position =
      SeekPlatformFile(file, base::PLATFORM_FILE_FROM_CURRENT, position);
  return true;
}

size_t sk_ftell(SkFILE* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  return SeekPlatformFile(file, base::PLATFORM_FILE_FROM_CURRENT, 0);
}

void* sk_fmmap(SkFILE* sk_file, size_t* length) {
  // Not supported, but clients may try to call to see if it is supported.
  return NULL;
}

void* sk_fdmmap(int fd, size_t* length) {
  NOTREACHED() << __FUNCTION__;
  return NULL;
}

void sk_fmunmap(const void* addr, size_t length) {
  NOTREACHED() << __FUNCTION__;
}

bool sk_fidentical(SkFILE* sk_file_a, SkFILE* sk_file_b) {
  NOTREACHED() << __FUNCTION__;
  return false;
}

int sk_fileno(SkFILE* sk_file_a) {
  NOTREACHED() << __FUNCTION__;
  return -1;
}

bool sk_exists(const char* path, SkFILE_Flags /*flags*/) {
  return file_util::PathExists(FilePath(path));
}

bool sk_isdir(const char* path) {
  return file_util::DirectoryExists(FilePath(path));
}

int sk_feof(SkFILE* sk_file) {
  SkASSERT(sk_file);
  size_t size = sk_fgetsize(sk_file);
  size_t position = sk_ftell(sk_file);
  return (position >= size);
}

bool sk_mkdir(const char* path) {
  // Strange linking error on windows when calling file_util::CreateDirectory,
  // having something to do with a system #define. This isn't used, anyway.
  NOTREACHED() << __FUNCTION__;
  return false;
}

// SkOSFile::Iter::Pimpl represents our private implementation of Skia's file
// iterator class.  We implement it in terms of Chromium's FileEnumerator,
// which provides very similar functionality.
class SkOSFile::Iter::Pimpl {
 public:
  Pimpl(const char path[], const char suffix[])
      : path_(path ? path : ""), suffix_(suffix ? suffix : "") {}

  bool next(SkString* name, bool get_dir) {
    if (file_enumerator_ == base::nullopt) {
      // We lazily initialize the FileEnumerator here because we need to wait
      // until next() is called before we know whether we're looking for
      // directories or files.
      file_enumerator_.emplace(FilePath(path_.c_str()), false,
                               get_dir ? file_util::FileEnumerator::DIRECTORIES
                                       : file_util::FileEnumerator::FILES);
      get_dir_ = get_dir;
    } else {
      DCHECK_EQ(*get_dir_, get_dir)
          << "get_dir should be consistent between calls.";
    }

    while (true) {
      FilePath next_file = file_enumerator_->Next();
      if (next_file.empty()) {
        // We are done iterating, return false to indicate that.
        return false;
      } else {
        // Check if this file has the correct suffix.
        const FilePath::StringType& file_str = next_file.value();
        if (std::equal(suffix_.rbegin(), suffix_.rend(), file_str.rbegin())) {
          *name = SkString(next_file.BaseName().value().c_str());
          return true;
        }

        // If the file does not have the correct suffix, continue to the next
        // file.
      }
    }
  }

 private:
  base::optional<file_util::FileEnumerator> file_enumerator_;

  // Only valid if file_enumerator_ is constructed.  Specifies whether we are
  // searching for directories or files.
  base::optional<bool> get_dir_;

  std::string path_;
  std::string suffix_;
};

SkOSFile::Iter::Iter() {}

SkOSFile::Iter::Iter(const char path[], const char suffix[])
    : fPimpl(new Pimpl(path, suffix)) {}

SkOSFile::Iter::~Iter() {}

void SkOSFile::Iter::reset(const char path[], const char suffix[]) {
  fPimpl.reset(new Pimpl(path, suffix));
}

bool SkOSFile::Iter::next(SkString* name, bool getDir) {
  if (fPimpl.get()) {
    return fPimpl->next(name, getDir);
  } else {
    return false;
  }
}
