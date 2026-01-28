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

import static android.content.Context.AUDIO_SERVICE;
import static android.media.AudioManager.GET_DEVICES_INPUTS;
import static dev.cobalt.util.Log.TAG;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Service;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.hardware.input.InputManager;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.util.Size;
import android.util.SizeF;
import android.view.Display;
import android.view.InputDevice;
import android.view.accessibility.CaptioningManager;
import androidx.annotation.Nullable;
import dev.cobalt.media.AudioOutputManager;
import dev.cobalt.shell.StartupGuard;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Holder;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.TimeZone;
import java.util.concurrent.ConcurrentHashMap;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/** Implementation of the required JNI methods called by the Starboard C++ code. */
@JNINamespace("starboard::android::shared")
// TODO(cobalt, b/383301493): we expect this class to be a singleton and should consider enforcing
// this property.
public class StarboardBridge {

  /** Interface to be implemented by the Android Application hosting the starboard app. */
  public interface HostApplication {
    void setStarboardBridge(StarboardBridge starboardBridge);

    StarboardBridge getStarboardBridge();
  }

  private CobaltSystemConfigChangeReceiver sysConfigChangeReceiver;
  private CobaltTextToSpeechHelper ttsHelper;
  // TODO(cobalt): Re-enable these classes or remove if unnecessary.
  private AudioOutputManager audioOutputManager;
  private CobaltMediaSession cobaltMediaSession;
  private AudioPermissionRequester audioPermissionRequester;
  private ResourceOverlay resourceOverlay;
  private AdvertisingId advertisingId;
  private VolumeStateReceiver volumeStateReceiver;

  private final Context appContext;
  private final Holder<Activity> activityHolder;
  private final Holder<Service> serviceHolder;
  private final String[] args;
  private final long nativeApp;
  private final Runnable stopRequester =
      new Runnable() {
        @Override
        public void run() {
          // When the platform locale setting is updated, the application needs
          // to exit or the Accept-Language request header configuration needs
          // to be updated. The Accept-Language header configuration is set on
          // application start.
          //
          // To match cobalt 25, we're exiting the application.
          // https://source.corp.google.com/piper///depot/google3/third_party/cobalt/app/android/coat/branch_25_lts/java/dev/cobalt/coat/StarboardBridge.java;l=96
          afterStopped();
        }
      };

  private volatile boolean applicationStopped;
  private volatile boolean applicationStarted;

  private long mAppStartTimestamp = 0;
  private long mAppStartDuration = 0;

  private final Map<String, CobaltService.Factory> cobaltServiceFactories = new HashMap<>();
  private final Map<String, CobaltService> cobaltServices = new ConcurrentHashMap<>();

  private static final String GOOGLE_PLAY_SERVICES_PACKAGE = "com.google.android.gms";
  private static final String AMATI_EXPERIENCE_FEATURE =
      "com.google.android.feature.AMATI_EXPERIENCE";
  private final boolean isAmatiDevice;
  private static final TimeZone DEFAULT_TIME_ZONE = TimeZone.getTimeZone("America/Los_Angeles");
  private final long timeNanosecondsPerMicrosecond = 1000;

  public StarboardBridge(
      Context appContext,
      Holder<Activity> activityHolder,
      Holder<Service> serviceHolder,
      ArtworkDownloader artworkDownloader,
      String[] args,
      String startDeepLink) {

    Log.i(TAG, "StarboardBridge init.");

    // Make sure the JNI stack is properly initialized first as there is a
    // race condition as soon as any of the following objects creates a new thread.
    StarboardBridgeJni.get().initJNI(this);

    this.appContext = appContext;
    this.activityHolder = activityHolder;
    this.serviceHolder = serviceHolder;
    this.args = args;
    this.sysConfigChangeReceiver = new CobaltSystemConfigChangeReceiver(appContext, stopRequester);
    this.ttsHelper = new CobaltTextToSpeechHelper(appContext);
    this.audioOutputManager = new AudioOutputManager(appContext);
    this.cobaltMediaSession = new CobaltMediaSession(appContext, activityHolder, artworkDownloader);
    this.audioPermissionRequester = new AudioPermissionRequester(appContext, activityHolder);
    this.resourceOverlay = new ResourceOverlay(appContext);
    this.advertisingId = new AdvertisingId(appContext);
    this.volumeStateReceiver = new VolumeStateReceiver(appContext);
    this.isAmatiDevice = appContext.getPackageManager().hasSystemFeature(AMATI_EXPERIENCE_FEATURE);

    nativeApp = StarboardBridgeJni.get().startNativeStarboard(
      getAssetsFromContext(),
      getFilesAbsolutePath(),
      getCacheAbsolutePath(),
      getNativeLibraryDir());

    StarboardBridgeJni.get().handleDeepLink(startDeepLink, /*applicationStarted=*/ false);
    StarboardBridgeJni.get().setAndroidBuildFingerprint(getBuildFingerprint());
    StarboardBridgeJni.get().setAndroidOSExperience(this.isAmatiDevice);
    StarboardBridgeJni.get().setAndroidPlayServicesVersion(getPlayServicesVersion());
  }

  @NativeMethods
  interface Natives {
    long currentMonotonicTime();

    long startNativeStarboard(
      AssetManager assetManager,
      String filesDir,
      String cacheDir,
      String nativeLibraryDir);

    boolean initJNI(StarboardBridge starboardBridge);

    void closeNativeStarboard(long app);

    void initializePlatformAudioSink();

    void handleDeepLink(String url, boolean applicationStarted);

    void setAndroidBuildFingerprint(String fingerprint);
    void setAndroidOSExperience(boolean isAmatiDevice);
    void setAndroidPlayServicesVersion(long version);

    boolean isReleaseBuild();
  }

  protected void onActivityStart(Activity activity) {
    Log.i(TAG, "onActivityStart ran");
    activityHolder.set(activity);
    sysConfigChangeReceiver.setForeground(true);
    beforeStartOrResume();
  }

  protected void onActivityStop(Activity activity) {
    Log.i(TAG, "onActivityStop ran");
    beforeSuspend();
    cobaltMediaSession.onActivityStop();
    if (activityHolder.get() == activity) {
      activityHolder.set(null);
    }
    sysConfigChangeReceiver.setForeground(false);
  }

  protected void onActivityDestroy(Activity activity) {
    if (applicationStopped) {
      // We can't restart the starboard app, so kill the process for a clean start next time.
      Log.i(TAG, "Activity destroyed after shutdown; killing app.");
      StarboardBridgeJni.get().closeNativeStarboard(nativeApp);
      closeAllServices();
      activity.finishAndRemoveTask();
    } else {
      Log.i(TAG, "Activity destroyed without shutdown; app suspended in background.");
    }
  }

  protected void onServiceStart(Service service) {
    serviceHolder.set(service);
  }

  protected void onServiceDestroy(Service service) {
    if (serviceHolder.get() == service) {
      serviceHolder.set(null);
    }
  }

  protected void beforeStartOrResume() {
    Log.i(TAG, "Prepare to resume");
    // Bring our platform services to life before resuming so that they're ready to deal with
    // whatever the web app wants to do with them as part of its start/resume logic.
    for (CobaltService service : cobaltServices.values()) {
      service.beforeStartOrResume();
    }
    advertisingId.refresh();
  }

  protected void beforeSuspend() {
    try {
      Log.i(TAG, "Prepare to suspend");
      // We want the MediaSession to be deactivated immediately before suspending so that by the
      // time, the launcher is visible our "Now Playing" card is already gone. Then Cobalt and
      // the web app can take their time suspending after that.
      for (CobaltService service : cobaltServices.values()) {
        service.beforeSuspend();
      }
    } catch (Throwable e) {
      Log.i(TAG, "Caught exception in beforeSuspend: " + e.getMessage());
    }
  }

  private void closeAllServices() {
    ttsHelper.shutdown();
    for (CobaltService service : cobaltServices.values()) {
      service.afterStopped();
    }
  }

  protected void afterStopped() {
    applicationStopped = true;
    closeAllServices();
    Activity activity = activityHolder.get();
    if (activity != null) {
      // Wait until the activity is destroyed to exit.
      Log.i(TAG, "Shutdown in foreground; finishing Activity and removing task.");
      activity.finishAndRemoveTask();
    } else {
      // We can't restart the starboard app, so kill the process for a clean start next time.
      Log.i(TAG, "Shutdown in background; killing app without removing task.");
      System.exit(0);
    }
  }

  @CalledByNative
  protected void applicationStarted() {
    applicationStarted = true;
  }

  @CalledByNative
  protected void applicationStopping() {
    applicationStarted = false;
    applicationStopped = true;
  }

  @CalledByNative
  public void requestSuspend() {
    Activity activity = activityHolder.get();
    if (activity != null) {
      activity.moveTaskToBack(false);
    }
  }

  /* Immediate shutdown, used at least by StandalonePlayerActivity. */
  public void requestStop(int errorLevel) {
    Activity currentActivity = activityHolder.get();
    if (currentActivity != null) {
      currentActivity.finishAndRemoveTask();
    }
  }

  public boolean onSearchRequested() {
    return false;
  }

  @CalledByNative
  public Context getApplicationContext() {
    if (appContext == null) {
      throw new IllegalArgumentException("appContext cannot be null");
    }
    return appContext;
  }

  @CalledByNative
  void raisePlatformError(@PlatformError.ErrorType int errorType, long data) {
    PlatformError error = new PlatformError(activityHolder, errorType, data);
    error.raise();
  }

  /** Returns true if the native code is compiled for release (i.e. 'gold' build). */
  public static boolean isReleaseBuild() {
    return StarboardBridgeJni.get().isReleaseBuild();
  }

  protected Holder<Activity> getActivityHolder() {
    return activityHolder;
  }

  @CalledByNative
  protected String[] getArgs() {
    if (args == null) {
      throw new IllegalArgumentException("args cannot be null");
    }
    return args;
  }

  // Initialize the platform's AudioTrackAudioSink. This must be done after the browser client
  // loads in the feature list and field trials.
  public void initializePlatformAudioSink() {
    StarboardBridgeJni.get().initializePlatformAudioSink();
  }

  /** Sends an event to the web app to navigate to the given URL */
  public void handleDeepLink(String url) {
    StarboardBridgeJni.get().handleDeepLink(url, applicationStarted);
  }

  public AssetManager getAssetsFromContext() {
    return appContext.getAssets();
  }

  public String getNativeLibraryDir() {
    return appContext.getApplicationInfo().nativeLibraryDir;
  }

  /**
   * Returns the absolute path to the directory where application specific files should be written.
   * May be overridden for use cases that need to segregate storage.
   */
  protected String getFilesAbsolutePath() {
    return appContext.getFilesDir().getAbsolutePath();
  }

  /**
   * Returns the absolute path to the application specific cache directory on the filesystem. May be
   * overridden for use cases that need to segregate storage.
   */
  protected String getCacheAbsolutePath() {
    return appContext.getCacheDir().getAbsolutePath();
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/speech_synthesis_speak.cc
  @CalledByNative
  CobaltTextToSpeechHelper getTextToSpeechHelper() {
    if (ttsHelper == null) {
      throw new IllegalArgumentException("ttsHelper cannot be null for native code");
    }
    return ttsHelper;
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/accessibility_get_caption_settings.cc
  /**
   * @return A new CaptionSettings object with the current system caption settings.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  CaptionSettings getCaptionSettings() {
    CaptioningManager cm =
        (CaptioningManager) appContext.getSystemService(Context.CAPTIONING_SERVICE);
    return new CaptionSettings(cm);
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/system_get_locale_id.cc
  /** Java-layer implementation of SbSystemGetLocaleId. */
  @SuppressWarnings("unused")
  @UsedByNative
  String systemGetLocaleId() {
    return Locale.getDefault().toLanguageTag();
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/time_zone_get_name.cc
  @SuppressWarnings("unused")
  @UsedByNative
  String getTimeZoneId() {
    Locale locale = Locale.getDefault();
    Calendar calendar = Calendar.getInstance(locale);
    TimeZone timeZone = DEFAULT_TIME_ZONE;
    if (calendar != null) {
      timeZone = calendar.getTimeZone();
    }
    return timeZone.getID();
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/window_get_diagonal_size_in_inches.cc
  @SuppressWarnings("unused")
  @UsedByNative
  SizeF getDisplayDpi() {
    return DisplayUtil.getDisplayDpi();
  }

  Size getDisplaySize() {
    return DisplayUtil.getSystemDisplaySize();
  }

  // TODO: (cobalt b/372559388) migrate JNI.
  @SuppressWarnings("unused")
  @UsedByNative
  public ResourceOverlay getResourceOverlay() {
    if (resourceOverlay == null) {
      throw new IllegalArgumentException("resourceOverlay cannot be null for native code");
    }
    return resourceOverlay;
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

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/window_get_size.cc
  @SuppressWarnings("unused")
  @UsedByNative
  Size getDeviceResolution() {
    String displaySize =
        android.os.Build.VERSION.SDK_INT < 28
            ? getSystemProperty("sys.display-size")
            : getSystemProperty("vendor.display-size");

    if (displaySize == null) {
      return getDisplaySize();
    }

    String[] sizes = displaySize.split("x");
    if (sizes.length != 2) {
      return getDisplaySize();
    }

    try {
      return new Size(Integer.parseInt(sizes[0]), Integer.parseInt(sizes[1]));
    } catch (NumberFormatException e) {
      return getDisplaySize();
    }
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/microphone_impl.cc
  /**
   * Checks if there is no microphone connected to the system.
   *
   * @return true if no device is connected.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  public boolean isMicrophoneDisconnected() {
    // A check specifically for microphones is not available before API 28, so it is assumed that a
    // connected input audio device is a microphone.
    AudioManager audioManager = (AudioManager) appContext.getSystemService(AUDIO_SERVICE);
    AudioDeviceInfo[] devices = audioManager.getDevices(GET_DEVICES_INPUTS);
    if (devices.length > 0) {
      return false;
    }

    // fallback to check for BT voice capable RCU
    InputManager inputManager = (InputManager) appContext.getSystemService(Context.INPUT_SERVICE);
    final int[] inputDeviceIds = inputManager.getInputDeviceIds();
    for (int inputDeviceId : inputDeviceIds) {
      final InputDevice inputDevice = inputManager.getInputDevice(inputDeviceId);
      final boolean hasMicrophone = inputDevice.hasMicrophone();
      if (hasMicrophone) {
        return false;
      }
    }
    return true;
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/microphone_impl.cc
  /**
   * Checks if the microphone is muted.
   *
   * @return true if the microphone mute is on.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  public boolean isMicrophoneMute() {
    AudioManager audioManager = (AudioManager) appContext.getSystemService(AUDIO_SERVICE);
    return audioManager.isMicrophoneMute();
  }

  /** Returns string for kSbSystemPropertyUserAgentAuxField */
  @CalledByNative
  protected String getUserAgentAuxField() {
    StringBuilder sb = new StringBuilder();

    String packageName = appContext.getApplicationInfo().packageName;
    sb.append(packageName);
    sb.append('/');

    try {
      if (android.os.Build.VERSION.SDK_INT < 33) {
        sb.append(appContext.getPackageManager().getPackageInfo(packageName, 0).versionName);
      } else {
        sb.append(
            appContext
                .getPackageManager()
                .getPackageInfo(packageName, PackageManager.PackageInfoFlags.of(0))
                .versionName);
      }
    } catch (PackageManager.NameNotFoundException ex) {
      // Should never happen
      Log.e(TAG, "Can't find our own package", ex);
    }

    return sb.toString();
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/system_get_property.cc
  /** Returns string for kSbSystemPropertyAdvertisingId */
  @CalledByNative
  protected String getAdvertisingId() {
    return this.advertisingId.getId();
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/system_get_property.cc
  /** Returns boolean for kSbSystemPropertyLimitAdTracking */
  @CalledByNative
  protected boolean getLimitAdTracking() {
    return this.advertisingId.isLimitAdTrackingEnabled();
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/audio_track_bridge.cc
  @SuppressWarnings("unused")
  @UsedByNative
  AudioOutputManager getAudioOutputManager() {
    if (audioOutputManager == null) {
      throw new IllegalArgumentException("audioOutputManager cannot be null for native code");
    }
    return audioOutputManager;
  }

  /** Returns Java layer implementation for AudioPermissionRequester */
  @SuppressWarnings("unused")
  @UsedByNative
  AudioPermissionRequester getAudioPermissionRequester() {
    return audioPermissionRequester;
  }

  void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
    audioPermissionRequester.onRequestPermissionsResult(requestCode, permissions, grantResults);
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/video_window.cc
  @SuppressWarnings("unused")
  @UsedByNative
  public void resetVideoSurface() {
    Activity activity = activityHolder.get();
    if (activity instanceof CobaltActivity) {
      ((CobaltActivity) activity).resetVideoSurface();
    }
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/player_set_bounds.cc
  @SuppressWarnings("unused")
  @UsedByNative
  public void setVideoSurfaceBounds(final int x, final int y, final int width, final int height) {
    Activity activity = activityHolder.get();
    if (activity instanceof CobaltActivity) {
      ((CobaltActivity) activity).setVideoSurfaceBounds(x, y, width, height);
    }
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/media_capabilities_cache.cc
  /** Return supported hdr types. */
  @CalledByNative
  public int[] getSupportedHdrTypes() {
    Display defaultDisplay = DisplayUtil.getDefaultDisplay();
    if (defaultDisplay == null) {
      return null;
    }

    Display.HdrCapabilities hdrCapabilities = defaultDisplay.getHdrCapabilities();
    if (hdrCapabilities == null) {
      return null;
    }

    return hdrCapabilities.getSupportedHdrTypes();
  }

  public CobaltMediaSession cobaltMediaSession() {
    return cobaltMediaSession;
  }

  public void registerCobaltService(CobaltService.Factory factory) {
    cobaltServiceFactories.put(factory.getServiceName(), factory);
  }

  public boolean hasCobaltService(String serviceName) {
    return cobaltServiceFactories.get(serviceName) != null;
  }

  // Explicitly pass activity as parameter.
  // Avoid using activityHolder.get(), because onActivityStop() can set it to null.
  public CobaltService openCobaltService(Activity activity, long nativeService, String serviceName) {
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
      service.receiveStarboardBridge(this);
      cobaltServices.put(serviceName, service);
      Log.i(TAG, String.format("Opened platform service %s.", serviceName));

      if (activity instanceof CobaltActivity) {
        service.setCobaltActivity((CobaltActivity) activity);
      }
    }
    return service;
  }

  public CobaltService getOpenedCobaltService(String serviceName) {
    return cobaltServices.get(serviceName);
  }

  public void closeCobaltService(String serviceName) {
    cobaltServices.remove(serviceName);
    Log.i(TAG, String.format("Closed platform service %s.", serviceName));
  }

  @CalledByNative
  public void closeAllCobaltService() {
    for (String serviceName : new ArrayList<>(cobaltServices.keySet())) {
      closeCobaltService(serviceName);
    }
  }

  public byte[] sendToCobaltService(String serviceName, byte[] data) {
    CobaltService service = cobaltServices.get(serviceName);
    if (service == null) {
      Log.e(TAG, String.format("Service not opened: %s", serviceName));
      return null;
    }
    CobaltService.ResponseToClient response = service.receiveFromClient(data);
    if (response.invalidState) {
      Log.e(TAG, String.format("Service %s received invalid data, closing.", serviceName));
      closeCobaltService(serviceName);
      return null;
    }
    return response.data;
  }

  /** Returns the application start timestamp. */
  protected void measureAppStartTimestamp() {
    if (mAppStartTimestamp != 0) {
      return;
    }
    measureAppStartDuration();
    long cppTimestamp = StarboardBridgeJni.get().currentMonotonicTime();
    mAppStartTimestamp = cppTimestamp - mAppStartDuration;
  }

  /** Returns the application start duration. */
  protected void measureAppStartDuration() {
    if (mAppStartDuration != 0) {
      return;
    }
    Activity activity = activityHolder.get();
    if (!(activity instanceof CobaltActivity)) {
      return;
    }
    long javaStartTimestamp = ((CobaltActivity) activity).getAppStartTimestamp();
    long javaStopTimestamp = System.nanoTime();
    mAppStartDuration = (javaStopTimestamp - javaStartTimestamp) / timeNanosecondsPerMicrosecond;
  }

  // Returns the saved app start timestamp.
  @CalledByNative
  protected long getAppStartTimestamp() {
    return mAppStartTimestamp;
  }

  // Returns the saved app start timestamp.
  @CalledByNative
  protected long getAppStartDuration() {
    return mAppStartDuration;
  }

  // TODO: (cobalt b/372559388) remove or migrate JNI?
  // Used in starboard/android/shared/graphics.cc
  @SuppressWarnings("unused")
  @UsedByNative
  void reportFullyDrawn() {
    Activity activity = activityHolder.get();
    if (activity != null) {
      activity.reportFullyDrawn();
    }
  }

  public void setCrashContext(String key, String value) {
    CrashContext.INSTANCE.setCrashContext(key, value);
  }

  public HashMap<String, String> getCrashContext() {
    return CrashContext.INSTANCE.getCrashContext();
  }

  public void registerCrashContextUpdateHandler(CrashContextUpdateHandler handler) {
    CrashContext.INSTANCE.registerCrashContextUpdateHandler(handler);
  }

  @CalledByNative
  protected boolean getIsAmatiDevice() {
    return this.isAmatiDevice;
  }

  @CalledByNative
  protected String getBuildFingerprint() {
    return Build.FINGERPRINT;
  }

  @CalledByNative
  protected long getPlayServicesVersion() {
    try {
      if (android.os.Build.VERSION.SDK_INT < 28) {
        return appContext
            .getPackageManager()
            .getPackageInfo(GOOGLE_PLAY_SERVICES_PACKAGE, 0)
            .versionCode;
      } else if (android.os.Build.VERSION.SDK_INT < 33) {
        return appContext
            .getPackageManager()
            .getPackageInfo(GOOGLE_PLAY_SERVICES_PACKAGE, 0)
            .getLongVersionCode();
      } else {
        return appContext
            .getPackageManager()
            .getPackageInfo(GOOGLE_PLAY_SERVICES_PACKAGE, PackageManager.PackageInfoFlags.of(0))
            .getLongVersionCode();
      }
    } catch (Exception e) {
      Log.w(TAG, "Unable to query Google Play Services package version", e);
      return 0;
    }
  }

  public void setWebContents(WebContents webContents) {
    cobaltMediaSession.setWebContents(webContents);
    volumeStateReceiver.setWebContents(webContents);
  }

  @CalledByNative
  protected void hideSplashScreen() {
    StartupGuard.getInstance().disarm();
  }
}
