// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/web_request/web_request_resource_type.h"

#include <array>
#include <string_view>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

struct ResourceTypes {
  const char* const name;
  const WebRequestResourceType type;
};
constexpr auto kResourceTypes = std::to_array<ResourceTypes>({
    {"main_frame", WebRequestResourceType::MAIN_FRAME},
    {"sub_frame", WebRequestResourceType::SUB_FRAME},
    {"stylesheet", WebRequestResourceType::STYLESHEET},
    {"script", WebRequestResourceType::SCRIPT},
    {"image", WebRequestResourceType::IMAGE},
    {"font", WebRequestResourceType::FONT},
    {"object", WebRequestResourceType::OBJECT},
    {"xmlhttprequest", WebRequestResourceType::XHR},
    {"ping", WebRequestResourceType::PING},
    {"csp_report", WebRequestResourceType::CSP_REPORT},
    {"media", WebRequestResourceType::MEDIA},
    {"websocket", WebRequestResourceType::WEB_SOCKET},
    {"webtransport", WebRequestResourceType::WEB_TRANSPORT},
    {"webbundle", WebRequestResourceType::WEBBUNDLE},
    {"other", WebRequestResourceType::OTHER},
});

constexpr size_t kResourceTypesLength = std::size(kResourceTypes);

static_assert(kResourceTypesLength ==
                  base::strict_cast<size_t>(WebRequestResourceType::OTHER) + 1,
              "Each WebRequestResourceType should have a string name.");

}  // namespace

const char* WebRequestResourceTypeToString(WebRequestResourceType type) {
  size_t index = base::strict_cast<size_t>(type);
  DCHECK_LT(index, kResourceTypesLength);
  DCHECK_EQ(kResourceTypes[index].type, type);
  return kResourceTypes[index].name;
}

bool ParseWebRequestResourceType(std::string_view text,
                                 WebRequestResourceType* type) {
  for (size_t i = 0; i < kResourceTypesLength; ++i) {
    if (text == kResourceTypes[i].name) {
      *type = kResourceTypes[i].type;
      DCHECK_EQ(static_cast<WebRequestResourceType>(i), *type);
      return true;
    }
  }
  return false;
}

}  // namespace extensions
