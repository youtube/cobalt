// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "gpu/config/gpu_info_collector.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  TRACE_EVENT0("gpu", "gpu_info_collector::CollectGraphicsInfo");

#if BUILDFLAG(IS_CASTOS)
  // Chromecast doesn't go through CollectBasicGraphicsInfo() step, so
  // this is the only place we need to set machine model.
  // TODO(zmo): maybe distinguish different Chromecast products.
  gpu_info->machine_model_name = "Chromecast";
#endif  // BUILDFLAG(IS_CASTOS)

  return CollectGraphicsInfoGL(gpu_info, gl::GetDefaultDisplay());
}

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  angle::SystemInfo system_info;
  bool success = angle::GetSystemInfo(&system_info);
  FillGPUInfoFromSystemInfo(gpu_info, &system_info);
  return success;
}

}  // namespace gpu
