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

package dev.cobalt.browser.crashannotator;

import static dev.cobalt.util.Log.TAG;

import dev.cobalt.util.Log;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.crashannotator.mojom.CrashAnnotator;
import org.chromium.mojo.system.MojoException;

/** Implements the CrashAnnotator Mojo interface in Java. */
public class CrashAnnotatorImpl implements CrashAnnotator {
    // TODO(cobalt, b/383301493): confirm that this member is needed for proper
    // lifetime management.
    private final RenderFrameHost mRenderFrameHost;

    public CrashAnnotatorImpl(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public void setString(String key,
                          String value,
                          SetString_Response callback) {
        // TODO(cobalt, b/383301493): actually implement this by calling
        // StarboardBridge.setCrashContext().
        Log.i(TAG, "Java CrashAnnotator impl key=%s value=%s", key, value);
        callback.call(false);
    }

    /** This abstract method must be overridden. */
    @Override
    public void close() {}

    /** This abstract method must be overridden. */
    @Override
    public void onConnectionError(MojoException e) {}
}
