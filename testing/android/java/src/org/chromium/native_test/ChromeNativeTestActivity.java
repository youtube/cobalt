// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;

import org.chromium.base.PathUtils;

// Android's NativeActivity is mostly useful for pure-native code.
// Our tests need to go up to our own java classes, which is not possible using
// the native activity class loader.
public class ChromeNativeTestActivity extends Activity {
    private final String TAG = "ChromeNativeTestActivity";
    private final String EXTRA_RUN_IN_SUB_THREAD = "RunInSubThread";
    // We post a delayed task to run tests so that we do not block onCreate().
    private static long RUN_TESTS_DELAY_IN_MS = 300;

    // Name of our shlib as obtained from a string resource.
    private String mLibrary;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mLibrary = getResources().getString(R.string.native_library);
        if ((mLibrary == null) || mLibrary.startsWith("replace")) {
            nativeTestFailed();
            return;
        }

        // Needed by path_utils_unittest.cc
        PathUtils.setPrivateDataDirectorySuffix("chrome");

        try {
            loadLibrary();
            Bundle extras = this.getIntent().getExtras();
            if (extras != null && extras.containsKey(EXTRA_RUN_IN_SUB_THREAD)) {
                // Create a new thread and run tests on it.
                new Thread() {
                    @Override
                    public void run() {
                        runTests();
                    }
                }.start();
            } else {
                // Post a task to run the tests. This allows us to not block
                // onCreate and still run tests on the main thread.
                new Handler().postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        runTests();
                    }
                }, RUN_TESTS_DELAY_IN_MS);
            }
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Unable to load lib" + mLibrary + ".so: " + e);
            nativeTestFailed();
            throw e;
        }
    }

    private void runTests() {
        Log.d(TAG, ">>nativeRunTests");
        nativeRunTests(getFilesDir().getAbsolutePath(), getApplicationContext());
        Log.d(TAG, "<<nativeRunTests");
    }

    // Signal a failure of the native test loader to python scripts
    // which run tests.  For example, we look for
    // RUNNER_FAILED build/android/test_package.py.
    private void nativeTestFailed() {
        Log.e(TAG, "[ RUNNER_FAILED ] could not load native library");
    }

    private void loadLibrary() throws UnsatisfiedLinkError {
        Log.i(TAG, "loading: " + mLibrary);
        System.loadLibrary(mLibrary);
        Log.i(TAG, "loaded: " + mLibrary);
    }

    private native void nativeRunTests(String filesDir, Context appContext);
}
