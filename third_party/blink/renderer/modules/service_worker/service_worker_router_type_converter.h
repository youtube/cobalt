// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_ROUTER_TYPE_CONVERTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_ROUTER_TYPE_CONVERTER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_router_rule.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class RouterRule;

// Convert V8 `RouterRule` to blink::ServiceWorkerRouterRule`
//
// Returns `absl::nullopt` on error, and `exception_state.HadException()`
// will be true.
// This is not a regular Mojo converter because we need `ExceptionState&`
// to tell errors.
MODULES_EXPORT absl::optional<ServiceWorkerRouterRule>
ConvertV8RouterRuleToBlink(const RouterRule* input,
                           const KURL& url_pattern_base_url,
                           ExceptionState& exception_state);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_ROUTER_TYPE_CONVERTER_H_
