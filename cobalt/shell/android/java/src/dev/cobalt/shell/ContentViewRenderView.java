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
    private boolean mIsRecreatingSurface = false;

    private int mWidth;
    private int mHeight;

    /**
     * Constructs a new ContentViewRenderView.
     * This should be called and the {@link ContentViewRenderView} should be added to the view
     * hierarchy before the first draw to avoid a black flash that is seen every time a
     * {@link SurfaceView} is added.
     * @param context The context used to create this.
     */
    public ContentViewRenderView(Context context) {
        super(context);

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
        assert !getSurfaceView().getHolder().getSurface().isValid()
            : "Surface created before native library loaded.";
        assert rootWindow != null;
        mNativeContentViewRenderView =
                ContentViewRenderViewJni.get().init(ContentViewRenderView.this, rootWindow);
        assert mNativeContentViewRenderView != 0;
        mWindowAndroid = rootWindow;
        SurfaceHolder.Callback surfaceCallback = new SurfaceHolder.Callback() {
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
                assert mNativeContentViewRenderView != 0;
                ContentViewRenderViewJni.get().surfaceCreated(
                        mNativeContentViewRenderView, ContentViewRenderView.this);

                // On pre-M Android, layers start in the hidden state until a relayout happens.
                // There is a bug that manifests itself when entering overlay mode on pre-M
                // devices, where a relayout never happens. This bug is out of Chromium's
                // control, but can be worked around by forcibly re-setting the visibility of
                // the surface view. Otherwise, the screen stays black, and some tests fail.
                getSurfaceView().setVisibility(getSurfaceView().getVisibility());

                onReadyToRender();
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                assert mNativeContentViewRenderView != 0;
                ContentViewRenderViewJni.get().surfaceDestroyed(
                        mNativeContentViewRenderView, ContentViewRenderView.this);
            }
        };
        mSurfaceBridge.connect(surfaceCallback);
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
        mSurfaceBridge.disconnect();
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
        return getSurfaceView().getHolder().getSurface() != null;
    }

    /**
     * Set the Z-order of the surface view. Recreates the SurfaceView if the Z-order changes
     * to work around Android 10 dynamic Z-order bugs.
     */
    public void setUiResourceZOrder(boolean onTop) {
        // Only apply the Z-order recreation workaround on Android 11 (API 30) and below.
        // Android 12 (API 31) and above have reliable OS cleanup and do not suffer from the leak,
        // so we preserve the original smooth, flash-free behavior on newer devices.
        if (android.os.Build.VERSION.SDK_INT <= 30) {
            mSurfaceBridge.setZOrderMediaOverlay(this, onTop);
        }
    }

    /**
     * Enter or leave overlay video mode.
     * @param enabled Whether overlay mode is enabled.
     */
    public void setOverlayVideoMode(boolean enabled) {
        android.util.Log.i("CobaltContentView", "setOverlayVideoMode: " + enabled);
        mOverlayVideoModeEnabled = enabled;
        int format = enabled ? PixelFormat.TRANSLUCENT : PixelFormat.OPAQUE;
        getSurfaceView().getHolder().setFormat(format);
        ContentViewRenderViewJni.get().setOverlayVideoMode(
                mNativeContentViewRenderView, ContentViewRenderView.this, enabled);
    }

    @CalledByNative
    private void didSwapFrame() {
        if (mIsRecreatingSurface) {
            post(new Runnable() {
                @Override
                public void run() {
                    setBackgroundResource(0);
                    mIsRecreatingSurface = false;
                }
            });
        }
        if (getSurfaceView().getBackground() != null) {
            post(new Runnable() {
                @Override
                public void run() {
                    getSurfaceView().setBackgroundResource(0);
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
        private boolean mCurrentZOrder = false;

        protected SurfaceView getSurfaceView() {
            return mSurfaceView;
        }

        protected void initialize(ContentViewRenderView renderView) {
            mSurfaceView = renderView.createSurfaceView(renderView.getContext());
            boolean initialZOrder = android.os.Build.VERSION.SDK_INT >= 31;
            android.util.Log.i("CobaltContentView", "SurfaceBridge.initialize: setting ZOrderMediaOverlay to " + initialZOrder);
            mSurfaceView.setZOrderMediaOverlay(initialZOrder);
            mCurrentZOrder = initialZOrder;

            renderView.addView(mSurfaceView,
                    new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                            FrameLayout.LayoutParams.MATCH_PARENT));
            mSurfaceView.setVisibility(GONE);
        }

        protected void connect(SurfaceHolder.Callback surfaceCallback) {
            mSurfaceCallback = surfaceCallback;
            mSurfaceView.getHolder().addCallback(mSurfaceCallback);
            mSurfaceView.setVisibility(VISIBLE);
        }

        protected void disconnect() {
            mSurfaceView.getHolder().removeCallback(mSurfaceCallback);
        }

        protected void setZOrderMediaOverlay(ContentViewRenderView renderView, boolean onTop) {
            if (mSurfaceView == null) return;
            if (mCurrentZOrder == onTop) {
                android.util.Log.i("CobaltContentView", "SurfaceBridge.setZOrderMediaOverlay: already in state " + onTop);
                return;
            }
            android.util.Log.i("CobaltContentView", "SurfaceBridge.setZOrderMediaOverlay: recreating SurfaceView to set ZOrderMediaOverlay to " + onTop);

            renderView.mIsRecreatingSurface = true;
            renderView.setBackgroundColor(android.graphics.Color.BLACK); // Cover the transition flash

            boolean isVisible = mSurfaceView.getVisibility() == VISIBLE;
            renderView.removeView(mSurfaceView);
            if (mSurfaceCallback != null) {
                mSurfaceView.getHolder().removeCallback(mSurfaceCallback);
            }

            mSurfaceView = renderView.createSurfaceView(renderView.getContext());
            mSurfaceView.setZOrderMediaOverlay(onTop);
            mSurfaceView.setBackgroundColor(android.graphics.Color.TRANSPARENT);
            int format = renderView.mOverlayVideoModeEnabled ? PixelFormat.TRANSLUCENT : PixelFormat.OPAQUE;
            mSurfaceView.getHolder().setFormat(format);

            renderView.addView(mSurfaceView,
                    new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                            FrameLayout.LayoutParams.MATCH_PARENT));

            if (mSurfaceCallback != null) {
                mSurfaceView.getHolder().addCallback(mSurfaceCallback);
            }
            mSurfaceView.setVisibility(isVisible ? VISIBLE : GONE);
            mCurrentZOrder = onTop;
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
