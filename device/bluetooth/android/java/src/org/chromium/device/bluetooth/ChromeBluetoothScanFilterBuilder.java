// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.bluetooth.le.ScanFilter;
import android.os.ParcelUuid;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes android.bluetooth.le.ScanFilter.Builder as necessary for C++.
 * This class is used to implement
 * BluetoothAdapterAndroid::CreateAndroidFilter()
 */
@JNINamespace("device")
@JNIAdditionalImport(Wrappers.class)
final class ChromeBluetoothScanFilterBuilder {
    private ScanFilter.Builder mBuilder;

    /**
     * Constructs a ChromeBluetoothScanFilter
     */
    public ChromeBluetoothScanFilterBuilder() {
        mBuilder = new ScanFilter.Builder();
    }

    // Creates and returns a new ChromeBluetoothScanFilterBuilder
    @CalledByNative
    private static ChromeBluetoothScanFilterBuilder create() {
        return new ChromeBluetoothScanFilterBuilder();
    }

    @CalledByNative
    private void setServiceUuid(String uuid) {
        if (uuid != null) {
            mBuilder.setServiceUuid(ParcelUuid.fromString(uuid));
        }
    }

    @CalledByNative
    private void setDeviceName(String deviceName) {
        if (deviceName != null) {
            mBuilder.setDeviceName(deviceName);
        }
    }

    @CalledByNative
    public ScanFilter build() {
        return mBuilder.build();
    }
}
