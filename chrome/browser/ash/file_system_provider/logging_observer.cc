// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/logging_observer.h"

namespace ash {
namespace file_system_provider {

LoggingObserver::LoggingObserver() {}
LoggingObserver::~LoggingObserver() {}

void LoggingObserver::OnProvidedFileSystemMount(
    const ProvidedFileSystemInfo& file_system_info,
    MountContext context,
    base::File::Error error) {
  mounts.push_back(Event(file_system_info, context, error));
}

void LoggingObserver::OnProvidedFileSystemUnmount(
    const ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {
  // TODO(mtomasz): Split these events, as mount context doesn't make sense
  // for unmounting.
  unmounts.push_back(Event(file_system_info, MOUNT_CONTEXT_USER, error));
}

}  // namespace file_system_provider
}  // namespace ash
