/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPE_TRAITS_H_

#include <cstddef>
#include <type_traits>
#include <utility>
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "v8/include/cppgc/type-traits.h"  // nogncheck

namespace WTF {

// Returns a string that contains the type name of |T| as a substring.
template <typename T>
inline const char* GetStringWithTypeName() {
  return PRETTY_FUNCTION;
}

template <typename T, typename U>
struct IsSubclass {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  static YesType SubclassCheck(U*);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

template <typename T, template <typename... V> class U>
struct IsSubclassOfTemplate {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename... W>
  static YesType SubclassCheck(U<W...>*);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

template <typename T, template <typename V, size_t W> class U>
struct IsSubclassOfTemplateTypenameSize {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename X, size_t Y>
  static YesType SubclassCheck(U<X, Y>*);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

template <typename T, template <typename V, size_t W, typename X> class U>
struct IsSubclassOfTemplateTypenameSizeTypename {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename Y, size_t Z, typename A>
  static YesType SubclassCheck(U<Y, Z, A>*);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

template <typename T>
struct IsTraceable : cppgc::internal::IsTraceable<T> {};

template <typename T>
struct IsGarbageCollectedType
    : cppgc::internal::IsGarbageCollectedOrMixinType<T> {};

template <typename T>
struct IsWeak : cppgc::internal::IsWeak<T> {};

template <typename T>
struct IsMemberType : std::bool_constant<cppgc::IsMemberTypeV<T>> {};

template <typename T>
struct IsWeakMemberType : std::bool_constant<cppgc::IsWeakMemberTypeV<T>> {};

template <typename T>
struct IsMemberOrWeakMemberType
    : std::bool_constant<cppgc::IsMemberTypeV<T> ||
                         cppgc::IsWeakMemberTypeV<T>> {};

template <typename T>
struct IsAnyMemberType
    : std::bool_constant<IsMemberOrWeakMemberType<T>::value ||
                         cppgc::IsUntracedMemberTypeV<T>> {};

template <typename T, typename U>
struct IsTraceable<std::pair<T, U>>
    : std::bool_constant<IsTraceable<T>::value || IsTraceable<U>::value> {};

// Convenience template wrapping the IsTraceableInCollection template in
// Collection Traits. It helps make the code more readable.
template <typename Traits>
struct IsTraceableInCollectionTrait
    : std::bool_constant<Traits::template IsTraceableInCollection<>::value> {};

enum WeakHandlingFlag {
  kNoWeakHandling,
  kWeakHandling,
};

template <typename T>
struct WeakHandlingTrait
    : std::integral_constant<WeakHandlingFlag,
                             IsWeak<T>::value ? kWeakHandling
                                              : kNoWeakHandling> {};

// This is used to check that DISALLOW_NEW objects are not
// stored in off-heap Vectors, HashTables etc.
template <typename T>
struct IsDisallowNew {
 private:
  using YesType = char;
  struct NoType {
    char padding[8];
  };

  template <typename U>
  static YesType CheckMarker(typename U::IsDisallowNewMarker*);
  template <typename U>
  static NoType CheckMarker(...);

 public:
  static const bool value = sizeof(CheckMarker<T>(nullptr)) == sizeof(YesType);
};

template <>
class IsGarbageCollectedType<void> {
 public:
  static const bool value = false;
};

template <typename T,
          bool = std::is_function<typename std::remove_const<
                     typename std::remove_pointer<T>::type>::type>::value ||
                 std::is_void<typename std::remove_const<
                     typename std::remove_pointer<T>::type>::type>::value>
class IsPointerToGarbageCollectedType {
 public:
  static const bool value = false;
};

template <typename T>
class IsPointerToGarbageCollectedType<T*, false> {
 public:
  static const bool value = IsGarbageCollectedType<T>::value;
};

template <typename T, typename = void>
struct IsStackAllocatedType : std::false_type {};

template <typename T>
struct IsStackAllocatedType<T,
                            std::void_t<typename T::IsStackAllocatedTypeMarker>>
    : std::true_type {};

}  // namespace WTF

using WTF::IsGarbageCollectedType;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPE_TRAITS_H_
