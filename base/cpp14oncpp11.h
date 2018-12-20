// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CPP14ONCPP11_H_
#define BASE_CPP14ONCPP11_H_

#include <memory>

#if defined(STARBOARD)

// This file contains a collection of implementations of C++14 std classes
// and functions, if possible.  If it is not possible to reproduce C++14
// functionality, this class provides facilities to help with porting.

#if __cplusplus < 201402L
// Starboard is currently targetting C++11, not C++14, so unfortunately we
// do away with constexpr usage.
#define CONSTEXPR
// This type of STATIC_ASSERT is only used in unit tests so far for convenience
// purpose.
#define STATIC_ASSERT(value, message) EXPECT_TRUE(value)
#else
#define CONSTEXPR constexpr
#define STATIC_ASSERT(value, message) static_assert(value, message)
#endif

#if __cplusplus < 201402L
namespace {
template <int Index, class TargetType, class SkipType, class... Types>
struct get_internal {
  typedef typename get_internal<Index + 1, TargetType, Types...>::type type;
  // The line above will expand until TargetType is found,
  // Only the index of the get_internal not expanded will return.
  static constexpr int index = Index;
};

template <int Index, class TargetType, class... Types>
struct get_internal<Index, TargetType, TargetType, Types...> {
  typedef get_internal type;
  // This line is for the case that first type matches TargetType.
  static constexpr int index = Index;
};
}  // namespace

namespace std {

template <class TargetType, class... Types>
TargetType get(std::tuple<Types...> tuple) {
  return std::get<get_internal<0, TargetType, Types...>::type::index>(tuple);
}

template <size_t... Ints>
class index_sequence {};

namespace detail {
template <size_t N>
class GetSequenceHelper {
 public:
  template <typename T>
  struct AddIndex {};
  template <size_t... Ints>
  struct AddIndex<index_sequence<Ints...>> {
    typedef index_sequence<Ints..., N - 1> type;
  };

  typedef typename AddIndex<typename GetSequenceHelper<N - 1>::type>::type type;
};

template <>
class GetSequenceHelper<0> {
 public:
  typedef index_sequence<> type;
};


}  // namespace detail

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// In C++14, less<void> has a special meaning and defines the is_transparent
// member meaning that we can compare heterogeneous types (T and U).
template <> struct less<void> {
    template <class T, class U> auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) < std::forward<U>(u)) {
      return t < u;
    }

    typedef int is_transparent;
};

template <size_t N>
using make_index_sequence = typename detail::GetSequenceHelper<N>::type;

template<typename T>
using decay_t = typename decay<T>::type;

template<bool B, typename T, typename F>
using conditional_t = typename conditional<B,T,F>::type;

template<bool B, typename T = void>
using enable_if_t = typename enable_if<B,T>::type;

template <std::size_t I, typename T>
using tuple_element_t = typename tuple_element<I, T>::type;

template<typename T>
using remove_reference_t = typename remove_reference<T>::type;

template<typename T>
using remove_pointer_t = typename remove_pointer<T>::type;

template<typename T>
using remove_cv_t = typename remove_cv<T>::type;

template<typename T>
using remove_const_t = typename remove_const<T>::type;

template<typename T>
using remove_volatile_t = typename remove_volatile<T>::type;

template<typename T>
using add_cv_t = typename add_cv<T>::type;

template<typename T>
using add_const_t = typename add_const<T>::type;

template<typename T>
using add_volatile_t = typename add_volatile<T>::type;

template< class C > 
auto rbegin( C& c ) -> decltype(c.rbegin()) {
  return c.rbegin();
}

template< class C > 
auto rbegin( const C& c ) -> decltype(c.rbegin()) {
  return c.rbegin();
}

template< class T, size_t N > 
reverse_iterator<T*> rbegin( T (&array)[N] ) {
  return reverse_iterator<T*>(array + N);
}

template< class C > 
auto crbegin( const C& c ) -> decltype(std::rbegin(c)) {
  return std::rbegin(c);
}

template< class C > 
auto rend( C& c ) -> decltype(c.rend()) {
  return c.rend();
}

template< class C > 
auto rend( const C& c ) -> decltype(c.rend()) {
  return c.rend();
}

template< class T, size_t N > 
reverse_iterator<T*> rend( T (&array)[N] ) {
  return reverse_iterator<T*>(array);
}

template< class C > 
auto crend( const C& c ) -> decltype(std::rend(c)) {
  return std::rend(c);
}

}  // namespace std
#endif

#endif  // defined(STARBOARD)

#endif  // BASE_CPP14ONCPP11_H_
