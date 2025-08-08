// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_APPEND_QUADS_DATA_H_
#define CC_LAYERS_APPEND_QUADS_DATA_H_

#include <stdint.h>
#include <vector>

#include "cc/cc_export.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cc {

// Set by the layer appending quads.
class CC_EXPORT AppendQuadsData {
 public:
  AppendQuadsData();
  ~AppendQuadsData();

  int64_t num_incomplete_tiles = 0;
  int64_t num_missing_tiles = 0;
  int64_t visible_layer_area = 0;
  int64_t approximated_visible_content_area = 0;

  // This is total of the following two areas.
  int64_t checkerboarded_visible_content_area = 0;
  // This is the area outside interest rect.
  int64_t checkerboarded_no_recording_content_area = 0;
  // This is the area within interest rect.
  int64_t checkerboarded_needs_raster_content_area = 0;

  // The non-default number of BeginFrames to wait before forcibly activating
  // this CompositorFrame.
  absl::optional<uint32_t> deadline_in_frames;

  // Indicates whether or not one of the layers wants to use the default
  // activation deadline.
  bool use_default_lower_bound_deadline = false;

  // This is the set of surface IDs that must have corresponding
  // active CompositorFrames so that this CompositorFrame can
  // activate.
  std::vector<viz::SurfaceId> activation_dependencies;

  // Indicates if any layer has ViewTransitionElementResourceIds which need to
  // be swapped with actual viz::ResourceIds in the Viz process.
  bool has_shared_element_resources = false;
};

}  // namespace cc
#endif  // CC_LAYERS_APPEND_QUADS_DATA_H_
