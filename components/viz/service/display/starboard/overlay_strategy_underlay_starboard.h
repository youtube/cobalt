// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_OVERLAY_STRATEGY_UNDERLAY_STARBOARD_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_OVERLAY_STRATEGY_UNDERLAY_STARBOARD_H_

#include <memory>
#include <vector>

#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
// The underlay strategy looks for a video hole quad, where the underlay
// video can be displayed directly. This is similar to the underlay
// strategy for Cast.
class VIZ_SERVICE_EXPORT OverlayStrategyUnderlayStarboard
    : public OverlayStrategyUnderlay {
 public:
  explicit OverlayStrategyUnderlayStarboard(
      OverlayProcessorUsingStrategy* capability_checker);

  OverlayStrategyUnderlayStarboard(const OverlayStrategyUnderlayStarboard&) =
      delete;
  OverlayStrategyUnderlayStarboard& operator=(
      const OverlayStrategyUnderlayStarboard&) = delete;

  ~OverlayStrategyUnderlayStarboard() override;

  void Propose(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      const PrimaryPlane* primary_plane,
      std::vector<OverlayProposedCandidate>* candidates,
      std::vector<gfx::Rect>* content_bounds) override;

  bool Attempt(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      const PrimaryPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds,
      const OverlayProposedCandidate& proposed_candidate) override;

  void CommitCandidate(const OverlayProposedCandidate& proposed_candidate,
                       AggregatedRenderPass* render_pass) override;

  void AdjustOutputSurfaceOverlay(
      OverlayProcessorInterface::OutputSurfaceOverlayPlane*
          output_surface_plane) override;

 private:
  // Keep track if an overlay is being used on the previous frame.
  bool is_using_overlay_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_OVERLAY_STRATEGY_UNDERLAY_STARBOARD_H_
