// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.coat.javabridge;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import org.chromium.base.Log;

/**
 * A simple example to implement CobaltJavaScriptAndroidObject.
 * Allows querying the platform for whether the device supports
 * GoogleTV (Googles see go/what-is-amati).
 */
public class AmatiDeviceInspector implements CobaltJavaScriptAndroidObject {

    private final Context mContext;

    public AmatiDeviceInspector(Context context) {
        this.mContext = context;
    }

    @Override
    public String getJavaScriptInterfaceName() {
        return "Android_AmatiDeviceInspector";
    }

    @CobaltJavaScriptInterface
    public void printIsAmatiDevice() {
        boolean isAmatiDevice =
                mContext.getPackageManager().hasSystemFeature("com.google.android.feature.AMATI_EXPERIENCE");
        Log.i(TAG, "It is running on an Amati device? " + isAmatiDevice);
    }
}
