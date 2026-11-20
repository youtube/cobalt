// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SOLID_COLOR_POOL_BASE_H_
#define UI_GL_SOLID_COLOR_POOL_BASE_H_

#include <unknwn.h>
#include <windows.h>

#include <dcomp.h>
#include <wrl/client.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/dc_commit_error.h"
#include "ui/gl/gl_export.h"

namespace gl {

// The size of the surface in each entry. This can be any size since we just
// need a non-empty surface to display the background fill, so 1x1 is most
// efficient.
inline constexpr gfx::Size kSolidColorSurfaceSize = gfx::Size(1, 1);

// Abstract resource pool that produces DComp content filled with a solid color.
// Owns all bookkeeping including per-frame stats, the used/free partitioned
// entry pool, the pool-trim policy, and the cache-by-color orchestration in
// `GetSolidColorContent`.
class GL_EXPORT SolidColorPoolBase {
 public:
  virtual ~SolidColorPoolBase();

  // Returns DComp content suitable for `IDCompositionVisual::SetContent`,
  // filled `color`. The returned pointer is owned by the pool and is only valid
  // until the next `TrimAfterCommit` call.
  base::expected<IUnknown*, CommitError> GetSolidColorContent(
      const SkColor4f& color);

  // Submit gpu work for any pending fills if necessary.
  virtual base::expected<void, CommitError> FlushPendingFillsBeforeCommit();

  // Clean up any unused resources in the pool. Trims the entry pool down to a
  // fixed retention cap, logs per-frame stats, and resets per-frame counters.
  void TrimAfterCommit();

  // Number of entries currently tracked.
  size_t GetNumEntriesForTesting() const;

 protected:
  // Container for the DComp visual content and its color.
  // TODO(crbug.com/526495997) Consider adding a resources member to `Entry` so
  // that we don't have to use inheritance with `SolidColorPoolBase` and
  // `Entry`.
  class GL_EXPORT Entry {
   public:
    Entry();
    virtual ~Entry();

    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;

    // The DComp content handed to `IDCompositionVisual::SetContent`.
    // Must be stable for the lifetime of this object.
    virtual IUnknown* GetContent() const = 0;

    // The color this entry was last successfully filled with, or `nullopt`
    // if the entry has never been filled or the last fill attempt failed.
    const std::optional<SkColor4f>& color() const { return color_; }

   private:
    friend class SolidColorPoolBase;

    // Owned by `SolidColorPoolBase`, reset before `FillEntry` and set on
    // successful return.
    std::optional<SkColor4f> color_;
  };

  SolidColorPoolBase();

  // Maximum number of entries (in-use + free) the pool retains across frames.
  // If the in-use count this frame already exceeds this cap, the cap is
  // ignored. The value is copied from gbm_surfaceless_wayland.cc's
  // `kMaxSolidColorBuffers`, which was picked based on observationally seeing
  // max 9 in-flight buffers + some margin. Too small thrashes allocations;
  // too large wastes memory.
  static constexpr size_t kMaxEntriesToRetain = 12;

  // Fills `entry` with `color`. `entry` is an in/out param which if non-null is
  // attempted to be refilled in place, and if null, is assigned a new `Entry`.
  // Before returning `ok()` the entry must be non-null and fully rendered with
  // `color`; on failure it must not be left partially filled.
  virtual base::expected<void, CommitError> FillEntry(
      std::unique_ptr<Entry>& entry,
      const SkColor4f& color) = 0;

  const std::vector<std::unique_ptr<Entry>>& entries() const {
    return entries_;
  }

  // The following members are per-frame counters, reset by `TrimAfterCommit`.
  //
  // Number of `GetSolidColorContent` calls (~= solid color overlays in the
  // frame).
  int num_requested_since_trim_ = 0;
  // Number of entries that were (re)filled this frame.
  int num_recolored_since_trim_ = 0;
  // Index that partitions tracked entries into "used this frame" (indices <
  // num_used_this_frame_) and "free this frame" (indices >=
  // num_used_this_frame_). Reset to 0 by `TrimAfterCommit`.
  size_t num_used_this_frame_ = 0;

 private:
  // A list with `num_used_this_frame_` 'used' elements followed by 'free'
  // elements.
  std::vector<std::unique_ptr<Entry>> entries_;
};

// Factory to obtain a content provider that is appropriate for the active
// Skia/Dawn backend.
using SolidColorPoolFactory =
    base::RepeatingCallback<std::unique_ptr<SolidColorPoolBase>(
        IDCompositionDevice3* dcomp_device)>;

}  // namespace gl

#endif  // UI_GL_SOLID_COLOR_POOL_BASE_H_
