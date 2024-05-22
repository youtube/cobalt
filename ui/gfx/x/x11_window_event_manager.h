// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_X11_WINDOW_EVENT_MANAGER_H_
#define UI_GFX_X_X11_WINDOW_EVENT_MANAGER_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/x/connection.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace x11 {

class XWindowEventManager;

// Ensures events in |event_mask| are selected on |window| for the duration of
// this object's lifetime.
class COMPONENT_EXPORT(X11) XScopedEventSelector {
 public:
  XScopedEventSelector(Window window, EventMask event_mask);

  XScopedEventSelector(const XScopedEventSelector&) = delete;
  XScopedEventSelector& operator=(const XScopedEventSelector&) = delete;

  ~XScopedEventSelector();

 private:
  Window window_;
  EventMask event_mask_;
  base::WeakPtr<XWindowEventManager> event_manager_;
};

// Allows multiple clients within Chrome to select events on the same X window.
class XWindowEventManager {
 public:
  static XWindowEventManager* GetInstance();

  XWindowEventManager(const XWindowEventManager&) = delete;
  XWindowEventManager& operator=(const XWindowEventManager&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<XWindowEventManager>;
  friend class XScopedEventSelector;

  class MultiMask;

  XWindowEventManager();
  ~XWindowEventManager();

  // Guarantees that events in |event_mask| will be reported to Chrome.
  void SelectEvents(Window window, EventMask event_mask);

  // Deselects events on |event_mask|.  Chrome will stop receiving events for
  // any set bit in |event_mask| only if no other client has selected that bit.
  void DeselectEvents(Window window, EventMask event_mask);

  // Helper method called by SelectEvents and DeselectEvents whenever the mask
  // corresponding to |window| might have changed.  Calls SetEventMask if
  // necessary.
  void AfterMaskChanged(Window window, EventMask old_mask);

  std::map<Window, std::unique_ptr<MultiMask>> mask_map_;

  // This is used to set XScopedEventSelector::event_manager_.  If |this| is
  // destroyed before any XScopedEventSelector, the |event_manager_| will become
  // invalidated.
  base::WeakPtrFactory<XWindowEventManager> weak_ptr_factory_{this};
};

}  // namespace x11

#endif  // UI_GFX_X_X11_WINDOW_EVENT_MANAGER_H_
