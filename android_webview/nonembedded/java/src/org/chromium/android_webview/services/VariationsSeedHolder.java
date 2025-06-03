// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.ParcelFileDescriptor;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.VariationsFastFetchModeUtils;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.util.Date;
import java.util.LinkedList;
import java.util.Queue;

/**
 * VariationsSeedHolder is a singleton which manages the local copy of the variations seed - both
 * the file and the SeedInfo object containing the data loaded from that file - in WebView's
 * variations service process. VariationsSeedHolder is used by VariationsSeedServer (to serve the
 * seed to apps) and AwVariationsSeedFetcher (to update the seed). VariationsSeedHolder guards
 * concurrent access to the seed by serializing all operations onto mSeedThread.
 * VariationsSeedHolder is not meant to be used outside the variations service.
 */
public class VariationsSeedHolder {
    private static final String TAG = "VariationsSeedHolder";

    private static final VariationsSeedHolder sInstance = new VariationsSeedHolder();

    private static void writeSeedWithoutClosing(SeedInfo seed, ParcelFileDescriptor destination) {
        // writeSeed() will close "out", but closing "out" will not close "destination".
        FileOutputStream out = new FileOutputStream(destination.getFileDescriptor());
        VariationsUtils.writeSeed(out, seed);
    }

    // Use mSeedHandler to send tasks to mSeedThread.
    private final HandlerThread mSeedThread;
    private final Handler mSeedHandler;
    // mSeed is the service's copy of the seed. It should only be used on mSeedThread.
    private SeedInfo mSeed;
    // Set true when we fail to load a seed, to prevent future loads until SeedUpdater runs.
    private boolean mFailedReadingSeed;

    private SafeModeSeedUpdater mSafeModeSeedUpdater = new SafeModeSeedUpdater();
    private Date mDate = new Date();

    // A Runnable which handles an individual request for the seed. Must run on mSeedThread.
    private class SeedWriter implements Runnable {
        private ParcelFileDescriptor mDestination;

        // mDestinationDate is the date field of the requester's current seed, in milliseconds since
        // epoch, or Long.MIN_VALUE if the requester has no seed. Only write our seed if our seed is
        // newer than mDestinationDate.
        private long mDestinationDate;

        public SeedWriter(ParcelFileDescriptor destination, long date) {
            mDestination = destination;
            mDestinationDate = date;
        }

        @Override
        public void run() {
            assert Thread.currentThread() == mSeedThread;

            try {
                scheduleFetchIfNeeded();

                // Load the seed if necessary.
                if (VariationsSeedHolder.this.mSeed == null && !mFailedReadingSeed) {
                    VariationsSeedHolder.this.mSeed =
                            VariationsUtils.readSeedFile(VariationsUtils.getSeedFile());
                    mFailedReadingSeed = VariationsSeedHolder.this.mSeed == null;
                }

                // If there's no seed available, the app will have to request again.
                if (VariationsSeedHolder.this.mSeed == null) return;

                if (mDestinationDate < VariationsSeedHolder.this.mSeed.date) {
                    VariationsUtils.debugLog("Writing new seed to app's data directory");
                    writeSeedWithoutClosing(VariationsSeedHolder.this.mSeed, mDestination);
                }
            } finally {
                VariationsUtils.closeSafely(mDestination);
                onWriteFinished();
            }
        }
    }

    // A Runnable which updates both mSeed and the service's seed file. Must run on mSeedThread.
    private class SeedUpdater implements Runnable {
        private SeedInfo mNewSeed;
        private Runnable mOnFinished;

        public SeedUpdater(SeedInfo newSeed, Runnable onFinished) {
            mNewSeed = newSeed;
            mOnFinished = onFinished;
        }

        @Override
        public void run() {
            assert Thread.currentThread() == mSeedThread;
            try {
                VariationsSeedHolder.this.mSeed = mNewSeed;

                // Update the seed file.
                File newSeedFile = VariationsUtils.getNewSeedFile();
                FileOutputStream out;
                try {
                    out = new FileOutputStream(newSeedFile);
                } catch (FileNotFoundException e) {
                    Log.e(TAG, "Failed to open seed file " + newSeedFile + " for update");
                    return;
                }
                if (!VariationsUtils.writeSeed(out, VariationsSeedHolder.this.mSeed)) {
                    Log.e(TAG, "Failed to write seed file " + newSeedFile + " for update");
                    return;
                }
                VariationsUtils.replaceOldWithNewSeed();
                mFailedReadingSeed = false;
            } finally {
                mOnFinished.run();
            }
        }
    }

    @VisibleForTesting
    protected VariationsSeedHolder() {
        mSeedThread = new HandlerThread(/*name=*/"seed_holder");
        mSeedThread.start();
        mSeedHandler = new Handler(mSeedThread.getLooper());
    }

    public static VariationsSeedHolder getInstance() {
        return sInstance;
    }

    public void writeSeedIfNewer(ParcelFileDescriptor destination, long date) {
        mSeedHandler.post(new SeedWriter(destination, date));
    }

    public void updateSeed(SeedInfo newSeed, Runnable onFinished) {
        mSeedHandler.post(new SeedUpdater(newSeed, onFinished));
    }

    @VisibleForTesting
    public void scheduleFetchIfNeeded() {
        AwVariationsSeedFetcher.scheduleIfNeeded();
    }

    // overridden by tests
    public void onWriteFinished() {}

    /**
     * A seed updater class tailored to update Variations seeds specifically for SafeMode scenarios
     */
    private class SafeModeSeedUpdater {
        // Stores a list of requests to notify the requester, SafeModeVariationsSeedContentProvider,
        // as soon as the Variations Fast Fetch Mode seed is fresh, where fresh is considered to be
        // < 15 minutes old.
        private final Queue<Runnable> mSafeModeVariationsSeedContentProviderCallback =
                new LinkedList<Runnable>();

        public void hasSeedUpdateCompletedAsync(Runnable r) {
            mSafeModeVariationsSeedContentProviderCallback.add(r);
            if (isSeedFileFresh()) {
                reportSeedUpdateCompletion();
            }
        }

        public void reportSeedUpdateCompletion() {
            while (!mSafeModeVariationsSeedContentProviderCallback.isEmpty()) {
                mSafeModeVariationsSeedContentProviderCallback.poll().run();
            }
        }

        // Provides a way for the caller to update the seed files from the current thread instead of
        // the seed thread. This allowls the caller to block its current thread and wait for the
        // seed files to update.
        public boolean updateSeedFilesSynchronously(SeedInfo curInfo) {
            File newSeedFile = VariationsUtils.getNewSeedFile();
            FileOutputStream out;
            try {
                out = new FileOutputStream(newSeedFile);
                if (!VariationsUtils.writeSeed(out, curInfo)) {
                    Log.e(TAG, "Failed to write seed file " + newSeedFile + " for update");
                }
                VariationsUtils.replaceOldWithNewSeed();
            } catch (FileNotFoundException e) {
                Log.e(TAG, "Failed to open seed file " + newSeedFile + " for update");
                return false;
            }
            reportSeedUpdateCompletion();
            return true;
        }
    }

    public void hasSeedUpdateCompletedAsync(Runnable r) {
        mSafeModeSeedUpdater.hasSeedUpdateCompletedAsync(r);
    }

    public boolean updateSeedFilesSynchronously(SeedInfo curInfo) {
        return mSafeModeSeedUpdater.updateSeedFilesSynchronously(curInfo);
    }

    @VisibleForTesting
    public boolean isSeedFileFresh() {
        long currTimestamp = getCurrentTimestamp();
        return (VariationsUtils.getSeedFile().lastModified() > 0
                && (currTimestamp - VariationsUtils.getSeedFile().lastModified())
                        < VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS);
    }

    public void setDateForTesting(Date date) {
        var oldValue = mDate;
        mDate = date;
        ResettersForTesting.register(() -> mDate = oldValue);
    }

    private long getCurrentTimestamp() {
        return mDate.getTime();
    }
}
