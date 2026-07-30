// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CROSS_ORIGIN_ATTRIBUTE_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CROSS_ORIGIN_ATTRIBUTE_VALUE_H_

#include "services/network/public/mojom/link_header.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// This corresponds to the CORS settings attributes defined in the HTML spec:
// https://html.spec.whatwg.org/C/#cors-settings-attributes
enum CrossOriginAttributeValue {
  kCrossOriginAttributeNotSet,
  kCrossOriginAttributeAnonymous,
  kCrossOriginAttributeUseCredentials,
};

// Converts a network::mojom::CrossOriginAttribute (e.g. as carried by Link
// headers / Early Hints) to the equivalent CrossOriginAttributeValue.
PLATFORM_EXPORT CrossOriginAttributeValue
CrossOriginAttributeToBlink(network::mojom::CrossOriginAttribute attr);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CROSS_ORIGIN_ATTRIBUTE_VALUE_H_
