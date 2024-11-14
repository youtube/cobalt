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
import android.util.Base64;
import android.util.Log;
import dev.cobalt.coat.StarboardBridge;

/**
 *
 */
public class H5vccPlatformService implements CobaltJavaScriptAndroidObject {

    private final Context context;

    private final StarboardBridge bridge;

    // Instantiate the interface and set the context
    public H5vccPlatformService(Context context, StarboardBridge bridge) {
        this.context = context;
        this.bridge = bridge;
    }

    @Override
    public String getJavaScriptInterfaceName() {
        return "Android_H5vccPlatformService";
    }

    @Override
    public String getJavaScriptAssetName() {
        return "h5vcc_platform_service.js";
    }

    @CobaltJavaScriptInterface
    public boolean has(String servicename) {
        Log.i(TAG, "Colin test: has:" + servicename);
        return bridge.hasCobaltService(servicename);
    }

    @CobaltJavaScriptInterface
    public boolean has_platform_service(String servicename) {
        Log.i(TAG, "Colin test: has_platform_service:" + servicename);
        return bridge.hasCobaltService(servicename);
    }

    @CobaltJavaScriptInterface
    public void open_platform_service(long number, String servicename) {
        Log.i(TAG, "open_platform_service:" + servicename);
        bridge.openCobaltService(number, servicename);
    }

    @CobaltJavaScriptInterface
    public void close_platform_service(String servicename) {
        Log.i(TAG, "close_platform_service:" + servicename);
        bridge.closeCobaltService(servicename);
    }

    @CobaltJavaScriptInterface
    public String platform_service_send(String servicename, String base64Data) {
        byte[] data = Base64.decode(base64Data, Base64.DEFAULT);
        Log.i(TAG, "Colin test: 3. send to kabuki client:" + base64Data);
        byte[] result = bridge.sendToCobaltService(servicename, data);
        if (result != null) {
            Log.i(TAG, "Colin test: 6. Base64.encodeToString(result, Base64.DEFAULT)");
            return Base64.encodeToString(result, Base64.DEFAULT);
        }
        return "";
    }
}
