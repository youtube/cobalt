// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "base" command-line switches.

#ifndef BASE_BASE_SWITCHES_H_
#define BASE_BASE_SWITCHES_H_
#pragma once

#include "base/base_api.h"

namespace switches {

BASE_API extern const char kDebugOnStart[];
BASE_API extern const char kDisableBreakpad[];
BASE_API extern const char kEnableDCHECK[];
BASE_API extern const char kFullMemoryCrashReport[];
BASE_API extern const char kLocalePak[];
BASE_API extern const char kNoErrorDialogs[];
BASE_API extern const char kNoMessageBox[];
BASE_API extern const char kTestChildProcess[];
BASE_API extern const char kV[];
BASE_API extern const char kVModule[];
BASE_API extern const char kWaitForDebugger[];

}  // namespace switches

#endif  // BASE_BASE_SWITCHES_H_
