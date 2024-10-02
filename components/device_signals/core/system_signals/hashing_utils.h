// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_HASHING_UTILS_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_HASHING_UTILS_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace device_signals {

// Tries to generate a byte string containing the SHA256 value of the file at
// `file_path`. Will return absl::nullopt in invalid cases.
absl::optional<std::string> HashFile(const base::FilePath& file_path);

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_HASHING_UTILS_H_
