// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

import dev.cobalt.util.Log;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import org.chromium.content_public.browser.WebContents;

public class CobaltConnectivityDetector {
  private static final String TAG = "CobaltConnectivityDetector";

  private static final int NETWORK_CHECK_TIMEOUT_MS = 5000; 

  private static final String DEFAULT_PROBE_URL = "https://www.google.com/generate_204";
  private static final String FALLBACK_PROBE_URL =
      "http://connectivitycheck.gstatic.com/generate_204";
  private static final String[] PROBE_URLS = {DEFAULT_PROBE_URL, FALLBACK_PROBE_URL};

  private final CobaltActivity activity;
  private PlatformError platformError;
  protected boolean mShouldReloadOnResume = false;
  private volatile boolean mHasEncounteredConnectivityError = false;

  private final ExecutorService managementExecutor = Executors.newSingleThreadExecutor();
  private Future<?> managementFuture;

  public CobaltConnectivityDetector(CobaltActivity activity) {
    this.activity = activity;
  }

  public void activeNetworkCheck() {
    // Cancel any previously running check.
    if (managementFuture != null && !managementFuture.isDone()) {
      managementFuture.cancel(true);
    }

    managementFuture =
        managementExecutor.submit(
            () -> {
              boolean success = false;
              
              for (String urlString : PROBE_URLS) {
                // Check if we were cancelled (e.g. app paused)
                if (Thread.currentThread().isInterrupted()) {
                  return;
                }

                if (performSingleProbe(urlString)) {
                  success = true;
                  break;
                }
              }

              if (success) {
                handleSuccess();
              } else {
                handleFailure();
              }
            });
  }

  private void handleSuccess() {
    activity.runOnUiThread(
        () -> {
          Log.i(TAG, "Active Network check successful." + platformError);
          if (platformError != null) {
            platformError.setResponse(PlatformError.POSITIVE);
            platformError.dismiss();
            platformError = null;
          }
          if (mShouldReloadOnResume && mHasEncounteredConnectivityError) {
            WebContents webContents = activity.getActiveWebContents();
            if (webContents != null) {
              webContents.getNavigationController().reload(true);
            }
            mShouldReloadOnResume = false;
          }
          mHasEncounteredConnectivityError = false; 
        });
  }

  private void handleFailure() {
    // Only set this to true, don't toggle it off here (handled in success)
    mHasEncounteredConnectivityError = true;
    activity.runOnUiThread(
        () -> {
          if (platformError == null || !platformError.isShowing()) {
            platformError =
                new PlatformError(
                    activity.getStarboardBridge().getActivityHolder(),
                    PlatformError.CONNECTION_ERROR,
                    0);
            platformError.raise();
          }
        });
    mShouldReloadOnResume = true;
  }

  public void setShouldReloadOnResume(boolean shouldReload) {
    mShouldReloadOnResume = shouldReload;
  }

  private boolean performSingleProbe(String urlString) {
    HttpURLConnection urlConnection = null;
    try {
      URL url = new URL(urlString);
      urlConnection = (HttpURLConnection) url.openConnection();
      
      // Use the constant for the socket timeout.
      // This ensures we wait 5s for Google, and if that fails, 
      // we wait another 5s for the Fallback.
      urlConnection.setConnectTimeout(NETWORK_CHECK_TIMEOUT_MS);
      urlConnection.setReadTimeout(NETWORK_CHECK_TIMEOUT_MS);
      
      urlConnection.setInstanceFollowRedirects(false);
      urlConnection.setRequestMethod("GET");
      urlConnection.setUseCaches(false);
      urlConnection.connect();
      
      int responseCode = urlConnection.getResponseCode();
      return responseCode >= 200 && responseCode < 400;
    } catch (IOException e) {
      Log.w(TAG, "Probe failed for " + urlString + ": " + e.getMessage());
      return false;
    } finally {
      if (urlConnection != null) {
        urlConnection.disconnect();
      }
    }
  }

  public void destroy() {
    // Ensure we cancel the future before shutting down
    if (managementFuture != null) {
      managementFuture.cancel(true);
    }
    managementExecutor.shutdownNow();
  }
}