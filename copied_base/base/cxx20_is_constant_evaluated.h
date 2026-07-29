// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CXX20_IS_CONSTANT_EVALUATED_H_
#define BASE_CXX20_IS_CONSTANT_EVALUATED_H_

#include "build/build_config.h"

namespace base {

// Implementation of C++20's std::is_constant_evaluated.
//
// References:
// - https://en.cppreference.com/w/cpp/types/is_constant_evaluated
// - https://wg21.link/meta.const.eval
constexpr bool is_constant_evaluated() noexcept {
#if BUILDFLAG(BUILD_BASE_WITH_CPP17)
  // Compilers are not guaranteed to provide this builtin. Always returning
  // false should be safe: if a calling function that uses this result to select
  // a runtime or compile-time path were actually used in a constant-evaluated
  // context then we should get a compile-time error.
  return false;
#else
  return __builtin_is_constant_evaluated();
#endif
}

}  // namespace base

#endif  // BASE_CXX20_IS_CONSTANT_EVALUATED_H_
