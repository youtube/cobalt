// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.text.TextUtils;
import android.webkit.ConsoleMessage;
import android.webkit.GeolocationPermissions;
import android.webkit.PermissionRequest;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;

import androidx.webkit.WebViewClientCompat;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * This activity is used for running layout tests using webview. The activity
 * creates a webview instance, loads url and captures console messages from
 * JavaScript until the test is finished.
 * provides a blocking callback.
 */
public class WebViewLayoutTestActivity extends Activity {

    private final StringBuilder mConsoleLog = new StringBuilder();
    private final Object mLock = new Object();
    private static final String TEST_FINISHED_SENTINEL = "TEST FINISHED";

    private WebView mWebView;
    private boolean mFinished;
    private boolean mGrantPermission;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_webview);
        mWebView = (WebView) findViewById(R.id.webview);
        WebSettings settings = mWebView.getSettings();
        initializeSettings(settings);

        mWebView.setWebViewClient(new WebViewClientCompat() {
            @SuppressWarnings("deprecation") // because we support api level 19 and up.
            @Override
            public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                return false;
            }

            @SuppressWarnings("deprecation") // because we support api level 19 and up.
            @Override
            public void onReceivedError(WebView view, int errorCode, String description,
                    String failingUrl) {
                mConsoleLog.append("WebView error: " + description + ", " + failingUrl + "\n");
                mConsoleLog.append(TEST_FINISHED_SENTINEL + "\n");
                finishTest();
            }
        });

        mWebView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onGeolocationPermissionsShowPrompt(String origin,
                    GeolocationPermissions.Callback callback) {
                mConsoleLog.append("onGeolocationPermissionsShowPrompt" + "\n");
                if (mGrantPermission) {
                    mConsoleLog.append("geolocation request granted" + "\n");
                    callback.invoke(origin, true /* allow */, false);
                } else {
                    mConsoleLog.append("geolocation request denied" + "\n");
                    callback.invoke(origin, false /* allow */, false);
                }
            }

            @Override
            @SuppressLint("NewApi") // PermissionRequest#deny requires API level 21.
            public void onPermissionRequest(PermissionRequest request) {
                mConsoleLog.append("onPermissionRequest: "
                        + TextUtils.join(",", request.getResources()) + "\n");
                if (mGrantPermission) {
                    mConsoleLog.append("request granted: "
                            + TextUtils.join(",", request.getResources()) + "\n");
                    request.grant(request.getResources());
                } else {
                    mConsoleLog.append("request denied" + "\n");
                    request.deny();
                }
            }

            @Override
            public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
                // TODO(timvolodine): put log and warnings in separate string builders.
                mConsoleLog.append(consoleMessage.message() + "\n");
                if (consoleMessage.message().equals(TEST_FINISHED_SENTINEL)) {
                    finishTest();
                }
                return true;
            }
        });
    }

    public void waitForFinish(long timeout, TimeUnit unit) throws InterruptedException,
            TimeoutException {
        synchronized (mLock) {
            long deadline = System.currentTimeMillis() + unit.toMillis(timeout);
            while (!mFinished && System.currentTimeMillis() < deadline) {
                mLock.wait(deadline - System.currentTimeMillis());
            }
            if (!mFinished) {
                throw new TimeoutException("timeout");
            }
        }
    }

    public String getTestResult() {
        return mConsoleLog.toString();
    }

    public void loadUrl(String url) {
        mWebView.loadUrl(url);
        mWebView.requestFocus();
    }

    public void setGrantPermission(boolean allow) {
        mGrantPermission = allow;
    }

    private void initializeSettings(WebSettings settings) {
        settings.setJavaScriptEnabled(true);
        settings.setGeolocationEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowFileAccessFromFileURLs(true);
    }

    private void finishTest() {
        mFinished = true;
        synchronized (mLock) {
            mLock.notifyAll();
        }
    }
}
