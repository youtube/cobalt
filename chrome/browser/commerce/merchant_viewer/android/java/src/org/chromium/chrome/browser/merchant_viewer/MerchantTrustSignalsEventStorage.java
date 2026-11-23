// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import com.google.errorprone.annotations.CheckReturnValue;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.List;

/** Provides storage for merchant trust signals events. */
@NullMarked
public class MerchantTrustSignalsEventStorage implements Destroyable {
    private long mNativeMerchantSignalDB;

    MerchantTrustSignalsEventStorage(Profile profile) {
        assert !profile.isOffTheRecord()
                : "MerchantTrustSignalsEventStorage is not supported for incognito profiles";
        MerchantTrustSignalsEventStorageJni.get().init(this, profile);
        assert mNativeMerchantSignalDB != 0;
    }

    /**
     * Save one event to the database.
     * @param event The {@link MerchantTrustSignalsEvent} to store.
     */
    public void save(MerchantTrustSignalsEvent event) {
        saveWithCallback(event, null);
    }

    @MainThread
    @VisibleForTesting
    public void saveWithCallback(MerchantTrustSignalsEvent event, @Nullable Runnable onComplete) {
        if (!nativeExists()) return;
        MerchantTrustSignalsEventStorageJni.get()
                .save(mNativeMerchantSignalDB, event.getKey(), event.getTimestamp(), onComplete);
    }

    /**
     * Load one event from the database.
     *
     * @param key The key used to identify a event.
     * @param callback A callback with loaded result.
     */
    public void load(String key, Callback<MerchantTrustSignalsEvent> callback) {
        if (!nativeExists()) return;
        MerchantTrustSignalsEventStorageJni.get().load(mNativeMerchantSignalDB, key, callback);
    }

    /**
     * Load all events whose keys have specific prefix.
     *
     * @param prefix The prefix used to identify events.
     * @param callback A callback with loaded results.
     */
    public void loadWithPrefix(String prefix, Callback<List<MerchantTrustSignalsEvent>> callback) {
        if (!nativeExists()) return;
        MerchantTrustSignalsEventStorageJni.get()
                .loadWithPrefix(mNativeMerchantSignalDB, prefix, callback);
    }

    /**
     * Delete one event from the database.
     *
     * @param event The {@link MerchantTrustSignalsEvent} to delete.
     */
    public void delete(MerchantTrustSignalsEvent event) {
        if (!nativeExists()) return;
        MerchantTrustSignalsEventStorageJni.get()
                .delete(mNativeMerchantSignalDB, event.getKey(), null);
    }

    @MainThread
    public void deleteForTesting(MerchantTrustSignalsEvent event, Runnable onComplete) {
        if (!nativeExists()) return;
        MerchantTrustSignalsEventStorageJni.get()
                .delete(mNativeMerchantSignalDB, event.getKey(), onComplete);
    }

    /** Delete all events from the database. */
    public void deleteAll() {
        if (!nativeExists()) return;
        MerchantTrustSignalsEventStorageJni.get().deleteAll(mNativeMerchantSignalDB, null);
    }

    @MainThread
    public void deleteAllForTesting(Runnable onComplete) {
        if (!nativeExists()) return;
        MerchantTrustSignalsEventStorageJni.get().deleteAll(mNativeMerchantSignalDB, onComplete);
    }

    @Override
    public void destroy() {
        if (mNativeMerchantSignalDB == 0) return;

        long nativeMerchantSignalDB = mNativeMerchantSignalDB;
        mNativeMerchantSignalDB = 0;
        MerchantTrustSignalsEventStorageJni.get().destroy(nativeMerchantSignalDB);
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        assert nativePtr != 0;
        assert mNativeMerchantSignalDB == 0;
        mNativeMerchantSignalDB = nativePtr;
    }

    @CheckReturnValue
    private boolean nativeExists() {
        return mNativeMerchantSignalDB != 0;
    }

    @VisibleForTesting
    void setNativePtrForTesting(long nativePtr) {
        setNativePtr(nativePtr);
    }

    @NativeMethods
    interface Natives {
        void init(MerchantTrustSignalsEventStorage self, BrowserContextHandle handle);

        void save(
                long nativeMerchantSignalDB,
                @JniType("std::string") String key,
                long timestamp,
                @Nullable Runnable onComplete);

        void load(
                long nativeMerchantSignalDB,
                @JniType("std::string") String key,
                Callback<MerchantTrustSignalsEvent> callback);

        void loadWithPrefix(
                long nativeMerchantSignalDB,
                @JniType("std::string") String prefix,
                Callback<List<MerchantTrustSignalsEvent>> callback);

        void delete(
                long nativeMerchantSignalDB,
                @JniType("std::string") String key,
                @Nullable Runnable onComplete);

        void deleteAll(long nativeMerchantSignalDB, @Nullable Runnable onComplete);

        void destroy(long nativeMerchantSignalDB);
    }
}
