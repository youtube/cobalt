// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.coat;

import android.content.ComponentCallbacks2;
import android.content.res.Configuration;

import org.chromium.base.ContextUtils;
import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

/**
 * Android part of NearOomMonitor. This collects Java memory pressure signals
 * and delivers them to C++ counterpart.
 */
class NearOomMonitor implements ComponentCallbacks2 {
    private final long mNearOomMonitor;

    @CalledByNative
    private static NearOomMonitor create(long nearOomMonitor) {
        return new NearOomMonitor(nearOomMonitor);
    }

    private NearOomMonitor(long nearOomMonitor) {
        mNearOomMonitor = nearOomMonitor;
        ContextUtils.getApplicationContext().registerComponentCallbacks(this);
    }

    @Override
    public void onTrimMemory(int level) {}

    @Override
    public void onLowMemory() {
        NearOomMonitorJni.get().onLowMemory(mNearOomMonitor, NearOomMonitor.this);
    }

    @Override
    public void onConfigurationChanged(Configuration config) {}

    @NativeMethods
    interface Natives {
        void onLowMemory(long nativeNearOomMonitor, NearOomMonitor caller);
    }
}
