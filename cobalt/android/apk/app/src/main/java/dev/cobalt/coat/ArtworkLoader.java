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

package dev.cobalt.coat;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Pair;
import android.util.Size;
import androidx.annotation.CheckResult;
import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.DisplayUtil;
import java.util.List;
import org.chromium.services.media_session.MediaImage;

/** Loads MediaImage artwork, and caches one image. */
public class ArtworkLoader {

  /**
   * Fallback for the framework's config_mediaMetadataBitmapMaxSize, in dp.
   * MediaSession.setMetadata() scales larger artwork down to that size, so no consumer sees more.
   * Scaling here instead keeps the held bitmap small, and lets MediaSession keep the shared bitmap
   * from toShared() as is, since its own scaled copy would be a mutable bitmap that every parcel
   * copies into new ashmem.
   */
  private static final int DEFAULT_MAX_ARTWORK_SIZE_DP = 320;

  /** Callback to receive the image loaded in the background by getOrLoadArtwork() */
  public interface Callback {
    void onArtworkLoaded(Bitmap bitmap);
  }

  private final Object mLock = new Object();

  @GuardedBy("mLock")
  @VisibleForTesting
  @NonNull
  String mRequestedArtworkUrl = "";

  @GuardedBy("mLock")
  @VisibleForTesting
  @NonNull
  String mCurrentArtworkUrl = "";

  @GuardedBy("mLock")
  @VisibleForTesting
  Bitmap mCurrentArtwork = null;

  private final Handler mHandler = new Handler(Looper.getMainLooper());
  private final ArtworkDownloader mArtworkDownloader;
  private final Callback mCallback;

  /**
   * Constructs a new ArtworkLoader.
   *
   * @param callback The callback to receive the loaded image.
   * @param artworkDownloader The downloader to use for fetching the image.
   */
  public ArtworkLoader(Callback callback, ArtworkDownloader artworkDownloader) {
    this.mCallback = callback;
    this.mArtworkDownloader = artworkDownloader;
  }

  /**
   * Returns a cached image if available. If not cached, returns null and starts downloading it in
   * the background, and then when ready the callback will be called with the image.
   */
  public Bitmap getOrLoadArtwork(List<MediaImage> images) {
    if (images == null || images.isEmpty()) {
      return null;
    }

    MediaImage image = getBestFitImage(images, DisplayUtil.getDisplaySize());
    String url = image.getSrc().getSpec();

    synchronized (mLock) {
      // Check if this artwork is already loaded or requested.
      if (url.equals(mCurrentArtworkUrl)) {
        return mCurrentArtwork;
      } else if (url.equals(mRequestedArtworkUrl)) {
        return null;
      }

      mRequestedArtworkUrl = url;
    }

    new DownloadArtworkThread(url, this).start();
    return null;
  }

  /**
   * Returns the image that most closely matches the display size, or null if there are no images.
   * We don't really know what size view the image may appear in (on the Now Playing card on Android
   * TV launcher, or any other observer of the MediaSession), so we use display size as the largest
   * useful size on any particular device.
   */
  private MediaImage getBestFitImage(List<MediaImage> images, Size displaySize) {
    MediaImage bestImage = images.get(0);
    int minDiagonalSquared = Integer.MAX_VALUE;
    for (MediaImage image : images) {
      if (image.getSizes().isEmpty()) {
        continue;
      }
      Rect imageRect = image.getSizes().get(0);
      int widthDelta = displaySize.getWidth() - imageRect.width();
      int heightDelta = displaySize.getHeight() - imageRect.height();
      int diagonalSquared = widthDelta * widthDelta + heightDelta * heightDelta;
      if (diagonalSquared < minDiagonalSquared) {
        bestImage = image;
        minDiagonalSquared = diagonalSquared;
      }
    }
    return bestImage;
  }

  /**
   * Crops the bitmap to 16:9 aspect ratio if necessary.
   *
   * <p>If the bitmap is taller than 16:9, it is cropped from the center. If it is already 16:9 or
   * wider, it is returned as is.
   *
   * <p>The input bitmap is recycled if a new cropped bitmap is created. NOTE: This method is
   * accessed from google3(http://shortn/_SoIqBrEPhR).
   *
   * @param bitmap The source bitmap.
   * @return The 16:9 cropped bitmap, or the original bitmap.
   */
  @CheckResult
  public Bitmap cropTo16x9(Bitmap bitmap) {
    // Crop to 16:9 as needed
    if (bitmap == null) {
      return null;
    }
    int height = bitmap.getWidth() * 9 / 16;
    if (bitmap.getHeight() <= height) {
      return bitmap;
    }

    int top = (bitmap.getHeight() - height) / 2;
    Bitmap cropped = Bitmap.createBitmap(bitmap, 0, top, bitmap.getWidth(), height);
    bitmap.recycle();
    return cropped;
  }

  /**
   * Called when an artwork download has finished.
   *
   * @param urlBitmapPair A pair containing the URL and the downloaded Bitmap.
   */
  public void onDownloadFinished(Pair<String, Bitmap> urlBitmapPair) {
    String url = urlBitmapPair.first;
    boolean isRequested;
    synchronized (mLock) {
      isRequested = mRequestedArtworkUrl.equals(url);
    }
    if (!isRequested) {
      recycleIfNotNull(urlBitmapPair.second);
      return;
    }
    // Scale outside the lock; the request is checked again below before publishing.
    final Bitmap bitmap = toShared(scaleDownToMaxSize(urlBitmapPair.second, getMaxArtworkSizePx()));

    synchronized (mLock) {
      if (!mRequestedArtworkUrl.equals(url)) {
        recycleIfNotNull(bitmap);
        return;
      }

      mRequestedArtworkUrl = "";

      if (bitmap == null) {
        return;
      }

      final Bitmap oldArtwork = mCurrentArtwork;
      mCurrentArtworkUrl = url;
      mCurrentArtwork = bitmap;

      // Post under the lock so callbacks run in the order the artwork was replaced; otherwise a
      // later download's callback could recycle this bitmap before this callback delivers it.
      mHandler.post(
          new Runnable() {
            @Override
            public void run() {
              try {
                mCallback.onArtworkLoaded(bitmap);
              } finally {
                recycleIfNotNull(oldArtwork);
              }
            }
          });
    }
  }

  private static void recycleIfNotNull(Bitmap bitmap) {
    if (bitmap != null) {
      bitmap.recycle();
    }
  }

  /** Returns the framework's MediaSession bitmap size limit, honoring OEM overlays. */
  @VisibleForTesting
  static int getMaxArtworkSizePx() {
    Resources res = Resources.getSystem();
    int id = res.getIdentifier("config_mediaMetadataBitmapMaxSize", "dimen", "android");
    if (id != 0) {
      try {
        return res.getDimensionPixelSize(id);
      } catch (Resources.NotFoundException e) {
        // Fall through to the default.
      }
    }
    return Math.round(DEFAULT_MAX_ARTWORK_SIZE_DP * res.getDisplayMetrics().density);
  }

  /**
   * Scales the bitmap down, keeping its aspect ratio, so neither dimension exceeds maxSizePx.
   *
   * <p>The input bitmap is recycled if a new scaled bitmap is created.
   *
   * @param bitmap The source bitmap, or null.
   * @param maxSizePx The largest allowed width or height, in pixels.
   * @return The scaled bitmap, or the original bitmap if it already fits.
   */
  @CheckResult
  @VisibleForTesting
  static Bitmap scaleDownToMaxSize(Bitmap bitmap, int maxSizePx) {
    if (bitmap == null) {
      return null;
    }
    int largestSide = Math.max(bitmap.getWidth(), bitmap.getHeight());
    if (maxSizePx <= 0 || largestSide <= maxSizePx) {
      return bitmap;
    }
    float scale = (float) maxSizePx / largestSide;
    int width = Math.max(1, Math.round(bitmap.getWidth() * scale));
    int height = Math.max(1, Math.round(bitmap.getHeight() * scale));
    Bitmap scaled = Bitmap.createScaledBitmap(bitmap, width, height, /* filter= */ true);
    if (scaled != bitmap) {
      bitmap.recycle();
    }
    return scaled;
  }

  /**
   * Returns an immutable copy backed by shared memory (API 31+), recycling the input.
   *
   * <p>Parceling an immutable shared-memory bitmap passes its file descriptor instead of copying
   * the pixels into a new ashmem region. MediaSession.setMetadata() keeps a bitmap that is already
   * within its size limit, so the Binder write, and every in-process MediaController that converts
   * the metadata again, no longer needs a fresh contiguous mapping per call.
   */
  @VisibleForTesting
  static Bitmap toShared(Bitmap bitmap) {
    if (bitmap == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
      return bitmap;
    }
    try {
      Bitmap shared = bitmap.asShared();
      if (shared != bitmap) {
        bitmap.recycle();
      }
      return shared;
    } catch (RuntimeException e) {
      // asShared() throws if it can't allocate the shared region; keep the original.
      return bitmap;
    }
  }

  private class DownloadArtworkThread extends Thread {

    private final String mUrl;
    private final ArtworkLoader mArtworkLoader;

    DownloadArtworkThread(String url, ArtworkLoader artworkLoader) {
      super("ArtworkLoader");
      this.mUrl = url;
      this.mArtworkLoader = artworkLoader;
    }

    @Override
    public void run() {
      mArtworkDownloader.downloadArtwork(mUrl, mArtworkLoader);
    }
  }
}
