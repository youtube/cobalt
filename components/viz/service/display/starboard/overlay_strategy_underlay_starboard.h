// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_OVERLAY_STRATEGY_UNDERLAY_STARBOARD_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_OVERLAY_STRATEGY_UNDERLAY_STARBOARD_H_

#include <memory>
#include <vector>

#include "components/viz/service/display/overlay_strategy_underlay.h"

#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

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

  OverlayStrategy GetUMAEnum() const override;

  // For SbPlayer, OverlayStrategyUnderlayStarboard needs a valid mojo
  // interface to VideoGeometrySetter Service (shared by all instances of
  // OverlayStrategyUnderlayStarboard). This must be called before compositor
  // starts. Ideally, it can be called after compositor thread is created. Must
  // be called on compositor thread.
  static void ConnectVideoGeometrySetter(
      mojo::PendingRemote<cobalt::media::mojom::VideoGeometrySetter>
          video_geometry_setter);

 private:
  // Keep track if an overlay is being used on the previous frame.
  bool is_using_overlay_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_OVERLAY_STRATEGY_UNDERLAY_STARBOARD_H_
