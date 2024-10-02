// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.net.NetworkChangeNotifierAutoDetect;

/**
 * Registration policy to make sure we only listen to network changes when
 * there are live webview instances.
 */
public class AwNetworkChangeNotifierRegistrationPolicy
        extends NetworkChangeNotifierAutoDetect.RegistrationPolicy
        implements AwContentsLifecycleNotifier.Observer {

    @Override
    protected void init(NetworkChangeNotifierAutoDetect notifier) {
        super.init(notifier);
        AwContentsLifecycleNotifier.addObserver(this);
    }

    @Override
    protected void destroy() {
        AwContentsLifecycleNotifier.removeObserver(this);
    }

    // AwContentsLifecycleNotifier.Observer
    @Override
    public void onFirstWebViewCreated() {
        register();
    }

    @Override
    public void onLastWebViewDestroyed() {
        unregister();
    }
}
