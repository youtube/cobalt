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

import android.app.Activity;
import android.app.Service;
import android.os.Bundle;
import android.os.Process;

import dev.cobalt.coat.CobaltActivity;
import dev.cobalt.coat.StarboardBridge;

import org.chromium.test.reporter.TestStatusReporter;
import org.chromium.native_test.NativeBrowserTest;

/** An Activity base class for running browser tests against Cobalt. */
public class ContentShellBrowserTestActivity extends CobaltActivity {
    private static final String TAG = "CobaltBrowserTest";

    private TestStatusReporter mReporter;

    @Override
    protected StarboardBridge createStarboardBridge(String[] args, String startDeepLink) {
        return new StarboardBridge(
            getApplicationContext(),
            new dev.cobalt.util.Holder<Activity>(),
            new dev.cobalt.util.Holder<Service>(),
            null, // ArtworkDownloader is not needed for tests
            args,
            startDeepLink) {
            @Override
            public void requestStop(int errorLevel) {
                mReporter.testRunFinished(Process.myPid());
                super.requestStop(errorLevel);
            }
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        mReporter = new TestStatusReporter(this);
        super.onCreate(savedInstanceState);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
    }

    @Override
    protected void startApplication(Bundle savedInstanceState) {
        new android.os.Handler().post(() -> {
            mReporter.testRunStarted(Process.myPid());
            NativeBrowserTest.javaStartupTasksComplete();
            super.startApplication(savedInstanceState);
        });
    }
}
