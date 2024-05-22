// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Build;
import android.os.Handler;
import android.os.Process;
import android.os.UserHandle;

import androidx.annotation.RequiresApi;

import org.chromium.base.compat.ApiHelperForQ;
import org.chromium.build.BuildConfig;

import java.lang.reflect.Method;
import java.util.concurrent.Executor;

/**
 * Class of static helper methods to call Context.bindService variants.
 */
final class BindService {
    private static Method sBindServiceAsUserMethod;

    static boolean supportVariableConnections() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                && !BuildConfig.IS_INCREMENTAL_INSTALL;
    }

    // Note that handler is not guaranteed to be used, and client still need to correctly handle
    // callbacks on the UI thread.
    static boolean doBindService(Context context, Intent intent, ServiceConnection connection,
            int flags, Handler handler, Executor executor, String instanceName) {
        if (supportVariableConnections() && instanceName != null) {
            return ApiHelperForQ.bindIsolatedService(
                    context, intent, flags, instanceName, executor, connection);
        }

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N) {
            return bindServiceByCall(context, intent, connection, flags);
        }

        try {
            return bindServiceByReflection(context, intent, connection, flags, handler);
        } catch (ReflectiveOperationException reflectionException) {
            try {
                return bindServiceByCall(context, intent, connection, flags);
            } catch (RuntimeException runtimeException) {
                // Include the reflectionException in crash reports.
                throw new RuntimeException(runtimeException.getMessage(), reflectionException);
            }
        }
    }

    private static boolean bindServiceByCall(
            Context context, Intent intent, ServiceConnection connection, int flags) {
        return context.bindService(intent, connection, flags);
    }

    @RequiresApi(Build.VERSION_CODES.N)
    @SuppressLint("DiscouragedPrivateApi")
    private static boolean bindServiceByReflection(Context context, Intent intent,
            ServiceConnection connection, int flags, Handler handler)
            throws ReflectiveOperationException {
        if (sBindServiceAsUserMethod == null) {
            sBindServiceAsUserMethod =
                    Context.class.getDeclaredMethod("bindServiceAsUser", Intent.class,
                            ServiceConnection.class, int.class, Handler.class, UserHandle.class);
        }
        // No need for null checks or worry about infinite looping here. Otherwise a regular calls
        // into the ContextWrapper would lead to problems as well.
        while (context instanceof ContextWrapper) {
            context = ((ContextWrapper) context).getBaseContext();
        }
        return (Boolean) sBindServiceAsUserMethod.invoke(
                context, intent, connection, flags, handler, Process.myUserHandle());
    }

    private BindService() {}
}
