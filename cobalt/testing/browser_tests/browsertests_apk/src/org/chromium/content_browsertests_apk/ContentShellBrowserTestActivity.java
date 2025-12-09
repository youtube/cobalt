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

package org.chromium.content_browsertests_apk;

import android.content.Context;
import android.net.Uri;
import android.view.Window;
import android.view.WindowManager;

import androidx.core.content.FileProvider;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileProviderUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import dev.cobalt.shell.ShellManager;
import org.chromium.native_test.NativeBrowserTest;
import org.chromium.native_test.NativeBrowserTestActivity;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;

/** An Activity base class for running browser tests against ContentShell. */
public abstract class ContentShellBrowserTestActivity extends NativeBrowserTestActivity {
    private static final String TAG = "native_test";

    private ShellManager mShellManager;
    private WindowAndroid mWindowAndroid;

    private static class FileProviderHelper implements FileProviderUtils.FileProviderUtil {
        // Keep this variable in sync with the value defined in file_paths.xml.
        private static final String API_AUTHORITY_SUFFIX = ".FileProvider";

        @Override
        public Uri getContentUriFromFile(File file) {
            Context appContext = ContextUtils.getApplicationContext();
            return FileProvider.getUriForFile(
                    appContext, appContext.getPackageName() + API_AUTHORITY_SUFFIX, file);
        }
    }

    /**
     * Initializes the browser process.
     *
     * This generally includes loading native libraries and switching to the native command line,
     * among other things.
     */
    @Override
    protected void initializeBrowserProcess() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            LibraryLoader.getInstance().ensureInitialized();
        }

        FileProviderUtils.setFileProviderUtil(new FileProviderHelper());
        setContentView(getTestActivityViewId());
        mShellManager = new ShellManager(this);
        IntentRequestTracker intentRequestTracker = IntentRequestTracker.createFromActivity(this);
        mWindowAndroid =
                new ActivityWindowAndroid(
                        this,
                        /* listenToActivityState= */ true,
                        intentRequestTracker,
                        /* insetObserver= */ null,
                        /* trackOcclusion= */ true);
        mShellManager.setWindow(mWindowAndroid);

        Window wind = this.getWindow();
        wind.addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
        wind.addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
        wind.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);

        BrowserStartupController.getInstance()
                .setContentMainCallbackForTests(
                        () -> {
                            // This jumps into C++ to set up and run the test harness. The test
                            // harness runs ContentMain()-equivalent code, and then waits for
                            // javaStartupTasksComplete() to be called.
                            runTests();
                        });
        BrowserStartupController.getInstance()
                .startBrowserProcessesAsync(
                        LibraryProcessType.PROCESS_BROWSER,
                        false,
                        false,
                        new StartupCallback() {
                            @Override
                            public void onSuccess() {
                                // The C++ test harness is running thanks to runTests() above, but
                                // it waits for Java initialization to complete. This tells C++
                                // that it may continue now to finish running the tests.
                                NativeBrowserTest.javaStartupTasksComplete();
                            }

                            @Override
                            public void onFailure() {
                                throw new RuntimeException(
                                        "Failed to startBrowserProcessesAsync()");
                            }
                        });
    }

    /**
     * Ensure that the user data directory gets overridden to getPrivateDataDirectory() (which is
     * cleared at the start of every run); the directory that ANDROID_APP_DATA_DIR is set to in the
     * context of Java browsertests is not cleared as it also holds persistent state, which causes
     * test failures due to state bleedthrough. See crbug.com/617734 for details.
     */
    @Override
    protected String getUserDataDirectoryCommandLineSwitch() {
        return "user-data-dir";
    }

    protected int getTestActivityViewId() {
        return R.layout.test_activity;
    }

    protected int getShellManagerViewId() {
        return R.id.shell_container;
    }
}
