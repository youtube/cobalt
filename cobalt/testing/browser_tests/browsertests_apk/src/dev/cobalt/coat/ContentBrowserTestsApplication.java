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

import android.content.Context;

import org.chromium.base.PathUtils;
import org.chromium.native_test.NativeBrowserTestApplication;
import org.chromium.ui.base.ResourceBundle;

/**
 * A basic content_public.browser.tests {@link android.app.Application}.
 */
public class ContentBrowserTestsApplication extends NativeBrowserTestApplication {
    static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "content_shell";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);

        if (isBrowserProcess()) {
            // Test-only stuff, see also NativeUnitTest.java.
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
            ResourceBundle.setNoAvailableLocalePaks();
        }
    }
}
