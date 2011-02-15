// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEMPLATE_UTIL_H_
#define BASE_TEMPLATE_UTIL_H_
#pragma once

#include <cstddef>  // For size_t.

#include "build/build_config.h"

namespace base {

// template definitions from tr1

template<class T, T v>
struct integral_constant {
  static const T value = v;
  typedef T value_type;
  typedef integral_constant<T, v> type;
};

template <class T, T v> const T integral_constant<T, v>::value;

typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

template <class T> struct is_pointer : false_type {};
template <class T> struct is_pointer<T*> : true_type {};

template<class> struct is_array : public false_type {};
template<class T, size_t n> struct is_array<T[n]> : public true_type {};
template<class T> struct is_array<T[]> : public true_type {};

template <class T> struct is_non_const_reference : false_type {};
template <class T> struct is_non_const_reference<T&> : true_type {};
template <class T> struct is_non_const_reference<const T&> : false_type {};

namespace internal {

// Types YesType and NoType are guaranteed such that sizeof(YesType) <
// sizeof(NoType).
typedef char YesType;

struct NoType {
  YesType dummy[2];
};

#if !defined(OS_WIN)

// This class is an implementation detail for is_convertible, and you
// don't need to know how it works to use is_convertible. For those
// who care: we declare two different functions, one whose argument is
// of type To and one with a variadic argument list. We give them
// return types of different size, so we can use sizeof to trick the
// compiler into telling us which function it would have chosen if we
// had called it with an argument of type From.  See Alexandrescu's
// _Modern C++ Design_ for more details on this sort of trick.

template <typename From, typename To>
struct ConvertHelper {
  static YesType Test(To);
  static NoType Test(...);
  static From Create();
};

#endif  // !defined(OS_WIN)

// Used to determine if a type is a struct/union/class. Inspired by Boost's
// is_class type_trait implementation.
struct IsClassHelper {
  template <typename C>
  static YesType Test(void(C::*)(void));

  template <typename C>
  static NoType Test(...);
};

}  // namespace internal

#if !defined(OS_WIN)

// Inherits from true_type if From is convertible to To, false_type otherwise.
template <typename From, typename To>
struct is_convertible
    : integral_constant<bool,
                        sizeof(internal::ConvertHelper<From, To>::Test(
                                  internal::ConvertHelper<From, To>::Create()))
                        == sizeof(internal::YesType)> {
};

#endif  // !defined(OS_WIN)

template <typename T>
struct is_class
    : integral_constant<bool,
                        sizeof(internal::IsClassHelper::Test<T>(0)) ==
                            sizeof(internal::YesType)> {
};

}  // namespace base

#endif  // BASE_TEMPLATE_UTIL_H_
