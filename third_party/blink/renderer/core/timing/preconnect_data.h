// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PRECONNECT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PRECONNECT_DATA_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_cross_origin_mode.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// https://github.com/WICG/speculative_load_measurement
class PreconnectData final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PreconnectData(const String& origin,
                 CrossOriginAttributeValue crossorigin,
                 bool earlyhint);

  const String& origin() const { return origin_; }
  V8CrossOriginMode crossorigin() const;
  bool earlyhint() const { return earlyhint_; }

  void Trace(Visitor*) const override;

 private:
  // The serialized origin the connection was opened to.
  const String origin_;
  // The reflected crossorigin attribute value of the preconnect.
  const CrossOriginAttributeValue crossorigin_;
  // Whether the preconnect was delivered via an Early Hints response.
  const bool earlyhint_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PRECONNECT_DATA_H_
