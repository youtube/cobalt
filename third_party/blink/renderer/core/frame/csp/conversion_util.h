// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CONVERSION_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CONVERSION_UTIL_H_

#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// Convert a ContentSecurityPolicy into a WebContentSecurityPolicy. These two
// classes represent the exact same thing, but one is public, the other is
// private.
// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
CORE_EXPORT
WebContentSecurityPolicy ConvertToPublic(
    network::mojom::blink::ContentSecurityPolicyPtr policy);

// Convert a WebContentSecurityPolicy into a ContentSecurityPolicy. These two
// classes represent the exact same thing, but one is in public, the other is
// private.
CORE_EXPORT
network::mojom::blink::ContentSecurityPolicyPtr ConvertToMojoBlink(
    const WebContentSecurityPolicy& policy);

// Helper function that applies ConvertToBlink above to a WebVector.
CORE_EXPORT
Vector<network::mojom::blink::ContentSecurityPolicyPtr> ConvertToMojoBlink(
    const WebVector<WebContentSecurityPolicy>& policy);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CONVERSION_UTIL_H_
