// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_PROBE_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_PROBE_SERVICE_ASH_H_

#include <memory>
#include <vector>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class ProbeServiceAsh : public crosapi::mojom::TelemetryProbeService {
 public:
  class Factory {
   public:
    static std::unique_ptr<crosapi::mojom::TelemetryProbeService> Create(
        mojo::PendingReceiver<crosapi::mojom::TelemetryProbeService> receiver);
    static void SetForTesting(Factory* test_factory);

    virtual ~Factory();

   protected:
    virtual std::unique_ptr<crosapi::mojom::TelemetryProbeService>
    CreateInstance(mojo::PendingReceiver<crosapi::mojom::TelemetryProbeService>
                       receiver) = 0;

   private:
    static Factory* test_factory_;
  };

  ProbeServiceAsh();
  ProbeServiceAsh(const ProbeServiceAsh&) = delete;
  ProbeServiceAsh& operator=(const ProbeServiceAsh&) = delete;
  ~ProbeServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::TelemetryProbeService> receiver);

 private:
  // crosapi::mojom::TelemetryProbeService override
  void ProbeTelemetryInfo(
      const std::vector<crosapi::mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;
  void GetOemData(GetOemDataCallback callback) override;

  // Ensures that |service_| created and connected to the
  // CrosHealthdProbeService.
  cros_healthd::mojom::CrosHealthdProbeService* GetService();

  void OnDisconnect();

  // Pointer to real implementation.
  mojo::Remote<cros_healthd::mojom::CrosHealthdProbeService> service_;

  // We must destroy |receiver_| before destroying |service_|, so we will close
  // interface pipe before destroying pending response callbacks owned by
  // |service_|. It is an error to drop response callbacks which still
  // correspond to an open interface pipe.
  //
  // Support any number of connections.
  mojo::ReceiverSet<crosapi::mojom::TelemetryProbeService> receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_PROBE_SERVICE_ASH_H_
