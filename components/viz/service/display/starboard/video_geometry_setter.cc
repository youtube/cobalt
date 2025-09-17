// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/starboard/video_geometry_setter.h"

#include "base/no_destructor.h"

namespace viz {

mojo::Remote<cobalt::media::mojom::VideoGeometrySetter>&
GetVideoGeometrySetter() {
  static base::NoDestructor<
      mojo::Remote<cobalt::media::mojom::VideoGeometrySetter>>
      g_video_geometry_setter;
  return *g_video_geometry_setter;
}

void ConnectVideoGeometrySetter(
    mojo::PendingRemote<cobalt::media::mojom::VideoGeometrySetter>
        video_geometry_setter) {
  GetVideoGeometrySetter().Bind(std::move(video_geometry_setter));
}

}  // namespace viz
