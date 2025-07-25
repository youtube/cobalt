// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"

namespace blink {

void Path2D::Trace(Visitor* visitor) const {
  visitor->Trace(identifiability_study_helper_);
  visitor->Trace(context_);
  ScriptWrappable::Trace(visitor);
  CanvasPath::Trace(visitor);
}

}  // namespace blink
