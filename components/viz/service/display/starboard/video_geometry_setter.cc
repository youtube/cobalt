// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/starboard/video_geometry_setter.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/no_destructor.h"

namespace viz {

namespace {

constinit thread_local ScopedVideoHoleGeometryBatch* current_batch = nullptr;

void SendVideoGeometries(
    std::vector<cobalt::media::mojom::VideoHoleGeometryPtr> front_to_back) {
  DCHECK(GetVideoGeometrySetter());
  GetVideoGeometrySetter()->SetVideoGeometries(std::move(front_to_back));
}

}  // namespace

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

ScopedVideoHoleGeometryBatch::ScopedVideoHoleGeometryBatch()
    : previous_batch_(std::exchange(current_batch, this)) {}

ScopedVideoHoleGeometryBatch::~ScopedVideoHoleGeometryBatch() {
  DCHECK_EQ(current_batch, this);
  current_batch = previous_batch_;
  if (!front_to_back_.empty()) {
    SendVideoGeometries(std::move(front_to_back_));
  }
}

// static
void ScopedVideoHoleGeometryBatch::Add(
    const base::UnguessableToken& overlay_plane_id,
    const gfx::RectF& rect_f,
    gfx::OverlayTransform transform) {
  auto video_hole = cobalt::media::mojom::VideoHoleGeometry::New(
      overlay_plane_id, rect_f, transform);
  if (current_batch) {
    current_batch->front_to_back_.push_back(std::move(video_hole));
    return;
  }
  std::vector<cobalt::media::mojom::VideoHoleGeometryPtr> front_to_back;
  front_to_back.push_back(std::move(video_hole));
  SendVideoGeometries(std::move(front_to_back));
}

}  // namespace viz
