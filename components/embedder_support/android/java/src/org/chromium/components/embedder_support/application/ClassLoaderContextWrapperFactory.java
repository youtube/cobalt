// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.application;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.LayoutInflater;

import org.chromium.base.ContextUtils;
import org.chromium.base.WrappedClassLoader;

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

/**
 * This class allows us to wrap the application context so that a Chromium implementation loaded
 * from a separate APK can correctly reference both org.chromium.* and application classes which is
 * necessary to properly inflate UI.
 */
public class ClassLoaderContextWrapperFactory {
    private ClassLoaderContextWrapperFactory() {}

    // Note WeakHashMap only guarantees that keys are weakly held, and ContextWrapper holds a strong
    // reference to the wrapped context. So need a WeakReference here to ensure the Context does not
    // leak.
    private static final WeakHashMap<Context, WeakReference<ClassLoaderContextWrapper>>
            sCtxToWrapper = new WeakHashMap<>();
    private static final Object sLock = new Object();

    @SuppressWarnings("StaticFieldLeak")
    private static Context sLightModeResourceOverrideContext;
    @SuppressWarnings("StaticFieldLeak")
    private static Context sDarkModeResourceOverrideContext;

    /**
     * Sets light and dark mode contexts that will override the return values from getAssets(),
     * getResources(), and getSystemService() when asking for layout inflater.
     */
    public static void setLightDarkResourceOverrideContext(
            Context lightModeContext, Context darkModeContext) {
        sLightModeResourceOverrideContext = lightModeContext;
        sDarkModeResourceOverrideContext = darkModeContext;
    }

    public static Context get(Context ctx) {
        // Avoid double-wrapping a context.
        if (ctx instanceof ClassLoaderContextWrapper) {
            return ctx;
        }
        ClassLoaderContextWrapper wrapper = null;
        synchronized (sLock) {
            WeakReference<ClassLoaderContextWrapper> ref = sCtxToWrapper.get(ctx);
            wrapper = (ref == null) ? null : ref.get();
            if (wrapper == null) {
                wrapper = new ClassLoaderContextWrapper(ctx);
                sCtxToWrapper.put(ctx, new WeakReference<>(wrapper));
            }
        }
        return wrapper;
    }

    private static class ClassLoaderContextWrapper extends ContextWrapper {
        private Context mApplicationContext;

        public ClassLoaderContextWrapper(Context base) {
            super(base);
        }

        @Override
        public ClassLoader getClassLoader() {
            final ClassLoader appCl = getBaseContext().getClassLoader();
            final ClassLoader chromiumCl = ClassLoaderContextWrapper.class.getClassLoader();

            return new WrappedClassLoader(chromiumCl, appCl);
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
                Context context = getResourceOverrideContext();
                if (context != null) {
                    return LayoutInflater.from(context);
                }
                LayoutInflater i = (LayoutInflater) getBaseContext().getSystemService(name);
                return i.cloneInContext(this);
            } else {
                return getBaseContext().getSystemService(name);
            }
        }

        @Override
        @SuppressWarnings("NoContextGetApplicationContext")
        public Context getApplicationContext() {
            if (mApplicationContext == null) {
                Context appCtx = getBaseContext().getApplicationContext();
                if (appCtx == getBaseContext()) {
                    mApplicationContext = this;
                } else {
                    mApplicationContext = get(appCtx);
                }
            }
            return mApplicationContext;
        }

        @Override
        public void registerComponentCallbacks(ComponentCallbacks callback) {
            // We have to override registerComponentCallbacks and unregisterComponentCallbacks
            // since they call getApplicationContext().[un]registerComponentCallbacks()
            // which causes us to go into a loop.
            getBaseContext().registerComponentCallbacks(callback);
        }

        @Override
        public void unregisterComponentCallbacks(ComponentCallbacks callback) {
            getBaseContext().unregisterComponentCallbacks(callback);
        }

        @Override
        public void startActivity(Intent intent) {
            if (ContextUtils.activityFromContext(this) == null) {
                // FLAG_ACTIVITY_NEW_TASK is needed to start activities from a non-activity
                // context.
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }

            super.startActivity(intent);
        }

        @Override
        public AssetManager getAssets() {
            return getResourceContext().getAssets();
        }

        @Override
        public Resources getResources() {
            return getResourceContext().getResources();
        }

        @Override
        public Resources.Theme getTheme() {
            return getResourceContext().getTheme();
        }

        private Context getResourceOverrideContext() {
            int uiMode = getBaseContext().getResources().getConfiguration().uiMode;
            boolean darkModeEnabled =
                    (uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
            return darkModeEnabled ? sDarkModeResourceOverrideContext
                                   : sLightModeResourceOverrideContext;
        }

        private Context getResourceContext() {
            Context resourceOverrideContext = getResourceOverrideContext();
            return (resourceOverrideContext != null) ? resourceOverrideContext : getBaseContext();
        }
    }
}
