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

import android.app.Activity;
import android.util.Base64;
import dev.cobalt.coat.StarboardBridge;

/**
 * H5vccPlatformService JavaScript object.
 */
public class H5vccPlatformService implements CobaltJavaScriptAndroidObject {

    private final Activity activity;
    private final StarboardBridge bridge;

    public H5vccPlatformService(Activity activity, StarboardBridge bridge) {
        this.activity = activity;
        this.bridge = bridge;
    }

    @Override
    public String getJavaScriptInterfaceName() {
        return "Android_H5vccPlatformService";
    }

    @CobaltJavaScriptInterface
    public boolean hasPlatformService(String servicename) {
        return bridge.hasCobaltService(servicename);
    }

    @CobaltJavaScriptInterface
    public void openPlatformService(long serviceId, String servicename) {
        bridge.openCobaltService(activity, serviceId, servicename);
    }

    @CobaltJavaScriptInterface
    public void closePlatformService(String servicename) {
        bridge.closeCobaltService(servicename);
    }

    @CobaltJavaScriptInterface
    public String platformServiceSend(String servicename, String base64Data) {
        byte[] data = Base64.decode(base64Data, Base64.DEFAULT);
        byte[] result = bridge.sendToCobaltService(servicename, data);
        if (result == null) {
            return "";
        }

        return Base64.encodeToString(result, Base64.DEFAULT);
    }
}
