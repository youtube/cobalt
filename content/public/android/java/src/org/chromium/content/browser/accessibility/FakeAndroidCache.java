// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Build;
import android.os.Parcel;
import android.os.SystemClock;

import androidx.annotation.Nullable;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A fake cache for {@link AccessibilityNodeInfoCompat} objects. This is used to simulate the
 * behavior of the Android framework's cache. This must be used with Android Tiramisu and above.
 */
@JNINamespace("content")
@NullMarked
public class FakeAndroidCache {
    private final Map<Integer, CachedNodeState> mCache = new HashMap<>();
    private final WebContentsAccessibilityImpl mWebContentsAccessibilityImpl;
    @Nullable private final AccessibilityHistogramRecorder mHistogramRecorder;
    // Only for testing
    private int mStaleNodeCount;
    private int mRemovedNodeCount;

    public FakeAndroidCache(WebContentsAccessibilityImpl webContentsAccessibilityImpl) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            throw new UnsupportedOperationException(
                    "FakeAndroidCache is only available on Android Tiramisu and above.");
        }
        mWebContentsAccessibilityImpl = webContentsAccessibilityImpl;
        mHistogramRecorder = null;
    }

    public FakeAndroidCache(
            WebContentsAccessibilityImpl webContentsAccessibilityImpl,
            AccessibilityHistogramRecorder histogramRecorder) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            throw new UnsupportedOperationException(
                    "FakeAndroidCache is only available on Android Tiramisu and above.");
        }
        mWebContentsAccessibilityImpl = webContentsAccessibilityImpl;
        mHistogramRecorder = histogramRecorder;
    }

    private static class CachedNodeState {
        // A safe copy of the node's state at the time of caching.
        final @Nullable byte[] mNodeInfoParcelData;

        // The virtual view IDs of the children at the time of caching.
        final List<Integer> mChildIds;

        // The timestamp representing when the node was added to the cache.
        final long mAddedToCacheTimestamp;

        private CachedNodeState(AccessibilityNodeInfoCompat mNodeInfo, List<Integer> childIds) {
            Parcel nodeParcel = Parcel.obtain();
            mNodeInfo.unwrap().writeToParcel(nodeParcel, 0);
            mNodeInfoParcelData = nodeParcel.marshall();
            mChildIds = new ArrayList<>(childIds);
            mAddedToCacheTimestamp = SystemClock.elapsedRealtime();
        }
    }

    // Validate accessibility node info throughout the fake android cache.
    @CalledByNative
    public void validateAccessibilityForExperiment() {
        if (mHistogramRecorder == null) {
            return;
        }

        for (Map.Entry<Integer, CachedNodeState> entry : mCache.entrySet()) {
            int virtualViewId = entry.getKey();
            CachedNodeState cachedState = entry.getValue();
            // Build a completely fresh node for comparison.
            AccessibilityNodeInfoCompat freshInfo =
                    mWebContentsAccessibilityImpl.buildFreshAccessibilityNodeInfo(virtualViewId);
            if (freshInfo == null) {
                continue;
            }
            if (freshInfo.getUniqueId() == null) {
                throw new IllegalArgumentException("Unique ID for node info should not be null.");
            }
            // Avoid comparing against the root node given its event handing.
            if (freshInfo
                    .getUniqueId()
                    .equals(
                            String.valueOf(
                                    mWebContentsAccessibilityImpl
                                            .getCurrentRootIdForExperiment()))) {
                continue;
            }

            Parcel freshInfoParcel = Parcel.obtain();
            freshInfo.unwrap().writeToParcel(freshInfoParcel, 0);
            if (!Arrays.equals(freshInfoParcel.marshall(), cachedState.mNodeInfoParcelData)) {
                mStaleNodeCount++;
            }
        }

        mHistogramRecorder.recordFakeCacheHistograms(
                getPeakCacheNodesInBatch(), mRemovedNodeCount, mStaleNodeCount, mCache.size());

        mRemovedNodeCount = 0;
        mStaleNodeCount = 0;
    }

    /**
     * Adds a node to the fake Android cache for testing.
     *
     * @param virtualViewId The virtual view id of the node.
     */
    public void addNode(int virtualViewId, AccessibilityNodeInfoCompat nodeInfo) {
        if (nodeInfo == null) {
            return;
        }
        int[] childIds = mWebContentsAccessibilityImpl.getChildIdsForExperiment(virtualViewId);
        List<Integer> childIdList = new ArrayList<>(childIds != null ? childIds.length : 0);
        if (childIds != null) {
            for (int id : childIds) {
                childIdList.add(id);
            }
        }
        mCache.put(virtualViewId, new CachedNodeState(nodeInfo, childIdList));
    }

    // Clear the entire fake cache.
    private void clear(long now) {
        if (mHistogramRecorder != null) {
            for (Map.Entry<Integer, CachedNodeState> entry : mCache.entrySet()) {
                mHistogramRecorder.reportNodeRemovedFromFakeCache(
                        entry.getKey(), now - entry.getValue().mAddedToCacheTimestamp);
                mRemovedNodeCount++;
            }
        }
        mCache.clear();
    }

    /**
     * Clears this node from the fake cache.
     *
     * @param virtualViewId The virtual view id of the node.
     * @param recursive Whether to clear all of the node's children.
     */
    public void clearNode(int virtualViewId, boolean recursive) {
        if (recursive) {
            clearNodeTimeStamp(virtualViewId, /* timeStamp= */ SystemClock.elapsedRealtime());
        } else {
            clearNodeTimeStamp(virtualViewId, /* timeStamp= */ null);
        }
    }

    /**
     * Clears this node from the fake cache.
     *
     * @param virtualViewId The virtual view id of the node.
     * @param timeStamp Timestamp which presence determines if clearing is recursive or not.
     */
    private void clearNodeTimeStamp(int virtualViewId, @Nullable Long timeStamp) {
        CachedNodeState cachedNodeState = mCache.get(virtualViewId);
        if (cachedNodeState != null) {
            mCache.remove(virtualViewId);
            mRemovedNodeCount++;
            if (mHistogramRecorder != null) {
                long now = timeStamp != null ? timeStamp : SystemClock.elapsedRealtime();
                mHistogramRecorder.reportNodeRemovedFromFakeCache(
                        virtualViewId, now - cachedNodeState.mAddedToCacheTimestamp);
            }
            if (timeStamp != null) {
                for (int childId : cachedNodeState.mChildIds) {
                    clearNodeTimeStamp(childId, timeStamp);
                }
            }
        } else if (timeStamp != null) {
            // According to Android's AccessibilityCache, if a node is not found, children might
            // be present so we clear all of the cache.
            clear(timeStamp);
        }
    }

    /**
     * Checks if cache holds a node.
     *
     * @param virtualViewId The virtual view id of the node.
     * @return If the cache either holds it or not.
     */
    @CalledByNative
    private boolean isNodeLikelyKnownByAndroidFrameworkForExperiment(int virtualViewId) {
        return mCache.containsKey(virtualViewId);
    }

    /**
     * Returns the timestamp of when the node was added to the cache.
     *
     * @param virtualViewId The virtual view id of the node.
     * @return The timestamp in milliseconds, or -1 if the node is not in the cache.
     */
    public long getAddedToCacheTimestampForTesting(int virtualViewId) {
        CachedNodeState cachedState = mCache.get(virtualViewId);
        return cachedState != null ? cachedState.mAddedToCacheTimestamp : -1;
    }

    /**
     * Returns the denominator used to calculate the cache churn percentage for the current batch.
     * The denominator represents the peak number of unique nodes that existed in the cache at any
     * point during this validation period (current cache size + removed nodes). Using this sum
     * instead of the starting cache size prevents additions in the same batch from inflating the
     * churn metric beyond 100%.
     */
    private int getPeakCacheNodesInBatch() {
        return mCache.size() + mRemovedNodeCount;
    }

    public int getPeakCacheNodesInBatchForTesting() {
        return getPeakCacheNodesInBatch();
    }
}
