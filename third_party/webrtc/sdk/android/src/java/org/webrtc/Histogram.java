/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import org.jni_zero.NativeMethods;

/**
 * Class for holding the native pointer of a histogram. Since there is no way to destroy a
 * histogram, please don't create unnecessary instances of this object. This class is thread safe.
 *
 * Usage example:
 * private static final Histogram someMetricHistogram =
 *     Histogram.createCounts("WebRTC.Video.SomeMetric", 1, 10000, 50);
 * someMetricHistogram.addSample(someVariable);
 */
class Histogram {
  private final long handle;

  private Histogram(long handle) {
    this.handle = handle;
  }

  static public Histogram createCounts(String name, int min, int max, int bucketCount) {
    return new Histogram(HistogramJni.get().createCounts(name, min, max, bucketCount));
  }

  static public Histogram createEnumeration(String name, int max) {
    return new Histogram(HistogramJni.get().createEnumeration(name, max));
  }

  public void addSample(int sample) {
    HistogramJni.get().addSample(handle, sample);
  }

  @NativeMethods
  interface Natives {
    long createCounts(String name, int min, int max, int bucketCount);

    long createEnumeration(String name, int max);

    void addSample(long handle, int sample);
  }
}
