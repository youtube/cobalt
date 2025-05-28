// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORAGE_NOTIFICATION_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_STORAGE_NOTIFICATION_SERVICE_H_

#include "base/functional/bind.h"
#include "url/origin.h"

namespace blink {
class StorageKey;
}

namespace {
using StoragePressureNotificationCallback =
    base::RepeatingCallback<void(const blink::StorageKey&)>;
}

namespace content {

// This interface is used to create a connection between the storage layer and
// the embedder layer, where calls to UI code can be made. Embedders should
// vend an instance of this interface in an override of
// BrowserContext::GetStorageNotificationService().
class StorageNotificationService {
 public:
  StorageNotificationService() = default;

  StorageNotificationService(const StorageNotificationService&) = delete;
  StorageNotificationService& operator=(const StorageNotificationService&) =
      delete;

  ~StorageNotificationService() = default;

  // These pure virtual functions should be implemented in the embedder layer
  // where calls to UI and notification code can be implemented. This closure
  // is passed to QuotaManager in StoragePartitionImpl, where it is called
  // when QuotaManager determines appropriate to alert the user that the device
  // is in a state of storage pressure.
  virtual void MaybeShowStoragePressureNotification(
      const blink::StorageKey& storage_key) = 0;
  virtual StoragePressureNotificationCallback
  CreateThreadSafePressureNotificationCallback() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORAGE_NOTIFICATION_SERVICE_H_
