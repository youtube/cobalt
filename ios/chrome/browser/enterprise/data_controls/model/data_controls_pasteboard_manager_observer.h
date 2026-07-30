// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_PASTEBOARD_MANAGER_OBSERVER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_PASTEBOARD_MANAGER_OBSERVER_H_

#import "base/observer_list_types.h"

namespace data_controls {

class DataControlsPasteboardManagerObserver : public base::CheckedObserver {
 public:
  virtual void OnPasteboardContentChanged() = 0;
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_PASTEBOARD_MANAGER_OBSERVER_H_
