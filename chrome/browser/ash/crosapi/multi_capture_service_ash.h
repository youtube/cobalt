// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_MULTI_CAPTURE_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_MULTI_CAPTURE_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {
class MultiCaptureServiceClient;
}  // namespace ash

namespace crosapi {

// Forwards multi capture events to multi_capture_client_.
class MultiCaptureServiceAsh : public mojom::MultiCaptureService {
 public:
  // Relies on ash::Shell::Get()->multi_capture_service_client() returning
  // a pointer to a valid object. The multi capture service client has to be
  // alive for the complete lifetime of this object.
  MultiCaptureServiceAsh();
  ~MultiCaptureServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::MultiCaptureService> receiver);

  // mojom::MultiCaptureService:
  void MultiCaptureStarted(const std::string& label,
                           const std::string& host) override;
  void MultiCaptureStopped(const std::string& label) override;

 private:
  mojo::ReceiverSet<mojom::MultiCaptureService>
      multi_capture_service_receiver_set_;
  base::raw_ptr<ash::MultiCaptureServiceClient, DanglingUntriaged>
      multi_capture_client_ = nullptr;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_MULTI_CAPTURE_SERVICE_ASH_H_
