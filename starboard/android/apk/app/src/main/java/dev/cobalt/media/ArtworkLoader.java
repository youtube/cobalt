// Copyright 2017 Google Inc. All Rights Reserved.
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

import static dev.cobalt.media.Log.TAG;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;
import android.support.annotation.NonNull;
import android.util.Pair;
import android.util.Size;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/** Loads MediaImage artwork, and caches one image. */
public class ArtworkLoader {

  /** Callback to receive the image loaded in the background by getOrLoadArtwork() */
  public interface Callback {
    void onArtworkLoaded(Bitmap bitmap);
  }

  @NonNull private volatile String requestedArtworkUrl = "";
  @NonNull private volatile String currentArtworkUrl = "";
  private volatile Bitmap currentArtwork = null;

  private final Callback callback;
  private final Size displaySize;

  public ArtworkLoader(Callback callback, Size displaySize) {
    this.callback = callback;
    this.displaySize = displaySize;
  }

  /**
   * Returns a cached image if available. If not cached, returns null and starts downloading it in
   * the background, and then when ready the callback will be called with the image.
   */
  public synchronized Bitmap getOrLoadArtwork(MediaImage[] artwork) {
    MediaImage image = getBestFitImage(artwork);
    String url = (image == null) ? "" : image.src;

    // Check if this artwork is already loaded or requested.
    if (url.equals(currentArtworkUrl)) {
      return currentArtwork;
    } else if (url.equals(requestedArtworkUrl)) {
      return null;
    }

    requestedArtworkUrl = url;
    new DownloadArtworkTask().execute(url);
    return null;
  }

  /**
   * Returns the image that most closely matches the display size, or null if there are no images.
   * We don't really know what size view the image may appear in (on the Now Playing card on Android
   * TV launcher, or any other observer of the MediaSession), so we use display size as the largest
   * useful size on any particular device.
   */
  private MediaImage getBestFitImage(MediaImage[] artwork) {
    if (artwork == null || artwork.length == 0) {
      return null;
    }
    MediaImage bestImage = artwork[0];
    int minDiagonalSquared = Integer.MAX_VALUE;
    for (MediaImage image : artwork) {
      Size imageSize = parseImageSize(image);
      int widthDelta = displaySize.getWidth() - imageSize.getWidth();
      int heightDelta = displaySize.getHeight() - imageSize.getHeight();
      int diagonalSquared = widthDelta * widthDelta + heightDelta * heightDelta;
      if (diagonalSquared < minDiagonalSquared) {
        bestImage = image;
        minDiagonalSquared = diagonalSquared;
      }
    }
    return bestImage;
  }

  private Size parseImageSize(MediaImage image) {
    try {
      String sizeStr = image.sizes.split("\\s+", -1)[0];
      return Size.parseSize(sizeStr.toLowerCase());
    } catch (NumberFormatException | NullPointerException e) {
      return new Size(0, 0);
    }
  }

  private synchronized void onDownloadFinished(Pair<String, Bitmap> urlBitmapPair) {
    String url = urlBitmapPair.first;
    Bitmap bitmap = urlBitmapPair.second;
    if (url.equals(requestedArtworkUrl)) {
      requestedArtworkUrl = "";
      if (bitmap != null) {
        currentArtworkUrl = url;
        currentArtwork = bitmap;
        callback.onArtworkLoaded(bitmap);
      }
    }
  }

  private class DownloadArtworkTask extends AsyncTask<String, Void, Pair<String, Bitmap>> {

    @Override
    protected Pair<String, Bitmap> doInBackground(String... params) {
      String url = params[0];
      Bitmap bitmap = null;
      HttpURLConnection conn = null;
      InputStream is = null;
      try {
        conn = (HttpURLConnection) new URL(url).openConnection();
        is = conn.getInputStream();
        bitmap = BitmapFactory.decodeStream(is);
      } catch (IOException e) {
        android.util.Log.e(TAG, "Could not download artwork", e);
      } finally {
        try {
          if (conn != null) {
            conn.disconnect();
          }
          if (is != null) {
            is.close();
          }
        } catch (Exception e) {
          android.util.Log.e(TAG, "Error closing connection for artwork", e);
        }
      }

      // Crop to 16:9 as needed
      if (bitmap != null) {
        int height = bitmap.getWidth() * 9 / 16;
        if (bitmap.getHeight() > height) {
          int top = (bitmap.getHeight() - height) / 2;
          bitmap = Bitmap.createBitmap(bitmap, 0, top, bitmap.getWidth(), height);
        }
      }

      return Pair.create(url, bitmap);
    }

    @Override
    protected void onPostExecute(Pair<String, Bitmap> urlBitmapPair) {
      onDownloadFinished(urlBitmapPair);
    }

    @Override
    protected void onCancelled(Pair<String, Bitmap> urlBitmapPair) {
      onDownloadFinished(urlBitmapPair);
    }
  }
}
