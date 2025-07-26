// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAPTURE_MODE_LENS_OVERLAY_REQUEST_ID_GENERATOR_H_
#define CHROME_BROWSER_UI_ASH_CAPTURE_MODE_LENS_OVERLAY_REQUEST_ID_GENERATOR_H_

#include "third_party/lens_server_proto/lens_overlay_routing_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"

// The update modes for the request id generator.
enum class RequestIdUpdateMode {
  // Indicates that the request id should not be modified.
  kNone = 0,
  // Indicates that the request id should be modified for a full image request,
  // i.e. incrementing the image sequence, sequence id, and creating a new
  // analytics id.
  kFullImageRequest = 1,
  // Indicates that the request id should be modified for an interaction
  // request, i.e. incrementing the sequence id and creating a new analytics
  // id.
  kInteractionRequest = 2,
  // Indicates that the request id should be modified for a search url.
  // i.e. just incrementing the sequence id.
  kSearchUrl = 3,
};

// Manages creating lens overlay request IDs. Owned by a single Lens overlay
// query controller.
class LensOverlayRequestIdGenerator {
 public:
  LensOverlayRequestIdGenerator();
  LensOverlayRequestIdGenerator(const LensOverlayRequestIdGenerator&) = delete;
  LensOverlayRequestIdGenerator& operator=(
      const LensOverlayRequestIdGenerator&) = delete;
  ~LensOverlayRequestIdGenerator();

  // Resets the request id generator, creating a new uuid and resetting the
  // sequence.
  void ResetRequestId();

  // Updates the request id based on the given update mode and returns the
  // request id proto.
  std::unique_ptr<lens::LensOverlayRequestId> GetNextRequestId(
      RequestIdUpdateMode update_mode);

  // Returns the current analytics id as a base32 encoded string.
  std::string GetBase32EncodedAnalyticsId();

  // Sets the routing info to be included in the request id.
  void SetRoutingInfo(lens::LensOverlayRoutingInfo routing_info);

  bool HasRoutingInfo() { return routing_info_.has_value(); }

 private:
  // The current uuid. Valid for the duration of a Lens overlay session.
  uint64_t uuid_;

  // The analytics id for the current request. Will be updated on each
  // query.
  std::string analytics_id_;

  // The current sequence id.
  int sequence_id_;

  // The current image sequence id.
  int image_sequence_id_;

  // The current routing info. Not guaranteed to exist if not returned from the
  // server.
  std::optional<lens::LensOverlayRoutingInfo> routing_info_;
};

#endif  // CHROME_BROWSER_UI_ASH_CAPTURE_MODE_LENS_OVERLAY_REQUEST_ID_GENERATOR_H_
