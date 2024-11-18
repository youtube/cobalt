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

import android.content.Context;

/** Implementation of HTMLMediaElement extensions. */
public class HTMLMediaElementExtension implements CobaltJavaScriptAndroidObject {

  private final Context context;

  public HTMLMediaElementExtension(Context context) {
    this.context = context;
  }

  @Override
  public String getJavaScriptInterfaceName() {
    return "HTMLMediaElementExtension";
  }

  @Override
  public String getJavaScriptAssetName() {
    return "html_media_element_extension.js";
  }

  @CobaltJavaScriptInterface
  public String canPlayType(String mimeType, String keySystem) {
    return nativeCanPlayType(mimeType, keySystem);
  }

  private static native String nativeCanPlayType(String mimeType, String keySystem);
}
