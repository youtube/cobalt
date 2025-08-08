// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SWAP_COMPLETION_CALLBACK_H_
#define UI_OZONE_PUBLIC_SWAP_COMPLETION_CALLBACK_H_

#include "base/functional/callback.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/swap_result.h"

namespace gfx {
struct PresentationFeedback;
}  // namespace gfx

namespace ui {

using SwapCompletionOnceCallback =
    base::OnceCallback<void(gfx::SwapResult, gfx::GpuFenceHandle)>;

using PresentationOnceCallback =
    base::OnceCallback<void(const gfx::PresentationFeedback&)>;

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SWAP_COMPLETION_CALLBACK_H_
