// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.os.Environment;

import org.chromium.base.CalledByNative;

import java.io.File;

/**
 * This class provides the path related methods for the native library.
 */
class PathUtils {

    /**
     * @return the private directory that used to store application data.
     */
    @CalledByNative
    public static String getDataDirectory(Context appContext) {
        // TODO(beverloo) base/ should not know about "chrome": http://b/6057342
        return appContext.getDir("chrome", Context.MODE_PRIVATE).getPath();
    }

    /**
     * @return the cache directory.
     */
    @CalledByNative
    public static String getCacheDirectory(Context appContext) {
        return appContext.getCacheDir().getPath();
    }

    /**
     * @return the public downloads directory.
     */
    @CalledByNative
    public static String getDownloadsDirectory(Context appContext) {
        return Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DOWNLOADS).getPath();
    }
}
