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

package dev.cobalt.util;

import android.app.Activity;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.util.DisplayMetrics;
import android.util.Size;
import android.util.SizeF;
import android.view.Display;
import android.view.WindowManager;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

/** Utility functions for querying display attributes. */
public class DisplayUtil {

  private DisplayUtil() {}

  private static Display defaultDisplay;
  private static DisplayMetrics cachedDisplayMetrics = null;

  public static final double DISPLAY_REFRESH_RATE_UNKNOWN = -1;

  /** Returns the physical pixels per inch of the screen in the X and Y dimensions. */
  public static SizeF getDisplayDpi() {
    DisplayMetrics metrics = getDisplayMetrics();
    return new SizeF(metrics.xdpi, metrics.ydpi);
  }

  /** Returns the default display associated with a context. */
  @Nullable
  public static Display getDefaultDisplay() {
    synchronized (DisplayUtil.class) {
      if (defaultDisplay != null && !defaultDisplay.isValid()) {
        return null;
      }

      return defaultDisplay;
    }
  }

  /** Returns the refresh rate of the default display. */
  public static double getDefaultDisplayRefreshRate() {
    Display defaultDisplay = getDefaultDisplay();
    return defaultDisplay != null ? defaultDisplay.getRefreshRate() : DISPLAY_REFRESH_RATE_UNKNOWN;
  }

  /**
   * Cache the default display and display metrics, this will be triggered when a NativeActivity
   * starts.
   */
  public static void cacheDefaultDisplay(Context context) {
    Display display = getDisplayFromContext(context);
    synchronized (DisplayUtil.class) {
      defaultDisplay = display;
      cachedDisplayMetrics = ((Activity) context).getResources().getDisplayMetrics();
    }
  }

  /**
   * Returns the default display associated with a context. When API Level >= 30, pass {@link
   * android.app.Activity} for the context parameter.
   */
  @Nullable
  public static Display getDisplayFromContext(Context context) {
    if (context == null) {
      return null;
    }
    if (android.os.Build.VERSION.SDK_INT >= 30) {
      return getDefaultDisplayV30(context);
    } else {
      return getDefaultDisplayDeprecated(context);
    }
  }

  @Nullable
  @SuppressWarnings("deprecation")
  private static Display getDefaultDisplayDeprecated(Context context) {
    WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
    return (wm == null) ? null : wm.getDefaultDisplay();
  }

  @Nullable
  @RequiresApi(30)
  private static Display getDefaultDisplayV30(Context context) {
    return context.getDisplay();
  }

  /**
   * Returns the size of the physical display size in pixels.
   *
   * <p>This differs from {@link #getSystemDisplaySize()} because it only uses {@link
   * DisplayMetrics}.
   */
  public static Size getDisplaySize() {
    DisplayMetrics metrics = getDisplayMetrics();
    return new Size(metrics.widthPixels, metrics.heightPixels);
  }

  /**
   * Returns the size of the current physical display size in pixels.
   *
   * <p>This differs from {@link #getDisplaySize()} because it allows the system property
   * "sys.display-size" to override {@link DisplayMetrics}.
   */
  public static Size getSystemDisplaySize() {
    Size widthAndHeightPx = getSystemDisplayWidthAndHeightPxInternal();
    if (widthAndHeightPx == null) {
      widthAndHeightPx = getDisplaySize();
    }
    return widthAndHeightPx;
  }

  /**
   * Returns the size of the current physical display size in pixels. or {@code null} if
   * unavailable.
   */
  private static Size getSystemDisplayWidthAndHeightPxInternal() {
    final String displaySize = SystemPropertiesHelper.getString("sys.display-size");
    if (displaySize != null) {
      final String[] widthAndHeightPx = displaySize.split("x");
      if (widthAndHeightPx.length == 2) {
        try {
          return new Size(
              Integer.parseInt(widthAndHeightPx[0]), Integer.parseInt(widthAndHeightPx[1]));
        } catch (NumberFormatException exception) {
          // pass
        }
      }
    }
    return null;
  }

  private static DisplayMetrics getDisplayMetrics() {
    return cachedDisplayMetrics;
  }

  private static DisplayListener displayerListener =
      new DisplayListener() {
        @Override
        public void onDisplayAdded(int displayId) {
          // nativeOnDisplayChanged();
        }

        @Override
        public void onDisplayChanged(int displayId) {
          // nativeOnDisplayChanged();
        }

        @Override
        public void onDisplayRemoved(int displayId) {
          // nativeOnDisplayChanged();
        }
      };

  private static boolean displayerListenerAdded = false;

  public static void addDisplayListener(Context context) {
    if (displayerListenerAdded) {
      return;
    }

    DisplayManager displayManager = context.getSystemService(DisplayManager.class);
    displayManager.registerDisplayListener(displayerListener, null);
    displayerListenerAdded = true;

    // Call nativeOnDisplayChanged() to reload supported hdr types here after a default
    // Display created.
    // nativeOnDisplayChanged();
  }

  // private static native void nativeOnDisplayChanged();
}
