// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/preconnect_data.h"

#include "third_party/blink/renderer/core/timing/cross_origin_mode_converter.h"

namespace blink {

PreconnectData::PreconnectData(const String& origin,
                               CrossOriginAttributeValue crossorigin,
                               bool earlyhint)
    : origin_(origin), crossorigin_(crossorigin), earlyhint_(earlyhint) {}

V8CrossOriginMode PreconnectData::crossorigin() const {
  return ToV8CrossOriginMode(crossorigin_);
}

void PreconnectData::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
