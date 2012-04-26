// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;

// Android's NativeActivity is mostly useful for pure-native code.
// Our tests need to go up to our own java classes, which is not possible using
// the native activity class loader.
// We start a background thread in here to run the tests and avoid an ANR.
// TODO(bulach): watch out for tests that implicitly assume they run on the main
// thread.
public class ChromeNativeTestActivity extends Activity {
    private final String TAG = "ChromeNativeTestActivity";

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

        try {
            loadLibrary();
            new Thread() {
                @Override
                public void run() {
                    Log.d(TAG, ">>nativeRunTests");
                    nativeRunTests(getFilesDir().getAbsolutePath(), getApplicationContext());
                    // TODO(jrg): make sure a crash in native code
                    // triggers nativeTestFailed().
                    Log.d(TAG, "<<nativeRunTests");
                }
            }.start();
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Unable to load lib" + mLibrary + ".so: " + e);
            nativeTestFailed();
            throw e;
        }
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
