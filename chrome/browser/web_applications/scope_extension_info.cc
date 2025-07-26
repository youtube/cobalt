// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scope_extension_info.h"

#include <tuple>

#include "base/values.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"

namespace web_app {

base::Value ScopeExtensionInfo::AsDebugValue() const {
  base::Value::Dict root = base::Value::Dict()
                               .Set("origin", origin.GetDebugString())
                               .Set("scope", scope.possibly_invalid_spec())
                               .Set("has_origin_wildcard", has_origin_wildcard);
  return base::Value(std::move(root));
}

// static
ScopeExtensionInfo ScopeExtensionInfo::CreateForOrigin(
    url::Origin origin,
    bool has_origin_wildcard) {
  return ScopeExtensionInfo(origin, origin.GetURL(), has_origin_wildcard);
}

// static
ScopeExtensionInfo ScopeExtensionInfo::CreateForScope(
    GURL scope,
    bool has_origin_wildcard) {
  return ScopeExtensionInfo(url::Origin::Create(scope), scope,
                            has_origin_wildcard);
}

// static
ScopeExtensionInfo ScopeExtensionInfo::CreateForProto(
    const WebAppScopeExtensionProto& scope_extension_proto) {
  url::Origin origin =
      url::Origin::Create(GURL(scope_extension_proto.origin()));
  CHECK(!origin.opaque());
  CHECK(origin != url::Origin());
  CHECK(GURL(scope_extension_proto.scope()).is_valid());
  return ScopeExtensionInfo(std::move(origin),
                            GURL(scope_extension_proto.scope()),
                            scope_extension_proto.has_origin_wildcard());
}

ScopeExtensionInfo::ScopeExtensionInfo(url::Origin origin,
                                       GURL scope,
                                       bool has_origin_wildcard)
    : origin(origin), has_origin_wildcard(has_origin_wildcard) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  this->scope = scope.ReplaceComponents(replacements);
  CHECK(this->scope.is_valid());
}

void ScopeExtensionInfo::Reset() {
  origin = url::Origin();
  scope = GURL();
  has_origin_wildcard = false;
}

bool operator==(const ScopeExtensionInfo& scope_extension1,
                const ScopeExtensionInfo& scope_extension2) {
  return scope_extension1.origin == scope_extension2.origin &&
         scope_extension1.scope == scope_extension2.scope &&
         scope_extension1.has_origin_wildcard ==
             scope_extension2.has_origin_wildcard;
}

bool operator!=(const ScopeExtensionInfo& scope_extension1,
                const ScopeExtensionInfo& scope_extension2) {
  return !(scope_extension1 == scope_extension2);
}

bool operator<(const ScopeExtensionInfo& scope_extension1,
               const ScopeExtensionInfo& scope_extension2) {
  return std::tie(scope_extension1.origin, scope_extension1.scope,
                  scope_extension1.has_origin_wildcard) <
         std::tie(scope_extension2.origin, scope_extension2.scope,
                  scope_extension2.has_origin_wildcard);
}

}  // namespace web_app
