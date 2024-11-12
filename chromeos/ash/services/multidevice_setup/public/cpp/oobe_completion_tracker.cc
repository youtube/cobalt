// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"

namespace ash {

namespace multidevice_setup {

OobeCompletionTracker::OobeCompletionTracker() = default;

OobeCompletionTracker::~OobeCompletionTracker() = default;

void OobeCompletionTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OobeCompletionTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void OobeCompletionTracker::MarkOobeShown() {
  for (auto& observer : observer_list_)
    observer.OnOobeCompleted();
}

}  // namespace multidevice_setup

}  // namespace ash
