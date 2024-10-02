// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.content.ClipData;
import android.content.ClipData.Item;
import android.content.ClipDescription;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.media.ThumbnailUtils;
import android.net.Uri;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityManager;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.compat.ApiHelperForO;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.R;
import org.chromium.ui.dragdrop.AnimatedImageDragShadowBuilder.CursorOffset;
import org.chromium.ui.dragdrop.AnimatedImageDragShadowBuilder.DragShadowSpec;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Drag and drop helper class in charge of building the clip data, wrapping calls to
 * {@link android.view.View#startDragAndDrop}. Also used for mocking out real function calls to
 * Android.
 */
public class DragAndDropDelegateImpl implements DragAndDropDelegate, DragStateTracker {
    /**
     * Java Enum of AndroidDragTargetType used for histogram recording for
     * Android.DragDrop.FromWebContent.TargetType. This is used for histograms and should therefore
     * be treated as append-only.
     */
    @IntDef({DragTargetType.INVALID, DragTargetType.TEXT, DragTargetType.IMAGE, DragTargetType.LINK,
            DragTargetType.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @interface DragTargetType {
        int INVALID = 0;
        int TEXT = 1;
        int IMAGE = 2;
        int LINK = 3;

        int NUM_ENTRIES = 4;
    }

    private int mShadowWidth;
    private int mShadowHeight;
    private boolean mIsDragStarted;

    /**
     * Whether the current drop has happened on top of the view this object tracks.
     */
    private boolean mIsDropOnView;

    /**
     * The type of drag target from the view this object tracks.
     */
    private @DragTargetType int mDragTargetType;

    private float mDragStartXDp;
    private float mDragStartYDp;

    private long mDragStartSystemElapsedTime;

    private @Nullable DragAndDropBrowserDelegate mDragAndDropBrowserDelegate;

    // Implements ViewAndroidDelegate.DragAndDropDelegate

    /**
     * Create DragShadowBuilder and start {@link View#startDragAndDrop(ClipData, DragShadowBuilder,
     * Object, int)}.
     *
     * @param containerView The container view where the drag starts.
     * @param shadowImage The bitmap which represents the shadow image.
     * @param dropData The drop data presenting the drag target.
     * @param cursorOffsetX The x offset of the cursor w.r.t. to top-left corner of the drag-image.
     * @param cursorOffsetY The y offset of the cursor w.r.t. to top-left corner of the drag-image.
     * @param dragObjRectWidth The width of the drag object.
     * @param dragObjRectHeight The height of the drag object.
     */
    @Override
    public boolean startDragAndDrop(@NonNull View containerView, @NonNull Bitmap shadowImage,
            @NonNull DropDataAndroid dropData, int cursorOffsetX, int cursorOffsetY,
            int dragObjRectWidth, int dragObjRectHeight) {
        // Drag and drop is disabled when gesture related a11y service is enabled.
        // See https://crbug.com/1250067.
        AccessibilityManager a11yManager =
                (AccessibilityManager) containerView.getContext().getSystemService(
                        Context.ACCESSIBILITY_SERVICE);
        if (a11yManager.isEnabled() && a11yManager.isTouchExplorationEnabled()) return false;

        ClipData clipdata = buildClipData(dropData);
        if (clipdata == null) {
            return false;
        }

        mIsDragStarted = true;
        mDragStartSystemElapsedTime = SystemClock.elapsedRealtime();
        mDragTargetType = getDragTargetType(dropData);
        int windowWidth = containerView.getRootView().getWidth();
        int windowHeight = containerView.getRootView().getHeight();
        Object myLocalState = null;
        if (mDragAndDropBrowserDelegate != null
                && mDragAndDropBrowserDelegate.getSupportDropInChrome()) {
            myLocalState = dropData;
        }
        View.DragShadowBuilder dragShadowBuilder = createDragShadowBuilder(containerView,
                shadowImage, dropData.hasImage(), windowWidth, windowHeight, cursorOffsetX,
                cursorOffsetY, dragObjRectWidth, dragObjRectHeight);
        return ApiHelperForN.startDragAndDrop(
                containerView, clipdata, dragShadowBuilder, myLocalState, buildFlags(dropData));
    }

    @Override
    public void setDragAndDropBrowserDelegate(DragAndDropBrowserDelegate delegate) {
        mDragAndDropBrowserDelegate = delegate;
    }

    // Implements DragStateTracker
    @Override
    public boolean isDragStarted() {
        return mIsDragStarted;
    }

    @Override
    public int getDragShadowWidth() {
        return mShadowWidth;
    }

    @Override
    public int getDragShadowHeight() {
        return mShadowHeight;
    }

    @Override
    public void destroy() {
        reset();
    }

    // Implements View.OnDragListener
    @Override
    public boolean onDrag(View view, DragEvent dragEvent) {
        if (!mIsDragStarted) {
            if (mDragAndDropBrowserDelegate != null
                    && mDragAndDropBrowserDelegate.getSupportDropInChrome()
                    && dragEvent.getAction() == DragEvent.ACTION_DROP) {
                onDropFromOutside(dragEvent);
            }
            return false;
        }

        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                onDragStarted(dragEvent);
                break;
            case DragEvent.ACTION_DROP:
                onDrop(dragEvent);
                break;
            case DragEvent.ACTION_DRAG_ENDED:
                onDragEnd(dragEvent);
                reset();
                break;
            default:
                // No action needed for other types of drag actions.
                break;
        }
        // Return false so this listener does not consume the drag event of the view it listened to.
        return false;
    }

    /**
     * Builds the clipData from the dragged data based on the data's type, it will return null when
     * the input DropData cannot be converted into ClipData (e.g. dragged item is image and there's
     * no content provider to handle it)
     *
     * @param dropData The data to be dropped.
     * @return ClipData based on the dropData type.
     */
    @Nullable
    protected ClipData buildClipData(DropDataAndroid dropData) {
        @DragTargetType
        int type = getDragTargetType(dropData);
        switch (type) {
            case DragTargetType.TEXT:
                return ClipData.newPlainText(null, dropData.text);
            case DragTargetType.IMAGE:
                Uri cachedUri = DropDataProviderUtils.cacheImageData(dropData);
                // If there's no content provider we shouldn't start the drag.
                if (cachedUri == null) {
                    return null;
                }
                ClipData clipData = ClipData.newUri(
                        ContextUtils.getApplicationContext().getContentResolver(), null, cachedUri);
                // Add image link URL to the ClipData if present. Since the ClipData MIME types for
                // the items are different, this will not be supported for O- versions where {@link
                // ClipData#addItem(ContentResolver, Item)} is not available.
                if (VERSION.SDK_INT >= VERSION_CODES.O && dropData.hasLink()) {
                    ApiHelperForO.addItem(clipData,
                            ContextUtils.getApplicationContext().getContentResolver(),
                            new Item(dropData.gurl.getSpec()));
                }
                return clipData;
            case DragTargetType.LINK:
                if (mDragAndDropBrowserDelegate != null) {
                    Intent intent =
                            mDragAndDropBrowserDelegate.createLinkIntent(dropData.gurl.getSpec());
                    if (intent != null) {
                        return new ClipData(null,
                                new String[] {ClipDescription.MIMETYPE_TEXT_PLAIN,
                                        ClipDescription.MIMETYPE_TEXT_INTENT},
                                new Item(getTextForLinkData(dropData), intent, null));
                    }
                }
                return ClipData.newPlainText(null, getTextForLinkData(dropData));
            case DragTargetType.INVALID:
                return null;
            case DragTargetType.NUM_ENTRIES:
            default:
                assert false : "Should not be reached!";
                return null;
        }
    }

    protected int buildFlags(DropDataAndroid dropData) {
        if (dropData.isPlainText()) {
            return View.DRAG_FLAG_GLOBAL;
        } else if (dropData.hasImage()) {
            int flag = View.DRAG_FLAG_GLOBAL | View.DRAG_FLAG_GLOBAL_URI_READ;
            if (mDragAndDropBrowserDelegate != null
                    && mDragAndDropBrowserDelegate.getSupportAnimatedImageDragShadow()) {
                flag |= View.DRAG_FLAG_OPAQUE;
            }
            return flag;
        } else if (dropData.hasLink()) {
            return View.DRAG_FLAG_GLOBAL;
        } else {
            return 0;
        }
    }

    protected View.DragShadowBuilder createDragShadowBuilder(View containerView, Bitmap shadowImage,
            boolean isImage, int windowWidth, int windowHeight, int cursorOffsetX,
            int cursorOffsetY, int dragObjRectWidth, int dragObjRectHeight) {
        Context context = containerView.getContext();
        ImageView imageView = new ImageView(context);
        if (isImage) {
            // If drag shadow image is an 1*1 image, it is not considered as a valid drag shadow.
            // In such cases, use a globe icon as placeholder instead. See
            // https://crbug.com/1304433.
            if (shadowImage.getHeight() <= 1 && shadowImage.getWidth() <= 1) {
                Drawable globeIcon =
                        AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp);
                assert globeIcon != null;

                final int minSize =
                        AnimatedImageDragShadowBuilder.getDragShadowMinSize(context.getResources());
                mShadowWidth = minSize;
                mShadowHeight = minSize;
                imageView.setLayoutParams(new ViewGroup.LayoutParams(minSize, minSize));
                imageView.setScaleType(ImageView.ScaleType.FIT_CENTER);
                imageView.setImageDrawable(globeIcon);
            } else {
                DragShadowSpec dragShadowSpec = AnimatedImageDragShadowBuilder.getDragShadowSpec(
                        context, shadowImage.getWidth(), shadowImage.getHeight(), windowWidth,
                        windowHeight);
                updateShadowSizeWithBorder(
                        context, dragShadowSpec.targetWidth, dragShadowSpec.targetHeight);
                if (mDragAndDropBrowserDelegate != null
                        && mDragAndDropBrowserDelegate.getSupportAnimatedImageDragShadow()) {
                    assert dragObjRectWidth != 0;
                    assert dragObjRectHeight != 0;
                    CursorOffset cursorOffset = AnimatedImageDragShadowBuilder.adjustCursorOffset(
                            cursorOffsetX, cursorOffsetY, dragObjRectWidth, dragObjRectHeight,
                            dragShadowSpec);
                    return new AnimatedImageDragShadowBuilder(containerView, shadowImage,
                            cursorOffset.x, cursorOffset.y, dragShadowSpec);
                } else {
                    updateShadowImage(context, shadowImage, imageView, dragShadowSpec.targetWidth,
                            dragShadowSpec.targetHeight);
                }
            }
        } else {
            mShadowWidth = shadowImage.getWidth();
            mShadowHeight = shadowImage.getHeight();
            imageView.setImageBitmap(shadowImage);
        }
        imageView.layout(0, 0, mShadowWidth, mShadowHeight);

        return new DragShadowBuilder(imageView);
    }

    /**
     * Helper function to update the drag shadow:
     * 1. Resize and center crop shadowImage to target size;
     * 2. Round corners to 8dp;
     * 3. Add 1dp border.
     */
    private void updateShadowImage(Context context, Bitmap shadowImage, ImageView imageView,
            int targetWidth, int targetHeight) {
        Resources res = context.getResources();
        shadowImage = ThumbnailUtils.extractThumbnail(
                shadowImage, targetWidth, targetHeight, ThumbnailUtils.OPTIONS_RECYCLE_INPUT);
        RoundedBitmapDrawable drawable = RoundedBitmapDrawableFactory.create(res, shadowImage);
        drawable.setCornerRadius(
                res.getDimensionPixelSize(R.dimen.drag_shadow_border_corner_radius));
        imageView.setImageDrawable(drawable);
        imageView.setBackgroundResource(R.drawable.drag_shadow_background);
        int borderSize = res.getDimensionPixelSize(R.dimen.drag_shadow_border_size);
        imageView.setPadding(borderSize, borderSize, borderSize, borderSize);
    }

    // Enlarge the shadow image by twice the size of the border.
    private void updateShadowSizeWithBorder(Context context, int targetWidth, int targetHeight) {
        Resources res = context.getResources();
        int borderSize = res.getDimensionPixelSize(R.dimen.drag_shadow_border_size);
        mShadowWidth = targetWidth + borderSize * 2;
        mShadowHeight = targetHeight + borderSize * 2;
    }

    private void onDragStarted(DragEvent dragStartEvent) {
        mDragStartXDp = dragStartEvent.getX();
        mDragStartYDp = dragStartEvent.getY();
    }

    private void onDrop(DragEvent dropEvent) {
        mIsDropOnView = true;

        final int dropDistance = Math.round(MathUtils.distance(
                mDragStartXDp, mDragStartYDp, dropEvent.getX(), dropEvent.getY()));
        RecordHistogram.recordExactLinearHistogram(
                "Android.DragDrop.FromWebContent.DropInWebContent.DistanceDip", dropDistance, 51);

        long dropDuration = SystemClock.elapsedRealtime() - mDragStartSystemElapsedTime;
        RecordHistogram.recordMediumTimesHistogram(
                "Android.DragDrop.FromWebContent.DropInWebContent.Duration", dropDuration);
    }

    private void onDropFromOutside(DragEvent dropEvent) {
        if (mDragAndDropBrowserDelegate == null) {
            return;
        }
        DragAndDropPermissions dragAndDropPermissions =
                mDragAndDropBrowserDelegate.getDragAndDropPermissions(dropEvent);
        if (dragAndDropPermissions == null) {
            return;
        }
        // TODO(shuyng): Read image data in background thread.
        dragAndDropPermissions.release();
    }

    private void onDragEnd(DragEvent dragEndEvent) {
        boolean dragResult = dragEndEvent.getResult();

        // Only record metrics when drop does not happen for ContentView.
        if (!mIsDropOnView) {
            assert mDragStartSystemElapsedTime > 0;
            assert mDragTargetType != DragTargetType.INVALID;
            long dragDuration = SystemClock.elapsedRealtime() - mDragStartSystemElapsedTime;
            recordDragDurationAndResult(dragDuration, dragResult);
            recordDragTargetType(mDragTargetType);
        }

        DropDataProviderUtils.clearImageCache(!mIsDropOnView && dragResult);
    }

    /**
     * Return the {@link DragTargetType} based on the content of DropDataAndroid. The result will
     * bias plain text > image > link.
     * TODO(https://crbug.com/1299994): Manage the ClipData bias with EventForwarder in one place.
     */
    static @DragTargetType int getDragTargetType(DropDataAndroid dropDataAndroid) {
        if (dropDataAndroid.isPlainText()) {
            return DragTargetType.TEXT;
        } else if (dropDataAndroid.hasImage()) {
            return DragTargetType.IMAGE;
        } else if (dropDataAndroid.hasLink()) {
            return DragTargetType.LINK;
        } else {
            return DragTargetType.INVALID;
        }
    }

    /**
     * Return the text to be dropped when {@link DropDataAndroid} contains a link.
     */
    static String getTextForLinkData(DropDataAndroid dropData) {
        assert dropData.hasLink();
        if (TextUtils.isEmpty(dropData.text)) return dropData.gurl.getSpec();
        return dropData.text + "\n" + dropData.gurl.getSpec();
    }

    private void reset() {
        mShadowHeight = 0;
        mShadowWidth = 0;
        mDragTargetType = DragTargetType.INVALID;
        mIsDragStarted = false;
        mIsDropOnView = false;
        mDragStartSystemElapsedTime = -1;
    }

    private void recordDragTargetType(@DragTargetType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DragDrop.FromWebContent.TargetType", type, DragTargetType.NUM_ENTRIES);
    }

    private void recordDragDurationAndResult(long duration, boolean result) {
        String histogramPrefix = "Android.DragDrop.FromWebContent.Duration.";
        String suffix = result ? "Success" : "Canceled";
        RecordHistogram.recordMediumTimesHistogram(histogramPrefix + suffix, duration);
    }

    @VisibleForTesting
    float getDragStartXDp() {
        return mDragStartXDp;
    }

    @VisibleForTesting
    float getDragStartYDp() {
        return mDragStartYDp;
    }
}
