// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/storage/store_upgrade/upgrade.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/storage/storage_constants.h"
#include "cobalt/storage/store/memory_store.h"
#include "cobalt/storage/store/storage.pb.h"
#include "cobalt/storage/store_upgrade/sql_vfs.h"
#include "cobalt/storage/store_upgrade/virtual_file.h"
#include "cobalt/storage/store_upgrade/virtual_file_system.h"
#include "nb/memory_scope.h"
#include "net/cookies/canonical_cookie.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "starboard/storage.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/gurl.h"

namespace cobalt {
namespace storage {
namespace store_upgrade {
namespace {

constexpr char kDefaultSaveFile[] = "cobalt_save.bin";

typedef std::map<std::string, std::string> StorageMap;

const std::string& GetFirstValidDatabaseFile(
    const std::vector<std::string>& filenames) {
  // Caller must ensure at least one file exists.
  DCHECK_GT(filenames.size(), size_t(0));

  for (size_t i = 0; i < filenames.size(); ++i) {
    sql::Connection connection;
    bool is_opened = connection.Open(base::FilePath(filenames[i]));
    if (!is_opened) {
      continue;
    }
    int err = connection.ExecuteAndReturnErrorCode("pragma schema_version;");
    if (err != SQLITE_OK) {
      continue;
    }
    // File can be opened as a database.
    return filenames[i];
  }

  // Caller must handle case where a valid database file cannot be found.
  DLOG(WARNING) << "Cannot find valid database file in save data";
  return filenames[0];
}

void GetAllCookies(sql::Connection* conn, Storage* storage) {
  sql::Statement get_all(conn->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT domain, path, name, value, creation, expiration, last_access, "
      "secure, http_only FROM CookieTable"));
  while (get_all.Step()) {
    Cookie* cookie = storage->add_cookies();
    cookie->set_domain(get_all.ColumnString(0));
    cookie->set_path(get_all.ColumnString(1));
    cookie->set_name(get_all.ColumnString(2));
    cookie->set_value(get_all.ColumnString(3));
    cookie->set_creation_time_us(get_all.ColumnInt64(4));
    cookie->set_expiration_time_us(get_all.ColumnInt64(5));
    cookie->set_last_access_time_us(get_all.ColumnInt64(6));
    cookie->set_secure(get_all.ColumnBool(7));
    cookie->set_http_only(get_all.ColumnBool(8));

    DLOG(INFO) << "GetAllCookies: "
               << " domain=" << cookie->domain() << " path=" << cookie->path()
               << " name=" << cookie->name() << " value=" << cookie->value()
               << " creation=" << cookie->creation_time_us()
               << " expiration=" << cookie->expiration_time_us()
               << " last_access=" << cookie->last_access_time_us()
               << " secure=" << cookie->secure()
               << " http_only=" << cookie->http_only();
  }
}

base::Optional<loader::Origin> ParseLocalStorageId(const std::string& id) {
  std::vector<std::string> id_tokens =
      base::SplitString(id, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (id_tokens.size() != 3) {
    DLOG(WARNING) << "Failed to parse id=" << id;
    return base::nullopt;
  }
  std::string url = id_tokens[0];
  url += "://";
  url += id_tokens[1];

  std::vector<std::string> port_tokens = base::SplitString(
      id_tokens[2], ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (port_tokens.size() != 2) {
    return base::nullopt;
  }
  if (port_tokens[0] != "0") {
    url += ":";
    url += port_tokens[0];
  }
  GURL gurl(url);
  loader::Origin origin(gurl);
  if (origin.is_opaque()) {
    DLOG(WARNING) << "Missing Serialized Origin for id=" << id;
    return base::nullopt;
  }
  return origin;
}

void GetLocalStorage(sql::Connection* conn, Storage* storage) {
  sql::Statement get_all(conn->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT site_identifier, key, value FROM LocalStorageTable"));
  std::map<std::string, LocalStorage*> map_storage;
  while (get_all.Step()) {
    std::string id(get_all.ColumnString(0));
    base::Optional<loader::Origin> origin = ParseLocalStorageId(id);
    if (!origin) {
      continue;
    }
    DLOG(INFO) << "GetLocalStorage: id=" << id;
    if (map_storage[origin->SerializedOrigin()] == nullptr) {
      LocalStorage* local_storage = storage->add_local_storages();
      local_storage->set_serialized_origin(origin->SerializedOrigin());
      map_storage[origin->SerializedOrigin()] = local_storage;
    }
    LocalStorageEntry* local_storage_entry =
        map_storage[origin->SerializedOrigin()]->add_local_storage_entries();
    local_storage_entry->set_key(get_all.ColumnString(1));
    local_storage_entry->set_value(get_all.ColumnString(2));
    DLOG(INFO) << "GetLocalStorage: key=" << local_storage_entry->key()
               << " value=" << local_storage_entry->value();
  }
}

bool OpenConnection(const std::vector<uint8>& raw_bytes,
                    const std::unique_ptr<VirtualFileSystem>& vfs,
                    const std::unique_ptr<SqlVfs>& sql_vfs,
                    const std::unique_ptr<sql::Connection>& connection) {
  VirtualFileSystem::SerializedHeader header = {};

  if (raw_bytes.size() > 0) {
    const char* buffer = reinterpret_cast<const char*>(raw_bytes.data());
    int buffer_size = static_cast<int>(raw_bytes.size());

    if (raw_bytes.size() >= sizeof(VirtualFileSystem::SerializedHeader)) {
      memcpy(&header, buffer, sizeof(VirtualFileSystem::SerializedHeader));
    }

    if (!vfs->Deserialize(raw_bytes.data(), buffer_size)) {
      DLOG(ERROR) << "Failed to deserialize vfs";
      return false;
    }
  }

  std::vector<std::string> filenames = vfs->ListFiles();
  if (filenames.size() == 0) {
    filenames.push_back(kDefaultSaveFile);
  }

  // Very old legacy save data may contain multiple files (e.g. db-journal as
  // well as db), so use the first one that looks like a valid database file.
  const std::string& save_name = GetFirstValidDatabaseFile(filenames);
  bool ok = connection->Open(base::FilePath(save_name));
  if (!ok) {
    DLOG(WARNING) << "Failed to open file: " << save_name;
    return false;
  }

  // Open() is lazy. Run a quick check to see if the database is valid.
  int err = connection->ExecuteAndReturnErrorCode("pragma schema_version;");
  if (err != SQLITE_OK) {
    // Database seems to be invalid.
    DLOG(WARNING) << "Database " << save_name << " appears to be corrupt.";
    return false;
  }

  // Disable journaling for our in-memory database.
  sql::Statement disable_journal(
      connection->GetUniqueStatement("PRAGMA journal_mode=OFF"));
  ok = disable_journal.Step();
  DCHECK(ok);

  return true;
}
}  // namespace

bool IsUpgradeRequired(const std::vector<uint8>& buffer) {
  bool result = buffer.size() >= kStorageHeaderSize &&
                memcmp(reinterpret_cast<const char*>(buffer.data()),
                       kOldStorageHeader, kStorageHeaderSize) == 0;
  if (result) {
    DLOG(INFO) << "Store upgrade required";
  } else {
    DLOG(INFO) << "Store upgrade not required";
  }
  return result;
}

bool UpgradeStore(std::vector<uint8>* buffer) {
  std::unique_ptr<VirtualFileSystem> vfs(new VirtualFileSystem());
  std::unique_ptr<SqlVfs> sql_vfs(new SqlVfs("cobalt_vfs", vfs.get()));
  std::unique_ptr<sql::Connection> connection(new sql::Connection());

  if (!OpenConnection(*buffer, vfs, sql_vfs, connection)) {
    return false;
  }

  Storage storage;
  GetAllCookies(connection.get(), &storage);
  GetLocalStorage(connection.get(), &storage);
  connection->Close();
  size_t size = storage.ByteSize();
  buffer->resize(kStorageHeaderSize + size);
  char* buffer_ptr = reinterpret_cast<char*>(buffer->data());
  memcpy(buffer_ptr, kStorageHeader, kStorageHeaderSize);
  if (size > 0 &&
      !storage.SerializeToArray(buffer_ptr + kStorageHeaderSize, size)) {
    DLOG(ERROR) << "Failed to serialize message with size=" << size;
    return false;
  }
  return true;
}
}  // namespace store_upgrade
}  // namespace storage
}  // namespace cobalt
