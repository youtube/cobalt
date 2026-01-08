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
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Detects network connectivity with a guaranteed timeout by using a generate_204 ping,
 * a special fast-loading URL to check for internet connectivity, similar to Chrome's
 * ConnectivityDetector.java. This class raises a platform error if it cannot verify a
 * valid internet connection. It also manages the dismissal of the error dialog once a
 * valid connection is established either automatically or through user input.
 */
public class CobaltConnectivityDetector {
  private static final String TAG = "CobaltConnectivityDetector";
  private static final int NETWORK_CHECK_TIMEOUT_MS = 5000; // 5-second overall timeout.
  private static final String DEFAULT_PROBE_URL = "https://www.google.com/generate_204";
  private static final String FALLBACK_PROBE_URL =
      "http://connectivitycheck.gstatic.com/generate_204";
  private static final String[] PROBE_URLS = {DEFAULT_PROBE_URL, FALLBACK_PROBE_URL};

  private final CobaltActivity activity;
  private PlatformError platformError;
  private boolean mAppHasSuccessfullyLoaded = false;
  private boolean mHasVerifiedConnectivity = false;

  private final ExecutorService managementExecutor = Executors.newSingleThreadExecutor();
  private Future<?> managementFuture;
  private final NetworkChangeNotifier.ConnectionTypeObserver mConnectionTypeObserver;

  public CobaltConnectivityDetector(CobaltActivity activity) {
    this.activity = activity;
    mConnectionTypeObserver =
        new NetworkChangeNotifier.ConnectionTypeObserver() {
          @Override
          public void onConnectionTypeChanged(@ConnectionType int connectionType) {
            if (connectionType != ConnectionType.CONNECTION_NONE) {
              // This triggers an activeNetworkCheck() on a new connection type that is not
              // None, which should auto-dismiss the error dialog if the connection is valid.
              activeNetworkCheck();
            }
          }
        };
  }

  public void registerObserver() {
    NetworkChangeNotifier.addConnectionTypeObserver(mConnectionTypeObserver);
  }

  public void activeNetworkCheck() {
    // Cancel any previously running check.
    if (managementFuture != null && !managementFuture.isDone()) {
      managementFuture.cancel(true);
    }

    // Manage a separate timeout to raise a platform error in the case that the connectivity
    // check takes too long ie. a hanging DNS resolution error.
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
              // Don't raise the error dialog if the thread is intentionally interrupted.
              // Otherwise handleFailure() for timeout exceptions and all other exceptions.
              } catch (TimeoutException e) {
                handleFailure();
              } catch (Exception e) {
                if (e instanceof InterruptedException || e instanceof java.util.concurrent.CancellationException) {
                  Thread.currentThread().interrupt(); // Preserve interrupt status.
                } else {
                  Log.w(TAG, "Connectivity check failed with exception: " + e.getClass().getName(), e);
                  handleFailure();
                }
              } finally {
                // Ensuring the probe thread is discarded.
                probeExecutor.shutdownNow();
              }
            });
  }

  private void handleSuccess() {
    mHasVerifiedConnectivity = true;
    activity.runOnUiThread(
        () -> {
          Log.i(TAG, "Active Network check successful." + platformError);
          // Dismiss the error dialog if it's currently visible.
          if (platformError != null) {
            platformError.setResponse(PlatformError.POSITIVE);
            platformError.dismiss();
            platformError = null;
          }
          // The app should only reload if we haven't previously successfully loaded past startup.
          if (!mAppHasSuccessfullyLoaded) {
            WebContents webContents = activity.getActiveWebContents();
            if (webContents != null) {
              webContents.getNavigationController().reload(true);
            }
          }
        });
  }

  // Raise a platform error for any connectivity check failure
  private void handleFailure() {
    mHasVerifiedConnectivity = false;
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
  }

  public void setAppHasSuccessfullyLoaded(boolean hasCompleted) {
    mAppHasSuccessfullyLoaded = hasCompleted;
  }

  public boolean hasVerifiedConnectivity() {
    return mHasVerifiedConnectivity;
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
    NetworkChangeNotifier.removeConnectionTypeObserver(mConnectionTypeObserver);
    managementExecutor.shutdownNow();
  }
}
