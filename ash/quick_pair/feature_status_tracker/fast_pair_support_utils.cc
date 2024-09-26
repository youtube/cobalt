// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/fast_pair_support_utils.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

bool HasHardwareSupport(scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter || !adapter->IsPresent())
    return false;

  if (features::IsFastPairSoftwareScanningEnabled())
    return true;

  return adapter->GetLowEnergyScanSessionHardwareOffloadingStatus() ==
         device::BluetoothAdapter::
             LowEnergyScanSessionHardwareOffloadingStatus::kSupported;
}

}  // namespace quick_pair
}  // namespace ash
