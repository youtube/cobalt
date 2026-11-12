// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.app;

import java.util.Collections;
import java.util.List;
import org.chromium.base.Callback;
import org.chromium.base.metrics.HistogramBucket;
import org.chromium.base.metrics.UmaRecorder;

/** A {@link UmaRecorder} that discards every sample. */
final class DiscardingUmaRecorder implements UmaRecorder {
  @Override
  public void recordBooleanHistogram(String name, boolean sample) {}

  @Override
  public void recordExponentialHistogram(
      String name, int sample, int min, int max, int numBuckets) {}

  @Override
  public void recordLinearHistogram(String name, int sample, int min, int max, int numBuckets) {}

  @Override
  public void recordSparseHistogram(String name, int sample) {}

  @Override
  public void recordUserAction(String name, long elapsedRealtimeMillis) {}

  @Override
  public int getHistogramValueCountForTesting(String name, int sample) {
    return 0;
  }

  @Override
  public int getHistogramTotalCountForTesting(String name) {
    return 0;
  }

  @Override
  public List<HistogramBucket> getHistogramSamplesForTesting(String name) {
    return Collections.emptyList();
  }

  @Override
  public void addUserActionCallbackForTesting(Callback<String> callback) {}

  @Override
  public void removeUserActionCallbackForTesting(Callback<String> callback) {}
}
