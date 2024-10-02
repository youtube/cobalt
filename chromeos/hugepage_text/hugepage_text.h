// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Author: Ken Chen <kenchen@google.com>
//
// Library support to remap process executable elf segment with hugepages.
//
// InitHugepagesAndMlockSelf() will search for an ELF executable segment,
// and remap it using hugepage.

#ifndef CHROMEOS_HUGEPAGE_TEXT_HUGEPAGE_TEXT_H_
#define CHROMEOS_HUGEPAGE_TEXT_HUGEPAGE_TEXT_H_

#include "chromeos/chromeos_export.h"

namespace chromeos {

// This function will scan ELF segments and attempt to do two things:
// - Reload some of .text into hugepages
// - Lock some of .text into memory, so it can't be swapped out.
//
// When this function returns, text segments that are naturally aligned on
// hugepage size will be backed by hugepages.
//
// Any and all errors encountered by this function are soft errors; Chrome
// should still be able to run.
CHROMEOS_EXPORT extern void InitHugepagesAndMlockSelf(void);
}  // namespace chromeos

#endif  // CHROMEOS_HUGEPAGE_TEXT_HUGEPAGE_TEXT_H_
