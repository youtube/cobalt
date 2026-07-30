// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_types.h"

namespace critical_actions {

CriticalActionEntry::CriticalActionEntry() = default;

CriticalActionEntry::CriticalActionEntry(const CriticalActionEntry&) = default;

CriticalActionEntry::CriticalActionEntry(CriticalActionEntry&&) noexcept =
    default;

CriticalActionEntry& CriticalActionEntry::operator=(
    const CriticalActionEntry&) = default;

CriticalActionEntry& CriticalActionEntry::operator=(
    CriticalActionEntry&&) noexcept = default;

CriticalActionEntry::~CriticalActionEntry() = default;

}  // namespace critical_actions
