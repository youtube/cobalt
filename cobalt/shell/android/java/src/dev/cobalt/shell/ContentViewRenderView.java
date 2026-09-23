// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.cobalt.shell;

import static dev.cobalt.shell.Shell.TAG;

import android.app.Activity;
import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.view.AttachedSurfaceControl;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.Window;
import android.widget.FrameLayout;
import org.chromium.base.Log;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.WindowAndroid;
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

  private final WindowSurfaceBridge mSurfaceBridge = new WindowSurfaceBridge();
  protected WebContents mWebContents;

  private int mWidth;
  private int mHeight;

  /**
   * Constructs a new ContentViewRenderView. This should be called and the {@link
   * ContentViewRenderView} should be added to the view hierarchy before the first draw.
   *
   * @param context The context used to create this.
   */
  public ContentViewRenderView(Context context) {
    super(context);

    Log.i(TAG, "ContentViewRenderView: created using WindowSurfaceBridge");
  }

  /**
   * Initialization that requires native libraries should be done here. Native code should
   * add/remove the layers to be rendered through the ContentViewLayerRenderer.
   *
   * @param rootWindow The {@link WindowAndroid} this render view should be linked to.
   */
  public void onNativeLibraryLoaded(WindowAndroid rootWindow) {
    assert rootWindow != null;
    mNativeContentViewRenderView =
        ContentViewRenderViewJni.get().init(ContentViewRenderView.this, rootWindow);
    assert mNativeContentViewRenderView != 0;
    mWindowAndroid = rootWindow;
    SurfaceHolder.Callback surfaceCallback =
        new SurfaceHolder.Callback() {
          @Override
          public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            assert mNativeContentViewRenderView != 0;
            // TODO: b/511379756 - Pass InputTransferToken instead of null for Android 15+
            // "Transfer Input to Viz" optimization, similar to upstream ContentViewRenderView.
            ContentViewRenderViewJni.get()
                .surfaceChanged(
                    mNativeContentViewRenderView,
                    ContentViewRenderView.this,
                    format,
                    width,
                    height,
                    holder.getSurface(),
                    mSurfaceBridge.getSurfaceControl());
            if (mWebContents != null) {
              ContentViewRenderViewJni.get()
                  .onPhysicalBackingSizeChanged(
                      mNativeContentViewRenderView,
                      ContentViewRenderView.this,
                      mWebContents,
                      width,
                      height);
            }
          }

          @Override
          public void surfaceCreated(SurfaceHolder holder) {
            assert mNativeContentViewRenderView != 0;
            ContentViewRenderViewJni.get()
                .surfaceCreated(mNativeContentViewRenderView, ContentViewRenderView.this);

            onReadyToRender();
          }

          @Override
          public void surfaceDestroyed(SurfaceHolder holder) {
            assert mNativeContentViewRenderView != 0;
            ContentViewRenderViewJni.get()
                .surfaceDestroyed(mNativeContentViewRenderView, ContentViewRenderView.this);
          }
        };
    mSurfaceBridge.connect(surfaceCallback, rootWindow);
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh) {
    mWidth = w;
    mHeight = h;
    if (mWebContents != null) mWebContents.setSize(w, h);
  }

  /** View's method override to notify WindowAndroid about changes in its visibility. */
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
   * Gets the View used for layout anchoring, animation placeholder, or accessibility (this host
   * View in Window Surface mode).
   */
  public View getAnchorView() {
    return this;
  }

  /**
   * Should be called when the ContentViewRenderView is not needed anymore so its associated native
   * resource can be freed.
   */
  public void destroy() {
    mSurfaceBridge.disconnect();
    mWindowAndroid = null;
    ContentViewRenderViewJni.get()
        .destroy(mNativeContentViewRenderView, ContentViewRenderView.this);
    mNativeContentViewRenderView = 0;
  }

  public void setCurrentWebContents(WebContents webContents) {
    assert mNativeContentViewRenderView != 0;
    mWebContents = webContents;

    if (webContents != null) {
      webContents.setSize(mWidth, mHeight);
      ContentViewRenderViewJni.get()
          .onPhysicalBackingSizeChanged(
              mNativeContentViewRenderView,
              ContentViewRenderView.this,
              webContents,
              mWidth,
              mHeight);
    }
    ContentViewRenderViewJni.get()
        .setCurrentWebContents(
            mNativeContentViewRenderView, ContentViewRenderView.this, webContents);
  }

  /**
   * This method should be subclassed to provide actions to be performed once the view is ready to
   * render.
   */
  protected void onReadyToRender() {}

  /**
   * Enter or leave overlay video mode.
   *
   * @param enabled Whether overlay mode is enabled.
   */
  public void setOverlayVideoMode(boolean enabled) {
    int format = enabled ? PixelFormat.TRANSLUCENT : PixelFormat.OPAQUE;
    mSurfaceBridge.setFormat(format);
    ContentViewRenderViewJni.get()
        .setOverlayVideoMode(mNativeContentViewRenderView, ContentViewRenderView.this, enabled);
  }

  /**
   * Takes ownership of the Activity's Window surface. This allows direct rendering to the window
   * surface instead of a child SurfaceView.
   *
   * <p>Lifetime: Bound to the lifetime of the outer ContentViewRenderView and the associated
   * Activity. Threading: Must be called on the UI thread.
   */
  private static class WindowSurfaceBridge {
    private Window mWindow;
    private SurfaceHolder mWindowSurfaceHolder;
    private SurfaceControl mSurfaceControl;

    /**
     * The pending PixelFormat (e.g. TRANSLUCENT for overlay video mode, OPAQUE for normal).
     * Buffered when setFormat() is called before the surface is ready, and consumed once the
     * SurfaceHolder becomes available.
     */
    private Integer mPendingSurfaceFormat;

    private final java.util.List<Runnable> mPendingTasks = new java.util.ArrayList<>();
    private boolean mIsNativeStarted;
    private boolean mIsSurfaceCreatedDispatched;

    private static Window getWindow(WindowAndroid windowAndroid) {
      if (windowAndroid == null || windowAndroid.getActivity() == null) {
        return null;
      }
      Activity activity = windowAndroid.getActivity().get();
      return activity != null ? activity.getWindow() : null;
    }

    private void ensureSurfaceControl() {
      if (mSurfaceControl != null || mWindow == null) {
        return;
      }
      if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
        return;
      }

      AttachedSurfaceControl rootSurfaceControl = mWindow.getRootSurfaceControl();
      if (rootSurfaceControl == null && mWindow.peekDecorView() != null) {
        rootSurfaceControl = mWindow.peekDecorView().getRootSurfaceControl();
      }
      if (rootSurfaceControl == null) {
        Log.w(TAG, "ContentViewRenderView: AttachedSurfaceControl rootSurfaceControl is null");
        return;
      }

      SurfaceControl surfaceControl =
          new SurfaceControl.Builder().setName("CobaltWindowSurfaceControl").build();
      try (SurfaceControl.Transaction transaction =
          rootSurfaceControl.buildReparentTransaction(surfaceControl)) {
        if (transaction == null) {
          Log.w(TAG, "ContentViewRenderView: buildReparentTransaction returned null");
          surfaceControl.release();
          return;
        }

        transaction.setVisibility(surfaceControl, true).apply();
      }
      mSurfaceControl = surfaceControl;
    }

    private void releaseSurfaceControl() {
      if (mSurfaceControl != null) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
          try (SurfaceControl.Transaction transaction = new SurfaceControl.Transaction()) {
            transaction.reparent(mSurfaceControl, null).apply();
          }
        }
        mSurfaceControl.release();
        mSurfaceControl = null;
      }
    }

    private void registerStartupListener() {
      // Fast-path: if native is already started, don't wait or register.
      if (BrowserStartupController.getInstance().isNativeStarted()) {
        mIsNativeStarted = true;
        return;
      }

      BrowserStartupController.getInstance()
          .addStartupCompletedObserver(
              new BrowserStartupController.StartupCallback() {
                @Override
                public void onSuccess() {
                  Log.i(TAG, "ContentViewRenderView: Startup complete");
                  while (!mPendingTasks.isEmpty()) {
                    mPendingTasks.remove(0).run();
                  }
                  mIsNativeStarted = true;
                }

                @Override
                public void onFailure() {
                  Log.e(
                      TAG, "ContentViewRenderView: Native startup failed; clearing pending tasks");
                  mPendingTasks.clear();
                }
              });
    }

    private void connect(SurfaceHolder.Callback surfaceCallback, WindowAndroid windowAndroid) {
      mWindow = getWindow(windowAndroid);
      if (mWindow == null) {
        Log.w(
            TAG,
            "ContentViewRenderView: WindowSurfaceBridge connect failed: Activity or Window is"
                + " null.");
        return;
      }

      // Native browser startup (Mojo initialization) may still be in progress during Activity
      // creation
      // on slower devices. Listen for startup completion to safely drain surface events
      // (b/539880857).
      registerStartupListener();

      mWindow.takeSurface(
          new SurfaceHolder.Callback2() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
              mWindowSurfaceHolder = holder;
              if (!mIsNativeStarted) {
                mPendingTasks.add(() -> handleSurfaceCreated(holder));
                return;
              }
              handleSurfaceCreated(holder);
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
              mWindowSurfaceHolder = holder;
              if (!mIsNativeStarted) {
                mPendingTasks.add(
                    () -> surfaceCallback.surfaceChanged(holder, format, width, height));
                return;
              }
              surfaceCallback.surfaceChanged(holder, format, width, height);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
              mWindowSurfaceHolder = null;
              mPendingTasks.clear();

              if (mIsSurfaceCreatedDispatched) {
                mIsSurfaceCreatedDispatched = false;
                surfaceCallback.surfaceDestroyed(holder);
              }
              releaseSurfaceControl();
            }

            @Override
            public void surfaceRedrawNeeded(SurfaceHolder holder) {
              if (!(surfaceCallback instanceof SurfaceHolder.Callback2)) {
                return;
              }
              ((SurfaceHolder.Callback2) surfaceCallback).surfaceRedrawNeeded(holder);
            }

            private void handleSurfaceCreated(SurfaceHolder holder) {
              Log.i(TAG, "ContentViewRenderView: Surface created");
              ensureSurfaceControl();
              applyPendingSurfaceFormat();
              surfaceCallback.surfaceCreated(holder);
              mIsSurfaceCreatedDispatched = true;
            }
          });
    }

    private void disconnect() {
      if (mWindow != null) {
        mWindow.takeSurface(null);
      } else {
        Log.w(TAG, "ContentViewRenderView: disconnect() is called w/o connect().");
      }
      releaseSurfaceControl();
      mWindow = null;
      mWindowSurfaceHolder = null;
      mPendingSurfaceFormat = null;
      mPendingTasks.clear();
      mIsSurfaceCreatedDispatched = false;
    }

    private SurfaceControl getSurfaceControl() {
      return mSurfaceControl;
    }

    private void setFormat(int format) {
      mPendingSurfaceFormat = format;
      applyPendingSurfaceFormat();
    }

    private void applyPendingSurfaceFormat() {
      if (mPendingSurfaceFormat == null) {
        return;
      }
      if (mWindowSurfaceHolder == null) {
        Log.i(TAG, "ContentViewRenderView: surface is not ready yet. Will apply format later");
        return;
      }
      Log.i(TAG, "ContentViewRenderView: Applying pending format");
      mWindowSurfaceHolder.setFormat(mPendingSurfaceFormat);
      mPendingSurfaceFormat = null;
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

    void setCurrentWebContents(
        long nativeContentViewRenderView, ContentViewRenderView caller, WebContents webContents);

    void onPhysicalBackingSizeChanged(
        long nativeContentViewRenderView,
        ContentViewRenderView caller,
        WebContents webContents,
        int width,
        int height);

    void surfaceCreated(long nativeContentViewRenderView, ContentViewRenderView caller);

    void surfaceDestroyed(long nativeContentViewRenderView, ContentViewRenderView caller);

    void surfaceChanged(
        long nativeContentViewRenderView,
        ContentViewRenderView caller,
        int format,
        int width,
        int height,
        Surface surface,
        Object hostInputToken);

    void setOverlayVideoMode(
        long nativeContentViewRenderView, ContentViewRenderView caller, boolean enabled);
  }
}
