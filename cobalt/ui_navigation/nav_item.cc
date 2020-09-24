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
std::vector<base::Closure>* g_pending_updates = nullptr;

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

// This processes all pending updates assuming the update spinlock is locked.
void ProcessPendingChangesLocked(void* /* context */) {
  for (size_t i = 0; i < g_pending_updates->size(); ++i) {
    (*g_pending_updates)[i].Run();
  }
  g_pending_updates->clear();
  if (g_pending_focus != kNativeItemInvalid) {
    GetInterface().set_focus(g_pending_focus);
    g_pending_focus = kNativeItemInvalid;
  }
}

void ProcessPendingChanges(void* context) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  ProcessPendingChangesLocked(context);
}
}  // namespace

NativeCallbacks NavItem::s_callbacks_ = {
  &NavItem::OnBlur,
  &NavItem::OnFocus,
  &NavItem::OnScroll,
};

NavItem::NavItem(NativeItemType type,
                 const base::Closure& onblur_callback,
                 const base::Closure& onfocus_callback,
                 const base::Closure& onscroll_callback)
    : onblur_callback_(onblur_callback),
      onfocus_callback_(onfocus_callback),
      onscroll_callback_(onscroll_callback),
      nav_item_type_(type),
      nav_item_(GetInterface().create_item(type, &s_callbacks_, this)),
      enabled_(false) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  if (++g_nav_item_count == 1) {
    g_pending_updates = new std::vector<base::Closure>();
    g_pending_focus = kNativeItemInvalid;
  }
}

NavItem::~NavItem() {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  GetInterface().set_item_enabled(nav_item_, false);
  if (g_pending_focus == nav_item_) {
    g_pending_focus = kNativeItemInvalid;
  }
  g_pending_updates->emplace_back(
      base::Bind(GetInterface().destroy_item, nav_item_));
  if (--g_nav_item_count == 0) {
    ProcessPendingChangesLocked(nullptr);
    delete g_pending_updates;
    g_pending_updates = nullptr;
  }
}

void NavItem::PerformQueuedUpdates() {
  GetInterface().do_batch_update(&ProcessPendingChanges, nullptr);
}

void NavItem::Focus() {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  if (enabled_) {
    g_pending_focus = nav_item_;
  }
}

void NavItem::UnfocusAll() {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  g_pending_focus = kNativeItemInvalid;
#if SB_API_VERSION >= SB_UI_NAVIGATION2_VERSION
  g_pending_updates->emplace_back(
      base::Bind(GetInterface().set_focus, kNativeItemInvalid));
#endif
}

void NavItem::SetEnabled(bool enabled) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  enabled_ = enabled;
  if (enabled) {
    g_pending_updates->emplace_back(
        base::Bind(GetInterface().set_item_enabled, nav_item_, enabled));
  } else {
    // Disable immediately to avoid errant callbacks on this item.
    GetInterface().set_item_enabled(nav_item_, enabled);
    if (g_pending_focus == nav_item_) {
      g_pending_focus = kNativeItemInvalid;
    }
  }
}

void NavItem::SetDir(NativeItemDir dir) {
  // Do immediately in case it impacts content offset queries.
  GetInterface().set_item_dir(nav_item_, dir);
}

void NavItem::SetSize(float width, float height) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  g_pending_updates->emplace_back(
      base::Bind(GetInterface().set_item_size, nav_item_, width, height));
}

void NavItem::SetTransform(const NativeMatrix2x3* transform) {
  starboard::ScopedSpinLock lock(&g_pending_updates_lock);
  g_pending_updates->emplace_back(
      base::Bind(&SetTransformHelper, nav_item_, *transform));
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
void NavItem::OnBlur(NativeItem item, void* callback_context) {
  NavItem* this_ptr = static_cast<NavItem*>(callback_context);
  if (!this_ptr->onblur_callback_.is_null()) {
    this_ptr->onblur_callback_.Run();
  }
}

// static
void NavItem::OnFocus(NativeItem item, void* callback_context) {
  NavItem* this_ptr = static_cast<NavItem*>(callback_context);
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
