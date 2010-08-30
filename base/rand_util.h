// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RAND_UTIL_H_
#define BASE_RAND_UTIL_H_
#pragma once

#include "base/basictypes.h"

namespace base {

// Returns a random number in range [0, kuint64max]. Thread-safe.
uint64 RandUint64();

// Returns a random number between min and max (inclusive). Thread-safe.
int RandInt(int min, int max);

// Returns a random number in range [0, max).  Thread-safe.
//
// Note that this can be used as an adapter for std::random_shuffle():
// Given a pre-populated |std::vector<int> myvector|, shuffle it as
//   std::random_shuffle(myvector.begin(), myvector.end(), base::RandGenerator);
uint64 RandGenerator(uint64 max);

// Returns a random double in range [0, 1). Thread-safe.
double RandDouble();

}  // namespace base

#endif  // BASE_RAND_UTIL_H_
