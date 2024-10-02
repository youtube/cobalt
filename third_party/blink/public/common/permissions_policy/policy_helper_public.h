// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_POLICY_HELPER_PUBLIC_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_POLICY_HELPER_PUBLIC_H_

#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"

namespace blink {

using PermissionsPolicyFeatureToNameMap =
    base::flat_map<mojom::PermissionsPolicyFeature, base::StringPiece>;

using PermissionsPolicyNameToFeatureMap =
    base::flat_map<base::StringPiece, mojom::PermissionsPolicyFeature>;

// This method defines the feature names which will be recognized by the parser
// for the Permissions-Policy HTTP header and the <iframe> "allow" attribute, as
// well as the features which will be recognized by the document or iframe
// policy object.
const PermissionsPolicyNameToFeatureMap& GetPermissionsPolicyNameToFeatureMap();

// This method returns an inverted version of the map returned by
// GetPermissionsPolicyNameToFeatureMap() so that users can look up the string
// representation of a feature by it's mojo enum value.
const PermissionsPolicyFeatureToNameMap& GetPermissionsPolicyFeatureToNameMap();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_POLICY_HELPER_PUBLIC_H_
