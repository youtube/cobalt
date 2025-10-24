// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MATHML_MATH_OPERATOR_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MATHML_MATH_OPERATOR_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"

namespace blink {

class CORE_EXPORT MathOperatorLayoutAlgorithm
    : public LayoutAlgorithm<BlockNode, BoxFragmentBuilder, BlockBreakToken> {
 public:
  explicit MathOperatorLayoutAlgorithm(const LayoutAlgorithmParams& params);

  const LayoutResult* Layout();
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&);

 private:
  UChar32 GetBaseCodePoint() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MATHML_MATH_OPERATOR_LAYOUT_ALGORITHM_H_
