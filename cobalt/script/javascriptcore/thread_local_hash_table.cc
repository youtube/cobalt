/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "cobalt/script/javascriptcore/thread_local_hash_table.h"

#include <map>

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

namespace {
typedef std::map<const JSC::ClassInfo*, JSC::HashTable> HashTableMap;
}

// static
ThreadLocalHashTable* ThreadLocalHashTable::GetInstance() {
  return Singleton<ThreadLocalHashTable>::get();
}

ThreadLocalHashTable::ThreadLocalHashTable() : slot_(SlotDestructor) {}

ThreadLocalHashTable::~ThreadLocalHashTable() {
  // If there is any data stored in our slot on this thread, destroy it now
  // before freeing the slot itself, otherwise that data will be leaked.
  SlotDestructor(slot_.Get());
  slot_.Free();
}

JSC::HashTable* ThreadLocalHashTable::GetHashTable(
    const JSC::ClassInfo* class_info, const JSC::HashTable& prototype) {
  if (!slot_.Get()) {
    slot_.Set(new HashTableMap);
  }
  DCHECK(slot_.Get());
  HashTableMap* hash_table_map = static_cast<HashTableMap*>(slot_.Get());
  if (hash_table_map->find(class_info) == hash_table_map->end()) {
    (*hash_table_map)[class_info] = prototype;
  }
  return &(*hash_table_map)[class_info];
}

// static
void ThreadLocalHashTable::SlotDestructor(void* value) {
  HashTableMap* hash_table_map = static_cast<HashTableMap*>(value);
  if (hash_table_map) {
    for (HashTableMap::iterator it = hash_table_map->begin();
         it != hash_table_map->end(); ++it) {
      it->second.deleteTable();
    }
    delete hash_table_map;
  }
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
