// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.annotations.MainDex;
import org.chromium.components.module_installer.engine.InstallEngine;
import org.chromium.components.module_installer.engine.InstallListener;
import org.chromium.components.module_installer.util.Timer;

/**
 * Represents a feature module. Can be used to install the module, access its interface, etc. See
 * {@link ModuleInterface} for how to conveniently create an instance of the module class for a
 * specific feature module. The @MainDex annotation supports module use in the renderer process.
 *
 * @param <T> The interface of the module
 */
@JNINamespace("module_installer")
@MainDex
public class Module<T> {
    private final String mName;
    private final Class<T> mInterfaceClass;
    private final String mImplClassName;
    private T mImpl;
    private InstallEngine mInstaller;
    private boolean mIsNativeLoaded;

    /**
     * Instantiates a module.
     *
     * @param name The module's name as used with {@link ModuleInstaller}.
     * @param interfaceClass {@link Class} object of the module interface.
     * @param implClassName fully qualified class name of the implementation of the module's
     *                      interface.
     */
    public Module(String name, Class<T> interfaceClass, String implClassName) {
        mName = name;
        mInterfaceClass = interfaceClass;
        mImplClassName = implClassName;
    }

    @VisibleForTesting
    public InstallEngine getInstallEngine() {
        if (mInstaller == null) {
            try (Timer timer = new Timer()) {
                mInstaller = new ModuleEngine(mImplClassName);
            }
        }
        return mInstaller;
    }

    @VisibleForTesting
    public void setInstallEngine(InstallEngine engine) {
        mInstaller = engine;
    }

    /**
     * Returns true if the module is currently installed and can be accessed.
     */
    public boolean isInstalled() {
        try (Timer timer = new Timer()) {
            return getInstallEngine().isInstalled(mName);
        }
    }

    /**
     * Requests install of the module.
     */
    public void install(InstallListener listener) {
        try (Timer timer = new Timer()) {
            assert !isInstalled();
            getInstallEngine().install(mName, listener);
        }
    }

    /**
     * Requests deferred install of the module.
     */
    public void installDeferred() {
        try (Timer timer = new Timer()) {
            getInstallEngine().installDeferred(mName);
        }
    }

    /**
     * Returns the implementation of the module interface. Must only be called if the module is
     * installed.
     */
    public T getImpl() {
        try (Timer timer = new Timer()) {
            if (mImpl != null) return mImpl;
            assert isInstalled();

            ModuleDescriptor moduleDescriptor = loadModuleDescriptor(mName);
            if (moduleDescriptor.getLoadNativeOnGetImpl()) {
                // Load the module's native code and/or resources if they are present, and the
                // Chrome native library itself has been loaded.
                ensureNativeLoaded();
            }

            Object impl = instantiateReflectively(mName, mImplClassName);
            try {
                mImpl = mInterfaceClass.cast(impl);
            } catch (ClassCastException e) {
                ClassLoader interfaceClassLoader = mInterfaceClass.getClassLoader();
                ClassLoader implClassLoader = impl.getClass().getClassLoader();
                throw new RuntimeException("Failure casting " + mName
                                + " module class, interface ClassLoader: " + interfaceClassLoader
                                + " (parent " + interfaceClassLoader.getParent() + ")"
                                + ", impl ClassLoader: " + implClassLoader + " (parent "
                                + implClassLoader.getParent() + ")"
                                + ", equal: " + interfaceClassLoader.equals(implClassLoader)
                                + " (parents equal: "
                                + interfaceClassLoader.getParent().equals(
                                        implClassLoader.getParent())
                                + ")",
                        e);
            }
            return mImpl;
        }
    }

    /**
     * Loads native libraries and/or resources if and only if this is not already done, assuming
     * that the module is installed, and enableNativeLoad() has been called.
     */
    public void ensureNativeLoaded() {
        // Can only initialize native once per lifetime of Chrome.
        if (mIsNativeLoaded) return;
        assert LibraryLoader.getInstance().isInitialized();
        ModuleDescriptor moduleDescriptor = loadModuleDescriptor(mName);
        String[] libraries = moduleDescriptor.getLibraries();
        String[] paks = moduleDescriptor.getPaks();
        if (libraries.length > 0 || paks.length > 0) {
            ModuleJni.get().loadNative(mName, libraries, paks);
        }
        mIsNativeLoaded = true;
    }

    /**
     * Loads the {@link ModuleDescriptor} for a module.
     *
     * For bundles, uses reflection to load the descriptor from inside the
     * module. For APKs, returns an empty descriptor since APKs won't have
     * descriptors packaged into them.
     *
     * @param name The module's name.
     * @return The module's {@link ModuleDescriptor}.
     */
    private static ModuleDescriptor loadModuleDescriptor(String name) {
        if (!BundleUtils.isBundle()) {
            return new ModuleDescriptor() {
                @Override
                public String[] getLibraries() {
                    return new String[0];
                }

                @Override
                public String[] getPaks() {
                    return new String[0];
                }

                @Override
                public boolean getLoadNativeOnGetImpl() {
                    return false;
                }
            };
        }

        return (ModuleDescriptor) instantiateReflectively(
                name, "org.chromium.components.module_installer.builder.ModuleDescriptor_" + name);
    }

    /**
     * Instantiates an object via reflection.
     *
     * Ignores strict mode violations since accessing code in a module may cause its DEX file to be
     * loaded and on some devices that can cause such a violation.
     *
     * @param moduleName The module's name.
     * @param className The object's class name.
     * @return The object.
     */
    private static Object instantiateReflectively(String moduleName, String className) {
        Context context = ContextUtils.getApplicationContext();
        if (BundleUtils.isIsolatedSplitInstalled(moduleName)) {
            context = BundleUtils.createIsolatedSplitContext(context, moduleName);
        }
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return context.getClassLoader().loadClass(className).newInstance();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @NativeMethods
    interface Natives {
        void loadNative(String name, String[] libraries, String[] paks);
    }
}
