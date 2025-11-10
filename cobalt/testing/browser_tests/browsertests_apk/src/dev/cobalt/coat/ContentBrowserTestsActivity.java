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

import android.os.Bundle;

import org.chromium.base.test.util.UrlUtils;

import java.io.File;

/**
 * Android activity for running content_public.browser.tests
 */
public class ContentBrowserTestsActivity extends ContentShellBrowserTestActivity {
    private static final String TAG = "native_test";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appendCommandLineFlags(
                "--remote-debugging-socket-name content_browsertests_devtools_remote");
    }

    @Override
    protected File getPrivateDataDirectory() {
        // TODO(agrieve): We should not be touching the side-loaded test data directory.
        //     https://crbug.com/617734
        return new File(UrlUtils.getIsolatedTestRoot(),
                ContentBrowserTestsApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
    }

    @Override
    protected int getTestActivityViewId() {
        return R.layout.test_activity;
    }

    @Override
    protected int getShellManagerViewId() {
        return R.id.shell_container;
    }

}
