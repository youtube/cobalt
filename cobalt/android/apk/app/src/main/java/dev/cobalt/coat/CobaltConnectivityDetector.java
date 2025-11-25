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
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import org.chromium.content_public.browser.WebContents;

/** Detects network connectivity with a guaranteed timeout. */
public class CobaltConnectivityDetector {
  private static final String TAG = "CobaltConnectivityDetector";
  private static final int NETWORK_CHECK_TIMEOUT_MS = 5000; // 5-second overall timeout.
  private static final String DEFAULT_PROBE_URL = "https://www.google.com/generate_204";
  private static final String FALLBACK_PROBE_URL =
      "http://connectivitycheck.gstatic.com/generate_204";
  private static final String[] PROBE_URLS = {DEFAULT_PROBE_URL, FALLBACK_PROBE_URL};

  private final CobaltActivity activity;
  private PlatformError platformError;
  protected boolean mShouldReloadOnResume = false;

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
              ExecutorService probeExecutor = Executors.newSingleThreadExecutor();
              try {
                Callable<Boolean> networkProbe =
                    () -> {
                      for (String urlString : PROBE_URLS) {
                        if (Thread.currentThread().isInterrupted()) {
                          return false;
                        }
                        if (performSingleProbe(urlString)) {
                          return true;
                        }
                      }
                      return false;
                    };

                Future<Boolean> probeFuture = probeExecutor.submit(networkProbe);
                boolean success =
                    probeFuture.get(NETWORK_CHECK_TIMEOUT_MS, TimeUnit.MILLISECONDS);

                if (success) {
                  handleSuccess();
                } else {
                  handleFailure();
                }
              } catch (TimeoutException e) {
                handleFailure();
              } catch (Exception e) {
                if (e instanceof InterruptedException) {
                  Thread.currentThread().interrupt(); // Preserve interrupt status.
                }
                Log.w(TAG, "Connectivity check failed with exception: " + e.getClass().getName(), e);
                handleFailure();
              } finally {
                // This is crucial. It ensures the probe thread is discarded.
                probeExecutor.shutdownNow();
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
          if (mShouldReloadOnResume) {
            WebContents webContents = activity.getActiveWebContents();
            if (webContents != null) {
              webContents.getNavigationController().reload(true);
            }
            mShouldReloadOnResume = false;
          }
        });
  }

  private void handleFailure() {
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
      // Shorter timeouts for connect/read, the overall timeout is the main guard.
      urlConnection.setConnectTimeout(4000);
      urlConnection.setReadTimeout(4000);
      urlConnection.setInstanceFollowRedirects(false);
      urlConnection.setRequestMethod("GET");
      urlConnection.setUseCaches(false);
      urlConnection.connect();
      int responseCode = urlConnection.getResponseCode();
      // We just need a valid response, not necessarily 204.
      return responseCode >= 200 && responseCode < 400;
    } catch (IOException e) {
      return false;
    } finally {
      if (urlConnection != null) {
        urlConnection.disconnect();
      }
    }
  }

  public void destroy() {
    managementExecutor.shutdownNow();
  }
}
