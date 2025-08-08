/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/shadow/progress_shadow_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"

namespace blink {

ProgressShadowElement::ProgressShadowElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
}

HTMLProgressElement* ProgressShadowElement::ProgressElement() const {
  return To<HTMLProgressElement>(OwnerShadowHost());
}

void ProgressShadowElement::AdjustStyle(ComputedStyleBuilder& builder) {
  const ComputedStyle* progress_style = ProgressElement()->GetComputedStyle();
  DCHECK(progress_style);
  if (progress_style->HasEffectiveAppearance()) {
    builder.SetDisplay(EDisplay::kNone);
  } else if (!IsHorizontalWritingMode(builder.GetWritingMode())) {
    // For vertical writing-mode, we need to set the direction to rtl so that
    // the progress value bar is rendered bottom up.
    builder.SetDirection(TextDirection::kRtl);
  }
}

}  // namespace blink
