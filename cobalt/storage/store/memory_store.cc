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

#include "cobalt/storage/store/memory_store.h"

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/debug/trace_event.h"
#include "cobalt/storage/store/storage.pb.h"
#include "googleurl/src/gurl.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace storage {
namespace {

constexpr char kHeader[] = "SAV1";
constexpr int kHeaderSize = 4;

bool IsValidFormat(const std::vector<uint8>& buffer) {
  return buffer.size() >= kHeaderSize &&
         memcmp(buffer.data(), kHeader, kHeaderSize) == 0;
}

}  // namespace

// Using Pimpl design pattern to hide protobuf implementation details.
// The generated proto code (including the header) needs special build
// configuration to work as expected. Which means adding special build settings
// to any place where the headers is included.
class MemoryStore::Impl {
 public:
  // Initialization
  Impl();
  bool Initialize(const std::vector<uint8>& in);

  // Serialization
  bool Serialize(std::vector<uint8>* out);

  // Cookies
  void GetAllCookies(std::vector<net::CanonicalCookie*>* cookies);
  void AddCookie(const net::CanonicalCookie& cc, int64 expiration_time_us);
  void UpdateCookieAccessTime(const net::CanonicalCookie& cc, int64 time_us);
  void DeleteCookie(const net::CanonicalCookie& cc);

  // Local Storage
  void ReadAllLocalStorage(const std::string& id,
                           LocalStorageMap* local_storage_map);

  void WriteToLocalStorage(const std::string& id, const std::string& key,
                           const std::string& value);
  void DeleteFromLocalStorage(const std::string& id, const std::string& key);
  void ClearLocalStorage(const std::string& id);

  // Disable copying
  Impl(const Impl& store) = delete;
  Impl& operator=(const Impl& store) = delete;

 private:
  typedef std::tuple<std::string, std::string, std::string> CookieKey;
  std::map<CookieKey, Cookie> cookies_map_;

  typedef std::map<std::string, std::string> LocalStorageKeyValueMap;
  typedef std::map<std::string, LocalStorageKeyValueMap> LocalStorageByIdMap;

  LocalStorageByIdMap local_storage_by_id_map_;
};

MemoryStore::Impl::Impl() {}

bool MemoryStore::Impl::Initialize(const std::vector<uint8>& in) {
  TRACK_MEMORY_SCOPE("Storage");
  if (!IsValidFormat(in)) {
    LOG(ERROR) << "Invalid format with size=" << in.size();
    return false;
  }
  Storage storage_data;
  if (!storage_data.ParseFromArray(
          reinterpret_cast<const char*>(in.data() + kHeaderSize),
          in.size() - kHeaderSize)) {
    LOG(ERROR) << "Unable to parse storage with size" << in.size();
    return false;
  }
  for (auto& cookie : storage_data.cookies()) {
    cookies_map_[std::make_tuple(cookie.domain(), cookie.path(),
                                 cookie.name())] = cookie;
  }
  for (const auto& local_storages : storage_data.local_storages()) {
    for (const auto& local_storage_entry :
         local_storages.local_storage_entries()) {
      local_storage_by_id_map_[local_storages.id()][local_storage_entry.key()] =
          local_storage_entry.value();
    }
  }
  return true;
}

bool MemoryStore::Impl::Serialize(std::vector<uint8>* out) {
  TRACK_MEMORY_SCOPE("Storage");
  Storage storage_data;
  for (const auto& cookie_pair : cookies_map_) {
    *(storage_data.add_cookies()) = cookie_pair.second;
  }
  for (const auto& id_storage_pair : local_storage_by_id_map_) {
    LocalStorage* local_storage = storage_data.add_local_storages();
    local_storage->set_id(id_storage_pair.first);
    for (const auto& key_value_pair : id_storage_pair.second) {
      LocalStorageEntry* local_storage_entry =
          local_storage->add_local_storage_entries();
      local_storage_entry->set_key(key_value_pair.first);
      local_storage_entry->set_value(key_value_pair.second);
    }
  }
  size_t size = storage_data.ByteSize();
  out->resize(kHeaderSize + size);
  char* buffer_ptr = reinterpret_cast<char*>(out->data());
  memcpy(buffer_ptr, kHeader, kHeaderSize);
  if (size > 0 &&
      !storage_data.SerializeToArray(buffer_ptr + kHeaderSize, size)) {
    LOG(ERROR) << "Failed to serialize message with size=" << size;
    return false;
  }

  return true;
}

void MemoryStore::Impl::GetAllCookies(
    std::vector<net::CanonicalCookie*>* cookies) {
  TRACK_MEMORY_SCOPE("Storage");
  for (const auto& cookie_pair : cookies_map_) {
    // We create a CanonicalCookie directly through its constructor instead of
    // through CanonicalCookie::Create() and its sanitization because these
    // values are just serialized from a former instance of a CanonicalCookie
    // object that *was* created through CanonicalCookie::Create().
    scoped_ptr<net::CanonicalCookie> cookie(new net::CanonicalCookie(
        GURL(""), cookie_pair.second.name(), cookie_pair.second.value(),
        cookie_pair.second.domain(), cookie_pair.second.path(),
        "" /* mac_key */, "" /* mac_algorithm */,
        base::Time::FromInternalValue(cookie_pair.second.creation_time_us()),
        base::Time::FromInternalValue(cookie_pair.second.expiration_time_us()),
        base::Time::FromInternalValue(cookie_pair.second.last_access_time_us()),
        cookie_pair.second.secure(), cookie_pair.second.http_only()));
    cookies->push_back(cookie.release());
  }
}

void MemoryStore::Impl::AddCookie(const net::CanonicalCookie& cc,
                                  int64 expiration_time_us) {
  TRACK_MEMORY_SCOPE("Storage");
  Cookie cookie;
  cookie.set_domain(cc.Domain());
  cookie.set_path(cc.Path());
  cookie.set_name(cc.Name());
  cookie.set_value(cc.Value());
  cookie.set_secure(cc.IsSecure());
  cookie.set_http_only(cc.IsHttpOnly());

  cookie.set_creation_time_us(cc.CreationDate().ToInternalValue());
  cookie.set_expiration_time_us(expiration_time_us);
  cookie.set_last_access_time_us(cc.LastAccessDate().ToInternalValue());

  CookieKey key = std::make_tuple(cc.Domain(), cc.Path(), cc.Name());
  cookies_map_[key] = cookie;
}

void MemoryStore::Impl::UpdateCookieAccessTime(const net::CanonicalCookie& cc,
                                               int64 time_us) {
  CookieKey key = std::make_tuple(cc.Domain(), cc.Path(), cc.Name());
  cookies_map_[key].set_last_access_time_us(time_us);
}

void MemoryStore::Impl::DeleteCookie(const net::CanonicalCookie& cc) {
  CookieKey key = std::make_tuple(cc.Domain(), cc.Path(), cc.Name());
  cookies_map_.erase(key);
}

void MemoryStore::Impl::ReadAllLocalStorage(
    const std::string& id, LocalStorageMap* local_storage_map) {
  TRACK_MEMORY_SCOPE("Storage");
  local_storage_map->insert(local_storage_by_id_map_[id].begin(),
                            local_storage_by_id_map_[id].end());
}

void MemoryStore::Impl::WriteToLocalStorage(const std::string& id,
                                            const std::string& key,
                                            const std::string& value) {
  TRACK_MEMORY_SCOPE("Storage");
  local_storage_by_id_map_[id][key] = value;
}

void MemoryStore::Impl::DeleteFromLocalStorage(const std::string& id,
                                               const std::string& key) {
  TRACK_MEMORY_SCOPE("Storage");
  local_storage_by_id_map_[id].erase(key);
}

void MemoryStore::Impl::ClearLocalStorage(const std::string& id) {
  TRACK_MEMORY_SCOPE("Storage");
  local_storage_by_id_map_[id].clear();
}

MemoryStore::MemoryStore() { impl_.reset(new Impl()); }

bool MemoryStore::Initialize(const std::vector<uint8>& in) {
  return impl_->Initialize(in);
}

bool MemoryStore::Serialize(std::vector<uint8>* out) {
  return impl_->Serialize(out);
}

void MemoryStore::GetAllCookies(std::vector<net::CanonicalCookie*>* cookies) {
  impl_->GetAllCookies(cookies);
}

void MemoryStore::AddCookie(const net::CanonicalCookie& cc,
                            int64 expiration_time_us) {
  impl_->AddCookie(cc, expiration_time_us);
}

void MemoryStore::UpdateCookieAccessTime(const net::CanonicalCookie& cc,
                                         int64 time_us) {
  impl_->UpdateCookieAccessTime(cc, time_us);
}

void MemoryStore::DeleteCookie(const net::CanonicalCookie& cc) {
  impl_->DeleteCookie(cc);
}

void MemoryStore::ReadAllLocalStorage(const std::string& id,
                                      LocalStorageMap* local_storage_map) {
  impl_->ReadAllLocalStorage(id, local_storage_map);
}

void MemoryStore::WriteToLocalStorage(const std::string& id,
                                      const std::string& key,
                                      const std::string& value) {
  impl_->WriteToLocalStorage(id, key, value);
}

void MemoryStore::DeleteFromLocalStorage(const std::string& id,
                                         const std::string& key) {
  impl_->DeleteFromLocalStorage(id, key);
}

void MemoryStore::ClearLocalStorage(const std::string& id) {
  impl_->ClearLocalStorage(id);
}

MemoryStore::~MemoryStore() {}
}  // namespace storage
}  // namespace cobalt
