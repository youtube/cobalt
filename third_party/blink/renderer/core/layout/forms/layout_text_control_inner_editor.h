// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_TEXT_CONTROL_INNER_EDITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_TEXT_CONTROL_INNER_EDITOR_H_

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// LayoutTextControlInnerEditor is a LayoutObject for 'InnerEditor' elements
// in <input> and <textarea>.
class LayoutTextControlInnerEditor final : public LayoutNGBlockFlow {
 public:
  explicit LayoutTextControlInnerEditor(Element* element)
      : LayoutNGBlockFlow(element) {}

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutTextControlInnerEditor";
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FORMS_LAYOUT_TEXT_CONTROL_INNER_EDITOR_H_
