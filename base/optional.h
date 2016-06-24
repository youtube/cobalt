/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef BASE_OPTIONAL_H_
#define BASE_OPTIONAL_H_

#include <iosfwd>

#include "base/base_export.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"

namespace base {

// This class is based off of std::experimental::optional:
//   http://en.cppreference.com/w/cpp/experimental/optional
//
// It is a template class where instances parameterized by type T contain
// memory for an instance of type T, but it may or may not be constructed.
// If it is not constructed, it cannot be accessed, and if it is, it can
// be accessed.  This allows one to check if the inner object exists or not
// before using it, and is useful for functions that may or may not return
// a value.  Note that the memory for the object is stored internally, so
// no heap allocations are made over the course of construction and destruction
// of the internal object (unless the internal object allocates memory within
// its constructor).
//
// Some functionality is left out.  For example, most C++11 functionality
// is not implemented, since we would like this to be friendly to non-C++11
// compilers.
//
// In the future, if C++11 functionality is needed, it can be implemented
// and surrounded by preprocessor guards to maintain compatibility with non
// C++11 compilers.
//

// The nullopt_t type is used as a signal for an empty optional.  If any
// optional is assigned the value of nullopt, it will be disengaged.
// For example,
//   base::optional<int> my_int_optional(5);
//   EXPECT_FALSE(!my_int_optional);
//   my_int_optional = base::nullopt;
//   EXPECT_TRUE(!my_int_optional);
//
struct nullopt_t {
  nullopt_t() {}
};
extern const nullopt_t nullopt;

// The in_place_t type is used to signal in-place construction of the internal
// object.  This is used by the in place constructor of optional, which forwards
// its parameters to the internal object's constructor.
// For example,
//   class Foo {
//    public:
//     Foo(int x, int y) { x_ = x; y_ = y; }
//     int x() const { return x_; }
//     int y() const { return y_; }
//
//    private:
//     int x_;
//     int y_;
//   };
//
//   ...
//
//   base::optional<Foo> my_foo(base::in_place, 2, 3);
//   EXPECT_FALSE(!my_foo);
//   EXPECT_EQ(2, my_foo->x());
//   EXPECT_EQ(3, my_foo->y());
//
struct in_place_t {
  in_place_t() {}
};
extern const in_place_t in_place;

template <typename T>
class BASE_EXPORT optional {
 public:
  // Construction via the default constructor results in an optional that is
  // not engaged.
  optional() { InitializeAsDisengaged(); }

  optional(nullopt_t) { InitializeAsDisengaged(); }

  // This non-explicit singleton constructor is provided so users can pass in a
  // T wherever a optional<T> is expected.
  optional(const T& value) { SetValue(value); }

  optional(const optional<T>& other) {
    if (other.engaged_) {
      SetValue(other.value());
    } else {
      InitializeAsDisengaged();
    }
  }

  // Destruct contained object upon optional's destruction.
  ~optional() { EnsureDisengaged(); }

  // Disengages the optional, calling the destructor of the contained object
  // if it is engaged.
  optional<T>& operator=(nullopt_t) {
    EnsureDisengaged();
    return *this;
  }

  // Reassigns the underlying optional to value passed in on the right hand
  // side.  This will destruct the lhs contained object first if it exists.
  template <typename U>
  optional<T>& operator=(const U& other) {
    if (engaged_) {
      value() = other;
    } else {
      SetValue(other);
    }
    return *this;
  }

  // Copy assignment
  optional<T>& operator=(const optional<T>& other) {
    if (engaged_ && other.engaged_) {
      value() = other.value();
    } else if (!engaged_ && other.engaged_) {
      SetValue(other.value());
    } else if (engaged_ && !other.engaged_) {
      EnsureDisengaged();
    }
    // Do nothing if lhs and rhs are both not engaged.
    return *this;
  }

  // Overloaded conversion to bool operator for determining whether the optional
  // is engaged or not.  It returns true if the optional is engaged, and false
  // otherwise.
#if defined(_MSC_VER) && _MSC_VER < 1800
  // MSVC 2012 does not support explicit cast operators.
  // http://blogs.msdn.com/b/vcblog/archive/2011/09/12/10209291.aspx

  // For any compiler that doesn't support explicit bool operators, we instead
  // use the Safe Bool Idiom: http://www.artima.com/cppsource/safebool.html
 private:
  // The type of SafeBoolIdiomType (pointer to data member of a private type) is
  // limited in functionality so much that the only thing a user can do with it
  // is test for null, or apply to operator==/operator!=.  Since both operators
  // == and != are already overloaded for optional, this leaves null tests,
  // which we use for boolean testing.
  class PrivateSafeBoolIdiomFakeMemberType;
  typedef PrivateSafeBoolIdiomFakeMemberType optional::*SafeBoolIdiomType;
 public:
  operator const SafeBoolIdiomType() const {
    // If we wish to return true, we cast engaged_ to our private type giving
    // a non-null pointer to data member.  Otherwise, we return NULL.  The
    // only thing the user can do with the return type is test for NULL.
    return engaged_ ?
        reinterpret_cast<const SafeBoolIdiomType>(&optional::engaged_) :
        NULL;
  }
#else
  explicit operator bool() const { return engaged_; }
#endif

  // Dereferences the internal object.
  const T* operator->() const { return &(value()); }

  T* operator->() { return &(value()); }

  const T& operator*() const { return value(); }

  T& operator*() { return value(); }

  // Dereferences and returns the internal object.
  const T& value() const {
    DCHECK(engaged_) << "Attempted to access object in a disengaged optional.";
    return *static_cast<const T*>(value_memory_.void_data());
  }

  T& value() {
    DCHECK(engaged_) << "Attempted to access object in a disengaged optional.";
    return *static_cast<T*>(value_memory_.void_data());
  }

  template <typename U>
  T value_or(const U& value) const {
    if (engaged_) {
      return this->value();
    } else {
      return value;
    }
  }

  // Swaps the values of two optionals.
  void swap(optional<T>& other) {
    if (engaged_ && other.engaged_) {
      // Swap the value contents with each other.
      std::swap(value(), other.value());
    } else if (engaged_) {
      other.SetValue(value());
      EnsureDisengaged();
    } else if (other.engaged_) {
      SetValue(other.value());
      other.EnsureDisengaged();
    }
    // If both the lhs and rhs are not engaged, we do nothing.
  }

// include the pump.py-generated declaration and impelmentation for
// forwarding constructor and emplace.
#include "optional_internal.h"

 private:
  // Sets a non-engaged optional to a specified value, and marks it as engaged.
  template <typename U>
  void SetValue(const U& value) {
    new (value_memory_.void_data()) T(value);
    engaged_ = true;
#if !defined(NDEBUG)
    value_ptr_ = static_cast<const T*>(value_memory_.void_data());
#endif
  }

  // If an optional is engaged, it destructs the wrapped value and marks the
  // optional as disengaged.  Does nothing to a disengaged optional.
  void EnsureDisengaged() {
    if (engaged_) {
      static_cast<T*>(value_memory_.void_data())->~T();
      engaged_ = false;
#if !defined(NDEBUG)
      value_ptr_ = NULL;
#endif
    }
  }

  // Called upon object construction to initialize the object into a disengaged
  // state.
  void InitializeAsDisengaged() {
    engaged_ = false;
#if !defined(NDEBUG)
    value_ptr_ = NULL;
#endif
  }

  // The actual memory reserved for the object that may or may not exist.
  base::AlignedMemory<sizeof(T), ALIGNOF(T)> value_memory_;
  // This boolean tracks whether or not the object is constructed yet or not.
  bool engaged_;
#if !defined(NDEBUG)
  // In debug builds, this member makes it easy to inspect the value contained
  // in the optional via a debugger.
  const T* value_ptr_;
#endif
};

// Comparison between 2 optionals
template <typename T>
inline bool operator==(const optional<T>& lhs, const optional<T>& rhs) {
  if (!lhs) {
    return !rhs;
  }

  return rhs == lhs.value();
}

template <typename T>
inline bool operator<(const optional<T>& lhs, const optional<T>& rhs) {
  if (lhs && rhs) {
    return lhs.value() < rhs.value();
  } else {
    // Handle all other cases simply in terms of whether the optionals are
    // engaged or not.
    return static_cast<bool>(lhs) < static_cast<bool>(rhs);
  }
}

// Comparison with nullopt_t
template <typename T>
inline bool operator==(nullopt_t, const optional<T>& rhs) {
  return !rhs;
}

template <typename T>
inline bool operator==(const optional<T>& lhs, nullopt_t rhs) {
  return rhs == lhs;
}

template <typename T>
inline bool operator<(const optional<T>& /* lhs */, nullopt_t) {
  return false;
}

template <typename T>
inline bool operator<(nullopt_t, const optional<T>& rhs) {
  return static_cast<bool>(rhs);
}

// Comparison between an optional and a value
template <typename T>
inline bool operator==(const optional<T>& lhs, const T& rhs) {
  return (!lhs ? false : lhs.value() == rhs);
}

template <typename T>
inline bool operator==(const T& lhs, const optional<T>& rhs) {
  return rhs == lhs;
}

template <typename T>
inline bool operator<(const T& lhs, const optional<T>& rhs) {
  return rhs && lhs < rhs.value();
}

template <typename T>
inline bool operator<(const optional<T>& lhs, const T& rhs) {
  return !lhs || lhs.value() < rhs;
}

// This is a convenient but non-standard method, do not rely on it if you expect
// the compatibility with upcoming C++ versions.
template <typename T>
inline std::ostream& operator<<(std::ostream& stream,
                                const optional<T>& maybe_value) {
  if (maybe_value) {
    stream << *maybe_value;
  } else {
    stream << "nullopt";
  }
  return stream;
}

template <typename T>
optional<T> make_optional(const T& value) {
  return optional<T>(value);
}

}  // namespace base

namespace BASE_HASH_NAMESPACE {

#if defined(BASE_HASH_USE_HASH_STRUCT)
template <typename T>
struct hash<base::optional<T> > {
 public:
  size_t operator()(const base::optional<T>& value) const {
    if (!value) {
      return 0;
    } else {
      return value_hash_(value.value());
    }
  }

 private:
  hash<T> value_hash_;
};

#else
template <typename T>
inline size_t hash_value(const base::optional<T>& value) {
  if (!value) {
    return 0;
  } else {
    return hash_value(value.value());
  }
}

#endif  // defined(BASE_HASH_USE_HASH_STRUCT)
}  // namespace BASE_HASH_NAMESPACE

namespace std {

template <typename T>
void swap(base::optional<T>& lhs, base::optional<T>& rhs) {
  lhs.swap(rhs);
}

}  // namespace std

#endif  // BASE_OPTIONAL_H_
