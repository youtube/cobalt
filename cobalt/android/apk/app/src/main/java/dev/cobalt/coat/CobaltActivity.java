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

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewParent;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.Toast;
import androidx.annotation.Nullable;
import dev.cobalt.coat.javabridge.CobaltJavaScriptAndroidObject;
import dev.cobalt.coat.javabridge.CobaltJavaScriptInterface;
import dev.cobalt.coat.javabridge.H5vccPlatformService;
import dev.cobalt.coat.javabridge.HTMLMediaElementExtension;
import dev.cobalt.media.AudioOutputManager;
import dev.cobalt.media.MediaCodecCapabilitiesLogger;
import dev.cobalt.media.VideoSurfaceView;
import dev.cobalt.shell.Shell;
import dev.cobalt.shell.ShellManager;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Pattern;
import org.chromium.base.CommandLine;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.JavascriptInjector;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;

/** Native activity that has the required JNI methods called by the Starboard implementation. */
@JNINamespace("cobalt")
public abstract class CobaltActivity extends Activity {
  private static final String URL_ARG = "--url=";
  private static final String META_DATA_APP_URL = "cobalt.APP_URL";

  private static final String SPLASH_URL_ARG = "--splash-url=";
  private static final String META_DATA_APP_SPLASH_URL = "cobalt.APP_SPLASH_URL";
  private static final String SPLASH_TIMEOUT_MS_ARG = "--splash-timeout-ms=";
  private static final String META_DATA_APP_SPLASH_TIMEOUT_MS = "cobalt.APP_SPLASH_TIMEOUT_MS";
  private static final String DISABLE_NATIVE_SPLASH = "disable-native-splash";

  // This key differs in naming format for legacy reasons
  public static final String COMMAND_LINE_ARGS_KEY = "commandLineArgs";

  private static final Pattern URL_PARAM_PATTERN = Pattern.compile("^[a-zA-Z0-9_=]*$");

  // Maintain the list of JavaScript-exposed objects as a member variable
  // to prevent them from being garbage collected prematurely.
  private List<CobaltJavaScriptAndroidObject> javaScriptAndroidObjectList = new ArrayList<>();

  @SuppressWarnings("unused")
  private CobaltA11yHelper a11yHelper;

  private VideoSurfaceView videoSurfaceView;

  private boolean forceCreateNewVideoSurfaceView;

  private long timeInNanoseconds;

  private ShellManager mShellManager;
  private ActivityWindowAndroid mWindowAndroid;
  private Intent mLastSentIntent;
  private String mStartupUrl;
  private String mSplashUrl;
  private int mSplashTimeoutMs;
  private boolean mDisableNativeSplash;
  private IntentRequestTracker mIntentRequestTracker;
  // Tracks the status of the FLAG_KEEP_SCREEN_ON window flag.
  private Boolean isKeepScreenOnEnabled = false;
  private CobaltConnectivityDetector cobaltConnectivityDetector;

  private Boolean isMainFrameLoaded = false;
  private final Object lock = new Object();

  // Initially copied from ContentShellActiviy.java
  protected void createContent(final Bundle savedInstanceState) {
    // Initializing the command line must occur before loading the library.
    if (!CommandLine.isInitialized()) {
      CommandLine.init(null);

      String[] commandLineArgs = null;
      if (!VersionInfo.isReleaseBuild()) {
        commandLineArgs = getCommandLineParamsFromIntent(getIntent(), COMMAND_LINE_ARGS_KEY);
      }
      CommandLineOverrideHelper.getFlagOverrides(
          new CommandLineOverrideHelper.CommandLineOverrideHelperParams(
              VersionInfo.isOfficialBuild(), commandLineArgs));
    }
    mDisableNativeSplash = CommandLine.getInstance().hasSwitch(DISABLE_NATIVE_SPLASH);

    DeviceUtils.addDeviceSpecificUserAgentSwitch();

    // This initializes JNI and ends up calling JNI_OnLoad in native code
    LibraryLoader.getInstance().ensureInitialized();

    // StarboardBridge initialization must happen right after library loading,
    // before Browser/Content module is started. It currently tracks its own JNI state
    // variables, and needs to set up an early copy.

    // TODO(b/374147993): how to handle deeplink in Chrobalt?
    String startDeepLink = getIntentUrlAsString(getIntent());
    if (startDeepLink == null) {
      Log.w(TAG, "startDeepLink cannot be null, set it to empty string.");
      startDeepLink = "";
    }
    if (getStarboardBridge() == null) {
      // Cold start - Instantiate the singleton StarboardBridge.
      StarboardBridge starboardBridge = createStarboardBridge(getArgs(), startDeepLink);
      ((StarboardBridge.HostApplication) getApplication()).setStarboardBridge(starboardBridge);
    } else {
      // Warm start - Pass the deep link to the running Starboard app.
      getStarboardBridge().handleDeepLink(startDeepLink);
    }

    mShellManager = new ShellManager(this);
    final boolean listenToActivityState = true;
    mIntentRequestTracker = IntentRequestTracker.createFromActivity(this);
    mWindowAndroid = new ActivityWindowAndroid(this, listenToActivityState, mIntentRequestTracker);
    mIntentRequestTracker.restoreInstanceState(savedInstanceState);
    mShellManager.setWindow(mWindowAndroid);
    // Set up the animation placeholder to be the SurfaceView. This disables the
    // SurfaceView's 'hole' clipping during animations that are notified to the window.
    mWindowAndroid.setAnimationPlaceholderView(
        mShellManager.getContentViewRenderView().getSurfaceView());
    a11yHelper =
        new CobaltA11yHelper(this, mShellManager.getContentViewRenderView().getSurfaceView());

    if (mStartupUrl == null
        || mStartupUrl.isEmpty()
        || mSplashUrl == null
        || mSplashUrl.isEmpty()) {
      String[] args = getStarboardBridge().getArgs();
      mStartupUrl = parseArg(args, URL_ARG);
      mSplashUrl = parseArg(args, SPLASH_URL_ARG);
      String splashTimeoutMsStr = parseArg(args, SPLASH_TIMEOUT_MS_ARG);
      try {
        mSplashTimeoutMs = Integer.parseInt(splashTimeoutMsStr);
      } catch (NumberFormatException e) {
        Log.w(TAG, "Invalid splash timeout value: " + splashTimeoutMsStr + ", using default.");
        mSplashTimeoutMs = 1500;
      }
    }
    if (!TextUtils.isEmpty(mStartupUrl)) {
      mShellManager.setStartupUrl(Shell.sanitizeUrl(mStartupUrl));
    }

    // TODO(b/377025559): Bring back WebTests launch capability
    BrowserStartupController.getInstance()
        .startBrowserProcessesAsync(
            LibraryProcessType.PROCESS_BROWSER,
            false, // Do not start a separate GPU process
            // TODO(b/377025565): Figure out what this means
            false, // Do not start in "minimal" or paused mode
            new BrowserStartupController.StartupCallback() {
              @Override
              public void onSuccess() {
                Log.i(TAG, "Browser process init succeeded");
                finishInitialization(savedInstanceState);
                getStarboardBridge().measureAppStartTimestamp();
              }

              @Override
              public void onFailure() {
                Log.e(TAG, "Browser process init failed");
                initializationFailed();
              }
            });
  }

  // Initially copied from ContentShellActiviy.java
  private void finishInitialization(Bundle savedInstanceState) {

    // Initialize the platform's AudioSinkImpl. This is called here as this must come after the
    // browser client's feature list and field trials are initialized. The feature list and field
    // trials are initialized in CobaltContentBrowserClient::CreateFeatureListAndFieldTrials().
    getStarboardBridge().initializePlatformAudioSink();

    // Load an empty page to let shell create WebContents. Override Shell.java's
    // onWebContentsReady() to only continue with initializeJavaBridge() and setting the
    // webContents once it's confirmed that the webContents are correctly created and not null.
    // Two shells workflow:
    //   - App shell: Created by launchShell(), loads an empty URL (" ") initially. This shell is
    //     intended to load the main application URL in the background.
    //   - Splash shell: Created by default. If native splash is disabled, it does nothing.
    //     Otherwise, it loads the native splash screen URL.
    //   - mShellManager.showAppShell() switches the visible shell from
    //     the active shell to the App shell.
    if (mShellManager.getSplashShell() == null) {
      // If splash shell is not created by default, create one. This also allows to show splash
      // screen after swtiching ATV accounts.
      Log.i(TAG, "NativeSplash: create splash shell");
      mShellManager.launchShell("");
    }
    mShellManager.launchShell(
        "",
        new Shell.OnWebContentsReadyListener() {
          @Override
          public void onWebContentsReady() {
            // Inject JavaBridge objects to the WebContents.
            initializeJavaBridge();
            getStarboardBridge().setWebContents(getAppWebContents());

            // Load the `url` with the same shell we created above.
            Log.i(TAG, "shellManager load url:" + mStartupUrl);
            mShellManager.getAppShell().loadUrl(mStartupUrl);
          }

          @Override
          public void onWebContentsLoaded() {
            new android.os.Handler(android.os.Looper.getMainLooper())
                .postDelayed(
                    new Runnable() {
                      @Override
                      public void run() {
                        synchronized (lock) {
                          if (isMainFrameLoaded == false) {
                            // Main app loaded in App shell, switch to it.
                            Log.i(TAG, "NativeSplash: main shell is loaded");
                            isMainFrameLoaded = true;
                            mShellManager.showAppShell();
                          }
                        }
                      }
                    },
                    mSplashTimeoutMs);
          }
        });
    if (mDisableNativeSplash) {
      // No native splash, show the App shell (main app) immediately.
      Log.i(TAG, "Show main app without splash screen.");
      mShellManager.showAppShell();
    } else {
      // Native splash enabled: Load splash in active shell and set a timeout to switch to App
      // shell.
      mShellManager
          .getSplashShell()
          .setWebContentsReadyListener(
              new Shell.OnWebContentsReadyListener() {
                @Override
                public void onWebContentsReady() {}

                @Override
                public void onWebContentsLoaded() {
                  // Switch to pending shell after a timeout, or when the main app finishes loading,
                  // whichever comes first.
                  Log.i(
                      TAG,
                      String.format(
                          "NativeSplash: shellManager load splash timeout %dms", mSplashTimeoutMs));
                  final android.os.Handler handler =
                      new android.os.Handler(android.os.Looper.getMainLooper());
                  handler.postDelayed(
                      new SwitchRunnable(CobaltActivity.this, handler), mSplashTimeoutMs);
                }
              });
      Log.i(TAG, "shellManager load splash url:" + mSplashUrl);
      mShellManager.getSplashShell().loadUrl(mSplashUrl);
    }
  }

  private static class SwitchRunnable implements Runnable {
    private final java.lang.ref.WeakReference<CobaltActivity> activityReference;
    private final android.os.Handler handler;
    private int retries = 0;
    private static final int MAX_RETRIES = 50; // max timeout is 5s
    private static final int RETRY_DELAY_MS = 100;

    SwitchRunnable(CobaltActivity activity, android.os.Handler handler) {
      this.activityReference = new java.lang.ref.WeakReference<>(activity);
      this.handler = handler;
    }

    @Override
    public void run() {
      CobaltActivity activity = activityReference.get();
      if (activity == null || activity.isFinishing() || activity.isDestroyed()) {
        Log.w(TAG, "Activity is no longer valid, cancelling SwitchRunnable.");
        return;
      }

      synchronized (activity.lock) {
        if (activity.isMainFrameLoaded) {
          return;
        }
        if (activity.mShellManager.getAppShell() != null
            && (activity.mShellManager.getSplashShell() == null
                || activity.mShellManager.getSplashShell().isDestroyed())) {
          Log.i(
              TAG,
              String.format(
                  "NativeSplash: switch to main shell after timeout %dms",
                  activity.mSplashTimeoutMs));
          activity.isMainFrameLoaded = true;
          activity.mShellManager.showAppShell();
        } else if (retries < MAX_RETRIES) {
          retries++;
          handler.postDelayed(this, RETRY_DELAY_MS);
        } else {
          Log.w(
              TAG,
              "Timed out waiting for AppShell. Deferring to onWebContentsLoaded() on AppShell.");
        }
      }
    }
  }

  // Initially copied from ContentShellActiviy.java
  private void initializationFailed() {
    Log.e(TAG, "ContentView initialization failed.");
    Toast.makeText(
            CobaltActivity.this, R.string.browser_process_initialization_failed, Toast.LENGTH_SHORT)
        .show();
    finish();
  }

  protected boolean dispatchKeyEventToIme(int keyCode, int action) {
    ImeAdapterImpl imeAdapter = getImeAdapterImpl();
    if (imeAdapter == null) {
      return false;
    }

    // |KeyEvent| needs to be created with |downTime| and |eventTime| set. If they are not set the
    // app closes.
    long eventTime = SystemClock.uptimeMillis();
    return imeAdapter.dispatchKeyEvent(new KeyEvent(eventTime, eventTime, action, keyCode, 0));
  }

  @Override
  public boolean onKeyDown(int keyCode, KeyEvent event) {
    // If input is a from a gamepad button, it shouldn't be dispatched to IME which incorrectly
    // consumes the event as a VKEY_UNKNOWN
    if (KeyEvent.isGamepadButton(keyCode)) {
      return super.onKeyDown(keyCode, event);
    }
    return dispatchKeyEventToIme(keyCode, KeyEvent.ACTION_DOWN) || super.onKeyDown(keyCode, event);
  }

  @Override
  public boolean onKeyUp(int keyCode, KeyEvent event) {
    if (KeyEvent.isGamepadButton(keyCode)) {
      return super.onKeyUp(keyCode, event);
    }
    return dispatchKeyEventToIme(keyCode, KeyEvent.ACTION_UP) || super.onKeyUp(keyCode, event);
  }

  // Initially copied from ContentShellActiviy.java
  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data) {
    super.onActivityResult(requestCode, resultCode, data);
    mIntentRequestTracker.onActivityResult(requestCode, resultCode, data);
  }

  // Initially copied from ContentShellActiviy.java
  @Override
  public void startActivity(Intent i) {
    mLastSentIntent = i;
    super.startActivity(i);
  }

  public Intent getLastSentIntent() {
    return mLastSentIntent;
  }

  private static String getUrlFromIntent(Intent intent) {
    return intent != null ? intent.getDataString() : null;
  }

  private static String[] getCommandLineParamsFromIntent(Intent intent, String key) {
    return intent != null ? intent.getStringArrayExtra(key) : null;
  }

  /**
   * @return The {@link ShellManager} configured for the activity or null if it has not been created
   *     yet.
   */
  public ShellManager getShellManager() {
    return mShellManager;
  }

  /**
   * @return The currently visible {@link Shell} or null if one is not showing.
   */
  public Shell getActiveShell() {
    return mShellManager != null ? mShellManager.getActiveShell() : null;
  }

  /**
   * @return The current app shell.
   */
  public Shell getAppShell() {
    return mShellManager != null ? mShellManager.getAppShell() : null;
  }

  /**
   * @return The {@link WebContents} owned by the currently visible {@link Shell} or null if one is
   *     not showing.
   */
  @Nullable
  public WebContents getActiveWebContents() {
    Shell shell = getActiveShell();
    return shell != null ? shell.getWebContents() : null;
  }

  /**
   * @return The {@link WebContents} owned by the currently visible {@link Shell} or null if one is
   *     not showing.
   */
  @Nullable
  public WebContents getAppWebContents() {
    Shell shell = getAppShell();
    return shell != null ? shell.getWebContents() : null;
  }

  @Nullable
  protected ImeAdapterImpl getImeAdapterImpl() {
    WebContents webContents = getActiveWebContents();
    return webContents != null ? ImeAdapterImpl.fromWebContents(webContents) : null;
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // Record the application start timestamp.
    timeInNanoseconds = System.nanoTime();

    // To ensure that volume controls adjust the correct stream, make this call
    // early in the app's lifecycle. This connects the volume controls to
    // STREAM_MUSIC whenever the target activity or fragment is visible.
    setVolumeControlStream(AudioManager.STREAM_MUSIC);

    super.onCreate(savedInstanceState);
    cobaltConnectivityDetector = new CobaltConnectivityDetector(this);
    createContent(savedInstanceState);
    MemoryPressureMonitor.INSTANCE.registerComponentCallbacks();
    NetworkChangeNotifier.init();
    NetworkChangeNotifier.setAutoDetectConnectivityState(true);

    videoSurfaceView = new VideoSurfaceView(this);
    addContentView(
        videoSurfaceView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
  }

  /**
   * Initializes the Java Bridge to allow communication between Java and JavaScript. This method
   * injects Java objects into the WebView and loads corresponding JavaScript code.
   */
  private void initializeJavaBridge() {
    Log.i(TAG, "initializeJavaBridge");

    if (mShellManager == null || mShellManager.getAppShell() == null) {
      throw new RuntimeException(
          "mShellManager or mAppShell is null in initializeJavaBridge. This should never happen.");
    }
    WebContents webContents = getAppWebContents();
    if (webContents == null) {
      throw new RuntimeException(
          "webContents is null in initializeJavaBridge. This should never happen.");
    }

    // --- Initialize the Java Bridge ---

    // 1. Gather all Java objects that need to be exposed to JavaScript.
    // TODO(b/379701165): consider to refine the way to add JavaScript interfaces.
    javaScriptAndroidObjectList.add(new H5vccPlatformService(this, getStarboardBridge()));
    javaScriptAndroidObjectList.add(new HTMLMediaElementExtension(this));

    // 2. Use JavascriptInjector to inject Java objects into the WebContents.
    //    This makes the annotated methods in these objects accessible from JavaScript.
    JavascriptInjector javascriptInjector = JavascriptInjector.fromWebContents(webContents, false);

    javascriptInjector.setAllowInspection(true);
    for (CobaltJavaScriptAndroidObject javascriptAndroidObject : javaScriptAndroidObjectList) {
      Log.d(
          TAG,
          "Add JavaScriptAndroidObject:" + javascriptAndroidObject.getJavaScriptInterfaceName());
      javascriptInjector.addPossiblyUnsafeInterface(
          javascriptAndroidObject,
          javascriptAndroidObject.getJavaScriptInterfaceName(),
          CobaltJavaScriptInterface.class);
    }
  }

  /**
   * Instantiates the StarboardBridge. Apps not supporting sign-in should inject an instance of
   * NoopUserAuthorizer. Apps may subclass StarboardBridge if they need to override anything.
   */
  protected abstract StarboardBridge createStarboardBridge(String[] args, String startDeepLink);

  @UsedByNative
  protected StarboardBridge getStarboardBridge() {
    return ((StarboardBridge.HostApplication) getApplication()).getStarboardBridge();
  }

  public CobaltConnectivityDetector getCobaltConnectivityDetector() {
    return cobaltConnectivityDetector;
  }

  @Override
  protected void onStart() {
    if (!isReleaseBuild()) {
      getStarboardBridge().getAudioOutputManager().dumpAllOutputDevices();
      MediaCodecCapabilitiesLogger.dumpAllDecoders();
    }
    if (forceCreateNewVideoSurfaceView) {
      Log.w(TAG, "Force to create a new video surface.");
      createNewSurfaceView();
    }

    DisplayUtil.cacheDefaultDisplay(this);
    DisplayUtil.addDisplayListener(this);
    AudioOutputManager.addAudioDeviceListener(this);

    getStarboardBridge().onActivityStart(this);
    super.onStart();

    WebContents webContents = getActiveWebContents();
    if (webContents != null) {
      // document.onresume event
      webContents.onResume();
      // visibility:visible event
      webContents.onShow();
    }
    MemoryPressureMonitor.INSTANCE.enablePolling();
  }

  @Override
  protected void onPause() {
    WebContents webContents = getActiveWebContents();
    if (webContents != null) {
      // Flush immediately since activity may stop before callback is called.
      // Still need to flush after the window blur listener(s) are run since
      // the web app may update local strorage and/or cookies in a blur event
      // listener.
      CobaltActivityJni.get().flushCookiesAndLocalStorage();
      evaluateJavaScript(
          "window.dispatchEvent(new Event('blur'));",
          jsonResult -> {
            CobaltActivityJni.get().flushCookiesAndLocalStorage();
          });
    }
    super.onPause();
  }

  @Override
  protected void onStop() {
    getStarboardBridge().onActivityStop(this);
    super.onStop();

    WebContents webContents = getActiveWebContents();
    if (webContents != null) {
      // visibility:hidden event
      webContents.onHide();
      // document.onfreeze event
      webContents.onFreeze();
    }

    if (VideoSurfaceView.getCurrentSurface() != null) {
      forceCreateNewVideoSurfaceView = true;
    }
    MemoryPressureMonitor.INSTANCE.disablePolling();

    // Set the SurfaceView to fullscreen.
    View rootView = getWindow().getDecorView();
    setVideoSurfaceBounds(0, 0, rootView.getWidth(), rootView.getHeight());
  }

  @Override
  protected void onResume() {
    super.onResume();
    cobaltConnectivityDetector.activeNetworkCheck();
    View rootView = getWindow().getDecorView().getRootView();
    if (rootView != null && rootView.isAttachedToWindow() && !rootView.hasFocus()) {
      rootView.requestFocus();
      Log.i(TAG, "Request focus on the root view on resume.");
    }
    evaluateJavaScript("window.dispatchEvent(new Event('focus'));");
  }

  @Override
  protected void onDestroy() {
    if (cobaltConnectivityDetector != null) {
      cobaltConnectivityDetector.destroy();
    }
    if (mShellManager != null) {
      mShellManager.destroy();
    }
    mWindowAndroid.destroy();
    super.onDestroy();
    getStarboardBridge().onActivityDestroy(this);
  }

  @Override
  public boolean onSearchRequested() {
    return getStarboardBridge().onSearchRequested();
  }

  /** Returns true if the argument list contains an arg starting with argName. */
  private static boolean hasArg(List<String> args, String argName) {
    for (String arg : args) {
      if (arg.startsWith(argName)) {
        return true;
      }
    }
    return false;
  }

  private String parseArg(String[] args, String argName) {
    return Arrays.stream(args)
        .filter(line -> line.startsWith(argName))
        .findAny()
        .map(arg -> arg.substring(argName.length()))
        .orElse(null);
  }

  /**
   * Get argv/argc style args, if any from intent extras. Returns empty array if there are none
   *
   * <p>To use, invoke application via, eg, adb shell am start --esa args arg1,arg2 \
   * dev.cobalt.coat/dev.cobalt.app.MainActivity
   */
  protected String[] getArgs() {
    ArrayList<String> args = new ArrayList<>();
    String[] commandLineArgs = null;
    if (!isReleaseBuild()) {
      commandLineArgs = getCommandLineParamsFromIntent(getIntent(), COMMAND_LINE_ARGS_KEY);
    }
    if (commandLineArgs != null) {
      args.addAll(Arrays.asList(commandLineArgs));
    }

    // If the URL arg isn't specified, get it from AndroidManifest.xml.
    boolean hasUrlArg = hasArg(args, URL_ARG);
    // If the SPLASH_URL arg isn't specified, get it from AndroidManifest.xml.
    boolean hasSplashUrlArg = hasArg(args, SPLASH_URL_ARG);
    // If the SPLASH_TIMEOUT_MS arg isn't specified, get it from AndroidManifest.xml.
    boolean hasSplashTimeoutMsArg = hasArg(args, SPLASH_TIMEOUT_MS_ARG);
    if (!hasUrlArg || !hasSplashUrlArg || !hasSplashTimeoutMsArg) {
      try {
        ActivityInfo ai =
            getPackageManager()
                .getActivityInfo(getIntent().getComponent(), PackageManager.GET_META_DATA);
        if (ai.metaData != null) {
          if (!hasUrlArg) {
            String url = ai.metaData.getString(META_DATA_APP_URL);
            if (url != null) {
              if (mDisableNativeSplash) {
                url += "/splash";
              }
              args.add(URL_ARG + url);
            }
          }
          if (!hasSplashUrlArg) {
            String splash_url = ai.metaData.getString(META_DATA_APP_SPLASH_URL);
            if (splash_url != null) {
              args.add(SPLASH_URL_ARG + splash_url);
            }
          }
          if (!hasSplashTimeoutMsArg) {
            int timeout = ai.metaData.getInt(META_DATA_APP_SPLASH_TIMEOUT_MS, 1500);
            args.add(SPLASH_TIMEOUT_MS_ARG + timeout);
          }
        }
      } catch (NameNotFoundException e) {
        throw new RuntimeException("Error getting activity info", e);
      }
    }

    Bundle extras = getIntent().getExtras();
    CharSequence[] urlParams = (extras == null) ? null : extras.getCharSequenceArray("url_params");
    if (urlParams != null) {
      appendUrlParamsToUrl(args, urlParams);
    }

    return args.toArray(new String[0]);
  }

  private void appendUrlParamsToUrl(List<String> args, CharSequence[] urlParams) {
    int idx = -1;
    for (int i = 0; i < args.size(); i++) {
      if (args.get(i).startsWith(URL_ARG)) {
        idx = i;
        break;
      }
    }

    if (idx >= 0) {
      StringBuilder urlBuilder = new StringBuilder();
      urlBuilder.append(args.get(idx));
      // append & if ? is already in the url, otherwise append ?
      if (urlBuilder.indexOf("?") > 0) {
        urlBuilder.append("&");
      } else {
        urlBuilder.append("?");
      }

      for (int j = 0; j < urlParams.length; j++) {
        // sanitize the input before append to the url.
        String paramKeyValuePair = urlParams[j].toString();
        if (URL_PARAM_PATTERN.matcher(paramKeyValuePair).matches()) {
          urlBuilder.append(paramKeyValuePair);
          urlBuilder.append('&');
        }
      }

      urlBuilder.deleteCharAt(urlBuilder.length() - 1);
      args.set(idx, urlBuilder.toString());
    }
  }

  protected boolean isReleaseBuild() {
    return StarboardBridge.isReleaseBuild();
  }

  @Override
  protected void onNewIntent(Intent intent) {
    getStarboardBridge().handleDeepLink(getIntentUrlAsString(intent));
  }

  /**
   * Returns the URL from an Intent as a string. This may be overridden for additional processing.
   */
  protected String getIntentUrlAsString(Intent intent) {
    Uri intentUri = intent.getData();
    return (intentUri == null) ? "" : intentUri.toString();
  }

  @Override
  public void onRequestPermissionsResult(
      int requestCode, String[] permissions, int[] grantResults) {
    getStarboardBridge().onRequestPermissionsResult(requestCode, permissions, grantResults);
  }

  public void resetVideoSurface() {
    runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            createNewSurfaceView();
          }
        });
  }

  public void setVideoSurfaceBounds(final int x, final int y, final int width, final int height) {
    if (width == 0 || height == 0) {
      // The SurfaceView should be covered by our UI layer in this case.
      return;
    }
    runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            LayoutParams layoutParams = videoSurfaceView.getLayoutParams();
            // Since videoSurfaceView is added directly to the Activity's content view, which is a
            // FrameLayout, we expect its layout params to become FrameLayout.LayoutParams.
            if (layoutParams instanceof FrameLayout.LayoutParams) {
              ((FrameLayout.LayoutParams) layoutParams).setMargins(x, y, x + width, y + height);
            } else {
              Log.w(
                  TAG,
                  "Unexpected video surface layout params class "
                      + layoutParams.getClass().getName());
            }
            layoutParams.width = width;
            layoutParams.height = height;
            // Even though as a NativeActivity we're not using the Android UI framework, by setting
            // the  layout params it will force a layout to be requested. That will cause the
            // SurfaceView to position its underlying Surface to match the screen coordinates of
            // where the view would be in a UI layout and to set the surface transform matrix to
            // match the view's size.
            videoSurfaceView.setLayoutParams(layoutParams);
          }
        });
  }

  private void createNewSurfaceView() {
    ViewParent parent = videoSurfaceView.getParent();
    if (parent instanceof FrameLayout) {
      FrameLayout frameLayout = (FrameLayout) parent;
      int index = frameLayout.indexOfChild(videoSurfaceView);
      frameLayout.removeView(videoSurfaceView);
      Log.i(TAG, "removed videoSurfaceView at index:" + index);

      videoSurfaceView = new VideoSurfaceView(this);
      frameLayout.addView(
          videoSurfaceView,
          index,
          new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
      Log.i(TAG, "inserted new videoSurfaceView at index:" + index);
    } else {
      Log.w(TAG, "Unexpected surface view parent class " + parent.getClass().getName());
    }
  }

  public long getAppStartTimestamp() {
    return timeInNanoseconds;
  }

  public void evaluateJavaScript(String jsCode, @Nullable JavaScriptCallback callback) {
    // evaluateJavaScript must run on UI thread.
    runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            WebContents webContents = getAppWebContents();
            if (webContents != null) {
              webContents.evaluateJavaScript(jsCode, callback);
            }
          }
        });
  }

  public void evaluateJavaScript(String jsCode) {
    evaluateJavaScript(jsCode, null);
  }

  public void toggleKeepScreenOn(boolean keepOn) {
    if (isKeepScreenOnEnabled != keepOn) {
      runOnUiThread(
          new Runnable() {
            @Override
            public void run() {
              if (keepOn) {
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                Log.i(TAG, "Screen keep-on enabled for video playback");
              } else {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                Log.i(TAG, "Screen keep-on disabled");
              }
            }
          });
      isKeepScreenOnEnabled = keepOn;
    }
  }

  public boolean isDisableNativeSplash() {
    return mDisableNativeSplash;
  }

  @NativeMethods
  interface Natives {
    void flushCookiesAndLocalStorage();
  }
}
