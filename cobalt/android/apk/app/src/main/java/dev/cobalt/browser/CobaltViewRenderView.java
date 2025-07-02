// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.cobalt.browser;

import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/***
 * Copied from org.chromium.components.embedder_support.view.ContentViewRenderView.
 * This view is used by a ContentView to render its content.
 * Call {@link #setCurrentWebContents(WebContents)} with the webContents that should be
 * managing the content.
 * Note that only one WebContents can be shown at a time.
 */
@JNINamespace("cobalt")
public class CobaltViewRenderView extends FrameLayout {
    // The native side of this object.
    private long mNativeCobaltViewRenderView;
    private WindowAndroid mWindowAndroid;

    protected SurfaceBridge mSurfaceBridge;
    protected WebContents mWebContents;

    private int mWidth;
    private int mHeight;

    /**
     * Constructs a new CobaltViewRenderView.
     * This should be called and the {@link CobaltViewRenderView} should be added to the view
     * hierarchy before the first draw to avoid a black flash that is seen every time a
     * {@link SurfaceView} is added.
     * @param context The context used to create this.
     */
    public CobaltViewRenderView(Context context) {
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
        mNativeCobaltViewRenderView =
                CobaltViewRenderViewJni.get().init(CobaltViewRenderView.this, rootWindow);
        assert mNativeCobaltViewRenderView != 0;
        mWindowAndroid = rootWindow;
        SurfaceHolder.Callback surfaceCallback = new SurfaceHolder.Callback() {
            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                assert mNativeCobaltViewRenderView != 0;
                CobaltViewRenderViewJni.get().surfaceChanged(mNativeCobaltViewRenderView,
                        CobaltViewRenderView.this, format, width, height, holder.getSurface());
                if (mWebContents != null) {
                    CobaltViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                            mNativeCobaltViewRenderView, CobaltViewRenderView.this, mWebContents,
                            width, height);
                }
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                assert mNativeCobaltViewRenderView != 0;
                CobaltViewRenderViewJni.get().surfaceCreated(
                        mNativeCobaltViewRenderView, CobaltViewRenderView.this);

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
                assert mNativeCobaltViewRenderView != 0;
                CobaltViewRenderViewJni.get().surfaceDestroyed(
                        mNativeCobaltViewRenderView, CobaltViewRenderView.this);
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
     * background color of CobaltViewRenderView itself is covered by the background of
     * SurfaceView.
     * @param color The color of the background.
     */
    public void setSurfaceViewBackgroundColor(int color) {
        if (getSurfaceView() != null) {
            getSurfaceView().setBackgroundColor(color);
        }
    }

    /**
     * Gets the SurfaceView for this CobaltViewRenderView
     */
    public SurfaceView getSurfaceView() {
        return mSurfaceBridge.getSurfaceView();
    }

    /**
     * Should be called when the CobaltViewRenderView is not needed anymore so its associated
     * native resource can be freed.
     */
    public void destroy() {
        mSurfaceBridge.disconnect();
        mWindowAndroid = null;
        CobaltViewRenderViewJni.get().destroy(
                mNativeCobaltViewRenderView, CobaltViewRenderView.this);
        mNativeCobaltViewRenderView = 0;
    }

    public void setCurrentWebContents(WebContents webContents) {
        assert mNativeCobaltViewRenderView != 0;
        mWebContents = webContents;

        if (webContents != null) {
            webContents.setSize(mWidth, mHeight);
            CobaltViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                    mNativeCobaltViewRenderView, CobaltViewRenderView.this, webContents, mWidth,
                    mHeight);
        }
        CobaltViewRenderViewJni.get().setCurrentWebContents(
                mNativeCobaltViewRenderView, CobaltViewRenderView.this, webContents);
    }

    /**
     * This method should be subclassed to provide actions to be performed once the view is ready to
     * render.
     */
    protected void onReadyToRender() {}

    /**
     * This method could be subclassed optionally to provide a custom SurfaceView object to
     * this CobaltViewRenderView.
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
     * Enter or leave overlay video mode.
     * @param enabled Whether overlay mode is enabled.
     */
    public void setOverlayVideoMode(boolean enabled) {
        int format = enabled ? PixelFormat.TRANSLUCENT : PixelFormat.OPAQUE;
        getSurfaceView().getHolder().setFormat(format);
        CobaltViewRenderViewJni.get().setOverlayVideoMode(
                mNativeCobaltViewRenderView, CobaltViewRenderView.this, enabled);
    }

    @CalledByNative
    private void didSwapFrame() {
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
     * TODO add doc.
     */
    protected static class SurfaceBridge {
        private SurfaceView mSurfaceView;
        private SurfaceHolder.Callback mSurfaceCallback;

        protected SurfaceView getSurfaceView() {
            return mSurfaceView;
        }

        protected void initialize(CobaltViewRenderView renderView) {
            mSurfaceView = renderView.createSurfaceView(renderView.getContext());
            mSurfaceView.setZOrderMediaOverlay(true);

            renderView.setSurfaceViewBackgroundColor(Color.WHITE);
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
    }

    @NativeMethods
    interface Natives {
        long init(CobaltViewRenderView caller, WindowAndroid rootWindow);
        void destroy(long nativeCobaltViewRenderView, CobaltViewRenderView caller);
        void setCurrentWebContents(long nativeCobaltViewRenderView, CobaltViewRenderView caller,
                WebContents webContents);
        void onPhysicalBackingSizeChanged(long nativeCobaltViewRenderView,
                CobaltViewRenderView caller, WebContents webContents, int width, int height);
        void surfaceCreated(long nativeCobaltViewRenderView, CobaltViewRenderView caller);
        void surfaceDestroyed(long nativeCobaltViewRenderView, CobaltViewRenderView caller);
        void surfaceChanged(long nativeCobaltViewRenderView, CobaltViewRenderView caller,
                int format, int width, int height, Surface surface);
        void setOverlayVideoMode(
                long nativeCobaltViewRenderView, CobaltViewRenderView caller, boolean enabled);
    }
}
