// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_COMPOSITING_LAYER_TREE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_COMPOSITING_LAYER_TREE_SETTINGS_H_

#include "build/build_config.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

PLATFORM_EXPORT cc::ManagedMemoryPolicy GetGpuMemoryPolicy(
    const cc::ManagedMemoryPolicy& default_policy,
    const gfx::Size& initial_screen_size,
    float initial_device_scale_factor);

#if BUILDFLAG(IS_COBALT)
// Returns Cobalt's default GPU tile memory budget, in MB, for devices where the
// budget is capped by default (32-bit, and 64-bit Android with a 96 MB floor).
// The budget steps up with the UI resolution in physical pixels (64 MB up to
// 1080p, 128 MB up to 1440p, 256 MB above) and is capped at 1/8 of the device's
// physical memory, but never below 64 MB. |physical_memory_mb| <= 0 means
// unknown.
PLATFORM_EXPORT size_t
GetCobaltDefaultGpuMemoryLimitMB(const gfx::Size& screen_size,
                                 float device_scale_factor,
                                 int physical_memory_mb);
#endif  // BUILDFLAG(IS_COBALT)

cc::LayerTreeSettings GenerateLayerTreeSettings(
    bool has_compositor,
    bool is_for_embedded_frame,
    bool is_for_scalable_page,
    const gfx::Size& initial_screen_size,
    float initial_device_scale_factor);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_COMPOSITING_LAYER_TREE_SETTINGS_H_
