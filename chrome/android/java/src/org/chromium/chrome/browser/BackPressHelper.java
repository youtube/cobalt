// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.activity.OnBackPressedCallback;
import androidx.activity.OnBackPressedDispatcher;
import androidx.lifecycle.LifecycleOwner;

import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * Helper class for back press event handling via {@link OnBackPressedDisptacher}. This is
 * a recommended way over {@link Activity#onBackPressed}. Refer to the Android developer's guide
 * {@link https://developer.android.com/guide/navigation/navigation-custom-back}.
 */
public final class BackPressHelper {
    /**
     * @deprecated Handles back press event. #onBackPressed is deprecated starting from U. Prefer
     * {@link BackPressHandler} whenever possible.
     */
    public interface ObsoleteBackPressedHandler {
        /**
         * @return {@code true} if the event was consumed. {@code false} otherwise.
         */
        boolean onBackPressed();
    }

    /**
     * @deprecated Create a {@link BackPressHelper} that can handle a chain of handlers.
     * Prefer {@link #create(LifecycleOwner, OnBackPressedDispatcher, BackPressHandler)}
     * whenever possible.
     * @param lifecycleOwner {@link LifecycleOwner} managing the back press logic's lifecycle.
     * @param dispatcher {@link OnBackPressedDispatcher} that holds other callbacks.
     * @param handler {@link ObsoleteBackPressedHandler} implementing the caller's back press
     *         handler.
     */
    @Deprecated
    public static void create(LifecycleOwner lifecycleOwner, OnBackPressedDispatcher dispatcher,
            ObsoleteBackPressedHandler handler) {
        new BackPressHelper(lifecycleOwner, dispatcher, handler);
    }

    /**
     * Register a {@link BackPressHandler} on a given {@link  OnBackPressedDispatcher}.
     * @param lifecycleOwner {@link LifecycleOwner} managing the back press logic's lifecycle.
     * @param dispatcher {@link OnBackPressedDispatcher} that holds other callbacks.
     * @param handler {@link BackPressHandler} observing back press state and consuming back press.
     */
    public static void create(LifecycleOwner lifecycleOwner, OnBackPressedDispatcher dispatcher,
            BackPressHandler handler) {
        var callback = new OnBackPressedCallback(/* enabled */ false) {
            @Override
            public void handleOnBackPressed() {
                handler.handleBackPress();
            }
        };
        handler.getHandleBackPressChangedSupplier().addObserver(callback::setEnabled);
        dispatcher.addCallback(lifecycleOwner, callback);
    }

    /**
     * Register a list of {@link BackPressHandler} on a given {@link  OnBackPressedDispatcher}.
     * The first handler has the top priority and the last one has the least.
     * TODO(https://crbug.com/1406012): consider introducing a lightweight
     * {@link org.chromium.chrome.browser.back_press.BackPressManager} if too many handlers should
     * be registered.
     * @param lifecycleOwner {@link LifecycleOwner} managing the back press logic's lifecycle.
     * @param dispatcher {@link OnBackPressedDispatcher} that holds other callbacks.
     * @param handlers {@link BackPressHandler} observing back press state and consuming back press.
     */
    public static void create(LifecycleOwner lifecycleOwner, OnBackPressedDispatcher dispatcher,
            BackPressHandler[] handlers) {
        // OnBackPressedDispatcher triggers handlers in a reversed order.
        for (int i = handlers.length - 1; i >= 0; i--) {
            create(lifecycleOwner, dispatcher, handlers[i]);
        }
    }

    /**
     * Let the back press event be processed by next {@link OnBackPressedCallback}. A callback
     * needs to be in enabled state only when it plans to handle back press event. If the callback
     * cannot handle it for whatever reason, it can call this method to give the next callback
     * or a fallback runnable an opportunity to process it. If the callback needs to receive
     * back press events in the future again, it should enable itself after this call.
     * @param dispatcher {@link OnBackPressedDispatcher} holding other callbacks.
     * @param callback {@link OnBackPressedCallback} which just received the event but ended up
     *        not handling it.
     */
    public static void onBackPressed(
            OnBackPressedDispatcher dispatcher, OnBackPressedCallback callback) {
        callback.setEnabled(false);
        dispatcher.onBackPressed();
    }

    private BackPressHelper(LifecycleOwner lifecycleOwner, OnBackPressedDispatcher dispatcher,
            ObsoleteBackPressedHandler handler) {
        dispatcher.addCallback(lifecycleOwner, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                if (!handler.onBackPressed()) onBackPressed(dispatcher, this);
            }
        });
    }
}
