// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_PROPERTIES_H_
#define CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_PROPERTIES_H_

#include "ui/base/class_property.h"
#include "ui/base/window_open_disposition.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(WindowOpenDisposition)

namespace chrome {

extern const ui::ClassProperty<WindowOpenDisposition>* const kDispositionKey;

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_PROPERTIES_H_
