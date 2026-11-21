// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/consent_kit/consent_kit_url_builder.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"
#include "components/contextual_search/consent_kit/proto/privacy_primitive_config.pb.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace drive {

namespace {

// Base URL for the embedded ConsentKit UI.
constexpr char kConsentKitBaseUrl[] =
    "https://consent.google.com/signedin/embedded/datasets";

}  // namespace

ConsentKitUrlBuilder::ConsentKitUrlBuilder() = default;
ConsentKitUrlBuilder::~ConsentKitUrlBuilder() = default;

void ConsentKitUrlBuilder::SetSessionIndex(int session_index) {
  session_index_ = session_index;
}

void ConsentKitUrlBuilder::SetLocale(const std::string& locale) {
  locale_ = locale;
}

void ConsentKitUrlBuilder::SetFlowId(int32_t flow_id) {
  flow_id_ = flow_id;
}

void ConsentKitUrlBuilder::SetProductId(int32_t product_id) {
  product_id_ = product_id;
}

void ConsentKitUrlBuilder::SetEntrypointId(const std::string& entrypoint_id) {
  entrypoint_id_ = entrypoint_id;
}

void ConsentKitUrlBuilder::SetHostOrigin(const std::string& host_origin) {
  host_origin_ = host_origin;
}

GURL ConsentKitUrlBuilder::Build() {
  identity_consent::PrivacyPrimitiveConfig config;

  config.mutable_presentation_params()->set_locale(locale_);
  config.mutable_web_platform_params();  // Ensures the oneof is set.

  config.mutable_flow_params()->set_flow_id(flow_id_);
  config.mutable_product_entry_point()->set_product_id(product_id_);
  config.mutable_product_entry_point()->set_entrypoint_id(entrypoint_id_);
  config.mutable_session_info()->set_session_id(
      base::Uuid::GenerateRandomV4().AsLowercaseString());

  std::string serialized_config;
  if (!config.SerializeToString(&serialized_config)) {
    DLOG(ERROR) << "Failed to serialize PrivacyPrimitiveConfig";
    return GURL();  // Invalid URL
  }

  std::string base64_config = base::Base64Encode(serialized_config);

  // --- Assemble URL ---
  GURL url(kConsentKitBaseUrl);

  // Required Parameters:
  url = net::AppendQueryParameter(url, "ppc", base64_config);
  url = net::AppendQueryParameter(url, "authuser",
                                  base::NumberToString(session_index_));
  url = net::AppendQueryParameter(url, "hl", locale_);
  url = net::AppendQueryParameter(url, "origin", host_origin_);

  return url;
}

}  // namespace drive
