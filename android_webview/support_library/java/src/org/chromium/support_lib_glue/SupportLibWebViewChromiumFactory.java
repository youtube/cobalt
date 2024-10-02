// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.content.Context;
import android.net.Uri;
import android.webkit.ValueCallback;
import android.webkit.WebView;

import androidx.annotation.IntDef;

import com.android.webview.chromium.CallbackConverter;
import com.android.webview.chromium.SharedStatics;
import com.android.webview.chromium.SharedTracingControllerAdapter;
import com.android.webview.chromium.WebViewChromiumAwInit;
import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.android_webview.AwDebug;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.support_lib_boundary.StaticsBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewProviderFactoryBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Support library glue version of WebViewChromiumFactoryProvider.
 */
class SupportLibWebViewChromiumFactory implements WebViewProviderFactoryBoundaryInterface {
    // SupportLibWebkitToCompatConverterAdapter
    private final InvocationHandler mCompatConverterAdapter;
    private final WebViewChromiumAwInit mAwInit;
    // clang-format off
    private final String[] mWebViewSupportedFeatures =
            new String[] {
                    Features.VISUAL_STATE_CALLBACK,
                    Features.OFF_SCREEN_PRERASTER,
                    Features.SAFE_BROWSING_ENABLE,
                    Features.DISABLED_ACTION_MODE_MENU_ITEMS,
                    Features.START_SAFE_BROWSING,
                    Features.SAFE_BROWSING_ALLOWLIST,
                    Features.SAFE_BROWSING_WHITELIST,
                    Features.SAFE_BROWSING_PRIVACY_POLICY_URL,
                    Features.SERVICE_WORKER_BASIC_USAGE,
                    Features.SERVICE_WORKER_CACHE_MODE,
                    Features.SERVICE_WORKER_CONTENT_ACCESS,
                    Features.SERVICE_WORKER_FILE_ACCESS,
                    Features.SERVICE_WORKER_BLOCK_NETWORK_LOADS,
                    Features.SERVICE_WORKER_SHOULD_INTERCEPT_REQUEST,
                    Features.RECEIVE_WEB_RESOURCE_ERROR,
                    Features.RECEIVE_HTTP_ERROR,
                    Features.SAFE_BROWSING_HIT,
                    Features.SHOULD_OVERRIDE_WITH_REDIRECTS,
                    Features.WEB_RESOURCE_REQUEST_IS_REDIRECT,
                    Features.WEB_RESOURCE_ERROR_GET_DESCRIPTION,
                    Features.WEB_RESOURCE_ERROR_GET_CODE,
                    Features.SAFE_BROWSING_RESPONSE_BACK_TO_SAFETY,
                    Features.SAFE_BROWSING_RESPONSE_PROCEED,
                    Features.SAFE_BROWSING_RESPONSE_SHOW_INTERSTITIAL,
                    Features.WEB_MESSAGE_PORT_POST_MESSAGE,
                    Features.WEB_MESSAGE_PORT_CLOSE,
                    Features.WEB_MESSAGE_PORT_SET_MESSAGE_CALLBACK,
                    Features.CREATE_WEB_MESSAGE_CHANNEL,
                    Features.POST_WEB_MESSAGE,
                    Features.WEB_MESSAGE_CALLBACK_ON_MESSAGE,
                    Features.GET_WEB_VIEW_CLIENT,
                    Features.GET_WEB_CHROME_CLIENT,
                    Features.PROXY_OVERRIDE,
                    Features.SUPPRESS_ERROR_PAGE + Features.DEV_SUFFIX,
                    Features.GET_WEB_VIEW_RENDERER,
                    Features.WEB_VIEW_RENDERER_TERMINATE,
                    Features.TRACING_CONTROLLER_BASIC_USAGE,
                    Features.WEB_VIEW_RENDERER_CLIENT_BASIC_USAGE,
                    Features.MULTI_PROCESS_QUERY,
                    Features.FORCE_DARK,
                    Features.FORCE_DARK_BEHAVIOR,
                    Features.WEB_MESSAGE_LISTENER,
                    Features.SET_SUPPORT_LIBRARY_VERSION + Features.DEV_SUFFIX,
                    Features.DOCUMENT_START_SCRIPT,
                    Features.PROXY_OVERRIDE_REVERSE_BYPASS,
                    Features.GET_VARIATIONS_HEADER,
                    Features.ALGORITHMIC_DARKENING,
                    Features.ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY,
                    Features.GET_COOKIE_INFO,
                    Features.WEB_MESSAGE_ARRAY_BUFFER + Features.DEV_SUFFIX,
                    Features.REQUESTED_WITH_HEADER_ALLOW_LIST,
                    Features.IMAGE_DRAG_DROP + Features.DEV_SUFFIX,
                    Features.RESTRICT_SENSITIVE_WEB_CONTENT + Features.DEV_SUFFIX,
                    // Add new features above. New features must include `+ Features.DEV_SUFFIX`
                    // when they're initially added (this can be removed in a future CL). The final
                    // feature should have a trailing comma for cleaner diffs.
            };

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({ApiCall.ADD_WEB_MESSAGE_LISTENER,
            ApiCall.CLEAR_PROXY_OVERRIDE,
            ApiCall.GET_PROXY_CONTROLLER,
            ApiCall.GET_SAFE_BROWSING_PRIVACY_POLICY_URL,
            ApiCall.GET_SERVICE_WORKER_CONTROLLER,
            ApiCall.GET_SERVICE_WORKER_WEB_SETTINGS,
            ApiCall.GET_TRACING_CONTROLLER,
            ApiCall.GET_WEBCHROME_CLIENT,
            ApiCall.GET_WEBVIEW_CLIENT,
            ApiCall.GET_WEBVIEW_RENDERER,
            ApiCall.GET_WEBVIEW_RENDERER_CLIENT,
            ApiCall.INIT_SAFE_BROWSING,
            ApiCall.INSERT_VISUAL_STATE_CALLBACK,
            ApiCall.IS_MULTI_PROCESS_ENABLED,
            ApiCall.JS_REPLY_POST_MESSAGE,
            ApiCall.POST_MESSAGE_TO_MAIN_FRAME,
            ApiCall.REMOVE_WEB_MESSAGE_LISTENER,
            ApiCall.SERVICE_WORKER_SETTINGS_GET_ALLOW_CONTENT_ACCESS,
            ApiCall.SERVICE_WORKER_SETTINGS_GET_ALLOW_FILE_ACCESS,
            ApiCall.SERVICE_WORKER_SETTINGS_GET_BLOCK_NETWORK_LOADS,
            ApiCall.SERVICE_WORKER_SETTINGS_GET_CACHE_MODE,
            ApiCall.SERVICE_WORKER_SETTINGS_SET_ALLOW_CONTENT_ACCESS,
            ApiCall.SERVICE_WORKER_SETTINGS_SET_ALLOW_FILE_ACCESS,
            ApiCall.SERVICE_WORKER_SETTINGS_SET_BLOCK_NETWORK_LOADS,
            ApiCall.SERVICE_WORKER_SETTINGS_SET_CACHE_MODE,
            ApiCall.SET_PROXY_OVERRIDE,
            ApiCall.SET_SAFE_BROWSING_ALLOWLIST_DEPRECATED_NAME,
            ApiCall.SET_SERVICE_WORKER_CLIENT,
            ApiCall.SET_WEBVIEW_RENDERER_CLIENT,
            ApiCall.TRACING_CONTROLLER_IS_TRACING,
            ApiCall.TRACING_CONTROLLER_START,
            ApiCall.TRACING_CONTROLLER_STOP,
            ApiCall.WEB_MESSAGE_GET_DATA,
            ApiCall.WEB_MESSAGE_GET_PORTS,
            ApiCall.WEB_MESSAGE_PORT_CLOSE,
            ApiCall.WEB_MESSAGE_PORT_POST_MESSAGE,
            ApiCall.WEB_MESSAGE_PORT_SET_CALLBACK,
            ApiCall.WEB_MESSAGE_PORT_SET_CALLBACK_WITH_HANDLER,
            ApiCall.WEB_RESOURCE_REQUEST_IS_REDIRECT,
            ApiCall.WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS,
            ApiCall.WEB_SETTINGS_GET_FORCE_DARK,
            ApiCall.WEB_SETTINGS_GET_FORCE_DARK_BEHAVIOR,
            ApiCall.WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER,
            ApiCall.WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED,
            ApiCall.WEB_SETTINGS_GET_WILL_SUPPRESS_ERROR_PAGE,
            ApiCall.WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS,
            ApiCall.WEB_SETTINGS_SET_FORCE_DARK,
            ApiCall.WEB_SETTINGS_SET_FORCE_DARK_BEHAVIOR,
            ApiCall.WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER,
            ApiCall.WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED,
            ApiCall.WEB_SETTINGS_SET_WILL_SUPPRESS_ERROR_PAGE,
            ApiCall.WEBVIEW_RENDERER_TERMINATE,
            ApiCall.ADD_DOCUMENT_START_SCRIPT,
            ApiCall.REMOVE_DOCUMENT_START_SCRIPT,
            ApiCall.SET_SAFE_BROWSING_ALLOWLIST,
            ApiCall.SET_PROXY_OVERRIDE_REVERSE_BYPASS,
            ApiCall.WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE,
            ApiCall.WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE,
            ApiCall.SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE,
            ApiCall.SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE,
            ApiCall.GET_VARIATIONS_HEADER,
            ApiCall.WEB_SETTINGS_GET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED,
            ApiCall.WEB_SETTINGS_SET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED,
            ApiCall.COOKIE_MANAGER_GET_COOKIE_INFO,
            ApiCall.WEB_MESSAGE_GET_MESSAGE_PAYLOAD,
            ApiCall.WEB_MESSAGE_PAYLOAD_GET_TYPE,
            ApiCall.WEB_MESSAGE_PAYLOAD_GET_AS_STRING,
            ApiCall.WEB_MESSAGE_PAYLOAD_GET_AS_ARRAY_BUFFER,
            ApiCall.WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST,
            ApiCall.WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST,
            ApiCall.SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST,
            ApiCall.SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST,
            ApiCall.GET_IMAGE_DRAG_DROP_IMPLEMENTATION,
            ApiCall.RESTRICT_SENSITIVE_WEB_CONTENT,
            ApiCall.JS_REPLY_POST_MESSAGE_WITH_PAYLOAD,
            // Add new constants above. The final constant should have a trailing comma for cleaner
            // diffs.
            ApiCall.COUNT, // Added to suppress WrongConstant in #recordApiCall
    })
    public @interface ApiCall {
        int ADD_WEB_MESSAGE_LISTENER = 0;
        int CLEAR_PROXY_OVERRIDE = 1;
        int GET_PROXY_CONTROLLER = 2;
        int GET_SAFE_BROWSING_PRIVACY_POLICY_URL = 3;
        int GET_SERVICE_WORKER_CONTROLLER = 4;
        int GET_SERVICE_WORKER_WEB_SETTINGS = 5;
        int GET_TRACING_CONTROLLER = 6;
        int GET_WEBCHROME_CLIENT = 7;
        int GET_WEBVIEW_CLIENT = 8;
        int GET_WEBVIEW_RENDERER = 9;
        int GET_WEBVIEW_RENDERER_CLIENT = 10;
        int INIT_SAFE_BROWSING = 11;
        int INSERT_VISUAL_STATE_CALLBACK = 12;
        int IS_MULTI_PROCESS_ENABLED = 13;
        int JS_REPLY_POST_MESSAGE = 14;
        int POST_MESSAGE_TO_MAIN_FRAME = 15;
        int REMOVE_WEB_MESSAGE_LISTENER = 16;
        int SERVICE_WORKER_SETTINGS_GET_ALLOW_CONTENT_ACCESS = 17;
        int SERVICE_WORKER_SETTINGS_GET_ALLOW_FILE_ACCESS = 18;
        int SERVICE_WORKER_SETTINGS_GET_BLOCK_NETWORK_LOADS = 19;
        int SERVICE_WORKER_SETTINGS_GET_CACHE_MODE = 20;
        int SERVICE_WORKER_SETTINGS_SET_ALLOW_CONTENT_ACCESS = 21;
        int SERVICE_WORKER_SETTINGS_SET_ALLOW_FILE_ACCESS = 22;
        int SERVICE_WORKER_SETTINGS_SET_BLOCK_NETWORK_LOADS = 23;
        int SERVICE_WORKER_SETTINGS_SET_CACHE_MODE = 24;
        int SET_PROXY_OVERRIDE = 25;
        int SET_SAFE_BROWSING_ALLOWLIST_DEPRECATED_NAME = 26;
        int SET_SERVICE_WORKER_CLIENT = 27;
        int SET_WEBVIEW_RENDERER_CLIENT = 28;
        int TRACING_CONTROLLER_IS_TRACING = 29;
        int TRACING_CONTROLLER_START = 30;
        int TRACING_CONTROLLER_STOP = 31;
        int WEB_MESSAGE_GET_DATA = 32;
        int WEB_MESSAGE_GET_PORTS = 33;
        int WEB_MESSAGE_PORT_CLOSE = 34;
        int WEB_MESSAGE_PORT_POST_MESSAGE = 35;
        int WEB_MESSAGE_PORT_SET_CALLBACK = 36;
        int WEB_MESSAGE_PORT_SET_CALLBACK_WITH_HANDLER = 37;
        int WEB_RESOURCE_REQUEST_IS_REDIRECT = 38;
        int WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS = 39;
        int WEB_SETTINGS_GET_FORCE_DARK = 40;
        int WEB_SETTINGS_GET_FORCE_DARK_BEHAVIOR = 41;
        int WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER = 42;
        int WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED = 43;
        int WEB_SETTINGS_GET_WILL_SUPPRESS_ERROR_PAGE = 44;
        int WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS = 45;
        int WEB_SETTINGS_SET_FORCE_DARK = 46;
        int WEB_SETTINGS_SET_FORCE_DARK_BEHAVIOR = 47;
        int WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER = 48;
        int WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED = 49;
        int WEB_SETTINGS_SET_WILL_SUPPRESS_ERROR_PAGE = 50;
        int WEBVIEW_RENDERER_TERMINATE = 51;
        int ADD_DOCUMENT_START_SCRIPT = 52;
        int REMOVE_DOCUMENT_START_SCRIPT = 53;
        int SET_SAFE_BROWSING_ALLOWLIST = 54;
        int SET_PROXY_OVERRIDE_REVERSE_BYPASS = 55;
        @Deprecated
        int WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE = 56;
        @Deprecated
        int WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE = 57;
        @Deprecated
        int SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE = 58;
        @Deprecated
        int SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE = 59;
        int GET_VARIATIONS_HEADER = 60;
        int WEB_SETTINGS_GET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED = 61;
        int WEB_SETTINGS_SET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED = 62;
        int COOKIE_MANAGER_GET_COOKIE_INFO = 63;
        int WEB_MESSAGE_GET_MESSAGE_PAYLOAD = 64;
        int WEB_MESSAGE_PAYLOAD_GET_TYPE = 65;
        int WEB_MESSAGE_PAYLOAD_GET_AS_STRING = 66;
        int WEB_MESSAGE_PAYLOAD_GET_AS_ARRAY_BUFFER = 67;
        int WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST = 68;
        int WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST = 69;
        int SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST = 70;
        int SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST = 71;
        int GET_IMAGE_DRAG_DROP_IMPLEMENTATION = 72;
        int RESTRICT_SENSITIVE_WEB_CONTENT = 73;
        int JS_REPLY_POST_MESSAGE_WITH_PAYLOAD = 74;
        // Remember to update AndroidXWebkitApiCall in enums.xml when adding new values here
        int COUNT = 75;
    }
    // clang-format on

    public static void recordApiCall(@ApiCall int apiCall) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.AndroidX.ApiCall", apiCall, ApiCall.COUNT);
    }

    // Initialization guarded by mAwInit.getLock()
    private InvocationHandler mStatics;
    private InvocationHandler mServiceWorkerController;
    private InvocationHandler mTracingController;
    private InvocationHandler mProxyController;
    private InvocationHandler mDropDataProvider;

    public SupportLibWebViewChromiumFactory() {
        mCompatConverterAdapter = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebkitToCompatConverterAdapter());
        mAwInit = WebkitToSharedGlueConverter.getGlobalAwInit();
    }

    @Override
    public /* WebViewProvider */ InvocationHandler createWebView(WebView webView) {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebViewChromium(webView));
    }

    @Override
    public InvocationHandler getWebkitToCompatConverter() {
        return mCompatConverterAdapter;
    }

    private static class StaticsAdapter implements StaticsBoundaryInterface {
        private SharedStatics mSharedStatics;

        public StaticsAdapter(SharedStatics sharedStatics) {
            mSharedStatics = sharedStatics;
        }

        @Override
        public void initSafeBrowsing(Context context, ValueCallback<Boolean> callback) {
            recordApiCall(ApiCall.INIT_SAFE_BROWSING);
            mSharedStatics.initSafeBrowsing(context, CallbackConverter.fromValueCallback(callback));
        }

        @Override
        public void setSafeBrowsingAllowlist(Set<String> hosts, ValueCallback<Boolean> callback) {
            recordApiCall(ApiCall.SET_SAFE_BROWSING_ALLOWLIST);
            mSharedStatics.setSafeBrowsingAllowlist(
                    new ArrayList<>(hosts), CallbackConverter.fromValueCallback(callback));
        }

        @Override
        public void setSafeBrowsingWhitelist(List<String> hosts, ValueCallback<Boolean> callback) {
            recordApiCall(ApiCall.SET_SAFE_BROWSING_ALLOWLIST_DEPRECATED_NAME);
            mSharedStatics.setSafeBrowsingAllowlist(
                    hosts, CallbackConverter.fromValueCallback(callback));
        }

        @Override
        public Uri getSafeBrowsingPrivacyPolicyUrl() {
            recordApiCall(ApiCall.GET_SAFE_BROWSING_PRIVACY_POLICY_URL);
            return mSharedStatics.getSafeBrowsingPrivacyPolicyUrl();
        }

        @Override
        public boolean isMultiProcessEnabled() {
            recordApiCall(ApiCall.IS_MULTI_PROCESS_ENABLED);
            return mSharedStatics.isMultiProcessEnabled();
        }

        @Override
        public String getVariationsHeader() {
            recordApiCall(ApiCall.GET_VARIATIONS_HEADER);
            return mSharedStatics.getVariationsHeader();
        }
    }

    @Override
    public InvocationHandler getStatics() {
        synchronized (mAwInit.getLock()) {
            if (mStatics == null) {
                mStatics = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new StaticsAdapter(
                                WebkitToSharedGlueConverter.getGlobalAwInit().getStatics()));
            }
        }
        return mStatics;
    }

    @Override
    public String[] getSupportedFeatures() {
        return mWebViewSupportedFeatures;
    }

    @Override
    public InvocationHandler getServiceWorkerController() {
        recordApiCall(ApiCall.GET_SERVICE_WORKER_CONTROLLER);
        synchronized (mAwInit.getLock()) {
            if (mServiceWorkerController == null) {
                mServiceWorkerController =
                        BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                new SupportLibServiceWorkerControllerAdapter(
                                        mAwInit.getServiceWorkerController()));
            }
        }
        return mServiceWorkerController;
    }

    @Override
    public InvocationHandler getTracingController() {
        recordApiCall(ApiCall.GET_TRACING_CONTROLLER);
        synchronized (mAwInit.getLock()) {
            if (mTracingController == null) {
                mTracingController = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibTracingControllerAdapter(new SharedTracingControllerAdapter(
                                mAwInit.getRunQueue(), mAwInit.getAwTracingController())));
            }
        }
        return mTracingController;
    }

    @Override
    public InvocationHandler getProxyController() {
        recordApiCall(ApiCall.GET_PROXY_CONTROLLER);
        synchronized (mAwInit.getLock()) {
            if (mProxyController == null) {
                mProxyController = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibProxyControllerAdapter(
                                mAwInit.getRunQueue(), mAwInit.getAwProxyController()));
            }
        }
        return mProxyController;
    }

    @Override
    public void setSupportLibraryVersion(String version) {
        AwDebug.setSupportLibraryWebkitVersionCrashKey(version);
    }

    @Override
    public InvocationHandler getDropDataProvider() {
        recordApiCall(ApiCall.GET_IMAGE_DRAG_DROP_IMPLEMENTATION);
        synchronized (mAwInit.getLock()) {
            if (mDropDataProvider == null) {
                mDropDataProvider = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibDropDataContentProviderAdapter());
            }
        }
        return mDropDataProvider;
    }
}
