// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.chromium.base.PathUtils;

/**
 * Collection of URL utilities.
 */
public class UrlUtils {
    private final static String DATA_DIR = "/chrome/test/data/";

    /**
     * Construct a suitable URL for loading a test data file.
     *
     * @param path Pathname relative to external/chrome/testing/data
     */
    public static String getTestFileUrl(String path) {
        return "file://" + PathUtils.getExternalStorageDirectory() + DATA_DIR + path;
    }
}
