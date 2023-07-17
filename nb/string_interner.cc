/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "nb/hash.h"
#include "nb/string_interner.h"
#include "starboard/client_porting/poem/string_poem.h"
#include "starboard/common/string.h"

namespace nb {

StringInterner::StringInterner() {
}

StringInterner::~StringInterner() {
}

const std::string& StringInterner::Intern(const std::string& str) {
  starboard::ScopedLock lock(mutex_);
  return Intern_Locked(str);
}

const std::string& StringInterner::Intern(const char* c_string) {
  starboard::ScopedLock lock(mutex_);
  scratch_.assign(c_string);  // Good at recycling memory.
  return Intern_Locked(scratch_);
}

const std::string* StringInterner::Get(const std::string& str) const {
  starboard::ScopedLock lock(mutex_);
  return Get_Locked(str);
}

const std::string* StringInterner::Get(const char* c_string) const {
  starboard::ScopedLock lock(mutex_);
  scratch_.assign(c_string);  // Good at recycling memory.
  return Get_Locked(scratch_);
}

const std::string* StringInterner::Get_Locked(const std::string& str) const {
  std::set<std::string>::const_iterator it = string_set_.find(str);
  if (it == string_set_.end()) {
    return NULL;
  } else {
    const std::string& out = *it;
    return &out;
  }
}

size_t StringInterner::Size() const {
  starboard::ScopedLock lock(mutex_);
  return string_set_.size();
}

const std::string& StringInterner::Intern_Locked(const std::string& str) {
  std::set<std::string>::const_iterator it = string_set_.insert(str).first;
  const std::string& out = (*it);
  // Safe because std::set does not invalidate iterators on insert or when
  // erasing other iterators.
  return out;
}

ConcurrentStringInterner::ConcurrentStringInterner(size_t table_size) {
  Construct(table_size);
}

void ConcurrentStringInterner::Construct(size_t table_size) {
  if (table_size == 0) {
    table_size = 1;
  }
  string_interner_table_.reserve(table_size);
  for (size_t i = 0; i < table_size; ++i) {
    string_interner_table_.push_back(new StringInterner);
  }
}

ConcurrentStringInterner::~ConcurrentStringInterner() {
  for (size_t i = 0; i < string_interner_table_.size(); ++i) {
    delete string_interner_table_[i];
  }
}

const std::string& ConcurrentStringInterner::Intern(const std::string& str) {
  return GetBucket(str.c_str(), str.size()).Intern(str);
}

const std::string& ConcurrentStringInterner::Intern(const char* c_string) {
  return GetBucket(c_string, strlen(c_string)).Intern(c_string);
}

const std::string* ConcurrentStringInterner::Get(const std::string& str) const {
  return GetBucket(str.c_str(), str.size()).Get(str);
}

const std::string* ConcurrentStringInterner::Get(const char* c_string) const {
  return GetBucket(c_string, strlen(c_string)).Get(c_string);
}

size_t ConcurrentStringInterner::Size() const {
  size_t sum = 0;
  for (size_t i = 0; i < string_interner_table_.size(); ++i) {
    sum += string_interner_table_[i]->Size();
  }
  return sum;
}

nb::StringInterner&
ConcurrentStringInterner::GetBucket(const char* string, size_t n) {
  uint32_t hash_value = nb::RuntimeHash32(string, static_cast<int>(n));
  size_t index =
    static_cast<size_t>(hash_value % string_interner_table_.size());
  return *string_interner_table_[index];
}

const nb::StringInterner&
ConcurrentStringInterner::GetBucket(const char* string, size_t n) const {
  uint32_t hash_value = nb::RuntimeHash32(string, static_cast<int>(n));
  size_t index =
    static_cast<size_t>(hash_value % string_interner_table_.size());
  return *string_interner_table_[index];
}

 }  // namespace nb
