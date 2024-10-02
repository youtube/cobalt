// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"

namespace blink {

CompositionMarker::CompositionMarker(
    unsigned start_offset,
    unsigned end_offset,
    Color underline_color,
    ui::mojom::ImeTextSpanThickness thickness,
    ui::mojom::ImeTextSpanUnderlineStyle underline_style,
    Color text_color,
    Color background_color)
    : StyleableMarker(start_offset,
                      end_offset,
                      underline_color,
                      thickness,
                      underline_style,
                      text_color,
                      background_color) {}

DocumentMarker::MarkerType CompositionMarker::GetType() const {
  return DocumentMarker::kComposition;
}

}  // namespace blink
