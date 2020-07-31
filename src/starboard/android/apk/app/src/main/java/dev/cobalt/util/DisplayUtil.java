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

import android.content.Context;
import android.content.res.Resources;
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

  /**
   * Returns the default display associated with a context.
   */
  @Nullable
  public static Display getDefaultDisplay(Context context) {
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
   * Returns the physical pixels per inch of the screen in the X and Y
   * dimensions.
   */
  public static SizeF getDisplayDpi(Context context) {
    DisplayMetrics metrics = getDisplayMetrics(context);
    return new SizeF(metrics.xdpi, metrics.ydpi);
  }

  /**
   * Returns the size of the physical display size in pixels.
   *
   * <p>This differs from {@link #getSystemDisplaySize(Context)} because it only uses
   * {@link DisplayMetrics}.
   */
  public static Size getDisplaySize(Context context) {
    DisplayMetrics metrics = getDisplayMetrics(context);
    return new Size(metrics.widthPixels, metrics.heightPixels);
  }

  /**
   * Returns the size of the current physical display size in pixels.
   *
   * <p>This differs from {@link #getDisplaySize(Context)} because it allows the
   * system property "sys.display-size" to override {@link DisplayMetrics}.
   */
  public static Size getSystemDisplaySize(Context context) {
    Size widthAndHeightPx = getSystemDisplayWidthAndHeightPxInternal();
    if (widthAndHeightPx == null) {
      widthAndHeightPx = getDisplaySize(context);
    }
    return widthAndHeightPx;
  }

  /**
   * Returns the size of the current physical display size in pixels.
   * or {@code null} if unavailable.
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

  private static DisplayMetrics cachedDisplayMetrics = null;

  private static DisplayMetrics getDisplayMetrics(Context context) {
    Resources.getSystem().getDisplayMetrics();
    if (cachedDisplayMetrics == null) {
      cachedDisplayMetrics = new DisplayMetrics();
      getDefaultDisplay(context).getRealMetrics(cachedDisplayMetrics);
    }
    return cachedDisplayMetrics;
  }
}
