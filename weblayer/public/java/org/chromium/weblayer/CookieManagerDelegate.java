// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.webengine.interfaces.ExceptionType;
import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.ICookieManagerDelegate;
import org.chromium.webengine.interfaces.IStringCallback;
import org.chromium.weblayer_private.interfaces.APICallException;

/**
 * This class acts as a proxy between the embedding app's WebFragment and
 * the WebLayer implementation.
 */
class CookieManagerDelegate extends ICookieManagerDelegate.Stub {
    private CookieManager mCookieManager;
    private Handler mHandler = new Handler(Looper.getMainLooper());

    CookieManagerDelegate(CookieManager cookieManager) {
        mCookieManager = cookieManager;
    }

    @Override
    public void setCookie(String uri, String value, IBooleanCallback callback) {
        mHandler.post(() -> {
            try {
                mCookieManager.setCookie(Uri.parse(uri), value,
                        new org.chromium.weblayer_private.interfaces.IBooleanCallback.Stub() {
                            @Override
                            public void onResult(boolean result) {
                                try {
                                    callback.onResult(result);
                                } catch (RemoteException e) {
                                }
                            }
                            @Override
                            public void onException(@ExceptionType int type, String msg) {
                                try {
                                    callback.onException(ExceptionHelper.convertType(type), msg);
                                } catch (RemoteException e) {
                                }
                            }
                        });
            } catch (APICallException e) {
                try {
                    callback.onException(ExceptionType.UNKNOWN, e.getMessage());
                } catch (RemoteException re) {
                }
            }
        });
    }

    @Override
    public void getCookie(String uri, IStringCallback callback) {
        mHandler.post(() -> {
            try {
                mCookieManager.getCookie(Uri.parse(uri),
                        new org.chromium.weblayer_private.interfaces.IStringCallback.Stub() {
                            @Override
                            public void onResult(String result) {
                                try {
                                    callback.onResult(result);
                                } catch (RemoteException e) {
                                }
                            }
                            @Override
                            public void onException(@ExceptionType int type, String msg) {
                                try {
                                    callback.onException(ExceptionHelper.convertType(type), msg);
                                } catch (RemoteException e) {
                                }
                            }
                        });
            } catch (APICallException e) {
                try {
                    callback.onException(ExceptionType.UNKNOWN, e.getMessage());
                } catch (RemoteException re) {
                }
            }
        });
    }
}
