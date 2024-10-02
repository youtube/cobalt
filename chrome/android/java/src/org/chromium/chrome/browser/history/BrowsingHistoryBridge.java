// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** The JNI bridge for Android to fetch and manipulate browsing history. */
public class BrowsingHistoryBridge implements HistoryProvider {
    private BrowsingHistoryObserver mObserver;
    private long mNativeHistoryBridge;
    private boolean mRemovingItems;
    private boolean mHasPendingRemoveRequest;

    public BrowsingHistoryBridge(Profile profile) {
        mNativeHistoryBridge =
                BrowsingHistoryBridgeJni.get().init(BrowsingHistoryBridge.this, profile);
    }

    @Override
    public void setObserver(BrowsingHistoryObserver observer) {
        mObserver = observer;
    }

    @Override
    public void destroy() {
        if (mNativeHistoryBridge != 0) {
            BrowsingHistoryBridgeJni.get().destroy(
                    mNativeHistoryBridge, BrowsingHistoryBridge.this);
            mNativeHistoryBridge = 0;
        }
    }

    @Override
    public void queryHistory(String query) {
        BrowsingHistoryBridgeJni.get().queryHistory(mNativeHistoryBridge,
                BrowsingHistoryBridge.this, new ArrayList<HistoryItem>(), query, false);
    }

    @Override
    public void queryHistoryForHost(String hostName) {
        BrowsingHistoryBridgeJni.get().queryHistory(mNativeHistoryBridge,
                BrowsingHistoryBridge.this, new ArrayList<HistoryItem>(), hostName, true);
    }

    @Override
    public void queryHistoryContinuation() {
        BrowsingHistoryBridgeJni.get().queryHistoryContinuation(
                mNativeHistoryBridge, BrowsingHistoryBridge.this, new ArrayList<HistoryItem>());
    }

    @Override
    public void getLastVisitToHostBeforeRecentNavigations(
            String hostName, Callback<Long> callback) {
        BrowsingHistoryBridgeJni.get().getLastVisitToHostBeforeRecentNavigations(
                mNativeHistoryBridge, BrowsingHistoryBridge.this, hostName, callback);
    }

    @Override
    public void markItemForRemoval(HistoryItem item) {
        BrowsingHistoryBridgeJni.get().markItemForRemoval(mNativeHistoryBridge,
                BrowsingHistoryBridge.this, item.getUrl(), item.getNativeTimestamps());
    }

    @Override
    public void removeItems() {
        // Only one remove request may be in-flight at any given time. If items are currently being
        // removed, queue the new request and return early.
        if (mRemovingItems) {
            mHasPendingRemoveRequest = true;
            return;
        }
        mRemovingItems = true;
        mHasPendingRemoveRequest = false;
        BrowsingHistoryBridgeJni.get().removeItems(
                mNativeHistoryBridge, BrowsingHistoryBridge.this);
    }

    @CalledByNative
    public static void createHistoryItemAndAddToList(List<HistoryItem> items, GURL url,
            String domain, String title, long mostRecentJavaTimestamp, long[] nativeTimestamps,
            boolean blockedVisit) {
        items.add(new HistoryItem(
                url, domain, title, mostRecentJavaTimestamp, nativeTimestamps, blockedVisit));
    }

    @CalledByNative
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
        if (mObserver != null) mObserver.onQueryHistoryComplete(items, hasMorePotentialMatches);
    }

    @CalledByNative
    public void onRemoveComplete() {
        mRemovingItems = false;
        if (mHasPendingRemoveRequest) removeItems();
    }

    @CalledByNative
    public void onRemoveFailed() {
        mRemovingItems = false;
        if (mHasPendingRemoveRequest) removeItems();
        // TODO(twellington): handle remove failures.
    }

    @CalledByNative
    public void onHistoryDeleted() {
        if (mObserver != null) mObserver.onHistoryDeleted();
    }

    @CalledByNative
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {
        if (mObserver != null) {
            mObserver.hasOtherFormsOfBrowsingData(hasOtherForms);
        }
    }

    @NativeMethods
    interface Natives {
        long init(BrowsingHistoryBridge caller, Profile profile);
        void destroy(long nativeBrowsingHistoryBridge, BrowsingHistoryBridge caller);
        void queryHistory(long nativeBrowsingHistoryBridge, BrowsingHistoryBridge caller,
                List<HistoryItem> historyItems, String query, boolean hostOnly);
        void queryHistoryContinuation(long nativeBrowsingHistoryBridge,
                BrowsingHistoryBridge caller, List<HistoryItem> historyItems);
        void getLastVisitToHostBeforeRecentNavigations(long nativeBrowsingHistoryBridge,
                BrowsingHistoryBridge caller, String hostName, Callback<Long> callback);
        void markItemForRemoval(long nativeBrowsingHistoryBridge, BrowsingHistoryBridge caller,
                GURL url, long[] nativeTimestamps);
        void removeItems(long nativeBrowsingHistoryBridge, BrowsingHistoryBridge caller);
    }
}
