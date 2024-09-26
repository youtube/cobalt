// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token_data.h"

namespace blink {

struct NGFieldsetBreakTokenData final : NGBlockBreakTokenData {
  explicit NGFieldsetBreakTokenData(const NGBlockBreakTokenData* other_data)
      : NGBlockBreakTokenData(kFieldsetBreakTokenData, other_data) {}

  LayoutUnit legend_block_size_contribution;
};

template <>
struct DowncastTraits<NGFieldsetBreakTokenData> {
  static bool AllowFrom(const NGBlockBreakTokenData& token_data) {
    return token_data.IsFieldsetType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_BREAK_TOKEN_DATA_H_
