// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/ui_navigation/nav_item.h"

#include <vector>

#include "base/bind.h"
#include "starboard/atomic.h"
#include "starboard/common/spin_lock.h"

namespace cobalt {
namespace ui_navigation {

namespace {
// These data structures support queuing of UI navigation changes so they can
// be executed as a batch to atomically update the UI.
SbAtomic32 g_pending_updates_lock(starboard::kSpinLockStateReleased);

struct PendingUpdate {
  PendingUpdate(NativeItem nav_item, const base::Closure& closure)
      : nav_item(nav_item), closure(closure) {}
  NativeItem nav_item;
  base::Closure closure;
};
std::vector<PendingUpdate>* g_pending_updates = nullptr;

// Pending focus change should always be done after all updates. This ensures
// the focus target is up-to-date.
volatile NativeItem g_pending_focus = kNativeItemInvalid;

// This tracks the total number of NavItems. It is used to control allocation
// and deletion of data for queued updates.
int32_t g_nav_item_count = 0;

// This helper function is necessary to preserve the |transform| by value.
void SetTransformHelper(NativeItem native_item, NativeMatrix2x3 transform) {
  GetInterface().set_item_transform(native_item, &transform);
}

// This processes all pending updates specified by the given context. The update
// list should already be locked from modifications by other threads.
void ProcessPendingChanges(void* context) {
  std::vector<PendingUpdate>* updates =
      static_cast<std::vector<PendingUpdate>*>(context);
  for (size_t i = 0; i < updates->size(); ++i) {
    (*updates)[i].closure.Run();
  }
  updates->clear();
}

// Remove any pending changes associated with the specified item.
void RemovePendingChangesLocked(NativeItem nav_item) {
  DCHECK(nav_item != kNativeItemInvalid);

  for (size_t i = 0; i < g_pending_updates->size();) {
    if ((*g_pending_updates)[i].nav_item == nav_item) {
      g_pending_updates->erase(g_pending_updates->begin() + i);
      continue;
    }
    ++i;
  }

  if (g_pending_focus == nav_item) {
    g_pending_focus = kNativeItemInvalid;
  }
}

}  // namespace

// Use an atomic to track the currently-focused item. Don't use a mutex or
// spinlock in GetGlobalFocusTransform() / GetGlobalFocusVector() and
// OnBlur() / OnFocus() since this can result in a deadlock if the starboard
// implementation uses a lock of its own for the NativeItem.
SbAtomicPtr NavItem::s_focused_nav_item_(
    reinterpret_cast<intptr_t>(kNativeItemInvalid));

NativeCallbacks NavItem::s_callbacks_ = {
    &NavItem::OnBlur, &NavItem::OnFocus, &NavItem::OnScroll,
};

NavItem::NavItem(NativeItemType type, const base::Closure& onblur_callback,
                 const base::Closure& onfocus_callback,
                 const base::Closure& onscroll_callback)
    : onblur_callback_(onblur_callback),
      onfocus_callback_(onfocus_callback),
      onscroll_callback_(onscroll_callback),
      nav_item_type_(type),
      nav_item_(GetInterface().create_item(type, &s_callbacks_, this)),
      state_(kStateNew) {
  static_assert(sizeof(s_focused_nav_item_) == sizeof(NativeItem),
                "Size mismatch");
  static_assert(sizeof(s_focused_nav_item_) == sizeof(intptr_t),
                "Size mismatch");
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  if (++g_nav_item_count == 1) {
    g_pending_updates = new std::vector<PendingUpdate>();
    g_pending_focus = kNativeItemInvalid;
  }
}

NavItem::~NavItem() {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  DCHECK(state_ == kStatePendingDelete);
  DCHECK(SbAtomicNoBarrier_LoadPtr(&s_focused_nav_item_) !=
         reinterpret_cast<intptr_t>(nav_item_));
  g_pending_updates->emplace_back(
      nav_item_, base::Bind(GetInterface().destroy_item, nav_item_));
  if (--g_nav_item_count == 0) {
    DCHECK(g_pending_focus == kNativeItemInvalid);
    ProcessPendingChanges(g_pending_updates);
    delete g_pending_updates;
    g_pending_updates = nullptr;
  }
}

void NavItem::PerformQueuedUpdates() {
  std::vector<PendingUpdate> updates_snapshot;

  // Process only the updates that have been queued up to this point. Other
  // threads may be queueing more updates, and we shouldn't pick them up in this
  // batch as that may result in them getting processed before async tasks that
  // may be done by the platform.
  {
    starboard::ScopedSpinLock lock(&g_pending_updates_lock);
    updates_snapshot.swap(*g_pending_updates);
    if (g_pending_focus != kNativeItemInvalid) {
      updates_snapshot.emplace_back(
          g_pending_focus,
          base::Bind(GetInterface().set_focus, g_pending_focus));
      g_pending_focus = kNativeItemInvalid;
    }
  }

  GetInterface().do_batch_update(&ProcessPendingChanges, &updates_snapshot);
}

void NavItem::Focus() {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  if (state_ == kStateEnabled) {
    if (g_pending_updates->empty()) {
      // Immediately update focus if nothing else is queued for update.
      g_pending_focus = kNativeItemInvalid;
      GetInterface().set_focus(nav_item_);
    } else {
      g_pending_focus = nav_item_;
    }
  }
}

void NavItem::UnfocusAll() {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  g_pending_focus = kNativeItemInvalid;
#if SB_API_VERSION >= 13
  g_pending_updates->emplace_back(
      kNativeItemInvalid,
      base::Bind(GetInterface().set_focus, kNativeItemInvalid));
#endif
}

void NavItem::SetEnabled(bool enabled) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  if (enabled) {
    if (state_ == kStateNew) {
      state_ = kStateEnabled;
      g_pending_updates->emplace_back(
          nav_item_,
          base::Bind(GetInterface().set_item_enabled, nav_item_, enabled));
    }
  } else {
    if (state_ != kStatePendingDelete) {
      state_ = kStatePendingDelete;
      // Remove any pending changes associated with this item.
      RemovePendingChangesLocked(nav_item_);
      // Disable immediately to avoid errant callbacks on this item.
      GetInterface().set_item_enabled(nav_item_, enabled);
    }
    SbAtomicNoBarrier_CompareAndSwapPtr(
        &s_focused_nav_item_, reinterpret_cast<intptr_t>(nav_item_),
        reinterpret_cast<intptr_t>(kNativeItemInvalid));
  }
}

void NavItem::SetDir(NativeItemDir dir) {
  // Do immediately in case it impacts content offset queries.
  GetInterface().set_item_dir(nav_item_, dir);
}

void NavItem::SetFocusDuration(float seconds) {
  GetInterface().set_item_focus_duration(nav_item_, seconds);
}

void NavItem::SetSize(float width, float height) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  g_pending_updates->emplace_back(
      nav_item_,
      base::Bind(GetInterface().set_item_size, nav_item_, width, height));
}

void NavItem::SetTransform(const NativeMatrix2x3* transform) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  g_pending_updates->emplace_back(
      nav_item_, base::Bind(&SetTransformHelper, nav_item_, *transform));
}

bool NavItem::GetFocusTransform(NativeMatrix4* out_transform) {
  return GetInterface().get_item_focus_transform(nav_item_, out_transform);
}

bool NavItem::GetFocusVector(float* out_x, float* out_y) {
  return GetInterface().get_item_focus_vector(nav_item_, out_x, out_y);
}

void NavItem::SetContainerWindow(SbWindow window) {
  // Do immediately since the lifetime of |window| is not guaranteed after this
  // call.
  GetInterface().set_item_container_window(nav_item_, window);
}

void NavItem::SetContainerItem(const scoped_refptr<NavItem>& container) {
  // Set the container immediately so that subsequent calls to GetContainerItem
  // are correct. However, the actual nav_item_ change should still be queued.
  container_ = container;
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  g_pending_updates->emplace_back(
      nav_item_,
      base::Bind(GetInterface().set_item_container_item, nav_item_,
                 container ? container->nav_item_ : kNativeItemInvalid));
}

const scoped_refptr<NavItem>& NavItem::GetContainerItem() const {
  return container_;
}

void NavItem::SetContentOffset(float x, float y) {
  // Do immediately since content offset may be queried immediately after.
  GetInterface().set_item_content_offset(nav_item_, x, y);
}

void NavItem::GetContentOffset(float* out_x, float* out_y) {
  GetInterface().get_item_content_offset(nav_item_, out_x, out_y);
}

// static
bool NavItem::GetGlobalFocusTransform(NativeMatrix4* out_transform) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  NativeItem focused_item = reinterpret_cast<NativeItem>(
      SbAtomicNoBarrier_LoadPtr(&s_focused_nav_item_));
  if (focused_item == kNativeItemInvalid) {
    return false;
  }
  return GetInterface().get_item_focus_transform(focused_item, out_transform);
}

// static
bool NavItem::GetGlobalFocusVector(float* out_x, float* out_y) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  NativeItem focused_item = reinterpret_cast<NativeItem>(
      SbAtomicNoBarrier_LoadPtr(&s_focused_nav_item_));
  if (focused_item == kNativeItemInvalid) {
    return false;
  }
  return GetInterface().get_item_focus_vector(focused_item, out_x, out_y);
}

// static
void NavItem::OnBlur(NativeItem item, void* callback_context) {
  NavItem* this_ptr = static_cast<NavItem*>(callback_context);
  SbAtomicNoBarrier_CompareAndSwapPtr(
      &s_focused_nav_item_, reinterpret_cast<intptr_t>(this_ptr->nav_item_),
      reinterpret_cast<intptr_t>(kNativeItemInvalid));
  if (!this_ptr->onblur_callback_.is_null()) {
    this_ptr->onblur_callback_.Run();
  }
}

// static
void NavItem::OnFocus(NativeItem item, void* callback_context) {
  NavItem* this_ptr = static_cast<NavItem*>(callback_context);
  SbAtomicNoBarrier_StorePtr(&s_focused_nav_item_,
                             reinterpret_cast<intptr_t>(this_ptr->nav_item_));
  if (!this_ptr->onfocus_callback_.is_null()) {
    this_ptr->onfocus_callback_.Run();
  }
}

// static
void NavItem::OnScroll(NativeItem item, void* callback_context) {
  NavItem* this_ptr = static_cast<NavItem*>(callback_context);
  if (!this_ptr->onscroll_callback_.is_null()) {
    this_ptr->onscroll_callback_.Run();
  }
}

}  // namespace ui_navigation
}  // namespace cobalt
