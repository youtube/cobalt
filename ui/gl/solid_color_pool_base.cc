// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/solid_color_pool_base.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/types/expected_macros.h"

namespace gl {

SolidColorPoolBase::Entry::Entry() = default;
SolidColorPoolBase::Entry::~Entry() = default;

SolidColorPoolBase::SolidColorPoolBase() = default;

SolidColorPoolBase::~SolidColorPoolBase() = default;

base::expected<IUnknown*, CommitError> SolidColorPoolBase::GetSolidColorContent(
    const SkColor4f& color) {
  num_requested_since_trim_ += 1;

  auto first_unused_it = std::next(entries_.begin(), num_used_this_frame_);

  // Look for an existing entry already filled with `color`.
  const std::optional<SkColor4f> target_color(color);
  if (auto found_color_it = std::ranges::find(
          entries_, target_color,
          [](const std::unique_ptr<Entry>& entry)
              -> const std::optional<SkColor4f>& { return entry->color(); });
      found_color_it != entries_.end()) {
    // Found an existing entry that already has the requested color.
    if (found_color_it >= first_unused_it) {
      // If the entry is in the "unused" portion of `entries_`, make it be
      // tracked now.
      std::swap(*first_unused_it, *found_color_it);
      found_color_it = first_unused_it;
      num_used_this_frame_++;
    } else {
      // The entry is already used by another overlay in this frame, so we can
      // just share it with no extra work.
    }
    return (*found_color_it)->GetContent();
  }

  // There is no entry that already contains the requested `color`, so we'll
  // need to fill one:
  //  - Reuse path: there's a free-partition entry we can refill in place. The
  //  entry is already in the pool. We invalidate its cached color before
  //  `FillEntry` so a failure can't yield a stale cache hit on a future call.
  //  - Fresh-alloc path: pass a null `unique_ptr` to `FillEntry`. The new entry
  //  only becomes visible to the pool after `FillEntry` succeeds, so a failure
  //  leaves the pool identical to its pre-call state.
  if (first_unused_it != entries_.end()) {
    (*first_unused_it)->color_.reset();
    RETURN_IF_ERROR(FillEntry(*first_unused_it, color));
    (*first_unused_it)->color_ = color;
  } else {
    std::unique_ptr<Entry> fresh;
    RETURN_IF_ERROR(FillEntry(fresh, color));
    CHECK(fresh);
    fresh->color_ = color;
    entries_.push_back(std::move(fresh));
    first_unused_it = std::prev(entries_.end());
  }

  ++num_used_this_frame_;
  ++num_recolored_since_trim_;
  return (*first_unused_it)->GetContent();
}

base::expected<void, CommitError>
SolidColorPoolBase::FlushPendingFillsBeforeCommit() {
  return base::ok();
}

void SolidColorPoolBase::TrimAfterCommit() {
  const size_t num_tracked = entries_.size();
  // Preserve up to `kMaxEntriesToRetain` entries, even if they aren't used
  // this frame. If the in-use count already exceeds the cap, the cap is
  // ignored.
  size_t trim_target_size = std::max(num_used_this_frame_, kMaxEntriesToRetain);
  trim_target_size = std::min(trim_target_size, num_tracked);

  DVLOG(3) << "SolidColorPool stats before trim: "
           << "requested=" << num_requested_since_trim_
           << ", recolored=" << num_recolored_since_trim_
           << ", in-use/total=" << num_used_this_frame_ << "/" << num_tracked
           << (num_used_this_frame_ > kMaxEntriesToRetain
                   ? " (in-use exceeds kMaxEntriesToRetain)"
                   : "")
           << ", will trim to " << trim_target_size;

  entries_.erase(std::next(entries_.begin(), trim_target_size), entries_.end());

  num_used_this_frame_ = 0;
  num_requested_since_trim_ = 0;
  num_recolored_since_trim_ = 0;
}

size_t SolidColorPoolBase::GetNumEntriesForTesting() const {
  CHECK_IS_TEST();
  return entries_.size();
}

}  // namespace gl
