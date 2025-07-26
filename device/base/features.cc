// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/features.h"

#include "build/build_config.h"

namespace device {

#if BUILDFLAG(IS_WIN)
// Controls whether a more reliable GATT session handling
// implementation is used on Windows 10 1709 (RS3) and beyond.
//
// Disabled due to crbug/1120338.
BASE_FEATURE(kNewBLEGattSessionHandling,
             "NewBLEGattSessionHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

namespace features {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// Controls whether Web Bluetooth should support confirm-only and confirm-PIN
// pairing mode on Win/Linux
BASE_FEATURE(kWebBluetoothConfirmPairingSupport,
             "WebBluetoothConfirmPairingSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
// Controls whether to use uncached mode when triggering GATT discovery for
// creating a GATT connection.
BASE_FEATURE(kUncachedGattDiscoveryForGattConnection,
             "UncachedGattDiscoveryForGattConnection",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
// Controls whether to enable Bluetooth RFCOMM support on Android for Web
// Serial.
BASE_FEATURE(kBluetoothRfcommAndroid,
             "BluetoothRfcommAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
// Controls whether to enable Web serial. Blink runtime features don't allow
// pure Blink features on a subset of platforms, so we need a separate feature
// for non-Android platforms to keep the Finch switches.
BASE_FEATURE(kSerial, "Serial", base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
}  // namespace device
