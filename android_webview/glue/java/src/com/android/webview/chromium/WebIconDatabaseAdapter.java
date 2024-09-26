// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.ContentResolver;

import org.chromium.android_webview.AwContents;

/**
 * Chromium implementation of WebIconDatabase -- big old no-op (base class is deprecated).
 */
@SuppressWarnings("deprecation")
final class WebIconDatabaseAdapter extends android.webkit.WebIconDatabase {
    @Override
    public void open(String path) {
        AwContents.setShouldDownloadFavicons();
    }

    @Override
    public void close() {
        // Intentional no-op.
    }

    @Override
    public void removeAllIcons() {
        // Intentional no-op: we have no database so nothing to remove.
    }

    @Override
    public void requestIconForPageUrl(String url, IconListener listener) {
        // Intentional no-op.
    }

    @Override
    public void bulkRequestIconForPageUrl(ContentResolver cr, String where, IconListener listener) {
        // Intentional no-op: hidden in base class.
    }

    @Override
    public void retainIconForPageUrl(String url) {
        // Intentional no-op.
    }

    @Override
    public void releaseIconForPageUrl(String url) {
        // Intentional no-op.
    }
}
