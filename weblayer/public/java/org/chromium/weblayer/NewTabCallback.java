// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Used for handling new tabs (such as occurs when window.open() is called). If this is not
 * set, popups are disabled.
 */
abstract class NewTabCallback {
    /**
     * Called when a new tab has been created.
     *
     * @param tab The new tab.
     * @param type How the tab should be shown.
     */
    public abstract void onNewTab(@NonNull Tab tab, @NewTabType int type);

    /**
     * Called when a tab previously opened via onNewTab() was asked to close. Generally this should
     * destroy the Tab and/or Browser.
     * NOTE: This callback was deprecated in 84; WebLayer now internally closes tabs in this case
     * and the embedder will be notified via TabListCallback#onTabRemoved().
     *
     * @see Browser#destroyTab
     */
    @Deprecated
    public void onCloseTab() {}
}
