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

import dev.cobalt.coat.CrashContext;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.crashannotator.mojom.CrashAnnotator;
import org.chromium.mojo.system.MojoException;

/**
 * Implements the CrashAnnotator Mojo interface for platforms that use first-
 * party crash reporting systems.
 */
public class CrashAnnotatorImplFirstParty implements CrashAnnotator {
    // TODO(cobalt, b/383301493): confirm that this member is needed for proper
    // lifetime management.
    private final RenderFrameHost mRenderFrameHost;

    public CrashAnnotatorImplFirstParty(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public void setString(String key,
                          String value,
                          SetString_Response callback) {
        CrashContext.INSTANCE.setCrashContext(key, value);
        CrashAnnotatorImplFirstPartyJni.get().setAnnotation(key, value);

        // The browser has no visibility into what occurs after it has provided
        // the crash context to StarboardBridge. So we just assume the crash
        // context will be handled correctly.
        callback.call(true);
    }

    /** This abstract method must be overridden. */
    @Override
    public void close() {}

    /** This abstract method must be overridden. */
    @Override
    public void onConnectionError(MojoException e) {}

    @NativeMethods
    interface Natives {
        void setAnnotation(String key, String value);
    }
}
