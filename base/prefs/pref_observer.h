// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_PREF_OBSERVER_H_
#define BASE_PREFS_PREF_OBSERVER_H_

#include <string>

class PrefServiceBase;

// TODO(joi): Switch to base::Callback and remove this.
class PrefObserver {
 public:
  virtual void OnPreferenceChanged(PrefServiceBase* service,
                                   const std::string& pref_name) = 0;
};

#endif  // BASE_PREFS_PREF_OBSERVER_H_
