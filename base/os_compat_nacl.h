// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OS_COMPAT_NACL_H_
#define BASE_OS_COMPAT_NACL_H_
#pragma once

#include <sys/types.h>

// NaCl has no timegm().
extern "C" time_t timegm(struct tm* const t);

#endif  // BASE_OS_COMPAT_NACL_H_

