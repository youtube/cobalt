// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_VIDEO_GEOMETRY_SETTER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_VIDEO_GEOMETRY_SETTER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace viz {

// Returns a persistent mojo::Remote that is then used by all the instances
// of OverlayStrategyUnderlayStarboard and CALayerOverlayProcessor.
mojo::Remote<cobalt::media::mojom::VideoGeometrySetter>&
GetVideoGeometrySetter();

// For SbPlayer, OverlayStrategyUnderlayStarboard and CALayerOverlayProcessor
// need a valid mojo interface to the VideoGeometrySetter service (shared by all
// instances). This must be called before the compositor starts. Ideally, it can
// be called after compositor thread is created. Must be called on compositor
// thread.
void ConnectVideoGeometrySetter(
    mojo::PendingRemote<cobalt::media::mojom::VideoGeometrySetter>
        video_geometry_setter);

// Collects the video holes punched while a frame is processed, and reports them
// to the VideoGeometrySetter service in one SetVideoGeometries() call when it
// goes out of scope. The service derives the stacking order (z-index) of the
// videos from the order of the holes, so they must be added front-most first,
// i.e., in QuadList order. Must be used on the compositor thread.
class VIZ_SERVICE_EXPORT ScopedVideoHoleGeometryBatch {
 public:
  ScopedVideoHoleGeometryBatch();

  ScopedVideoHoleGeometryBatch(const ScopedVideoHoleGeometryBatch&) = delete;
  ScopedVideoHoleGeometryBatch& operator=(const ScopedVideoHoleGeometryBatch&) =
      delete;

  ~ScopedVideoHoleGeometryBatch();

  // Adds a video hole to the innermost live batch. Without a live batch, the
  // video hole is reported on its own right away.
  static void Add(const base::UnguessableToken& overlay_plane_id,
                  const gfx::RectF& rect_f,
                  gfx::OverlayTransform transform);

 private:
  std::vector<cobalt::media::mojom::VideoHoleGeometryPtr> front_to_back_;
  const raw_ptr<ScopedVideoHoleGeometryBatch> previous_batch_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_VIDEO_GEOMETRY_SETTER_H_
