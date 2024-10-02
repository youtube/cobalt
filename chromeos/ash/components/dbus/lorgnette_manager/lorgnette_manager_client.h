// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_LORGNETTE_MANAGER_CLIENT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"

namespace ash {

// LorgnetteManagerClient is used to communicate with the lorgnette
// document scanning daemon.
class COMPONENT_EXPORT(LORGNETTE_MANAGER) LorgnetteManagerClient
    : public chromeos::DBusClient {
 public:
  // Attributes provided to a scan request.
  struct ScanProperties {
    std::string mode;  // Can be "Color", "Gray", or "Lineart".
    int resolution_dpi = 0;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static LorgnetteManagerClient* Get();

  LorgnetteManagerClient(const LorgnetteManagerClient&) = delete;
  LorgnetteManagerClient& operator=(const LorgnetteManagerClient&) = delete;

  // Gets a list of scanners from the lorgnette manager.
  virtual void ListScanners(
      chromeos::DBusMethodCallback<lorgnette::ListScannersResponse>
          callback) = 0;

  // Gets the capabilities of the scanner corresponding to |device_name| and
  // returns them using the provided |callback|.
  virtual void GetScannerCapabilities(
      const std::string& device_name,
      chromeos::DBusMethodCallback<lorgnette::ScannerCapabilities>
          callback) = 0;

  // Request a scanned image using lorgnette's StartScan API. As each page is
  // completed, calls |page_callback| with the page number and a string
  // containing the image data. Calls |completion_callback| when the scan has
  // completed. Image data will be stored in the .png format.
  //
  // If |progress_callback| is provided, it will be called as scan progress
  // increases. The progress will be passed as a value from 0-100.
  virtual void StartScan(
      const std::string& device_name,
      const lorgnette::ScanSettings& settings,
      base::OnceCallback<void(lorgnette::ScanFailureMode)> completion_callback,
      base::RepeatingCallback<void(std::string, uint32_t)> page_callback,
      base::RepeatingCallback<void(uint32_t, uint32_t)> progress_callback) = 0;

  // Requests that lorgnette cancel the currently running scan job.
  // When this function returns, that guarantees that cancelling has been
  // requested, but the cancelled scan is not completely terminated until
  // |cancel_callback| reports a successful result.
  //
  // Once CancelScan() returns, it is safe to request another scan, because
  // lorgnette will prevent access to a device until the previous scan job has
  // released it.
  //
  // This function makes the assumption that LorgnetteManagerClient only has one
  // scan running at a time.
  virtual void CancelScan(chromeos::VoidDBusMethodCallback cancel_callback) = 0;

 protected:
  friend class LorgnetteManagerClientTest;

  // Initialize() should be used instead.
  LorgnetteManagerClient();
  ~LorgnetteManagerClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_LORGNETTE_MANAGER_CLIENT_H_
