// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.proto.SendTabToSelfPersistedTabData.SendTabToSelfPersistedTabDataProto;

import java.nio.ByteBuffer;
import java.util.concurrent.TimeUnit;

/** PersistedTabData attached to a Tab to store the sender device name for Send Tab To Self. */
@NullMarked
public class SendTabToSelfTabCardLabelData extends PersistedTabData {
    private static final long EXPIRATION_MS = TimeUnit.DAYS.toMillis(5); // 5 days
    private static final String TAG = "SendTabToSelfTabCardLabelData";
    private static final Class<SendTabToSelfTabCardLabelData> USER_DATA_KEY =
            SendTabToSelfTabCardLabelData.class;

    private String mSenderDeviceName;
    private long mAdditionTimestampMs;

    @SuppressWarnings("HidingField")
    private final SettableMonotonicObservableSupplier<Boolean> mIsPersistenceEnabledSupplier =
            ObservableSuppliers.createMonotonic();

    private boolean mObserverRegistered;

    /**
     * Observer that removes the SendTabToSelfTabCardLabelData from the UserDataHost when the tab is
     * interacted with by the user.
     */
    private final EmptyTabObserver mObserver =
            new EmptyTabObserver() {
                @Override
                public void onShown(Tab tab, @TabSelectionType int type) {
                    if (tab != null && tab.getUserDataHost() != null) {
                        SendTabToSelfTabCardLabelData data =
                                tab.getUserDataHost()
                                        .getUserData(SendTabToSelfTabCardLabelData.class);
                        if (data != null) {
                            data.removeAndDestroy();
                        }
                    }
                }
            };

    /** Removes the data from the host and destroys the instance. */
    private void removeAndDestroy() {
        UserDataHost host = mTab.getUserDataHost();
        // A race-condition is possible due to the asynchronous nature of PersistedTabData.from().
        if (host != null && host.getUserData(SendTabToSelfTabCardLabelData.class) == this) {
            host.removeUserData(SendTabToSelfTabCardLabelData.class);
            delete();
            destroy();
        }
    }

    /** Returns true if this is a negative cache instance (empty data), false otherwise. */
    public boolean isNegativeCache() {
        return mSenderDeviceName.isEmpty();
    }

    /** Safely registers the observer on the UI thread if the data is valid. */
    private void registerTabObserverIfNotNegativeCache() {
        org.chromium.base.ThreadUtils.assertOnUiThread();
        if (!mObserverRegistered && !isNegativeCache()) {
            mTab.addObserver(mObserver);
            mObserverRegistered = true;
        }
    }

    private void updateIsPersistenceEnabledSupplier() {
        mIsPersistenceEnabledSupplier.set(
                !mTab.isIncognito()
                        && !mTab.isDestroyed()
                        // Avoid saving the negative cache instances.
                        && !isNegativeCache());
    }

    /**
     * Returns whether the label data has expired.
     *
     * @return True if the data has exceeded the 5-day expiration window, false otherwise.
     */
    private boolean isExpired() {
        // Avoid marking the negative cache as expired.
        if (isNegativeCache()) return false;
        return System.currentTimeMillis() - mAdditionTimestampMs > EXPIRATION_MS;
    }

    /**
     * Asynchronously gets the SendTabToSelfTabCardLabelData associated with the given tab.
     *
     * @param tab The Tab to get the tab data for.
     * @param callback The Callback to be invoked when the SendTabToSelfTabCardLabelData is ready.
     */
    public static void from(Tab tab, Callback<@Nullable SendTabToSelfTabCardLabelData> callback) {
        PersistedTabData.from(
                tab,
                () ->
                        new SendTabToSelfTabCardLabelData(
                                tab, /* senderDeviceName= */ "", /* additionTimestampMs= */ 0),
                USER_DATA_KEY,
                (data) -> {
                    if (data != null && data.isExpired()) {
                        data.removeAndDestroy();
                        callback.onResult(null);
                        return;
                    }
                    callback.onResult(data);
                });
    }

    /**
     * Returns the valid SendTabToSelfTabCardLabelData for the tab, removing it if expired. Note:
     * This returns the in-memory instance if already loaded/attached.
     *
     * @param tab The Tab from which to retrieve the label data.
     * @return The valid SendTabToSelfTabCardLabelData object, or null if absent or expired.
     */
    public static @Nullable SendTabToSelfTabCardLabelData get(Tab tab) {
        if (tab == null || tab.getUserDataHost() == null) return null;
        SendTabToSelfTabCardLabelData data =
                tab.getUserDataHost().getUserData(SendTabToSelfTabCardLabelData.class);
        if (data == null) return null;

        if (data.isExpired()) {
            data.removeAndDestroy();
            return null;
        }
        return data;
    }

    /**
     * Constructs a new SendTabToSelfTabCardLabelData object and attaches it as a TabObserver.
     *
     * @param tab The Tab to which this label data is attached.
     * @param senderDeviceName The name of the device that sent the tab.
     * @param additionTimestampMs The timestamp in milliseconds when the tab was added in the
     *     background.
     */
    public SendTabToSelfTabCardLabelData(
            Tab tab, String senderDeviceName, long additionTimestampMs) {
        super(
                tab,
                PersistedTabDataConfiguration.get(
                                SendTabToSelfTabCardLabelData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(
                                SendTabToSelfTabCardLabelData.class, tab.isIncognito())
                        .getId());
        mSenderDeviceName = senderDeviceName;
        mAdditionTimestampMs = additionTimestampMs;

        registerTabObserverIfNotNegativeCache();

        registerIsTabSaveEnabledSupplier(mIsPersistenceEnabledSupplier);
        updateIsPersistenceEnabledSupplier();
    }

    /** Destroys the UserData object and removes the observer from the tab. */
    @Override
    public void destroy() {
        mTab.removeObserver(mObserver);
        super.destroy();
    }

    /**
     * Returns the localized label text to display on the tab card.
     *
     * @param context The Context used to retrieve localized string resources.
     * @return The localized label text indicating the sender device.
     */
    public String getLabelText(Context context) {
        return context.getString(
                R.string.send_tab_to_self_message_banner_subtitle, mSenderDeviceName);
    }

    /**
     * Sets the addition timestamp for testing purposes.
     *
     * @param additionTimestampMs The timestamp to set.
     */
    @VisibleForTesting
    public void setAdditionTimestampMsForTesting(long additionTimestampMs) {
        mAdditionTimestampMs = additionTimestampMs;
    }

    // PersistedTabData implementation.

    @Override
    Serializer<ByteBuffer> getSerializer() {
        return () ->
                SendTabToSelfPersistedTabDataProto.newBuilder()
                        .setSenderDeviceName(mSenderDeviceName)
                        .setAdditionTimestampMs(mAdditionTimestampMs)
                        .build()
                        .toByteString()
                        .asReadOnlyByteBuffer();
    }

    @Override
    boolean deserialize(@Nullable ByteBuffer bytes) {
        if (bytes == null || !bytes.hasRemaining()) return false;

        try {
            SendTabToSelfPersistedTabDataProto proto =
                    SendTabToSelfPersistedTabDataProto.parseFrom(bytes);
            mSenderDeviceName = proto.getSenderDeviceName();
            mAdditionTimestampMs = proto.getAdditionTimestampMs();
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        updateIsPersistenceEnabledSupplier();
                        registerTabObserverIfNotNegativeCache();
                    });
        } catch (InvalidProtocolBufferException e) {
            Log.i(TAG, "deserialize failed: \n" + e.toString());
            return false;
        }

        return true;
    }

    @Override
    public String getUmaTag() {
        return TAG;
    }
}
