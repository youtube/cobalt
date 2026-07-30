// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_hash_report_body.h"

#include "third_party/blink/public/common/scheme_registry.h"

namespace blink {

bool CSPHashReportBody::IsExtensionSource() const {
  // In production, extension URLs are stripped to just their scheme by
  // ContentSecurityPolicy::StripURLForUseInReport. Thus, we can just check if
  // the subresource_url_ string is a registered extension scheme.
  return CommonSchemeRegistry::IsExtensionScheme(subresource_url_.Utf8());
}

void CSPHashReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  builder.AddStringOrNull("subresourceURL", subresource_url_);
  builder.AddStringOrNull("hash", hash_);
  builder.AddStringOrNull("type", type_);
  builder.AddStringOrNull("destination", destination_);
}

}  // namespace blink
