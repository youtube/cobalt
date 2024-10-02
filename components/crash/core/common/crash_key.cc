// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key.h"

#include "base/debug/stack_trace.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace crash_reporter {
namespace internal {

std::string FormatStackTrace(const base::debug::StackTrace& trace,
                             size_t max_length) {
  size_t count = 0;
  const void* const* addresses = trace.Addresses(&count);

  std::string value;
  for (size_t i = 0; i < count; ++i) {
    std::string address = base::StringPrintf(
        "0x%" PRIx64, reinterpret_cast<uint64_t>(addresses[i]));
    if (value.size() + address.size() > max_length)
      break;
    value += address + " ";
  }

  if (!value.empty() && value.back() == ' ') {
    value.resize(value.size() - 1);
  }

  return value;
}

}  // namespace internal
}  // namespace crash_reporter
