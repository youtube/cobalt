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

import androidx.annotation.Nullable;

/**
 * Interface for Android objects that are exposed to JavaScript.
 */
public interface CobaltJavaScriptAndroidObject {

    /**
     * Gets the name used to expose this object to JavaScript.
     * This name is used in the `addJavascriptInterface` method of the WebView.
     *
     * @return The JavaScript interface name.
     */
    public String getJavaScriptInterfaceName();

    /**
     * Gets the name of the JavaScript asset file that uses this interface.
     * This allows the JavaScript code to be loaded and interact with this object.
     *
     * @return The name of the JavaScript asset file, or null if not applicable.
     */
    public @Nullable String getJavaScriptAssetName();
}
