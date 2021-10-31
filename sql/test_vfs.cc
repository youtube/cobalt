// Copyright 2016 Google Inc. All Rights Reserved.
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

// Adapted from cobalt/storage.

#include "sql/test_vfs.h"

#include <string.h>

#include <algorithm>
#include <map>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

// A "subclass" of sqlite3_file with our required data structures added.
struct virtual_file {
  sqlite3_file sql_internal_file;
  std::string* data;
  base::Lock* lock;
  int current_lock;
  int shared;
};

// A very simple in-memory virtual file system.
class TestVfs {
 public:
  typedef std::map<std::string, std::string> FileMap;

 public:
  TestVfs() {}
  ~TestVfs() {}

  void Register();
  void Unregister();

  std::string* Open(const char* path) {
    return &file_map_[path];
  }

  void Delete(const char* path) {
    file_map_.erase(path);
  }

 private:
  sqlite3_vfs vfs_;
  FileMap file_map_;
};

base::LazyInstance<TestVfs>::DestructorAtExit g_vfs = LAZY_INSTANCE_INITIALIZER;

int VfsClose(sqlite3_file* file) {
  virtual_file* vfile = reinterpret_cast<virtual_file*>(file);
  delete vfile->lock;
  return SQLITE_OK;
}

int VfsRead(sqlite3_file* file, void* out, int bytes, sqlite_int64 offset) {
  virtual_file* vfile = reinterpret_cast<virtual_file*>(file);
  base::AutoLock lock(*vfile->lock);
  if (offset >= static_cast<sqlite_int64>(vfile->data->length())) {
    return SQLITE_OK;
  }

  size_t available =
      std::max(static_cast<sqlite_int64>(vfile->data->length()) - offset,
               static_cast<sqlite_int64>(0));
  size_t to_read = std::min(available, static_cast<size_t>(bytes));
  if (to_read == 0) {
    return SQLITE_OK;
  }

  memcpy(out, &(vfile->data->c_str()[offset]), to_read);
  return SQLITE_OK;
}

int VfsWrite(sqlite3_file* file,
             const void* data,
             int bytes,
             sqlite3_int64 offset) {
  size_t max = offset + bytes;

  virtual_file* vfile = reinterpret_cast<virtual_file*>(file);
  base::AutoLock lock(*vfile->lock);
  if (vfile->data->length() < max) {
    vfile->data->resize(max);
  }

  vfile->data->replace(offset, bytes, reinterpret_cast<const char*>(data),
                       bytes);

  return SQLITE_OK;
}

int VfsSync(sqlite3_file* pFile, int flags) {
  return SQLITE_OK;
}

int VfsFileControl(sqlite3_file* pFile, int op, void* pArg) {
  return SQLITE_OK;
}

int VfsSectorSize(sqlite3_file* file) {
  // The number of bytes that can be read without disturbing other bytes in the
  // file.
  return 1;
}

int VfsLock(sqlite3_file* file, const int mode) {
  virtual_file* vfile = reinterpret_cast<virtual_file*>(file);
  base::AutoLock lock(*vfile->lock);

  // If there is already a lock of this type or more restrictive, do nothing
  if (vfile->current_lock >= mode) {
    return SQLITE_OK;
  }

  if (mode == SQLITE_LOCK_SHARED) {
    DCHECK_EQ(vfile->current_lock, SQLITE_LOCK_NONE);
    vfile->shared++;
    vfile->current_lock = SQLITE_LOCK_SHARED;
  }

  if (mode == SQLITE_LOCK_RESERVED) {
    DCHECK_EQ(vfile->current_lock, SQLITE_LOCK_SHARED);
    vfile->current_lock = SQLITE_LOCK_RESERVED;
  }

  if (mode == SQLITE_LOCK_EXCLUSIVE) {
    if (vfile->current_lock >= SQLITE_LOCK_PENDING) {
      return SQLITE_BUSY;
    }
    vfile->current_lock = SQLITE_LOCK_PENDING;
    if (vfile->shared > 1) {
      // There are some outstanding shared locks (greater than one because the
      // pending lock is an "upgraded" shared lock)
      return SQLITE_BUSY;
    }
    // Acquire the exclusive lock
    vfile->current_lock = SQLITE_LOCK_EXCLUSIVE;
  }

  return SQLITE_OK;
}

int VfsUnlock(sqlite3_file* file, int mode) {
  DCHECK_LE(mode, SQLITE_LOCK_SHARED);
  virtual_file* vfile = reinterpret_cast<virtual_file*>(file);
  base::AutoLock lock(*vfile->lock);

#ifdef STARBOARD
#undef COMPILE_ASSERT
#define COMPILE_ASSERT static_assert
#define sqlite_lock_constants_order_has_changed \
  "sqlite lock constants order has changed!"
#endif
  COMPILE_ASSERT(SQLITE_LOCK_NONE < SQLITE_LOCK_SHARED,
                 sqlite_lock_constants_order_has_changed);
  COMPILE_ASSERT(SQLITE_LOCK_SHARED < SQLITE_LOCK_RESERVED,
                 sqlite_lock_constants_order_has_changed);
  COMPILE_ASSERT(SQLITE_LOCK_RESERVED < SQLITE_LOCK_PENDING,
                 sqlite_lock_constants_order_has_changed);
  COMPILE_ASSERT(SQLITE_LOCK_PENDING < SQLITE_LOCK_EXCLUSIVE,
                 sqlite_lock_constants_order_has_changed);

  if (mode == SQLITE_LOCK_NONE && vfile->current_lock >= SQLITE_LOCK_SHARED) {
    vfile->shared--;
  }

  vfile->current_lock = mode;
  return SQLITE_OK;
}

int VfsCheckReservedLock(sqlite3_file* file, int* result) {
  virtual_file* vfile = reinterpret_cast<virtual_file*>(file);
  base::AutoLock lock(*vfile->lock);

  // The function expects a result is 1 if the lock is reserved, pending, or
  // exclusive; 0 otherwise.
  *result = vfile->current_lock >= SQLITE_LOCK_RESERVED ? 1 : 0;
  return SQLITE_OK;
}

int VfsFileSize(sqlite3_file* file, sqlite3_int64* out_size) {
  virtual_file* vfile = reinterpret_cast<virtual_file*>(file);
  *out_size = vfile->data->length();
  return SQLITE_OK;
}

int VfsTruncate(sqlite3_file* file, sqlite3_int64 size) {
  virtual_file* vfile = reinterpret_cast<virtual_file*>(file);
  base::AutoLock lock(*vfile->lock);
  vfile->data->resize(size);
  return SQLITE_OK;
}

int VfsDeviceCharacteristics(sqlite3_file* file) {
  return 0;
}

const sqlite3_io_methods s_cobalt_vfs_io = {
    1,                        // Structure version number
    VfsClose,                 // xClose
    VfsRead,                  // xRead
    VfsWrite,                 // xWrite
    VfsTruncate,              // xTruncate
    VfsSync,                  // xSync
    VfsFileSize,              // xFileSize
    VfsLock,                  // xLock
    VfsUnlock,                // xUnlock
    VfsCheckReservedLock,     // xCheckReservedLock
    VfsFileControl,           // xFileControl
    VfsSectorSize,            // xSectorSize
    VfsDeviceCharacteristics  // xDeviceCharacteristics
};

int VfsOpen(sqlite3_vfs* sql_vfs,
            const char* path,
            sqlite3_file* file,
            int flags,
            int* out_flags) {
  DCHECK(path) << "NULL filename not supported.";
  virtual_file* vfile = reinterpret_cast<virtual_file*>(file);
  vfile->lock = new base::Lock;
  TestVfs* vfs = reinterpret_cast<TestVfs*>(sql_vfs->pAppData);
  vfile->data = vfs->Open(path);
  file->pMethods = &s_cobalt_vfs_io;
  return SQLITE_OK;
}

int VfsDelete(sqlite3_vfs* sql_vfs, const char* path, int sync_dir) {
  TestVfs* vfs = reinterpret_cast<TestVfs*>(sql_vfs->pAppData);
  vfs->Delete(path);
  return SQLITE_OK;
}

int VfsFullPathname(sqlite3_vfs* sql_vfs,
                    const char* path,
                    int out_size,
                    char* out_path) {
  size_t path_size = static_cast<size_t>(out_size);
  if (base::strlcpy(out_path, path, path_size) < path_size) {
    return SQLITE_OK;
  }
  return SQLITE_ERROR;
}

int VfsAccess(sqlite3_vfs* sql_vfs, const char* name, int flags, int* result) {
  // We should always have a valid, readable/writable file.
  *result |= SQLITE_ACCESS_EXISTS | SQLITE_ACCESS_READWRITE;
  return SQLITE_OK;
}

int VfsRandomness(sqlite3_vfs* sql_vfs, int bytes, char* out) {
  base::RandBytes(out, static_cast<size_t>(bytes));
  return SQLITE_OK;
}

void TestVfs::Register() {
  memset(&vfs_, 0, sizeof(vfs_));
  vfs_.iVersion = 1;
  vfs_.szOsFile = sizeof(virtual_file);
  vfs_.mxPathname = 512;
  vfs_.pNext = NULL;
  vfs_.zName = "test_vfs";
  vfs_.pAppData = this;
  vfs_.xOpen = VfsOpen;
  vfs_.xDelete = VfsDelete;
  vfs_.xAccess = VfsAccess;
  vfs_.xFullPathname = VfsFullPathname;
  vfs_.xRandomness = VfsRandomness;

  // Ensure we are not registering multiple of these with the same name.
  // Behavior is undefined in that case.
  DCHECK(sqlite3_vfs_find(vfs_.zName) == NULL);

  int ret = sqlite3_vfs_register(&vfs_, 1 /* make_default */);
  DCHECK_EQ(ret, SQLITE_OK);
}

void TestVfs::Unregister() {
  int ret = sqlite3_vfs_unregister(&vfs_);
  file_map_.clear();
  DCHECK_EQ(ret, SQLITE_OK);
}

}  // namespace

void RegisterTestVfs() {
  g_vfs.Get().Register();
}

void UnregisterTestVfs() {
  g_vfs.Get().Unregister();
}

}  // namespace sql
