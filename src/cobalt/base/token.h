// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_TOKEN_H_
#define COBALT_BASE_TOKEN_H_

#include <ostream>
#include <string>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "starboard/common/string.h"

namespace base {

// Token is a class used to represent a string constant.  It will never be
// destroyed once created.  Multiple instances that have the same values share
// the same copy of the string.
// Token is fully multi-thread safe.
// Note that as the Token object is small in size, it should be passed by value
// instead of by reference.
class Token {
 public:
  static const uint32 kHashSlotCount = 8 * 1024;
  static const uint32 kStringsPerSlot = 4;

  Token();
  explicit Token(const char* str);
  explicit Token(const std::string& str);

  const char* c_str() const { return str_; }

  bool operator==(const Token& that) const { return str_ == that.str_; }

 private:
  void Initialize(const char* str);

  const char* str_;

#ifdef ENABLE_TOKEN_ALPHABETICAL_SORTING

 public:
  // For unit tests to sort in a predictable order at the cost of efficiency.
  // This is not thread safe!
  class ScopedAlphabeticalSorting {
   public:
    ScopedAlphabeticalSorting() {
      DLOG(WARNING)
          << "ScopedAlphabeticalSorting should only be used for testing.";
      DCHECK(!Token::sort_alphabetically_);
      Token::sort_alphabetically_ = true;
    }
    ~ScopedAlphabeticalSorting() {
      DCHECK(Token::sort_alphabetically_);
      Token::sort_alphabetically_ = false;
    }
  };

  static bool sort_alphabetically() { return sort_alphabetically_; }

 private:
  static bool sort_alphabetically_;
#endif  // ENABLE_TOKEN_ALPHABETICAL_SORTING
};

inline bool operator==(const Token& lhs, const std::string& rhs) {
  return strcmp(lhs.c_str(), rhs.c_str()) == 0;
}

inline bool operator==(const std::string& lhs, const Token& rhs) {
  return strcmp(lhs.c_str(), rhs.c_str()) == 0;
}

inline bool operator!=(const Token& lhs, const Token& rhs) {
  return !(lhs == rhs);
}

inline bool operator!=(const Token& lhs, const std::string& rhs) {
  return !(lhs == rhs);
}

inline bool operator!=(const std::string& lhs, const Token& rhs) {
  return !(lhs == rhs);
}

inline bool operator<(const Token& lhs, const Token& rhs) {
#ifdef ENABLE_TOKEN_ALPHABETICAL_SORTING
  if (Token::sort_alphabetically()) {
    return strcmp(lhs.c_str(), rhs.c_str()) < 0;
  }
#endif  // ENABLE_TOKEN_ALPHABETICAL_SORTING
  return lhs.c_str() < rhs.c_str();
}

inline bool operator>(const Token& lhs, const Token& rhs) {
#ifdef ENABLE_TOKEN_ALPHABETICAL_SORTING
  if (Token::sort_alphabetically()) {
    return strcmp(lhs.c_str(), rhs.c_str()) > 0;
  }
#endif  // ENABLE_TOKEN_ALPHABETICAL_SORTING
  return lhs.c_str() > rhs.c_str();
}

inline std::ostream& operator<<(std::ostream& os, base::Token token) {
  os << token.c_str();
  return os;
}

}  // namespace base

namespace BASE_HASH_NAMESPACE {
#if defined(BASE_HASH_USE_HASH_STRUCT)

template <>
struct hash<base::Token> {
  std::size_t operator()(const base::Token& token) const {
    return reinterpret_cast<size_t>(token.c_str());
  }
};

#else

template <>
inline size_t hash_value<base::Token>(const base::Token& token) {
  return reinterpret_cast<size_t>(token.c_str());
}

#endif  // BASE_HASH_USE_STRUCT
}  // namespace BASE_HASH_NAMESPACE

#endif  // COBALT_BASE_TOKEN_H_
