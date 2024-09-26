// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_RECT_H_

#include "third_party/blink/renderer/core/editing/forward.h"

namespace blink {

// This file provides utility functions for computing caret rect in LayoutNG.

struct LocalCaretRect;
struct NGCaretPosition;

// Given a position, returns the caret rect.
LocalCaretRect ComputeLocalCaretRect(const NGCaretPosition&);

// Almost the same as ComputeNGLocalCaretRect, except that the returned rect
// is adjusted to span the containing line box in the block direction.
LocalCaretRect ComputeLocalSelectionRect(const NGCaretPosition&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_RECT_H_
