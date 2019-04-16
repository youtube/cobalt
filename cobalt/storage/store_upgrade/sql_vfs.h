// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_STORAGE_STORE_UPGRADE_SQL_VFS_H_
#define COBALT_STORAGE_STORE_UPGRADE_SQL_VFS_H_

#include <memory>
#include <string>


struct sqlite3_vfs;

namespace cobalt {
namespace storage {
namespace store_upgrade {

class VirtualFileSystem;

// Implement the necessary APIs for a Sqlite virtual file system.
// Dispatch calls to the owning VirtualFileSystem.
class SqlVfs {
 public:
  SqlVfs(const std::string& name, VirtualFileSystem* vfs);
  ~SqlVfs();

 private:
  std::unique_ptr<sqlite3_vfs> sql_vfs_;
};

}  // namespace store_upgrade
}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_STORE_UPGRADE_SQL_VFS_H_
