// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_request_id_generator.h"

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "components/base32/base32.h"
#include "components/lens/lens_features.h"
#include "lens_overlay_request_id_generator.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"

namespace lens {

// The number of bytes to use in an analytics id.
constexpr size_t kAnalyticsIdBytesSize = 16;

LensOverlayRequestIdGenerator::LensOverlayRequestIdGenerator() {
  LensOverlayRequestIdGenerator::ResetRequestId();
}

LensOverlayRequestIdGenerator::~LensOverlayRequestIdGenerator() = default;

void LensOverlayRequestIdGenerator::ResetRequestId() {
  uuid_ = base::RandUint64();
  sequence_id_ = 0;
  image_sequence_id_ = 0;
  analytics_id_ = base::RandBytesAsString(kAnalyticsIdBytesSize);
  routing_info_.reset();
}

std::unique_ptr<lens::LensOverlayRequestId>
LensOverlayRequestIdGenerator::GetNextRequestId(
    RequestIdUpdateMode update_mode) {
  bool increment_image_sequence =
      update_mode == RequestIdUpdateMode::kFullImageRequest;
  bool increment_sequence = update_mode != RequestIdUpdateMode::kOpenInNewTab;
  bool create_analytics_id =
      update_mode != RequestIdUpdateMode::kSearchUrl &&
      update_mode != RequestIdUpdateMode::kPartialPageContentRequest;
  bool store_analytics_id = update_mode != RequestIdUpdateMode::kOpenInNewTab;

  // The server currently expects the image sequence id to be incremented for
  // every page content request. This is a temporary fix until the server
  // changes to index by sequence id instead of image sequence id.
  if (!lens::features::PageContentUploadRequestIdFixEnabled() &&
      update_mode == RequestIdUpdateMode::kPageContentRequest) {
    increment_image_sequence = true;
  }

  if (increment_image_sequence) {
    image_sequence_id_++;
  }
  if (increment_sequence) {
    sequence_id_++;
  }
  std::string analytics_id_to_set = analytics_id_;
  if (create_analytics_id) {
    analytics_id_to_set = base::RandBytesAsString(kAnalyticsIdBytesSize);
    if (store_analytics_id) {
      analytics_id_ = analytics_id_to_set;
    }
  }

  auto request_id = std::make_unique<lens::LensOverlayRequestId>();
  request_id->set_uuid(uuid_);
  request_id->set_sequence_id(sequence_id_);
  request_id->set_analytics_id(analytics_id_to_set);
  request_id->set_image_sequence_id(image_sequence_id_);
  if (routing_info_.has_value()) {
    request_id->mutable_routing_info()->CopyFrom(routing_info_.value());
  }
  return request_id;
}

std::string LensOverlayRequestIdGenerator::GetBase32EncodedAnalyticsId() {
  return base32::Base32Encode(base::as_byte_span(analytics_id_),
                              base32::Base32EncodePolicy::OMIT_PADDING);
}

void LensOverlayRequestIdGenerator::SetRoutingInfo(
    lens::LensOverlayRoutingInfo routing_info) {
  routing_info_ = routing_info;
}

}  // namespace lens
