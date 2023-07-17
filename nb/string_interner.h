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

#ifndef NB_STRING_INTERNER_H_
#define NB_STRING_INTERNER_H_

#include <set>
#include <string>
#include <vector>

#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
#include "starboard/types.h"

namespace nb {

// Takes a String and "interns" it, so that there is only one copy. Care is
// taken to prevent memory allocation whenever possible. Intern() will only
// allocate memory if the element is not found, otherwise it's expected to be
// memory allocation free.
//
// A common use case is to never call Clear() or destroy the StringIntern,
// since that will invalidate all std::string* that have been returned.
//
// Example:
//   StringInterner str_interner;
//   std::string a = "A";
//   std::string a_copy = "A";
//
//   EXPECT_EQ(str_interner.Intern(a),
//             str_interner.Intern(a_copy));
//
class StringInterner {
 public:
  StringInterner();
  ~StringInterner();  // All outstanding const std::string* are invalidated.
  // Returns an equivalent string to the input. If the input is missing from
  // the data store then a copy-by-value is made.
  const std::string& Intern(const std::string& str);
  const std::string& Intern(const char* c_string);

  // Returns the string object if it exists, otherwise NULL.
  const std::string* Get(const std::string& str) const;
  const std::string* Get(const char* c_string) const;

  size_t Size() const;

 private:
  const std::string& Intern_Locked(const std::string& str);
  const std::string* Get_Locked(const std::string& str) const;
  std::set<std::string> string_set_;
  mutable starboard::Mutex mutex_;
  mutable std::string scratch_;

  StringInterner(const StringInterner&) = delete;
  void operator=(const StringInterner&) = delete;
};

class ConcurrentStringInterner {
 public:
  explicit ConcurrentStringInterner(size_t table_size = 32);
  ~ConcurrentStringInterner();  // All outstanding const std::string* are
                                // invalidated.

  // Returns an equivalent string to the input. If the input is missing from
  // the data store then a copy-by-value is made.
  const std::string& Intern(const std::string& str);
  const std::string& Intern(const char* c_string);

  // Returns the string object if it exists, otherwise NULL.
  const std::string* Get(const std::string& str) const;
  const std::string* Get(const char* c_string) const;

  size_t Size() const;

 private:
  void Construct(size_t table_size);
  StringInterner& GetBucket(const char* string, size_t n);
  const StringInterner& GetBucket(const char* string, size_t n) const;
  std::vector<StringInterner*> string_interner_table_;

  ConcurrentStringInterner(const ConcurrentStringInterner&) = delete;
  void operator=(const ConcurrentStringInterner&) = delete;
};

}  // namespace nb

#endif  // NB_STRING_INTERNER_H_
