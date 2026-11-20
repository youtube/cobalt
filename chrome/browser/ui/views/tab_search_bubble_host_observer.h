// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_BUBBLE_HOST_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_BUBBLE_HOST_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

// This class has been temporarily factored out of
// TabSearchBubbleHost, in order to avoid a circular dependency against
// //c/b/ui:ui due to its usage by tab_strip_combo_button.h.
//
// TODO(crbug.com/369436587): Declare this class back as an inner class when:
// - tab_organization_observer.h
// - tab_slot_controller.h
// - tab_search_ui.h
// .. are componentized.
class TabSearchBubbleHostObserver : public base::CheckedObserver {
 public:
  virtual void OnBubbleInitializing() {}
  virtual void OnBubbleDestroying() {}
  virtual void OnHostDestroying() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_BUBBLE_HOST_OBSERVER_H_
