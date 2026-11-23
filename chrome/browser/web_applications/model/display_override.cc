// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/display_override.h"

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/to_value_list.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/web_applications/model/safe_url_pattern_to_value.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace web_app {

// static
DisplayOverride DisplayOverride::Create(
    blink::mojom::DisplayMode display_mode) {
  return DisplayOverride(display_mode, /*url_patterns=*/{});
}

// static
DisplayOverride DisplayOverride::CreateUnframed(
    std::vector<blink::SafeUrlPattern> url_patterns) {
  return DisplayOverride(blink::mojom::DisplayMode::kUnframed,
                         std::move(url_patterns));
}

DisplayOverride::DisplayOverride(
    blink::mojom::DisplayMode display_mode,
    std::vector<blink::SafeUrlPattern> url_patterns)
    : display_mode_(display_mode), url_patterns_(std::move(url_patterns)) {
  if (!url_patterns_.empty()) {
    CHECK_EQ(display_mode_, blink::mojom::DisplayMode::kUnframed);
  }
}

DisplayOverride::DisplayOverride(const DisplayOverride&) = default;
DisplayOverride::DisplayOverride(DisplayOverride&&) = default;

DisplayOverride::~DisplayOverride() = default;

DisplayOverride& DisplayOverride::operator=(const DisplayOverride&) = default;
DisplayOverride& DisplayOverride::operator=(DisplayOverride&&) = default;

bool DisplayOverride::operator==(const DisplayOverride& other) const = default;

// static
std::optional<DisplayOverride> DisplayOverride::Parse(
    const proto::WebApp::DisplayOverrideItem& proto) {
  if (!proto.has_display_mode()) {
    return std::nullopt;
  }
  blink::mojom::DisplayMode display_mode =
      ToMojomDisplayMode(proto.display_mode());
  std::optional<std::vector<blink::SafeUrlPattern>> url_patterns =
      ToUrlPatterns(proto.url_patterns());
  if (!url_patterns) {
    return std::nullopt;
  }
  if (!url_patterns->empty() &&
      display_mode != blink::mojom::DisplayMode::kUnframed) {
    return std::nullopt;
  }
  return DisplayOverride(display_mode, std::move(*url_patterns));
}

proto::WebApp::DisplayOverrideItem DisplayOverride::ToProto() const {
  proto::WebApp::DisplayOverrideItem proto;
  proto.set_display_mode(ToWebAppProtoDisplayMode(display_mode_));
  for (const auto& pattern : url_patterns_) {
    *proto.add_url_patterns() = ToUrlPatternProto(pattern);
  }
  return proto;
}

base::Value DisplayOverride::ToDebugValue() const {
  if (url_patterns_.empty()) {
    return base::Value(blink::DisplayModeToString(display_mode_));
  }
  base::DictValue item_dict;
  item_dict.Set("display", blink::DisplayModeToString(display_mode_));
  item_dict.Set("url_patterns", base::ToValueList(url_patterns_, ToValue));
  return base::Value(std::move(item_dict));
}

std::string DisplayOverride::ToString() const {
  return base::ToString(ToDebugValue());
}

std::ostream& operator<<(std::ostream& os,
                         const DisplayOverride& display_override) {
  return os << display_override.ToString();
}

}  // namespace web_app
