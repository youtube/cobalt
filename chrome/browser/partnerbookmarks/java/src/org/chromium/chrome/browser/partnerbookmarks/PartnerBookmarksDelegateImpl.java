// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnerbookmarks;

import androidx.annotation.Nullable;

/** Default {@link PartnerBookmarksDelegate} implementation. */
public class PartnerBookmarksDelegateImpl implements PartnerBookmarksDelegate {
    @Override
    @Nullable
    public PartnerBookmarkIterator createIterator() {
        return null;
    }
}