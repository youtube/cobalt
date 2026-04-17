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

package dev.cobalt.browser;

import dev.cobalt.browser.crashannotator.CrashAnnotatorImplFactory;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.crashannotator.mojom.CrashAnnotator;
import org.chromium.services.service_manager.InterfaceRegistry;
import org.jni_zero.CalledByNative;

/** Registers Mojo interface implementations exposed to C++ code at the Cobalt layer. */
class CobaltInterfaceRegistrar {
    @CalledByNative
    private static void registerMojoInterfaces() {
        InterfaceRegistrar.Registry.addRenderFrameHostRegistrar(
            new CobaltRenderFrameHostInterfaceRegistrar());
    }

    private static class CobaltRenderFrameHostInterfaceRegistrar
            implements InterfaceRegistrar<RenderFrameHost> {
        @Override
        public void registerInterfaces(
                InterfaceRegistry registry,
                final RenderFrameHost renderFrameHost) {
            registry.addInterface(
                CrashAnnotator.MANAGER,
                new CrashAnnotatorImplFactory(renderFrameHost));
        }
    }
}
