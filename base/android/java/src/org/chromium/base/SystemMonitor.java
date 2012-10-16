// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Looper;


/**
 * Integrates native SystemMonitor with the java side.
 */
@JNINamespace("base::android")
public class SystemMonitor {
    private static SystemMonitor sInstance;

    private boolean mIsBatteryPower;

    public static void createForTests(Context context) {
        // Applications will create this once the
        // JNI side has been fully wired up both sides.
        // For tests, we just need native -> java, that is,
        // we don't need to notify java -> native on creation.
        sInstance = new SystemMonitor();
    }

    public static void create(Context context) {
        if (sInstance == null) {
            sInstance = new SystemMonitor();
            IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
            Intent batteryStatusIntent = context.registerReceiver(null, ifilter);
            onBatteryChargingChanged(batteryStatusIntent);
        }
    }

    private SystemMonitor() {
    }

    public static void onBatteryChargingChanged(Intent intent) {
        if (sInstance == null) {
            // We may be called by the framework intent-filter before being
            // fully initialized. This is not a problem, since our constructor
            // will check for the state later on.
            return;
        }
        int chargePlug = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
        // If we're not plugged, assume we're running on battery power.
        sInstance.mIsBatteryPower = chargePlug != BatteryManager.BATTERY_PLUGGED_USB &&
                                    chargePlug != BatteryManager.BATTERY_PLUGGED_AC;
        nativeOnBatteryChargingChanged();
    }

    @CalledByNative
    private static boolean isBatteryPower() {
        return sInstance.mIsBatteryPower;
    }

    private static native void nativeOnBatteryChargingChanged();

}
