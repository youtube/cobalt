// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "base/observer_list.h"

namespace content_settings {

// ////////////////////////////////////////////////////////////////////////////
// ObservableProvider
//

ObservableProvider::ObservableProvider() {}

ObservableProvider::~ObservableProvider() {}

void ObservableProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ObservableProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ObservableProvider::NotifyObservers(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  DCHECK(primary_pattern.IsValid());
  DCHECK(secondary_pattern.IsValid());
  for (Observer& observer : observer_list_) {
    observer.OnContentSettingChanged(primary_pattern, secondary_pattern,
                                     ContentSettingsTypeSet(content_type));
    observer.OnContentSettingChanged(primary_pattern, secondary_pattern,
                                     content_type);
  }
}

void ObservableProvider::RemoveAllObservers() {
  observer_list_.Clear();
}

bool ObservableProvider::CalledOnValidThread() {
  return thread_checker_.CalledOnValidThread();
}

}  // namespace content_settings
