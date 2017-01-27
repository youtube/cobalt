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

#include "nb/string_interner.h"

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

}  // namespace nb
