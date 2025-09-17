// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_VIDEO_GEOMETRY_SETTER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_VIDEO_GEOMETRY_SETTER_H_

#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

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

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_STARBOARD_VIDEO_GEOMETRY_SETTER_H_
