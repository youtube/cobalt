// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.content.Context;
import android.os.Build;
import android.view.View;
import android.view.ViewStructure;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * This class receives captured content from native and forwards to ContetnCaptureConsumer.
 */
@JNINamespace("content_capture")
public class OnscreenContentProvider {
    private static final String TAG = "ContentCapture";
    private static Boolean sDump;

    private long mNativeOnscreenContentProviderAndroid;

    private ArrayList<ContentCaptureConsumer> mContentCaptureConsumers =
            new ArrayList<ContentCaptureConsumer>();

    private WeakReference<WebContents> mWebContents;

    public OnscreenContentProvider(
            Context context, View view, ViewStructure structure, WebContents webContents) {
        mWebContents = new WeakReference<WebContents>(webContents);
        if (sDump == null) sDump = ContentCaptureFeatures.isDumpForTestingEnabled();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ContentCaptureConsumer consumer =
                    PlatformContentCaptureConsumer.create(context, view, structure, webContents);
            if (consumer != null) {
                mContentCaptureConsumers.add(consumer);
            }
        }
        if (ContentCaptureFeatures.shouldTriggerContentCaptureForExperiment()) {
            mContentCaptureConsumers.add(new ExperimentContentCaptureConsumer());
        }
        if (!mContentCaptureConsumers.isEmpty()) {
            createNativeObject();
        }
    }

    public OnscreenContentProvider(Context context, View view, WebContents webContents) {
        this(context, view, null, webContents);
    }

    public void destroy() {
        destroyNativeObject();
    }

    private void destroyNativeObject() {
        if (mNativeOnscreenContentProviderAndroid == 0) return;
        OnscreenContentProviderJni.get().destroy(mNativeOnscreenContentProviderAndroid);
        mNativeOnscreenContentProviderAndroid = 0;
    }

    private void createNativeObject() {
        WebContents webContents = mWebContents.get();
        if (webContents != null) {
            mNativeOnscreenContentProviderAndroid =
                    OnscreenContentProviderJni.get().init(this, webContents);
        }
    }

    public void addConsumer(ContentCaptureConsumer consumer) {
        mContentCaptureConsumers.add(consumer);
        if (mNativeOnscreenContentProviderAndroid == 0) createNativeObject();
    }

    public void removeConsumer(ContentCaptureConsumer consumer) {
        mContentCaptureConsumers.remove(consumer);
        if (mContentCaptureConsumers.isEmpty()) destroyNativeObject();
    }

    public void onWebContentsChanged(WebContents current) {
        mWebContents = new WeakReference<WebContents>(current);
        if (mNativeOnscreenContentProviderAndroid != 0) {
            OnscreenContentProviderJni.get().onWebContentsChanged(
                    mNativeOnscreenContentProviderAndroid, current);
        }
    }

    @CalledByNative
    private void didCaptureContent(Object[] session, ContentCaptureFrame data) {
        FrameSession frameSession = toFrameSession(session);
        String[] urls = buildUrls(frameSession, data);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onContentCaptured(frameSession, data);
            }
        }
        if (sDump.booleanValue()) Log.i(TAG, "Captured Content: %s", data);
    }

    @CalledByNative
    private void didUpdateContent(Object[] session, ContentCaptureFrame data) {
        FrameSession frameSession = toFrameSession(session);
        String[] urls = buildUrls(frameSession, data);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onContentUpdated(frameSession, data);
            }
        }
        if (sDump.booleanValue()) Log.i(TAG, "Updated Content: %s", data);
    }

    @CalledByNative
    private void didRemoveContent(Object[] session, long[] data) {
        FrameSession frameSession = toFrameSession(session);
        String[] urls = buildUrls(frameSession, null);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onContentRemoved(frameSession, data);
            }
        }
        if (sDump.booleanValue()) {
            Log.i(TAG, "Removed Content: %s", frameSession.get(0) + " " + Arrays.toString(data));
        }
    }

    @CalledByNative
    private void didRemoveSession(Object[] session) {
        FrameSession frameSession = toFrameSession(session);
        String[] urls = buildUrls(frameSession, null);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onSessionRemoved(frameSession);
            }
        }
        if (sDump.booleanValue()) Log.i(TAG, "Removed Session: %s", frameSession.get(0));
    }

    @CalledByNative
    private void didUpdateTitle(ContentCaptureFrame mainFrame) {
        String[] urls = buildUrls(null, mainFrame);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onTitleUpdated(mainFrame);
            }
        }
        if (sDump.booleanValue()) Log.i(TAG, "Updated Title: %s", mainFrame.getTitle());
    }

    @CalledByNative
    private void didUpdateFavicon(ContentCaptureFrame mainFrame) {
        String[] urls = buildUrls(null, mainFrame);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onFaviconUpdated(mainFrame);
            }
        }
        if (sDump.booleanValue()) Log.i(TAG, "Updated Favicon: %s", mainFrame.getFavicon());
    }

    @CalledByNative
    private int getOffsetY(WebContents webContents) {
        return RenderCoordinates.fromWebContents(webContents).getContentOffsetYPixInt();
    }

    @CalledByNative
    private boolean shouldCapture(String url) {
        String[] urls = new String[] {url};
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) return true;
        }
        return false;
    }

    private FrameSession toFrameSession(Object[] session) {
        FrameSession frameSession = new FrameSession(session.length);
        for (Object s : session) frameSession.add((ContentCaptureFrame) s);
        return frameSession;
    }

    private String[] buildUrls(FrameSession session, ContentCaptureFrame data) {
        ArrayList<String> urls = new ArrayList<String>();
        if (session != null) {
            for (ContentCaptureFrame d : session) {
                urls.add(d.getUrl());
            }
        }
        if (data != null) urls.add(data.getUrl());
        String[] result = new String[urls.size()];
        urls.toArray(result);
        return result;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public List<ContentCaptureConsumer> getConsumersForTesting() {
        return mContentCaptureConsumers;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public void removePlatformConsumerForTesting() {
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer instanceof PlatformContentCaptureConsumer) {
                mContentCaptureConsumers.remove(consumer);
                return;
            }
        }
    }

    @NativeMethods
    interface Natives {
        long init(OnscreenContentProvider caller, WebContents webContents);
        void onWebContentsChanged(
                long nativeOnscreenContentProviderAndroid, WebContents webContents);
        void destroy(long nativeOnscreenContentProviderAndroid);
    }
}
