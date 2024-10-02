// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TOAST_MANAGER_H_
#define ASH_PUBLIC_CPP_SYSTEM_TOAST_MANAGER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

struct ToastData;

// Public interface to show toasts.
class ASH_PUBLIC_EXPORT ToastManager {
 public:
  static ToastManager* Get();

  // Show a toast. If there are queued toasts, succeeding toasts are queued as
  // well, and are shown one by one.
  virtual void Show(ToastData data) = 0;

  // Cancels a toast with the provided ID.
  virtual void Cancel(const std::string& id) = 0;

  // Toggles highlight on the dismiss button for an active toast. Returns false
  // if this is not possible or if the highlight has been removed from the
  // button.
  virtual bool MaybeToggleA11yHighlightOnActiveToastDismissButton(
      const std::string& id) = 0;

  // Activates the dismiss button on the active toast if there is a button and
  // that button is highlighted. Returns false if either case is not true, or if
  // there is no active toast.
  virtual bool MaybeActivateHighlightedDismissButtonOnActiveToast(
      const std::string& id) = 0;

  // Tells if the toast with the provided ID is running.
  virtual bool IsRunning(const std::string& id) const = 0;

 protected:
  ToastManager();
  virtual ~ToastManager();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TOAST_MANAGER_H_
