// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MOVE_H_
#define BASE_MOVE_H_

#include <algorithm>

namespace base {

// The move function provides a functional form of swap to move a swappable
// value leverage the compiler return value optimization and avoiding copy.
//
// The type being moved must have an efficient swap() and be default
// contructable and copyable.
//
// C++0x will contain an std::move() function that makes use of rvalue
// references to achieve the same result. When std::move() is available, it
// can replace base::move().
//
// For example, the following code will not produce any copies of the string.
//
// std::string f() {
//   std::string result("Hello World");
//   return result;
// }
//
// class Class {
//  public:
//   Class(std::string x)  // Pass sink argument by value
//     : m_(move(x));  // Move x into m_
//  private:
//   std::string m_;
// };
//
// int main() {
//  Class x(f());  // the result string of f() is allocated as the temp argument
//                 // to x() and then swapped into the member m_. No Strings
//                 // are copied by this call.
// ...

template <typename T>  // T models Regular
T move(T& x) {  // NOLINT : Non-const ref for standard library conformance
  using std::swap;
  T result;
  swap(x, result);
  return result;
}

}  // namespace base

#endif  // BASE_MOVE_H_

