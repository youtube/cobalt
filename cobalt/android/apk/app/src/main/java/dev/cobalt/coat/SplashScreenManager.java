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

package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import dev.cobalt.util.Log;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.util.Scanner;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Manages the downloading, caching, and updating of a splash screen video based on a web app
 * manifest.
 */
public class SplashScreenManager {

  public static final String PURPOSE_VIDEO_SPLASH = "yt-videosplash";

  private final Context appContext;

  /** A helper class to store information parsed from the web app manifest. */
  private static class ManifestInfo {
    final String version;
    final String videoUrl;

    ManifestInfo(String version, String videoUrl) {
      this.version = version;
      this.videoUrl = videoUrl;
    }
  }

  public SplashScreenManager(Context context) {
    this.appContext = context;
  }

  /**
   * Downloads a splash video based on a manifest URL, with caching.
   *
   * <p>This method initiates a background thread to: 1. Check for a locally cached manifest and
   * read its version. 2. Download the latest manifest from the network. 3. Compare the new
   * manifest's version with the cached version. 4. If the versions are different or the video file
   * is missing, it downloads the new video. 5. If the video download is successful, it caches the
   * new manifest.
   *
   * @param manifestUrl The URL of the web app manifest file.
   */
  public void updateFromManifest(String manifestUrl) {
    new Thread(
            () -> {
              File cachedManifestFile = new File(appContext.getCacheDir(), "manifest.json");
              File videoFile = new File(appContext.getExternalCacheDir(), "splash.webm");
              String oldVersion = null;

              // 1. Read old version from cached manifest if it exists.
              if (cachedManifestFile.exists()) {
                try {
                  String oldManifestJson = readStringFromFile(cachedManifestFile);
                  ManifestInfo oldInfo = parseManifestForInfo(oldManifestJson);
                  if (oldInfo != null) {
                    oldVersion = oldInfo.version;
                  }
                } catch (IOException e) {
                  Log.w(TAG, "Could not read cached manifest.", e);
                }
              }

              // 2. Download and parse new manifest.
              try {
                String newManifestJson = downloadManifest(manifestUrl);
                if (newManifestJson == null) {
                  return;
                }
                ManifestInfo newInfo = parseManifestForInfo(newManifestJson);
                if (newInfo == null || newInfo.videoUrl == null) {
                  Log.w(TAG, "New manifest is missing version or video URL.");
                  return;
                }

                // 3. Compare versions and check if video file exists.
                // Skip download only if versions match, are not empty, and the
                // video exists.
                if (!newInfo.version.isEmpty()
                    && newInfo.version.equals(oldVersion)
                    && videoFile.exists()) {
                  Log.i(
                      TAG,
                      "Manifest version is the same and video exists." + " Skipping download.");
                  return;
                }

                // 4. Download the video.
                URI baseUri = new URI(manifestUrl);
                URI resolvedUri = baseUri.resolve(newInfo.videoUrl);
                boolean downloadSucceeded = downloadSplashVideo(resolvedUri.toString());

                // 5. If download succeeded, cache the new manifest.
                if (downloadSucceeded) {
                  writeStringToFile(cachedManifestFile, newManifestJson);
                  Log.i(TAG, "Successfully downloaded splash video and cached new" + " manifest.");
                }
              } catch (Exception e) {
                Log.e(TAG, "Error processing splash video manifest", e);
              }
            })
        .start();
  }

  private String downloadManifest(String urlString) throws IOException {
    URL url = new URL(urlString);
    HttpURLConnection connection = (HttpURLConnection) url.openConnection();
    try {
      connection.connect();

      if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
        Log.e(
            TAG,
            "Server returned HTTP "
                + connection.getResponseCode()
                + " "
                + connection.getResponseMessage());
        return null;
      }

      try (InputStream input = connection.getInputStream();
          Scanner scanner = new Scanner(input, "UTF-8")) {
        return scanner.useDelimiter("\\A").next();
      }
    } finally {
      connection.disconnect();
    }
  }

  private boolean isSplashVideo(JSONObject icon) {
    String type = icon.optString("type");
    if (type == null || !type.startsWith("video/webm")) {
      return false;
    }

    String src = icon.optString("src");
    if (src == null || !src.endsWith(".webm")) {
      return false;
    }

    String purpose = icon.optString("purpose");
    if (purpose == null || !purpose.contains(PURPOSE_VIDEO_SPLASH)) {
      return false;
    }
    return true;
  }

  private ManifestInfo parseManifestForInfo(String manifestJson) {
    try {
      JSONObject manifest = new JSONObject(manifestJson);
      String version = manifest.optString("version", "");
      String videoUrl = null;

      if (manifest.has("icons")) {
        JSONArray icons = manifest.getJSONArray("icons");
        for (int i = 0; i < icons.length(); i++) {
          JSONObject icon = icons.getJSONObject(i);
          if (isSplashVideo(icon)) {
            return new ManifestInfo(version, icon.optString("src"));
          }
        }
      }
      return null;
    } catch (JSONException e) {
      Log.e(TAG, "Error parsing manifest JSON", e);
      return null;
    }
  }

  private boolean downloadSplashVideo(String urlString) {
    try {
      URL url = new URL(urlString);
      HttpURLConnection connection = (HttpURLConnection) url.openConnection();
      connection.connect();

      if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
        Log.e(
            TAG,
            "Server returned HTTP "
                + connection.getResponseCode()
                + " "
                + connection.getResponseMessage());
        return false;
      }

      File cacheDir = appContext.getExternalCacheDir();
      File videoFile = new File(cacheDir, "splash.webm");

      try (InputStream input = connection.getInputStream();
          OutputStream output = new FileOutputStream(videoFile)) {
        byte[] buffer = new byte[4096];
        int bytesRead;
        while ((bytesRead = input.read(buffer)) != -1) {
          output.write(buffer, 0, bytesRead);
        }
        Log.i(
            TAG,
            "Successfully downloaded and cached splash.webm to " + videoFile.getAbsolutePath());
        return true;
      }
    } catch (Exception e) {
      Log.e(TAG, "Error downloading splash video", e);
      return false;
    }
  }

  private void writeStringToFile(File file, String content) throws IOException {
    try (OutputStream outputStream = new FileOutputStream(file)) {
      outputStream.write(content.getBytes("UTF-8"));
    }
  }

  private String readStringFromFile(File file) throws IOException {
    try (InputStream inputStream = new FileInputStream(file);
        Scanner scanner = new Scanner(inputStream, "UTF-8")) {
      return scanner.useDelimiter("\\A").next();
    }
  }
}
