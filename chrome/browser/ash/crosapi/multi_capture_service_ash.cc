// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/multi_capture_service_ash.h"

#include "ash/multi_capture/multi_capture_service_client.h"
#include "ash/shell.h"
#include "base/check_is_test.h"

namespace crosapi {

MultiCaptureServiceAsh::MultiCaptureServiceAsh() {
  if (ash::Shell::HasInstance()) {
    multi_capture_client_ = ash::Shell::Get()->multi_capture_service_client();
    DCHECK(multi_capture_client_);
  } else {
    CHECK_IS_TEST();
  }
}
MultiCaptureServiceAsh::~MultiCaptureServiceAsh() = default;

void MultiCaptureServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::MultiCaptureService> receiver) {
  multi_capture_service_receiver_set_.Add(this, std::move(receiver));
}

void MultiCaptureServiceAsh::MultiCaptureStarted(const std::string& label,
                                                 const std::string& host) {
  // TODO(crbug.com/1399594): Origin cannot be used in a crosapi interface as it
  // is not stable. Currently, only the host of the origin is used. Pass the
  // complete origin when the `Origin` interface becomes stable.
  multi_capture_client_->MultiCaptureStarted(
      label, url::Origin::CreateFromNormalizedTuple(/*scheme=*/"https", host,
                                                    /*port=*/443));
}

void MultiCaptureServiceAsh::MultiCaptureStopped(const std::string& label) {
  multi_capture_client_->MultiCaptureStopped(label);
}

}  // namespace crosapi
