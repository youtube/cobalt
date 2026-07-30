// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_ITERATOR_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_ITERATOR_H_

#include <vector>

#include "base/functional/function_ref.h"

class BrowserWindowInterface;

// Returns all browser windows.
// This is primarily used for features that need to operate on all browser
// windows at the same time. You should almost never be using this to find
// a specific browser window. There are some very rare exceptions, such as when
// you need to retrieve a browser window from an identifier or criteria when the
// caller is unassociated with that browser window (for instance, extensions
// modifying browser windows).
// Note that this doesn't account for any BrowserWindowInterfaces that are added
// or removed after the vector is returned.
std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces();

// For these 2 functions, we chose not to simply return
// std::vector<BrowserWindowInterface*> because doing so would open up the
// possibility that a browser could be created or destroyed during iteration,
// resulting in potential use-after-frees (example: crbug.com/405910169).
//
// The return value in the passed-in function indicates whether or not we should
// continue iterating - true means continue, false means terminate.
//
// Example usage:
//
//   ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
//       [](BrowserWindowInterface* browser_window) {
//         // do something with |browser_window|
//         return true;
//       });
//
// UNSAFE FOR "FIRST-MATCH" CONSUMERS: This helper iterates from the most
// recently activated browser to the least recently activated one. Activation
// order is runtime state that changes whenever the user focuses a window, so it
// is not an intrinsic property of the windows and is not stable over time. Do
// NOT use this helper to select a single window by terminating on the first
// match (e.g. returning false on the first window that satisfies a predicate)
// and treating it as "the" window: the result then depends on activation
// history, which leads to flaky, hard-to-reproduce behavior.
//
// Prefer a deterministic approach when you need to select a specific window:
//   * Use the BrowserCollection::ForEach() API (e.g. via
//     GlobalBrowserCollection::GetInstance()) and apply explicit selection
//     criteria based on intrinsic, order-independent properties (e.g. the
//     associated profile or session ID). When order matters, request a stable
//     order such as BrowserCollection::Order::kCreation, or
//   * If you only care whether a *particular* browser is active, check that
//     directly via `browser->GetWindow()->IsActive()`, or
//   * If you genuinely want the most recently active browser, use
//     GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser() in
//     chrome/browser/ui/browser_window/public/global_browser_collection.h,
//     which states that intent explicitly.
// Relying on activation order is appropriate only when the operation is applied
// to every window and the order is purely a presentation/iteration detail.
void ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
    base::FunctionRef<bool(BrowserWindowInterface*)> on_browser);

// Note here that any windows added during iteration may not remain in
// activation order.
//
// The "UNSAFE FOR FIRST-MATCH CONSUMERS" guidance documented above on
// ForEachCurrentBrowserWindowInterfaceOrderedByActivation() applies equally
// here: do not rely on activation order to select a single window.
void ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
    base::FunctionRef<bool(BrowserWindowInterface*)> on_browser);

// Returns the last active browser window interface. This can be nullptr if
// there are no browser windows.
// CAUTION: This can return a browser window with *any* profile. Please verify
// the profile.
// If you only care whether a *particular* browser is active, prefer checking
// that with `browser->GetWindow()->IsActive()`.
// DEPRECATED: Use
// GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser() in
// chrome/browser/ui/browser_window/public/global_browser_collection.h instead.
BrowserWindowInterface* GetLastActiveBrowserWindowInterfaceWithAnyProfile();

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_ITERATOR_H_
