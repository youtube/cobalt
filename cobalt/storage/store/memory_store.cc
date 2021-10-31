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

#include "cobalt/storage/store/memory_store.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/trace_event/trace_event.h"
#include "cobalt/storage/storage_constants.h"
#include "cobalt/storage/store/storage.pb.h"
#include "nb/memory_scope.h"
#include "url/gurl.h"

namespace cobalt {
namespace storage {
namespace {

bool IsValidFormat(const std::vector<uint8>& buffer) {
  return buffer.size() >= kStorageHeaderSize &&
         memcmp(buffer.data(), kStorageHeader, kStorageHeaderSize) == 0;
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
  bool Serialize(std::vector<uint8>* out) const;

  // Cookies
  void GetAllCookies(
      std::vector<std::unique_ptr<net::CanonicalCookie>>* cookies) const;
  void AddCookie(const net::CanonicalCookie& cc, int64 expiration_time_us);
  void UpdateCookieAccessTime(const net::CanonicalCookie& cc, int64 time_us);
  void DeleteCookie(const net::CanonicalCookie& cc);

  // Local Storage
  void ReadAllLocalStorage(const loader::Origin& origin,
                           LocalStorageMap* local_storage_map) const;

  void WriteToLocalStorage(const loader::Origin& origin, const std::string& key,
                           const std::string& value);
  void DeleteFromLocalStorage(const loader::Origin& origin,
                              const std::string& key);
  void ClearLocalStorage(const loader::Origin& origin);

  // Disable copying
  Impl(const Impl& store) = delete;
  Impl& operator=(const Impl& store) = delete;

 private:
  typedef std::tuple<std::string, std::string, std::string> CookieKey;
  std::map<CookieKey, Cookie> cookies_map_;

  typedef std::map<std::string, std::string> LocalStorageKeyValueMap;
  typedef std::map<std::string, LocalStorageKeyValueMap>
      LocalStorageByOriginMap;

  LocalStorageByOriginMap local_storage_by_origin_map_;
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
          reinterpret_cast<const char*>(in.data() + kStorageHeaderSize),
          in.size() - kStorageHeaderSize)) {
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
      GURL gurl(local_storages.serialized_origin());
      loader::Origin origin(gurl);
      if (origin.is_opaque()) {
        LOG(WARNING) << "Ignoring storage for opaque origin "
                     << local_storages.serialized_origin();
        continue;
      }
      local_storage_by_origin_map_[origin.SerializedOrigin()]
                                  [local_storage_entry.key()] =
                                      local_storage_entry.value();
    }
  }
  return true;
}

bool MemoryStore::Impl::Serialize(std::vector<uint8>* out) const {
  TRACK_MEMORY_SCOPE("Storage");
  Storage storage_data;
  for (const auto& cookie_pair : cookies_map_) {
    *(storage_data.add_cookies()) = cookie_pair.second;
  }
  for (const auto& origin_storage_pair : local_storage_by_origin_map_) {
    LocalStorage* local_storage = storage_data.add_local_storages();
    local_storage->set_serialized_origin(origin_storage_pair.first);
    for (const auto& key_value_pair : origin_storage_pair.second) {
      LocalStorageEntry* local_storage_entry =
          local_storage->add_local_storage_entries();
      local_storage_entry->set_key(key_value_pair.first);
      local_storage_entry->set_value(key_value_pair.second);
    }
  }
  size_t size = storage_data.ByteSize();
  out->resize(kStorageHeaderSize + size);
  char* buffer_ptr = reinterpret_cast<char*>(out->data());
  memcpy(buffer_ptr, kStorageHeader, kStorageHeaderSize);
  if (size > 0 &&
      !storage_data.SerializeToArray(buffer_ptr + kStorageHeaderSize, size)) {
    LOG(ERROR) << "Failed to serialize message with size=" << size;
    return false;
  }

  return true;
}

void MemoryStore::Impl::GetAllCookies(
    std::vector<std::unique_ptr<net::CanonicalCookie>>* cookies) const {
  TRACK_MEMORY_SCOPE("Storage");
  for (const auto& cookie_pair : cookies_map_) {
    // We create a CanonicalCookie directly through its constructor instead of
    // through CanonicalCookie::Create() and its sanitization because these
    // values are just serialized from a former instance of a CanonicalCookie
    // object that *was* created through CanonicalCookie::Create().
    auto cookie = std::make_unique<net::CanonicalCookie>(
        cookie_pair.second.name(), cookie_pair.second.value(),
        cookie_pair.second.domain(), cookie_pair.second.path(),
        base::Time::FromInternalValue(cookie_pair.second.creation_time_us()),
        base::Time::FromInternalValue(cookie_pair.second.expiration_time_us()),
        base::Time::FromInternalValue(cookie_pair.second.last_access_time_us()),
        cookie_pair.second.secure(), cookie_pair.second.http_only(),
        net::CookieSameSite::DEFAULT_MODE, net::COOKIE_PRIORITY_DEFAULT);
    cookies->push_back(std::move(cookie));
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
    const loader::Origin& origin, LocalStorageMap* local_storage_map) const {
  TRACK_MEMORY_SCOPE("Storage");
  const auto& it = local_storage_by_origin_map_.find(origin.SerializedOrigin());

  if (it != local_storage_by_origin_map_.end()) {
    local_storage_map->insert(it->second.begin(), it->second.end());
  }
}

void MemoryStore::Impl::WriteToLocalStorage(const loader::Origin& origin,
                                            const std::string& key,
                                            const std::string& value) {
  TRACK_MEMORY_SCOPE("Storage");
  local_storage_by_origin_map_[origin.SerializedOrigin()][key] = value;
}

void MemoryStore::Impl::DeleteFromLocalStorage(const loader::Origin& origin,
                                               const std::string& key) {
  TRACK_MEMORY_SCOPE("Storage");
  local_storage_by_origin_map_[origin.SerializedOrigin()].erase(key);
}

void MemoryStore::Impl::ClearLocalStorage(const loader::Origin& origin) {
  TRACK_MEMORY_SCOPE("Storage");
  local_storage_by_origin_map_[origin.SerializedOrigin()].clear();
}

MemoryStore::MemoryStore() { impl_.reset(new Impl()); }

bool MemoryStore::Initialize(const std::vector<uint8>& in) {
  return impl_->Initialize(in);
}

bool MemoryStore::Serialize(std::vector<uint8>* out) const {
  return impl_->Serialize(out);
}

void MemoryStore::GetAllCookies(
    std::vector<std::unique_ptr<net::CanonicalCookie>>* cookies) const {
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

void MemoryStore::ReadAllLocalStorage(
    const loader::Origin& origin, LocalStorageMap* local_storage_map) const {
  impl_->ReadAllLocalStorage(origin, local_storage_map);
}

void MemoryStore::WriteToLocalStorage(const loader::Origin& origin,
                                      const std::string& key,
                                      const std::string& value) {
  impl_->WriteToLocalStorage(origin, key, value);
}

void MemoryStore::DeleteFromLocalStorage(const loader::Origin& origin,
                                         const std::string& key) {
  impl_->DeleteFromLocalStorage(origin, key);
}

void MemoryStore::ClearLocalStorage(const loader::Origin& origin) {
  impl_->ClearLocalStorage(origin);
}

MemoryStore::~MemoryStore() {}
}  // namespace storage
}  // namespace cobalt
