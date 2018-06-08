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

#ifndef COBALT_STORAGE_STORE_MEMORY_STORE_H_
#define COBALT_STORAGE_STORE_MEMORY_STORE_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "net/cookies/canonical_cookie.h"

namespace cobalt {
namespace storage {

// A store for cookies and local storage implemented in memory.
class MemoryStore {
 public:
  typedef base::hash_map<std::string, std::string> LocalStorageMap;
  typedef const std::vector<net::CanonicalCookie*> Cookies;

  MemoryStore();
  bool Initialize(const std::vector<uint8>& in);

  // Serialization
  bool Serialize(std::vector<uint8>* out) const;

  // Cookies
  void GetAllCookies(std::vector<net::CanonicalCookie*>* cookies) const;
  void AddCookie(const net::CanonicalCookie& cc, int64 expiration_time_us);
  void UpdateCookieAccessTime(const net::CanonicalCookie& cc, int64 time_us);
  void DeleteCookie(const net::CanonicalCookie& cc);

  // Local Storage
  void ReadAllLocalStorage(const std::string& id,
                           LocalStorageMap* local_storage_map) const;

  void WriteToLocalStorage(const std::string& id, const std::string& key,
                           const std::string& value);
  void DeleteFromLocalStorage(const std::string& id, const std::string& key);
  void ClearLocalStorage(const std::string& id);

  // Disable copying
  MemoryStore(const MemoryStore& store) = delete;
  MemoryStore& operator=(const MemoryStore& store) = delete;

  ~MemoryStore();

 private:
  class Impl;
  scoped_ptr<Impl> impl_;
};

}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_STORE_MEMORY_STORE_H_
