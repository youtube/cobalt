// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_exchange_utils.h"

#include <unordered_map>

#include "ui/base/clipboard/custom_data_helper.h"

namespace exo {

std::optional<base::Pickle> FilterCustomData(const std::vector<uint8_t>& data) {
  if (auto map = ui::ReadCustomDataIntoMap(data)) {
    std::erase_if(*map,
                  [](const auto& kv) { return kv.first.starts_with(u"fs/"); });
    if (!map->empty()) {
      base::Pickle pickle;
      ui::WriteCustomDataToPickle(*map, &pickle);
      return pickle;
    }
  }
  return std::nullopt;
}

}  // namespace exo
