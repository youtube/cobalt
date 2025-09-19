// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CXX20_IS_CONSTANT_EVALUATED_H_
#define BASE_CXX20_IS_CONSTANT_EVALUATED_H_

#include "build/build_config.h"

namespace base {

#if __cplusplus <= 201811L && (BUILDFLAG(IS_NATIVE_TARGET_BUILD) || BUILDFLAG(IS_STARBOARD_TOOLCHAIN))
constexpr bool is_constant_evaluated() noexcept {
    struct C {};
    struct M : C { int a; };
    struct N : C { int a; };
    // At compile-time, &M::a != static_cast<int C::*>(&N::a) is true
    // because the exact member pointers are compared.
    // At runtime, the comparison might be false because only offsets are considered.
    // This difference allows detection of constant evaluation.
    return &M::a != static_cast<int C::*>(&N::a);
}

#else
// Implementation of C++20's std::is_constant_evaluated.
//
// References:
// - https://en.cppreference.com/w/cpp/types/is_constant_evaluated
// - https://wg21.link/meta.const.eval
constexpr bool is_constant_evaluated() noexcept {
  return __builtin_is_constant_evaluated();
}
#endif

}  // namespace base

#endif  // BASE_CXX20_IS_CONSTANT_EVALUATED_H_
