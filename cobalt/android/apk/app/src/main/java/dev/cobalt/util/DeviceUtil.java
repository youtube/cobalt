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

package dev.cobalt.util;

import android.content.Context;
import android.os.Build;
import android.util.DisplayMetrics;
import androidx.annotation.VisibleForTesting;
import java.util.Locale;
import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;

/**
 * Utility methods for querying device hardware capabilities and constraints.
 *
 * <p>This class is used to determine hardware-based configurations at runtime, such as adjusting
 * the UI scale factor on low-memory devices.
 *
 * <p>This is a non-instantiable utility class with no instance lifetime or ownership; its static
 * helper methods exist for the lifetime of the application.
 *
 * <p>This class is thread-safe and its static methods can be called from any thread.
 */
public final class DeviceUtil {
  private DeviceUtil() {} // Prevent instantiation.

  // Physical RAM threshold for 1GB RAM devices in KB (1024 * 1024 KB).
  public static final int ONE_GB_RAM_THRESHOLD_KB = 1024 * 1024;

  // Threshold (in pixels) for the larger display dimension to qualify as at least 1080p.
  // 1600 is chosen as the midpoint between 720p (1280px) and 1080p (1920px). Using the
  // maximum dimension is orientation-independent (landscape vs. portrait) and provides
  // a safe margin for system decor (e.g. navigation/status bars) reducing available pixels.
  public static final int DISPLAY_1080P_MIN_MAX_DIMENSION_PX = 1600;

  // Physical RAM threshold in KB above which a device counts as "3 GB or more". Mirrors the
  // server-side device.Is3GBOrMore condition (DeviceRam > 2673868 KB, i.e. 2.55+ GB).
  public static final int THREE_GB_OR_MORE_RAM_THRESHOLD_KB = 2673868;

  private static Boolean sIs1GbDeviceForTesting;
  private static Boolean sIsDisplayAtLeast1080pForTesting;
  private static Boolean sIsHighEndGpuMemoryDeviceForTesting;

  @VisibleForTesting
  public static void setIs1GbDeviceForTesting(Boolean is1Gb) {
    sIs1GbDeviceForTesting = is1Gb;
  }

  @VisibleForTesting
  public static void setIsDisplayAtLeast1080pForTesting(Boolean is1080p) {
    sIsDisplayAtLeast1080pForTesting = is1080p;
  }

  @VisibleForTesting
  public static void setIsHighEndGpuMemoryDeviceForTesting(Boolean isHighEnd) {
    sIsHighEndGpuMemoryDeviceForTesting = isHighEnd;
  }

  @VisibleForTesting
  public static void resetForTesting() {
    sIs1GbDeviceForTesting = null;
    sIsDisplayAtLeast1080pForTesting = null;
    sIsHighEndGpuMemoryDeviceForTesting = null;
  }

  /**
   * Returns true if the device has 1 GB RAM or less.
   *
   * <p>Note: SysUtils.isLowEndDevice() cannot be used because Cobalt enables
   * --enable-low-end-device-mode by default, which causes isLowEndDevice() to return true for all
   * devices. We query SysUtils.amountOfPhysicalMemoryKB() directly instead.
   */
  public static boolean is1GbDevice() {
    if (sIs1GbDeviceForTesting != null) {
      return sIs1GbDeviceForTesting;
    }
    int physicalRamKb = SysUtils.amountOfPhysicalMemoryKB();
    return physicalRamKb > 0 && physicalRamKb <= ONE_GB_RAM_THRESHOLD_KB;
  }

  /** Returns true if the display resolution is at least 1080p. */
  public static boolean isDisplayAtLeast1080p() {
    if (sIsDisplayAtLeast1080pForTesting != null) {
      return sIsDisplayAtLeast1080pForTesting;
    }
    try {
      Context context = ContextUtils.getApplicationContext();
      if (context != null) {
        DisplayMetrics metrics = context.getResources().getDisplayMetrics();
        return metrics != null
            && Math.max(metrics.widthPixels, metrics.heightPixels)
                >= DISPLAY_1080P_MIN_MAX_DIMENSION_PX;
      }
    } catch (Throwable t) {
      // Ignored if context is not available.
    }
    return false;
  }

  /**
   * Returns true if the device should get the raised GPU memory budget (256 MB) that the server
   * launched for high-end Android TV devices. See b/586564688 and b/549674099.
   *
   * <p>Mirrors the server-side targeting of the launched force_gpu_mem_available_mb=256 value:
   * device.IsNvidiaShield, device.IsGoogleStreamer or device.Is3GBOrMore. Keep the two in sync.
   *
   * <p>Note: SysUtils.isLowEndDevice() cannot be used; see is1GbDevice().
   */
  public static boolean isHighEndGpuMemoryDevice() {
    if (sIsHighEndGpuMemoryDeviceForTesting != null) {
      return sIsHighEndGpuMemoryDeviceForTesting;
    }
    return isHighEndGpuMemoryDevice(
        Build.MANUFACTURER, Build.BRAND, Build.MODEL, SysUtils.amountOfPhysicalMemoryKB());
  }

  @VisibleForTesting
  static boolean isHighEndGpuMemoryDevice(
      String manufacturer, String brand, String model, int physicalRamKb) {
    String deviceModel = toLowerCase(model);
    // device.IsNvidiaShield. Newer SHIELD firmware (e.g. darcy_nova on Android 16) reports
    // "NVIDIA Android TV" instead of "SHIELD Android TV". SHIELD reports ~1.98 GB of RAM, so it is
    // not covered by the RAM threshold below.
    boolean isNvidiaShield =
        isBrand(manufacturer, brand, "nvidia")
            && (deviceModel.equals("shield android tv") || deviceModel.equals("nvidia android tv"));
    // device.IsGoogleStreamer.
    boolean isGoogleStreamer =
        (isBrand(manufacturer, brand, "google") || isBrand(manufacturer, brand, "google inc."))
            && deviceModel.contains("streamer");
    // device.Is3GBOrMore. A positive RAM test, so devices that fail to report RAM (0 or negative)
    // keep the low default.
    boolean is3GbOrMore = physicalRamKb > THREE_GB_OR_MORE_RAM_THRESHOLD_KB;
    return isNvidiaShield || isGoogleStreamer || is3GbOrMore;
  }

  // The server matches on the reported device brand. Build.BRAND and Build.MANUFACTURER agree on
  // the devices targeted here, so accept either.
  private static boolean isBrand(String manufacturer, String brand, String expected) {
    return expected.equals(toLowerCase(manufacturer)) || expected.equals(toLowerCase(brand));
  }

  private static String toLowerCase(String value) {
    return value == null ? "" : value.toLowerCase(Locale.US);
  }
}
