// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_DRIVER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_DRIVER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-forward.h"

namespace content {

// Interface to abstract away the starting of the worklet service.
class SharedStorageWorkletDriver {
 public:
  virtual ~SharedStorageWorkletDriver() = default;

  // Called when starting the worklet service.
  virtual void StartWorkletService(
      mojo::PendingReceiver<blink::mojom::SharedStorageWorkletService>
          pending_receiver) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_DRIVER_H_
