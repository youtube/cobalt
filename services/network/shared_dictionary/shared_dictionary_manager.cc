// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager.h"

#include "services/network/shared_dictionary/shared_dictionary_manager_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"

namespace network {

// static
std::unique_ptr<SharedDictionaryManager>
SharedDictionaryManager::CreateInMemory() {
  return std::make_unique<SharedDictionaryManagerInMemory>();
}

SharedDictionaryManager::SharedDictionaryManager() = default;
SharedDictionaryManager::~SharedDictionaryManager() = default;

scoped_refptr<SharedDictionaryStorage> SharedDictionaryManager::GetStorage(
    const SharedDictionaryStorageIsolationKey& isolation_key) {
  auto it = storages_.find(isolation_key);
  if (it != storages_.end()) {
    DCHECK(it->second);
    return it->second.get();
  }
  scoped_refptr<SharedDictionaryStorage> storage = CreateStorage(isolation_key);
  storages_.emplace(isolation_key, storage.get());
  return storage;
}

void SharedDictionaryManager::OnStorageDeleted(
    const SharedDictionaryStorageIsolationKey& isolation_key) {
  size_t removed_count = storages_.erase(isolation_key);
  DCHECK_EQ(1U, removed_count);
}

base::WeakPtr<SharedDictionaryManager> SharedDictionaryManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace network
