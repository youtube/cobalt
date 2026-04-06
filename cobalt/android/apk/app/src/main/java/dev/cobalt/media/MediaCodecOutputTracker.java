// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.media;

import java.util.Collections;
import java.util.Set;
import java.util.WeakHashMap;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;

/**
 * Memory tracker for MediaCodecBridge instances to estimate video memory usage.
 * Reports metrics periodically (1 minute) while bridges are active.
 */
public class MediaCodecOutputTracker {
  private static MediaCodecOutputTracker sInstance;
  private static final int BYTES_PER_MIB = 1024 * 1024;
  private static final long DEFAULT_REPORT_INTERVAL_MS = 60000; // 1 minute

  private final Set<MediaCodecBridge> mBridges =
      Collections.synchronizedSet(Collections.newSetFromMap(new WeakHashMap<>()));

  private final SequencedTaskRunner mTaskRunner =
      PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT);
  private final Runnable mReportRunnable =
      new Runnable() {
        @Override
        public void run() {
          synchronized (MediaCodecOutputTracker.this) {
            if (!mIsReporting) {
              return;
            }
            reportMetrics();
            mTaskRunner.postDelayedTask(this, DEFAULT_REPORT_INTERVAL_MS);
          }
        }
      };

  private boolean mIsReporting;

  private MediaCodecOutputTracker() {}

  public static synchronized MediaCodecOutputTracker get() {
    if (sInstance == null) {
      sInstance = new MediaCodecOutputTracker();
    }
    return sInstance;
  }


  public synchronized void register(MediaCodecBridge bridge) {
    if (mBridges.isEmpty()) {
      startReporting();
    }
    mBridges.add(bridge);
  }

  public synchronized void unregister(MediaCodecBridge bridge) {
    mBridges.remove(bridge);
    if (mBridges.isEmpty()) {
      stopReporting();
    }
  }

  private void startReporting() {
    if (!mIsReporting) {
      mIsReporting = true;
      mTaskRunner.postDelayedTask(mReportRunnable, DEFAULT_REPORT_INTERVAL_MS);
    }
  }

  private void stopReporting() {
    mIsReporting = false;
  }

  private void reportMetrics() {
    long totalMemory = getTotalOutputMemoryUsage();
    if (totalMemory > 0) {
      RecordHistogram.recordMemoryMediumMBHistogram(
          "Memory.Media.EstimatedDecodedBuffer", (int) (totalMemory / BYTES_PER_MIB));
    }
  }

  public long getTotalOutputMemoryUsage() {
    long totalMemory = 0;
    synchronized (mBridges) {
      for (MediaCodecBridge bridge : mBridges) {
        int dimension = bridge.getCurrentMediaFormatDimension();
        int activeBuffers = bridge.sizeOfActiveOutputBuffers();
        if (dimension > 0 && activeBuffers > 0) {
          // Estimate memory based on YUV420 format: width * height * 1.5
          totalMemory += (long) dimension * 3 / 2 * activeBuffers;
        }
      }
    }
    return totalMemory;
  }
}
