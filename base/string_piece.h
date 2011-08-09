// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copied from strings/stringpiece.h with modifications
//
// A string-like object that points to a sized piece of memory.
//
// Functions or methods may use const StringPiece& parameters to accept either
// a "const char*" or a "string" value that will be implicitly converted to
// a StringPiece.  The implicit conversion means that it is often appropriate
// to include this .h file in other files rather than forward-declaring
// StringPiece as would be appropriate for most other Google classes.
//
// Systematic usage of StringPiece is encouraged as it will reduce unnecessary
// conversions from "const char*" to "string" and back again.
//
// StringPiece16 is similar to StringPiece but for base::string16 instead of
// std::string. We do not define as large of a subset of the STL functions
// from basic_string as in StringPiece, but this can be changed if these
// functions (find, find_first_of, etc.) are found to be useful in this context.
//

#ifndef BASE_STRING_PIECE_H_
#define BASE_STRING_PIECE_H_
#pragma once

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/string16.h"

namespace base {

class BASE_EXPORT StringPiece {
 public:
  // standard STL container boilerplate
  typedef size_t size_type;
  typedef char value_type;
  typedef const char* pointer;
  typedef const char& reference;
  typedef const char& const_reference;
  typedef ptrdiff_t difference_type;
  typedef const char* const_iterator;
  typedef const char* iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;

  static const size_type npos;

 public:
  // We provide non-explicit singleton constructors so users can pass
  // in a "const char*" or a "string" wherever a "StringPiece" is
  // expected.
  StringPiece() : ptr_(NULL), length_(0) { }
  StringPiece(const char* str)
    : ptr_(str), length_((str == NULL) ? 0 : strlen(str)) { }
  StringPiece(const std::string& str)
    : ptr_(str.data()), length_(str.size()) { }
  StringPiece(const char* offset, size_type len)
    : ptr_(offset), length_(len) { }

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated string.
  const char* data() const { return ptr_; }
  size_type size() const { return length_; }
  size_type length() const { return length_; }
  bool empty() const { return length_ == 0; }

  void clear() {
    ptr_ = NULL;
    length_ = 0;
  }
  void set(const char* data, size_type len) {
    ptr_ = data;
    length_ = len;
  }
  void set(const char* str) {
    ptr_ = str;
    length_ = str ? strlen(str) : 0;
  }
  void set(const void* data, size_type len) {
    ptr_ = reinterpret_cast<const char*>(data);
    length_ = len;
  }

  char operator[](size_type i) const { return ptr_[i]; }

  void remove_prefix(size_type n) {
    ptr_ += n;
    length_ -= n;
  }

  void remove_suffix(size_type n) {
    length_ -= n;
  }

  int compare(const StringPiece& x) const {
    int r = wordmemcmp(
        ptr_, x.ptr_, (length_ < x.length_ ? length_ : x.length_));
    if (r == 0) {
      if (length_ < x.length_) r = -1;
      else if (length_ > x.length_) r = +1;
    }
    return r;
  }

  std::string as_string() const {
    // std::string doesn't like to take a NULL pointer even with a 0 size.
    return std::string(!empty() ? data() : "", size());
  }

  void CopyToString(std::string* target) const;
  void AppendToString(std::string* target) const;

  // Does "this" start with "x"
  bool starts_with(const StringPiece& x) const {
    return ((length_ >= x.length_) &&
            (wordmemcmp(ptr_, x.ptr_, x.length_) == 0));
  }

  // Does "this" end with "x"
  bool ends_with(const StringPiece& x) const {
    return ((length_ >= x.length_) &&
            (wordmemcmp(ptr_ + (length_-x.length_), x.ptr_, x.length_) == 0));
  }

  iterator begin() const { return ptr_; }
  iterator end() const { return ptr_ + length_; }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(ptr_ + length_);
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(ptr_);
  }

  size_type max_size() const { return length_; }
  size_type capacity() const { return length_; }

  size_type copy(char* buf, size_type n, size_type pos = 0) const;

  size_type find(const StringPiece& s, size_type pos = 0) const;
  size_type find(char c, size_type pos = 0) const;
  size_type rfind(const StringPiece& s, size_type pos = npos) const;
  size_type rfind(char c, size_type pos = npos) const;

  size_type find_first_of(const StringPiece& s, size_type pos = 0) const;
  size_type find_first_of(char c, size_type pos = 0) const {
    return find(c, pos);
  }
  size_type find_first_not_of(const StringPiece& s, size_type pos = 0) const;
  size_type find_first_not_of(char c, size_type pos = 0) const;
  size_type find_last_of(const StringPiece& s, size_type pos = npos) const;
  size_type find_last_of(char c, size_type pos = npos) const {
    return rfind(c, pos);
  }
  size_type find_last_not_of(const StringPiece& s, size_type pos = npos) const;
  size_type find_last_not_of(char c, size_type pos = npos) const;

  StringPiece substr(size_type pos, size_type n = npos) const;

  static int wordmemcmp(const char* p, const char* p2, size_type N) {
    return memcmp(p, p2, N);
  }

 private:
  const char*   ptr_;
  size_type     length_;
};

class BASE_EXPORT StringPiece16 {
 public:
  // standard STL container boilerplate
  typedef size_t size_type;
  typedef char16 value_type;
  typedef const char16* pointer;
  typedef const char16& reference;
  typedef const char16& const_reference;
  typedef ptrdiff_t difference_type;
  typedef const char16* const_iterator;
  typedef const char16* iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;

 public:
  // We provide non-explicit singleton constructors so users can pass
  // in a "const char16*" or a "string16" wherever a "StringPiece16" is
  // expected.
  StringPiece16() : ptr_(NULL), length_(0) { }
  StringPiece16(const char16* str)
      : ptr_(str),
        length_((str == NULL) ? 0 : string16::traits_type::length(str)) { }
  StringPiece16(const string16& str)
      : ptr_(str.data()), length_(str.size()) { }
  StringPiece16(const char16* offset, size_type len)
      : ptr_(offset), length_(len) { }

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated string.
  const char16* data() const { return ptr_; }
  size_type size() const { return length_; }
  size_type length() const { return length_; }
  bool empty() const { return length_ == 0; }

  void clear() {
    ptr_ = NULL;
    length_ = 0;
  }
  void set(const char16* data, size_type len) {
    ptr_ = data;
    length_ = len;
  }
  void set(const char16* str) {
    ptr_ = str;
    length_ = str ? string16::traits_type::length(str) : 0;
  }

  char16 operator[](size_type i) const { return ptr_[i]; }

  string16 as_string16() const {
    // StringPiece claims that this is bad when data() is NULL, but unittesting
    // seems to say otherwise.
    return string16(data(), size());
  }

  iterator begin() const { return ptr_; }
  iterator end() const { return ptr_ + length_; }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(ptr_ + length_);
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(ptr_);
  }

  size_type max_size() const { return length_; }
  size_type capacity() const { return length_; }

  static int wordmemcmp(const char16* p, const char16* p2, size_type N) {
    return string16::traits_type::compare(p, p2, N);
  }

 private:
  const char16*   ptr_;
  size_type       length_;
};

BASE_EXPORT bool operator==(const StringPiece& x, const StringPiece& y);

inline bool operator!=(const StringPiece& x, const StringPiece& y) {
  return !(x == y);
}

inline bool operator<(const StringPiece& x, const StringPiece& y) {
  const int r = StringPiece::wordmemcmp(
      x.data(), y.data(), (x.size() < y.size() ? x.size() : y.size()));
  return ((r < 0) || ((r == 0) && (x.size() < y.size())));
}

inline bool operator>(const StringPiece& x, const StringPiece& y) {
  return y < x;
}

inline bool operator<=(const StringPiece& x, const StringPiece& y) {
  return !(x > y);
}

inline bool operator>=(const StringPiece& x, const StringPiece& y) {
  return !(x < y);
}

inline bool operator==(const StringPiece16& x, const StringPiece16& y) {
  if (x.size() != y.size())
    return false;

  return StringPiece16::wordmemcmp(x.data(), y.data(), x.size()) == 0;
}

inline bool operator!=(const StringPiece16& x, const StringPiece16& y) {
  return !(x == y);
}

inline bool operator<(const StringPiece16& x, const StringPiece16& y) {
  const int r = StringPiece16::wordmemcmp(
      x.data(), y.data(), (x.size() < y.size() ? x.size() : y.size()));
  return ((r < 0) || ((r == 0) && (x.size() < y.size())));
}

inline bool operator>(const StringPiece16& x, const StringPiece16& y) {
  return y < x;
}

inline bool operator<=(const StringPiece16& x, const StringPiece16& y) {
  return !(x > y);
}

inline bool operator>=(const StringPiece16& x, const StringPiece16& y) {
  return !(x < y);
}

}  // namespace base

// We provide appropriate hash functions so StringPiece and StringPiece16 can
// be used as keys in hash sets and maps.

// This hash function is copied from base/hash_tables.h. We don't use the
// ones already defined for string and string16 directly because it would
// require the string constructors to be called, which we don't want.
#define HASH_STRING_PIECE(StringPieceType, string_piece)                \
  std::size_t result = 0;                                               \
  for (StringPieceType::const_iterator i = string_piece.begin();        \
       i != string_piece.end(); ++i)                                    \
    result = (result * 131) + *i;                                       \
  return result;                                                        \

namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_GCC)

template<>
struct hash<base::StringPiece> {
  std::size_t operator()(const base::StringPiece& sp) const {
    HASH_STRING_PIECE(base::StringPiece, sp);
  }
};
template<>
struct hash<base::StringPiece16> {
  std::size_t operator()(const base::StringPiece16& sp16) const {
    HASH_STRING_PIECE(base::StringPiece16, sp16);
  }
};

#elif defined(COMPILER_MSVC)

inline size_t hash_value(const base::StringPiece& sp) {
  HASH_STRING_PIECE(base::StringPiece, sp);
}
inline size_t hash_value(const base::StringPiece16& sp16) {
  HASH_STRING_PIECE(base::StringPiece16, sp16);
}

#endif  // COMPILER

}  // namespace BASE_HASH_NAMESPACE

#endif  // BASE_STRING_PIECE_H_
