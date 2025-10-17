// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import android.content.Context;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.PasswordStoreAndroidBackend.BackendException;

/**
 * This factory returns an implementation for the {@link PasswordSyncControllerDelegate}. The
 * factory itself is implemented downstream, too.
 */
@NullMarked
public abstract class PasswordSyncControllerDelegateFactory {
    private static @Nullable PasswordSyncControllerDelegateFactory sInstance;

    /**
     * Returns a delegate factory to be invoked whenever {@link #createDelegate()} is called. If no
     * factory was used yet, it is created.
     *
     * @return The shared {@link PasswordSyncControllerDelegateFactory} instance.
     */
    public static PasswordSyncControllerDelegateFactory getInstance() {
        assertOnUiThread();
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(PasswordSyncControllerDelegateFactory.class);
        }
        if (sInstance == null) {
            sInstance = new PasswordSyncControllerDelegateFactoryUpstreamImpl();
        }
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link PasswordSyncControllerDelegate}. May be
     * null for builds without a downstream delegate implementation.
     */
    public @Nullable PasswordSyncControllerDelegate createDelegate() {
        return null;
    }

    /**
     * Creates and returns new instance of the downstream implementation provided by subclasses.
     *
     * Downstream should override this method with actual implementation.
     *
     * @return An implementation of the {@link PasswordSyncControllerDelegate} if one exists.
     */
    protected PasswordSyncControllerDelegate doCreateDelegate(Context context)
            throws BackendException {
        throw new BackendException(
                "Downstream implementation is not present.",
                AndroidBackendErrorType.BACKEND_NOT_AVAILABLE);
    }

    public static void setFactoryInstanceForTesting(
            @Nullable PasswordSyncControllerDelegateFactory factory) {
        var oldValue = sInstance;
        sInstance = factory;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }
}
