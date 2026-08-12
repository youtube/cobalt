// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.util;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.StringJoiner;

/** Defines the constant names for feature switches used in Kimono. */
public class JavaSwitches {
  public static final String ENABLE_QUIC = "EnableQUIC";
  public static final String DISABLE_STARTUP_GUARD = "DisableStartupGuard";
  public static final String STARTUP_GUARD_INTERVAL_IN_SECONDS = "StartupGuardIntervalInSeconds";

  /** flag to enable auto-retrying URL load on network recovery before splash screen is hidden. */
  public static final String ENABLE_AUTO_RETRY_ON_NETWORK_RECOVERY =
      "EnableAutoRetryOnNetworkRecovery";

  public static final String ENABLE_OPTIMIZED_FONT_LOADING = "EnableOptimizedFontLoading";
  public static final String ENABLE_OPTIMIZED_V8_CODE_CACHE = "EnableOptimizedV8CodeCache";

  /** flag to allow caching CSS and WebAssembly resources in the HTTP disk cache. */
  public static final String ENABLE_CSS_AND_WASM_FOR_HTTP_CACHE = "EnableCssAndWasmForHttpCache";

  /** flag to enable aggressive HTTP disk cache and V8 generated code cache tuning exclusions. */
  public static final String ENABLE_HTTP_AND_V8_CACHE_TUNING = "EnableHttpAndV8CacheTuning";

  /** flag to re-enable freeze and resume events */
  public static final String ENABLE_FREEZE = "EnableFreeze";

  public static final String USE_MINOR_MS_FOR_MINOR_GC = "UseMinorMSForMinorGC";

  /** flag to tune compositor offscreen interest area size in pixels. */
  public static final String INTEREST_AREA_SIZE_IN_PIXELS = "InterestAreaSizeInPixels";

  /** flag to tune delay in seconds before reclaiming prepaint tiles when idle. */
  public static final String RECLAIM_DELAY_IN_SECONDS = "ReclaimDelayInSeconds";

  /** flag to disable GPU memory buffer compositor resources. */
  public static final String DISABLE_GPU_MEMORY_BUFFER_COMPOSITOR_RESOURCES =
      "DisableGpuMemoryBufferCompositorResources";

  /** flag to enable the GPU shader disk cache. */
  public static final String ENABLE_GPU_SHADER_DISK_CACHE = "EnableGpuShaderDiskCache";

  /** flag to limit GPU image cache items */
  public static final String GPU_IMAGE_CACHE_LIMIT_ITEMS = "GpuImageCacheLimitItems";

  /** flag to globally configure max HTTP cache size ceiling in bytes. */
  public static final String MAX_HTTP_CACHE_SIZE = "MaxHttpCacheSize";

  /** flag to limit GPU image cache working set budget bytes */
  public static final String DECODED_IMAGE_WORKING_SET_BUDGET_BYTES =
      "DecodedImageWorkingSetBudgetBytes";

  /** flag to allow scaling clipped images in GpuImageDecodeCache */
  public static final String ENABLE_SCALING_CLIPPED_IMAGES = "EnableScalingClippedImages";

  /** flag to reduce starboard thread stack size. */
  public static final String REDUCE_STARBOARD_THREAD_STACK_SIZE = "ReduceStarboardThreadStackSize";

  /** flag to reduce android thread stack size. */
  public static final String REDUCE_ANDROID_THREAD_STACK_SIZE = "ReduceAndroidThreadStackSize";

  /** flag to enable dynamic mojo pipe sizing. */
  public static final String ENABLE_COBALT_DYNAMIC_MOJO_PIPE_SIZING =
      "EnableCobaltDynamicMojoPipeSizing";

  /** flag to tune cobalt dynamic mojo pipe sizing subresource size in bytes. */
  public static final String COBALT_DYNAMIC_MOJO_PIPE_SUBRESOURCE_SIZE =
      "CobaltDynamicMojoPipeSubresourceSize";

  /** flag to tune cobalt dynamic mojo pipe sizing media size in bytes. */
  public static final String COBALT_DYNAMIC_MOJO_PIPE_MEDIA_SIZE = "CobaltDynamicMojoPipeMediaSize";

  /** Avoid reuse resource. */
  public static final String AVOID_CC_REUSE_RESOURCE = "AvoidCCReuseResource";

  /** flag to bypass BufferingBytesConsumer Oilpan heap buffering. */
  public static final String COBALT_BYPASS_BUFFERING_BYTES_CONSUMER =
      "CobaltBypassBufferingBytesConsumer";

  /** flag to aggressively flush v8 bytecode after a configurable old time. */
  public static final String V8_SET_BYTECODE_OLD_TIME = "V8SetBytecodeOldTime";

  /** Flag for enable go/cobalt-direct-window-rendering */
  public static final String DIRECT_WINDOW_RENDERING = "DirectWindowRendering";

  public static final String V8_INITIAL_OLD_SPACE_SIZE = "V8InitialOldSpaceSize";

  /** flag to enable area based buffer budget experiment. */
  public static final String AREA_BASED_VIDEO_BUFFER_BUDGET = "AreaBasedVideoBufferBudget";

  /** flag to allow critical memory pressure handling in foreground for V8. */
  public static final String ALLOW_CRITICAL_MEMORY_PRESSURE_HANDLING_IN_FOREGROUND =
      "AllowCriticalMemoryPressureHandlingInForeground";

  /** flag to evict blink memory cache on critical memory pressure. */
  public static final String EVICT_MEMORY_CACHE_ON_CRITICAL_MEMORY_PRESSURE =
      "EvictMemoryCacheOnCriticalMemoryPressure";

  public static List<String> getExtraCommandLineArgs(Map<String, String> javaSwitches) {
    List<String> extraCommandLineArgs = new ArrayList<>();
    StringJoiner jsFlags = new StringJoiner(";");

    if (!javaSwitches.containsKey(JavaSwitches.ENABLE_QUIC)) {
      extraCommandLineArgs.add("--disable-quic");
    }

    if (javaSwitches.containsKey(JavaSwitches.USE_MINOR_MS_FOR_MINOR_GC)) {
      jsFlags.add("--minor-ms");
      jsFlags.add("--minor-ms-min-new-space-capacity-for-concurrent-marking-mb=0");
    }

    String oldTimeStr = javaSwitches.get(JavaSwitches.V8_SET_BYTECODE_OLD_TIME);
    if (oldTimeStr != null) {
      String oldTime = oldTimeStr.replaceAll("[^0-9]", "");
      if (!oldTime.isEmpty()) {
        jsFlags.add("--flush-bytecode");
        jsFlags.add("--bytecode-old-time=" + oldTime);
      }
    }

    if (javaSwitches.containsKey(JavaSwitches.V8_INITIAL_OLD_SPACE_SIZE)) {
      jsFlags.add(
          "--initial-old-space-size="
              + javaSwitches.get(JavaSwitches.V8_INITIAL_OLD_SPACE_SIZE).replaceAll("[^0-9]", ""));
    } else {
      jsFlags.add("--initial-old-space-size=64");
    }

    if (javaSwitches.containsKey(JavaSwitches.DISABLE_GPU_MEMORY_BUFFER_COMPOSITOR_RESOURCES)) {
      extraCommandLineArgs.add("--disable-gpu-memory-buffer-compositor-resources");
    }

    if (javaSwitches.containsKey(JavaSwitches.GPU_IMAGE_CACHE_LIMIT_ITEMS)) {
      String limit =
          javaSwitches.get(JavaSwitches.GPU_IMAGE_CACHE_LIMIT_ITEMS).replaceAll("[^0-9]", "");
      extraCommandLineArgs.add("--cc-image-cache-limit-items=" + limit);
    }
    if (javaSwitches.containsKey(JavaSwitches.DECODED_IMAGE_WORKING_SET_BUDGET_BYTES)) {
      String budget =
          javaSwitches
              .get(JavaSwitches.DECODED_IMAGE_WORKING_SET_BUDGET_BYTES)
              .replaceAll("[^0-9]", "");
      extraCommandLineArgs.add("--decoded-image-working-set-budget-bytes=" + budget);
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_SCALING_CLIPPED_IMAGES)) {
      extraCommandLineArgs.add("--enable-scaling-clipped-images");
    }

    StringJoiner mojoPipeParams = new StringJoiner("/");
    String subresourceSize =
        javaSwitches.get(JavaSwitches.COBALT_DYNAMIC_MOJO_PIPE_SUBRESOURCE_SIZE);
    if (subresourceSize != null) {
      String size = subresourceSize.replaceAll("[^0-9]", "");
      if (!size.isEmpty()) {
        mojoPipeParams.add("subresource_size/" + size);
      }
    }

    String mediaSize = javaSwitches.get(JavaSwitches.COBALT_DYNAMIC_MOJO_PIPE_MEDIA_SIZE);
    if (mediaSize != null) {
      String size = mediaSize.replaceAll("[^0-9]", "");
      if (!size.isEmpty()) {
        mojoPipeParams.add("media_size/" + size);
      }
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_COBALT_DYNAMIC_MOJO_PIPE_SIZING)
        || mojoPipeParams.length() > 0) {
      if (mojoPipeParams.length() > 0) {
        extraCommandLineArgs.add(
            "--enable-features=CobaltDynamicMojoPipeSizing:" + mojoPipeParams.toString());
      } else {
        extraCommandLineArgs.add("--enable-features=CobaltDynamicMojoPipeSizing");
      }
    }

    StringJoiner featureParams = new StringJoiner("/");
    if (javaSwitches.containsKey(JavaSwitches.INTEREST_AREA_SIZE_IN_PIXELS)) {
      String size =
          javaSwitches.get(JavaSwitches.INTEREST_AREA_SIZE_IN_PIXELS).replaceAll("[^0-9]", "");
      featureParams.add("size_in_pixels/" + size);
    }

    if (javaSwitches.containsKey(JavaSwitches.RECLAIM_DELAY_IN_SECONDS)) {
      String delay =
          javaSwitches.get(JavaSwitches.RECLAIM_DELAY_IN_SECONDS).replaceAll("[^0-9]", "");
      featureParams.add("reclaim_delay_s/" + delay);
    }

    if (featureParams.length() > 0) {
      extraCommandLineArgs.add("--enable-features=SmallerInterestArea:" + featureParams.toString());
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_OPTIMIZED_FONT_LOADING)) {
      extraCommandLineArgs.add("--enable-optimized-font-loading");
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_OPTIMIZED_V8_CODE_CACHE)) {
      extraCommandLineArgs.add("--enable-optimized-v8-code-cache");
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_GPU_SHADER_DISK_CACHE)) {
      extraCommandLineArgs.add("--enable-gpu-shader-disk-cache");
    }

    if (javaSwitches.containsKey(JavaSwitches.MAX_HTTP_CACHE_SIZE)) {
      String rawSize = javaSwitches.get(JavaSwitches.MAX_HTTP_CACHE_SIZE);
      if (rawSize != null) {
        String size = rawSize.replaceAll("[^0-9]", "");
        if (!size.isEmpty()) {
          extraCommandLineArgs.add("--max-http-cache-size=" + size);
        }
      }
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_CSS_AND_WASM_FOR_HTTP_CACHE)) {
      extraCommandLineArgs.add("--enable-css-and-wasm-for-http-cache");
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_HTTP_AND_V8_CACHE_TUNING)) {
      extraCommandLineArgs.add("--enable-http-and-v8-cache-tuning");
    }

    if (jsFlags.length() > 0) {
      extraCommandLineArgs.add("--js-flags=" + jsFlags.toString());
    }

    if (javaSwitches.containsKey(JavaSwitches.REDUCE_STARBOARD_THREAD_STACK_SIZE)) {
      extraCommandLineArgs.add(
          "--enable-features=" + JavaSwitches.REDUCE_STARBOARD_THREAD_STACK_SIZE);
    }

    if (javaSwitches.containsKey(JavaSwitches.REDUCE_ANDROID_THREAD_STACK_SIZE)) {
      extraCommandLineArgs.add(
          "--enable-features=" + JavaSwitches.REDUCE_ANDROID_THREAD_STACK_SIZE);
    }

    if (javaSwitches.containsKey(JavaSwitches.AVOID_CC_REUSE_RESOURCE)) {
      extraCommandLineArgs.add("--avoid-cc-reuse-resource");
    }

    if (javaSwitches.containsKey(JavaSwitches.COBALT_BYPASS_BUFFERING_BYTES_CONSUMER)) {
      extraCommandLineArgs.add(
          "--enable-features=" + JavaSwitches.COBALT_BYPASS_BUFFERING_BYTES_CONSUMER);
    }

    if (javaSwitches.containsKey(JavaSwitches.DIRECT_WINDOW_RENDERING)) {
      extraCommandLineArgs.add("--use-window-surface-for-ui");
      extraCommandLineArgs.add("--enable-h5vcc-settings=Media.ForceClearSurfaceView=1");
    }

    if (javaSwitches.containsKey(JavaSwitches.AREA_BASED_VIDEO_BUFFER_BUDGET)) {
      extraCommandLineArgs.add("--enable-features=AreaBasedVideoBufferBudget");
    }

    if (javaSwitches.containsKey(JavaSwitches.ALLOW_CRITICAL_MEMORY_PRESSURE_HANDLING_IN_FOREGROUND)) {
      extraCommandLineArgs.add("--allow-critical-memory-pressure-handling-in-foreground");
    }

    if (javaSwitches.containsKey(JavaSwitches.EVICT_MEMORY_CACHE_ON_CRITICAL_MEMORY_PRESSURE)) {
      extraCommandLineArgs.add("--evict-memory-cache-on-critical-memory-pressure");
    }

    return extraCommandLineArgs;
  }
}
