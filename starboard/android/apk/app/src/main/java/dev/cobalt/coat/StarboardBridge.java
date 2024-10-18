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

import static dev.cobalt.util.Log.TAG;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import androidx.annotation.Nullable;
import dev.cobalt.util.Log;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashMap;
import java.util.TimeZone;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Mock Starboard bridge
 */
public class StarboardBridge {

  /** Interface to be implemented by the Android Application hosting the starboard app. */
  public interface HostApplication {
    void setStarboardBridge(StarboardBridge starboardBridge);

    StarboardBridge getStarboardBridge();
  }

  interface JavaScriptCallback {
    void onStringResult(String result);
  }

  private NetworkStatus networkStatus;
  private AdvertisingId advertisingId;
  private final VolumeStateReceiver volumeStateReceiver;
  private final CobaltSystemConfigChangeReceiver sysConfigChangeReceiver;

  private final Context appContext;
  private final String[] args;
  private String startDeepLink;
  private final Runnable stopRequester =
      new Runnable() {
        @Override
        public void run() {
          requestStop(0);
        }
      };

  private final HashMap<String, CobaltService.Factory> cobaltServiceFactories = new HashMap<>();
  private final HashMap<String, CobaltService> cobaltServices = new HashMap<>();

  private static final String AMATI_EXPERIENCE_FEATURE =
      "com.google.android.feature.AMATI_EXPERIENCE";
  private final boolean isAmatiDevice;
  private static final TimeZone DEFAULT_TIME_ZONE = TimeZone.getTimeZone("America/Los_Angeles");
  private final long timeNanosecondsPerMicrosecond = 1000;

  private boolean starboardApplicationReady = true;

  private ExecutorService executor;

  private JavaScriptCallback evalJavaScriptCallback;
  private final Handler mainHandler = new Handler(Looper.getMainLooper());

  public StarboardBridge(Context appContext, String[] args, String startDeepLink) {

    this.appContext = appContext;
    this.args = args;
    this.startDeepLink = startDeepLink;
    this.sysConfigChangeReceiver = new CobaltSystemConfigChangeReceiver(appContext, stopRequester);
    this.networkStatus = new NetworkStatus(appContext);
    this.advertisingId = new AdvertisingId(appContext);
    this.volumeStateReceiver = new VolumeStateReceiver(appContext);
    this.isAmatiDevice = appContext.getPackageManager().hasSystemFeature(AMATI_EXPERIENCE_FEATURE);

    this.executor = Executors.newFixedThreadPool(2);
  }

  public Executor getExecutor() {
    return this.executor;
  }

  protected void onActivityStart(Activity activity) {
    sysConfigChangeReceiver.setForeground(true);
  }

  protected void onActivityStop(Activity activity) {
    sysConfigChangeReceiver.setForeground(false);
  }

  protected void onActivityDestroy(Activity activity) {
    Log.i(TAG, "Activity destroyed after shutdown; killing app.");
    System.exit(0);
  }


  public void requestStop(int errorLevel) {
      Log.i(TAG, "Request to stop");
  }

  /** Sends an event to the web app to navigate to the given URL */
  public void handleDeepLink(String url) {
    if (starboardApplicationReady) {
      //nativeHandleDeepLink(url);
    } else {
      // If this deep link event is received before the starboard application
      // is ready, it replaces the start deep link.
      startDeepLink = url;
    }
  }

  @Nullable
  private static String getSystemProperty(String name) {
    try {
      @SuppressLint("PrivateApi")
      Class<?> systemProperties = Class.forName("android.os.SystemProperties");
      Method getMethod = systemProperties.getMethod("get", String.class);
      return (String) getMethod.invoke(systemProperties, name);
    } catch (Exception e) {
      Log.e(TAG, "Failed to read system property " + name, e);
      return null;
    }
  }

  @SuppressWarnings("unused")
  boolean isNetworkConnected() {
    return networkStatus.isConnected();
  }

    /** Returns string for kSbSystemPropertyAdvertisingId */
    protected String getAdvertisingId() {
      return this.advertisingId.getId();
    }

    /** Returns boolean for kSbSystemPropertyLimitAdTracking */
    protected boolean getLimitAdTracking() {
      return this.advertisingId.isLimitAdTrackingEnabled();
    }


  /** Returns string for kSbSystemPropertyUserAgentAuxField */
  @SuppressWarnings("unused")
  protected String getUserAgentAuxField() {
    StringBuilder sb = new StringBuilder();

    String packageName = appContext.getApplicationInfo().packageName;
    sb.append(packageName);
    sb.append('/');

    try {
      sb.append(appContext.getPackageManager().getPackageInfo(packageName, 0).versionName);
    } catch (PackageManager.NameNotFoundException ex) {
      // Should never happen
      Log.e(TAG, "Can't find our own package", ex);
    }

    return sb.toString();
  }


  public void registerCobaltService(CobaltService.Factory factory) {
    Log.i(TAG, "Registered " + factory.getServiceName());
    cobaltServiceFactories.put(factory.getServiceName(), factory);
  }

  @SuppressWarnings("unused")
  boolean hasCobaltService(String serviceName) {
    boolean weHaveIt =cobaltServiceFactories.get(serviceName) != null;
    Log.e(TAG, "From bridge, hasCobaltService:" + serviceName + " got? : " + weHaveIt);
    return weHaveIt;
  }

  public void setJavaScriptCallback(JavaScriptCallback callback) {
    this.evalJavaScriptCallback = callback;
  }

  public void callbackFromService(long name, String foo) {
    mainHandler.post(
        () -> {
          this.evalJavaScriptCallback.onStringResult(
              "window.H5vccPlatformService.callback_from_android(" + name + ",'" + foo + "');");
        });
  }

  @SuppressWarnings("unused")
  CobaltService openCobaltService(long nativeService, String serviceName) {
    if (cobaltServices.get(serviceName) != null) {
      // Attempting to re-open an already open service fails.
      Log.e(TAG, String.format("Cannot open already open service %s", serviceName));
      return null;
    }
    final CobaltService.Factory factory = cobaltServiceFactories.get(serviceName);
    if (factory == null) {
      Log.e(TAG, String.format("Cannot open unregistered service %s", serviceName));
      return null;
    }
    CobaltService service = factory.createCobaltService(nativeService);
    if (service != null) {
      service.setCallback(this::callbackFromService);
      cobaltServices.put(serviceName, service);
    }
    return service;
  }

  public CobaltService getOpenedCobaltService(String serviceName) {
    return cobaltServices.get(serviceName);
  }

  @SuppressWarnings("unused")
  void closeCobaltService(String serviceName) {
    Log.i(TAG, String.format("Close service: %s", serviceName));
    cobaltServices.remove(serviceName);
  }

  // Differing impl
  void sendToCobaltService(String serviceName, byte [] data) {
    Log.i(TAG, String.format("Send to : %s data: %s", serviceName, Arrays.toString(data)));
    CobaltService service = cobaltServices.get(serviceName);
    if (service == null) {
      // Attempting to re-open an already open service fails.
      Log.e(TAG, String.format("Service not opened: %s", serviceName));
      return;
    }
    service.receiveFromClient(data);
  }

  @SuppressWarnings("unused")
  protected boolean getIsAmatiDevice() {
    return this.isAmatiDevice;
  }

  @SuppressWarnings("unused")
  protected String getBuildFingerprint() {
    return Build.FINGERPRINT;
  }
}
