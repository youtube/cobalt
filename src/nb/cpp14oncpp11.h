// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NB_CPP14ONCPP11_H_
#define NB_CPP14ONCPP11_H_

#include <memory>

#include "starboard/configuration.h"

#if defined(STARBOARD)

#define CONSTEXPR constexpr
#define CONSTEXPR_OR_INLINE constexpr
#define STATIC_ASSERT(value, message) static_assert(value, message)
#define CHECK14(expr) CHECK(expr)

#endif  // defined(STARBOARD)

#endif  // NB_CPP14ONCPP11_H_
