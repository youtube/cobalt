// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.util.ArraySet;

import org.chromium.base.BundleUtils;
import org.chromium.base.JNIUtils;
import org.chromium.base.JavaUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;

/**
 * Application class for Chrome that knows how to deal with isolated splits. This class will perform
 * any necessary initialization for non-browser processes without loading code from the chrome
 * split. In the browser process, the necessary logic is loaded from the chrome split using
 * reflection.
 */
@NullMarked
public class SplitChromeApplication extends SplitCompatApplication {

    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.ChromeApplicationImpl";

    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
    private static @IdentifierNameString String sChromePreloadName =
            "org.chromium.chrome.browser.ChromeTabbedActivity$Preload";

    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
    private static @IdentifierNameString String sGoogle3PreloadName =
            "org.chromium.chrome.modules.google3.Google3ModuleEntryImpl";

    private static final Object sSplitLock = new Object();
    private static final ArraySet<String> sCachedSplits = new ArraySet<>();

    @SuppressLint("StaticFieldLeak")
    private static @Nullable SplitPreloader sSplitPreloader;

    private final String mChromeApplicationClassName;
    private @Nullable Resources mResources;

    public SplitChromeApplication() {
        this(sImplClassName);
    }

    public SplitChromeApplication(String chromeApplicationClassName) {
        mChromeApplicationClassName = chromeApplicationClassName;
    }

    @Override
    public void onCreate() {
        finishPreload(CHROME_SPLIT_NAME);
        super.onCreate();
    }

    @Override
    protected void attachBaseContext(Context context) {
        if (isBrowserProcess()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                DexFixer.setHasIsolatedSplits(true);
            }
            setImplSupplier(
                    () -> {
                        return (Impl)
                                BundleUtils.newInstance(
                                        mChromeApplicationClassName, CHROME_SPLIT_NAME);
                    });
        } else {
            setImplSupplier(() -> createNonBrowserApplication());
        }
        // We need to call setImplSupplier before continuing attachBaseContext. See
        // crbug.com/395261363 for details.
        super.attachBaseContext(context);
    }

    private static @Nullable String getPreloadClassName(String split) {
        if (split.equals(CHROME_SPLIT_NAME)) {
            return sChromePreloadName;
        }
        if (split.equals("google3")) {
            return sGoogle3PreloadName;
        }
        return null;
    }

    private Context createContextForSplitNoWait(String name) {
        synchronized (sSplitLock) {
            boolean shouldRecordHistogram = !sCachedSplits.contains(name);
            try {
                long startTime = SystemClock.uptimeMillis();
                Context context = super.createContextForSplit(name);
                if (shouldRecordHistogram) {
                    // It is possible that the framework will load the split for us and cache the
                    // ClassLoader, making this unnaturally quick. Locally we could not reproduce
                    // this, but these entry points almost certainly exist.
                    RecordHistogram.recordTimesHistogram(
                            "Android.IsolatedSplits.ContextCreateTime2." + name,
                            SystemClock.uptimeMillis() - startTime);
                    sCachedSplits.add(name);
                    String preloadClass = getPreloadClassName(name);
                    if (preloadClass != null) {
                        long loadStartTime = SystemClock.uptimeMillis();
                        try {
                            context.getClassLoader().loadClass(preloadClass);
                        } catch (ReflectiveOperationException e) {
                            throw new RuntimeException(
                                    "Preload of "
                                            + preloadClass
                                            + " for split "
                                            + name
                                            + " failed.");
                        }
                        long endTime = SystemClock.uptimeMillis();
                        RecordHistogram.recordTimesHistogram(
                                "Android.IsolatedSplits.LoadFirstClassTime." + name,
                                endTime - loadStartTime);
                        RecordHistogram.recordTimesHistogram(
                                "Android.IsolatedSplits.ContextCreateTime3." + name,
                                endTime - startTime);
                    }
                }
                return context;
            } catch (PackageManager.NameNotFoundException e) {
                throw JavaUtils.throwUnchecked(e);
            }
        }
    }

    /**
     * From a reading of Android's source code, it appears that the only calls to this function from
     * the framework are when installing a Content Provider, handling a Broadcast Receiver, or
     * creating a Service. We only care about this method being called for 2 reasons - first, that
     * we can finish our preload (which won't happen in the case of the framework starting us) and
     * second because we emit an UMA histogram we're interested in (where these instances are rare
     * enough relative to typical startups that we are fine just getting UMA for typical startups).
     */
    @Override
    public Context createContextForSplit(String name) {
        try (TraceEvent te = TraceEvent.scoped("SplitChromeApplication.createContextForSplit")) {
            // Wait for any splits that are preloading so the framework does not have a race
            // condition when updating its class loader cache (b/172602571).
            finishPreload(name);
            return createContextForSplitNoWait(name);
        }
    }

    @Override
    protected void performBrowserProcessPreloading(Context context, boolean blockingLoad) {
        SplitPreloader.PreloadHooks hooks =
                new SplitPreloader.PreloadHooks() {
                    @Override
                    public void runImmediatelyInBackgroundThread(Context chromeContext) {
                        // A new thread is started here because we do not want to delay returning
                        // the chrome Context, since that slows down startup. This thread must be
                        // a HandlerThread because AsyncInitializationActivity (a base class of
                        // ChromeTabbedActivity) creates a Handler, so needs to have a Looper
                        // prepared.
                        HandlerThread thread = new HandlerThread("ActivityPreload");
                        thread.start();
                        new Handler(thread.getLooper())
                                .post(
                                        () -> {
                                            try {
                                                // Create a throwaway instance of
                                                // ChromeTabbedActivity. This will warm up
                                                // the chrome ClassLoader, and perform loading of
                                                // classes used early in startup in the
                                                // background.
                                                chromeContext
                                                        .getClassLoader()
                                                        .loadClass(sChromePreloadName)
                                                        .newInstance();
                                            } catch (ReflectiveOperationException e) {
                                                throw new RuntimeException(e);
                                            }
                                            thread.quit();
                                        });
                    }

                    @Override
                    public void runInUiThread(Context chromeContext) {
                        // If the chrome module is not enabled or isolated splits are not supported,
                        // chromeContext will have the same ClassLoader as the base context, so no
                        // need to replace the ClassLoaders here.
                        if (!context.getClassLoader().equals(chromeContext.getClassLoader())) {
                            // Replace the application Context's ClassLoader with the chrome
                            // ClassLoader, because the application ClassLoader is expected to be
                            // able to access all chrome classes.
                            BundleUtils.replaceClassLoader(
                                    SplitChromeApplication.this, chromeContext.getClassLoader());
                            JNIUtils.setDefaultClassLoader(chromeContext.getClassLoader());

                            if (GlobalAppLocaleController.getInstance().isOverridden()) {
                                Configuration config =
                                        GlobalAppLocaleController.getInstance()
                                                .getOverrideConfig(chromeContext);
                                chromeContext = chromeContext.createConfigurationContext(config);
                            }
                            // Resources holds a reference to a ClassLoader. Make our Application's
                            // getResources() return a reference to the Chrome split's resources
                            // since there are a spots where ContextUtils.getApplicationContext()
                            // is used to retrieve resources (https://crbug.com/1287000).
                            mResources = chromeContext.getResources();
                        }
                    }

                    @Override
                    public Context createIsolatedSplitContext(String name) {
                        return createContextForSplitNoWait(name);
                    }
                };

        if (blockingLoad) {
            Context chromeContext = hooks.createIsolatedSplitContext(CHROME_SPLIT_NAME);
            hooks.runInUiThread(chromeContext);
        } else {
            // The chrome split has a large amount of code, which can slow down startup. Loading
            // this in the background allows us to do this in parallel with startup tasks which do
            // not depend on code in the chrome split.
            sSplitPreloader = new SplitPreloader(context);
            // If the chrome module is not enabled or isolated splits are not supported (e.g. in
            // Android N), the onComplete function will run immediately so it must handle the case
            // where the base context of the application has not been set yet.
            sSplitPreloader.preload(CHROME_SPLIT_NAME, hooks);
        }
    }

    @Override
    protected void performBrowserProcessPreloading(Context context) {
        performBrowserProcessPreloading(context, false);
    }

    @Override
    public Resources getResources() {
        // If the cached resources from the Chrome split are available use those. Note that
        // retrieving resources will use resources from the base split until the Chrome split is
        // fully loaded. We don't want to ensure the Chrome split is loaded here because resources
        // may be accessed early in startup, and forcing a load here will reduce the benefits of
        // preloading the Chrome split in the background.
        if (mResources != null) {
            return mResources;
        }
        return getBaseContext().getResources();
    }

    /** Waits for the specified split to finish preloading if necessary. */
    public static void finishPreload(String name) {
        if (sSplitPreloader != null) {
            sSplitPreloader.wait(name);
        }
    }

    protected Impl createNonBrowserApplication() {
        return new Impl();
    }
}
