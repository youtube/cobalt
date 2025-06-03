// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/viz_utils.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rrect_f.h"

#if BUILDFLAG(IS_ANDROID)
#include <array>
#include <string>

#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include <poll.h>
#include <sys/resource.h>
#endif

namespace viz {

bool PreferRGB565ResourcesForDisplay() {
#if BUILDFLAG(IS_ANDROID)
  return base::SysInfo::AmountOfPhysicalMemoryMB() <= 512;
#else
  return false;
#endif
}

#if BUILDFLAG(IS_ANDROID)
bool AlwaysUseWideColorGamut() {
  // Full stack integration tests draw in sRGB and expect to read back in sRGB.
  // WideColorGamut causes pixels to be drawn in P3, but read back doesn't tell
  // us the color space. So disable WCG for tests.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  static const char kDisableWCGForTest[] = "disable-wcg-for-test";
  if (command_line.HasSwitch(kDisableWCGForTest))
    return false;

  // As it takes some work to compute this, cache the result.
  static bool is_always_use_wide_color_gamut_enabled = [] {
    const char* current_model =
        base::android::BuildInfo::GetInstance()->model();
    const std::array<std::string, 2> enabled_models = {
        std::string{"Pixel 4"}, std::string{"Pixel 4 XL"}};
    for (const std::string& model : enabled_models) {
      if (model == current_model)
        return true;
    }

    return false;
  }();

  return is_always_use_wide_color_gamut_enabled;
}
#endif

bool GetScaledRegion(const gfx::Rect& rect,
                     const gfx::QuadF* clip,
                     gfx::QuadF* scaled_region) {
  if (!clip)
    return false;

  gfx::PointF p1(((clip->p1().x() - rect.x()) / rect.width()) - 0.5f,
                 ((clip->p1().y() - rect.y()) / rect.height()) - 0.5f);
  gfx::PointF p2(((clip->p2().x() - rect.x()) / rect.width()) - 0.5f,
                 ((clip->p2().y() - rect.y()) / rect.height()) - 0.5f);
  gfx::PointF p3(((clip->p3().x() - rect.x()) / rect.width()) - 0.5f,
                 ((clip->p3().y() - rect.y()) / rect.height()) - 0.5f);
  gfx::PointF p4(((clip->p4().x() - rect.x()) / rect.width()) - 0.5f,
                 ((clip->p4().y() - rect.y()) / rect.height()) - 0.5f);
  *scaled_region = gfx::QuadF(p1, p2, p3, p4);
  return true;
}

bool GetScaledRRectF(const gfx::Rect& space,
                     const gfx::RRectF& rect,
                     gfx::RRectF* scaled_rect) {
  float x_scale = 1.0f / space.width();
  float y_scale = 1.0f / space.height();
  float new_x = (rect.rect().x() - space.x()) * x_scale - 0.5f;
  float new_y = (rect.rect().y() - space.y()) * y_scale - 0.5f;
  *scaled_rect = rect;
  scaled_rect->Scale(x_scale, y_scale);
  scaled_rect->Offset(-scaled_rect->rect().origin().x(),
                      -scaled_rect->rect().origin().y());
  scaled_rect->Offset(new_x, new_y);
  return true;
}

bool GetScaledUVs(const gfx::Rect& rect, const gfx::QuadF* clip, float uvs[8]) {
  if (!clip)
    return false;

  uvs[0] = ((clip->p1().x() - rect.x()) / rect.width());
  uvs[1] = ((clip->p1().y() - rect.y()) / rect.height());
  uvs[2] = ((clip->p2().x() - rect.x()) / rect.width());
  uvs[3] = ((clip->p2().y() - rect.y()) / rect.height());
  uvs[4] = ((clip->p3().x() - rect.x()) / rect.width());
  uvs[5] = ((clip->p3().y() - rect.y()) / rect.height());
  uvs[6] = ((clip->p4().x() - rect.x()) / rect.width());
  uvs[7] = ((clip->p4().y() - rect.y()) / rect.height());
  return true;
}

bool GatherFDStats(base::TimeDelta* delta_time_taken,
                   int* fd_max,
                   int* active_fd_count,
                   int* rlim_cur) {
#if !BUILDFLAG(IS_POSIX)
  return false;
#else   // BUILDFLAG(IS_POSIX)
  // https://stackoverflow.com/questions/7976769/
  // getting-count-of-current-used-file-descriptors-from-c-code
  base::ElapsedTimer timer;
  rlimit limit_data;
  getrlimit(RLIMIT_NOFILE, &limit_data);
  std::vector<pollfd> poll_data;
  constexpr int kMaxNumFDTested = 1 << 16;
  // |rlim_cur| is the soft max but is likely the value we can rely on instead
  // of the real max.
  *rlim_cur = static_cast<int>(limit_data.rlim_cur);
  *fd_max = std::max(1, std::min(*rlim_cur, kMaxNumFDTested));
  poll_data.resize(*fd_max);
  for (size_t i = 0; i < poll_data.size(); i++) {
    auto& each = poll_data[i];
    each.fd = static_cast<int>(i);
    each.events = 0;
    each.revents = 0;
  }

  poll(poll_data.data(), poll_data.size(), 0);
  *active_fd_count = 0;
  for (auto&& each : poll_data) {
    if (each.revents != POLLNVAL)
      (*active_fd_count)++;
  }
  *delta_time_taken = timer.Elapsed();
  return true;
#endif  // BUILDFLAG(IS_POSIX)
}
gfx::RectF ClippedQuadRectangleF(const DrawQuad* quad) {
  gfx::RectF quad_rect = cc::MathUtil::MapClippedRect(
      quad->shared_quad_state->quad_to_target_transform,
      gfx::RectF(quad->rect));
  if (quad->shared_quad_state->clip_rect)
    quad_rect.Intersect(gfx::RectF(*quad->shared_quad_state->clip_rect));
  return quad_rect;
}

gfx::Rect ClippedQuadRectangle(const DrawQuad* quad) {
  return gfx::ToEnclosingRect(ClippedQuadRectangleF(quad));
}

gfx::Rect GetExpandedRectWithPixelMovingForegroundFilter(
    const DrawQuad& rpdq,
    const cc::FilterOperations& filters) {
  const SharedQuadState* shared_quad_state = rpdq.shared_quad_state;
  gfx::Rect expanded_rect = filters.ExpandRectForPixelMovement(rpdq.rect);

  // expanded_rect in the target space
  return cc::MathUtil::MapEnclosingClippedRect(
      shared_quad_state->quad_to_target_transform, expanded_rect);
}

gfx::Transform GetViewTransitionTransform(
    gfx::Rect shared_element_quad,
    gfx::Rect view_transition_content_output) {
  gfx::Transform view_transition_transform;

  view_transition_transform.Scale(
      shared_element_quad.width() /
          static_cast<SkScalar>(view_transition_content_output.width()),
      shared_element_quad.height() /
          static_cast<SkScalar>(view_transition_content_output.height()));

  view_transition_transform.Translate(-view_transition_content_output.x(),
                                      -view_transition_content_output.y());

  return view_transition_transform;
}

}  // namespace viz
