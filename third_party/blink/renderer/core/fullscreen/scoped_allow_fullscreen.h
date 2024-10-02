// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FULLSCREEN_SCOPED_ALLOW_FULLSCREEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FULLSCREEN_SCOPED_ALLOW_FULLSCREEN_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT ScopedAllowFullscreen {
  STACK_ALLOCATED();

 public:
  enum Reason { kOrientationChange, kXrOverlay, kXrSession, kWindowOpen };

  static absl::optional<Reason> FullscreenAllowedReason();
  explicit ScopedAllowFullscreen(Reason);
  ScopedAllowFullscreen(const ScopedAllowFullscreen&) = delete;
  ScopedAllowFullscreen& operator=(const ScopedAllowFullscreen&) = delete;
  ~ScopedAllowFullscreen();

 private:
  static absl::optional<Reason> reason_;
  absl::optional<Reason> previous_reason_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FULLSCREEN_SCOPED_ALLOW_FULLSCREEN_H_
