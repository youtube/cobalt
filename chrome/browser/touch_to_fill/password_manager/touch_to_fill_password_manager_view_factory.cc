// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_password_manager_view_factory.h"

#include "chrome/browser/touch_to_fill/password_manager/android/touch_to_fill_password_manager_view_impl.h"

// static
std::unique_ptr<TouchToFillPasswordManagerView>
TouchToFillPasswordManagerViewFactory::Create(
    TouchToFillPasswordManagerController* controller) {
  return std::make_unique<TouchToFillPasswordManagerViewImpl>(controller);
}
