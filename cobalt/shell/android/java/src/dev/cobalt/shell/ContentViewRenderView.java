// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.cobalt.shell;

import android.content.Context;
import android.graphics.PixelFormat;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import android.util.Log;

import org.chromium.base.CommandLine;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.WindowAndroid;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/***
 * This view is used by a ContentView to render its content.
 * Call {@link #setCurrentWebContents(WebContents)} with the webContents that should be
 * managing the content.
 * Note that only one WebContents can be shown at a time.
 */
@JNINamespace("cobalt")
public class ContentViewRenderView extends FrameLayout {
    // The native side of this object.
    private long mNativeContentViewRenderView;
    private WindowAndroid mWindowAndroid;

    protected SurfaceBridge mSurfaceBridge;
    protected WebContents mWebContents;
    private boolean mOverlayVideoModeEnabled = false;

    private int mWidth;
    private int mHeight;

    private boolean mUseWindowSurface = true;
    private SurfaceHolder mExternalSurfaceHolder;
    private SurfaceHolder.Callback mSurfaceCallback;

    public void setExternalSurfaceHolder(SurfaceHolder holder) {
        mExternalSurfaceHolder = holder;
    }

    public SurfaceHolder.Callback getSurfaceCallback() {
        return mSurfaceCallback;
    }

    public SurfaceHolder getSurfaceHolder() {
        if (mExternalSurfaceHolder != null) {
            return mExternalSurfaceHolder;
        }
        return getSurfaceView() != null ? getSurfaceView().getHolder() : null;
    }

    /**
     * Constructs a new ContentViewRenderView.
     * This should be called and the {@link ContentViewRenderView} should be added to the view
     * hierarchy before the first draw to avoid a black flash that is seen every time a
     * {@link SurfaceView} is added.
     * @param context The context used to create this.
     */
    public ContentViewRenderView(Context context) {
        super(context);
        mUseWindowSurface = true;
        Log.i("Cobalt", "KJ: ContentViewRenderView constructor, mUseWindowSurface=" + mUseWindowSurface);

        mSurfaceBridge = createSurfaceBridge();
        mSurfaceBridge.initialize(this);
    }

    protected SurfaceBridge createSurfaceBridge() {
        return new SurfaceBridge();
    }

    /**
     * Initialization that requires native libraries should be done here.
     * Native code should add/remove the layers to be rendered through the ContentViewLayerRenderer.
     * @param rootWindow The {@link WindowAndroid} this render view should be linked to.
     */
    public void onNativeLibraryLoaded(WindowAndroid rootWindow) {
        assert getSurfaceHolder() == null || !getSurfaceHolder().getSurface().isValid()
            : "Surface created before native library loaded.";
        assert rootWindow != null;
        mNativeContentViewRenderView =
                ContentViewRenderViewJni.get().init(ContentViewRenderView.this, rootWindow);
        assert mNativeContentViewRenderView != 0;
        mWindowAndroid = rootWindow;
        mSurfaceCallback = new SurfaceHolder.Callback() {
            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                assert mNativeContentViewRenderView != 0;
                // TODO: b/511379756 - Pass InputTransferToken instead of null for Android 15+
                // "Transfer Input to Viz" optimization, similar to upstream ContentViewRenderView.
                ContentViewRenderViewJni.get().surfaceChanged(mNativeContentViewRenderView,
                        ContentViewRenderView.this, format, width, height, holder.getSurface(),
                        /* hostInputToken= */ null);
                if (mWebContents != null) {
                    ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                            mNativeContentViewRenderView, ContentViewRenderView.this, mWebContents,
                            width, height);
                }
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.i("Cobalt", "KJ: ContentViewRenderView.mSurfaceCallback.surfaceCreated");
                assert mNativeContentViewRenderView != 0;
                ContentViewRenderViewJni.get().surfaceCreated(
                        mNativeContentViewRenderView, ContentViewRenderView.this);

                // On pre-M Android, layers start in the hidden state until a relayout happens.
                // There is a bug that manifests itself when entering overlay mode on pre-M
                // devices, where a relayout never happens. This bug is out of Chromium's
                // control, but can be worked around by forcibly re-setting the visibility of
                // the surface view. Otherwise, the screen stays black, and some tests fail.
                if (getSurfaceView() != null) {
                    getSurfaceView().setVisibility(getSurfaceView().getVisibility());
                }

                onReadyToRender();
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.i("Cobalt", "KJ: ContentViewRenderView.mSurfaceCallback.surfaceDestroyed");
                assert mNativeContentViewRenderView != 0;
                ContentViewRenderViewJni.get().surfaceDestroyed(
                        mNativeContentViewRenderView, ContentViewRenderView.this);
            }
        };
        if (!mUseWindowSurface) {
            mSurfaceBridge.connect(mSurfaceCallback);
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mWidth = w;
        mHeight = h;
        if (mWebContents != null) mWebContents.setSize(w, h);
    }

    /**
     * View's method override to notify WindowAndroid about changes in its visibility.
     */
    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        if (mWindowAndroid == null) return;

        if (visibility == View.GONE) {
            mWindowAndroid.onVisibilityChanged(false);
        } else if (visibility == View.VISIBLE) {
            mWindowAndroid.onVisibilityChanged(true);
        }
    }

    /**
     * Sets the background color of the surface view.  This method is necessary because the
     * background color of ContentViewRenderView itself is covered by the background of
     * SurfaceView.
     * @param color The color of the background.
     */
    public void setSurfaceViewBackgroundColor(int color) {
        if (getSurfaceView() != null) {
            getSurfaceView().setBackgroundColor(color);
        }
    }

    /**
     * Gets the SurfaceView for this ContentViewRenderView
     */
    public SurfaceView getSurfaceView() {
        return mSurfaceBridge.getSurfaceView();
    }

    /**
     * Should be called when the ContentViewRenderView is not needed anymore so its associated
     * native resource can be freed.
     */
    public void destroy() {
        if (!mUseWindowSurface) {
            mSurfaceBridge.disconnect();
        }
        mWindowAndroid = null;
        ContentViewRenderViewJni.get().destroy(
                mNativeContentViewRenderView, ContentViewRenderView.this);
        mNativeContentViewRenderView = 0;
    }

    public void setCurrentWebContents(WebContents webContents) {
        assert mNativeContentViewRenderView != 0;
        mWebContents = webContents;

        if (webContents != null) {
            webContents.setSize(mWidth, mHeight);
            ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                    mNativeContentViewRenderView, ContentViewRenderView.this, webContents, mWidth,
                    mHeight);
        }
        ContentViewRenderViewJni.get().setCurrentWebContents(
                mNativeContentViewRenderView, ContentViewRenderView.this, webContents);
    }

    /**
     * This method should be subclassed to provide actions to be performed once the view is ready to
     * render.
     */
    protected void onReadyToRender() {}

    /**
     * This method could be subclassed optionally to provide a custom SurfaceView object to
     * this ContentViewRenderView.
     * @param context The context used to create the SurfaceView object.
     * @return The created SurfaceView object.
     */
    protected SurfaceView createSurfaceView(Context context) {
        return new SurfaceView(context);
    }

    /**
     * @return whether the surface view is initialized and ready to render.
     */
    public boolean isInitialized() {
        SurfaceHolder holder = getSurfaceHolder();
        return holder != null && holder.getSurface() != null;
    }

    /**
     * Enter or leave overlay video mode.
     * @param enabled Whether overlay mode is enabled.
     */
    public void setOverlayVideoMode(boolean enabled) {
        mOverlayVideoModeEnabled = enabled;
        int format = enabled ? PixelFormat.TRANSLUCENT : PixelFormat.OPAQUE;
        SurfaceHolder holder = getSurfaceHolder();
        if (holder != null) {
            holder.setFormat(format);
        }
        ContentViewRenderViewJni.get().setOverlayVideoMode(
                mNativeContentViewRenderView, ContentViewRenderView.this, enabled);
    }

    public boolean isOverlayVideoModeEnabled() {
        return mOverlayVideoModeEnabled;
    }

    @CalledByNative
    private void didSwapFrame() {
        if (getSurfaceView() != null && getSurfaceView().getBackground() != null) {
            post(new Runnable() {
                @Override
                public void run() {
                    if (getSurfaceView() != null) {
                        getSurfaceView().setBackgroundResource(0);
                    }
                }
            });
        }
    }

    /**
     * Connecting class to hold a SurfaceView.
     */
    protected static class SurfaceBridge {
        private SurfaceView mSurfaceView;
        private SurfaceHolder.Callback mSurfaceCallback;

        protected SurfaceView getSurfaceView() {
            return mSurfaceView;
        }

        protected void initialize(ContentViewRenderView renderView) {
            if (renderView.mUseWindowSurface) {
                return;
            }
            mSurfaceView = renderView.createSurfaceView(renderView.getContext());
            mSurfaceView.setZOrderMediaOverlay(true);

            renderView.addView(mSurfaceView,
                    new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                            FrameLayout.LayoutParams.MATCH_PARENT));
            mSurfaceView.setVisibility(GONE);
        }

        protected void connect(SurfaceHolder.Callback surfaceCallback) {
            mSurfaceCallback = surfaceCallback;
            if (mSurfaceView != null) {
                mSurfaceView.getHolder().addCallback(mSurfaceCallback);
                mSurfaceView.setVisibility(VISIBLE);
            }
        }

        protected void disconnect() {
            if (mSurfaceView != null) {
                mSurfaceView.getHolder().removeCallback(mSurfaceCallback);
            }
        }
    }

    private EventForwarder getEventForwarder() {
        if (mWebContents == null || mWebContents.isDestroyed()) {
            return null;
        }
        return mWebContents.getEventForwarder();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        EventForwarder forwarder = getEventForwarder();
        if (forwarder != null) {
            return forwarder.onTouchEvent(event);
        }
        return super.onTouchEvent(event);
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        EventForwarder forwarder = getEventForwarder();
        if (forwarder != null) {
            return forwarder.onHoverEvent(event);
        }
        return super.onHoverEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        EventForwarder forwarder = getEventForwarder();
        if (forwarder != null) {
            return forwarder.onGenericMotionEvent(event);
        }
        return super.onGenericMotionEvent(event);
    }

    @NativeMethods
    interface Natives {
        long init(ContentViewRenderView caller, WindowAndroid rootWindow);
        void destroy(long nativeContentViewRenderView, ContentViewRenderView caller);
        void setCurrentWebContents(long nativeContentViewRenderView, ContentViewRenderView caller,
                WebContents webContents);
        void onPhysicalBackingSizeChanged(long nativeContentViewRenderView,
                ContentViewRenderView caller, WebContents webContents, int width, int height);
        void surfaceCreated(long nativeContentViewRenderView, ContentViewRenderView caller);
        void surfaceDestroyed(long nativeContentViewRenderView, ContentViewRenderView caller);
        void surfaceChanged(long nativeContentViewRenderView, ContentViewRenderView caller,
                int format, int width, int height, Surface surface, Object hostInputToken);
        void setOverlayVideoMode(
                long nativeContentViewRenderView, ContentViewRenderView caller, boolean enabled);
    }
}
