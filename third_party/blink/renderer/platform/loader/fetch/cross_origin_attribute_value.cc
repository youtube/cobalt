// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"

#include "services/network/public/mojom/link_header.mojom-blink.h"

namespace blink {

CrossOriginAttributeValue CrossOriginAttributeToBlink(
    network::mojom::CrossOriginAttribute attr) {
  switch (attr) {
    case network::mojom::CrossOriginAttribute::kAnonymous:
      return kCrossOriginAttributeAnonymous;
    case network::mojom::CrossOriginAttribute::kUseCredentials:
      return kCrossOriginAttributeUseCredentials;
    case network::mojom::CrossOriginAttribute::kUnspecified:
      return kCrossOriginAttributeNotSet;
  }
}

}  // namespace blink
