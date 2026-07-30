// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_EXCHANGE_UTILS_H_
#define COMPONENTS_EXO_DATA_EXCHANGE_UTILS_H_

#include <optional>
#include <vector>

#include "base/pickle.h"

namespace exo {

// Reads custom data from |data|, filters out keys starting with "fs/", and if
// the resulting map is not empty, serializes it back to a Pickle. Returns
// nullopt if the input data cannot be read as custom data, or if all keys were
// filtered out.
// |data| comes from a guest VM. Re-serialize and drop FilesApp-internal `fs/*`
// keys so a guest cannot forge fs/tag + fs/sources and drive FilesApp /
// HoldingSpace into operating on host paths it was never shared.
std::optional<base::Pickle> FilterCustomData(const std::vector<uint8_t>& data);

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_EXCHANGE_UTILS_H_
