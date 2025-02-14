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

import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.crashannotator.mojom.CrashAnnotator;
import org.chromium.services.service_manager.InterfaceFactory;

/** Creates instances of CrashAnnotator Mojo interface implementations. */
public class CrashAnnotatorImplFactory implements InterfaceFactory {
    private final RenderFrameHost mRenderFrameHost;

    public CrashAnnotatorImplFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public CrashAnnotator createImpl() {
        return new CrashAnnotatorImpl(mRenderFrameHost);
    }
}
