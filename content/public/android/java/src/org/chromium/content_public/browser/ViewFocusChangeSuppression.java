// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;

/** UserData to suppress view focus changes during specific transient UI interactions. */
@NullMarked
public class ViewFocusChangeSuppression implements UserData {
    private static final class UserDataFactoryLazyHolder {
        private static final WebContents.UserDataFactory<ViewFocusChangeSuppression> INSTANCE =
                ViewFocusChangeSuppression::new;
    }

    private boolean mSuppressed;

    private ViewFocusChangeSuppression(WebContents webContents) {}

    public static ViewFocusChangeSuppression from(WebContents webContents) {
        return assumeNonNull(
                webContents.getOrSetUserData(
                        ViewFocusChangeSuppression.class, UserDataFactoryLazyHolder.INSTANCE));
    }

    /**
     * Sets whether focus changes should be suppressed.
     *
     * @param suppressed Whether to suppress focus changes.
     */
    public void setSuppressed(boolean suppressed) {
        mSuppressed = suppressed;
    }

    /**
     * Returns whether focus changes are currently suppressed.
     *
     * @return True if focus changes are suppressed.
     */
    public boolean isSuppressed() {
        return mSuppressed;
    }
}
