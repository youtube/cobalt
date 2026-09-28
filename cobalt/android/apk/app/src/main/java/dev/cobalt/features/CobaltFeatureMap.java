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

package dev.cobalt.features;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Java accessor for the Chromium base::Features that Cobalt exposes to Java.
 *
 * <p>Only features listed in //cobalt/common/features/cobalt_feature_map_android.cc can be queried
 * through this class.
 *
 * <p>This is a process-wide singleton, created on first use and never destroyed. The native
 * base::android::FeatureMap it wraps is a base::NoDestructor, so it also lives for the rest of the
 * process.
 *
 * <p>Threading: it can be queried from any thread once the native FeatureList is initialized, since
 * the native FeatureList is thread-safe after initialization. Before that, use the cached params in
 * {@link CobaltFeatureList}.
 */
@JNINamespace("cobalt::features")
@NullMarked
public final class CobaltFeatureMap extends FeatureMap {
  private static final CobaltFeatureMap sInstance = new CobaltFeatureMap();

  // Do not instantiate this class.
  private CobaltFeatureMap() {}

  /** Returns the singleton CobaltFeatureMap. */
  public static CobaltFeatureMap getInstance() {
    return sInstance;
  }

  @Override
  protected long getNativeMap() {
    return CobaltFeatureMapJni.get().getNativeMap();
  }

  /** Native methods for CobaltFeatureMap. */
  @NativeMethods
  public interface Natives {
    long getNativeMap();
  }
}
