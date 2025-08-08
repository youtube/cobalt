// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static androidx.core.view.WindowInsetsCompat.Type.systemBars;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.DialogStyles;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ModalDialogFeatureList.MODAL_DIALOG_LAYOUT_WITH_SYSTEM_INSETS)
public class AppModalPresenterUnitTest {
    private static final int WINDOW_WIDTH = 800;
    private static final int WINDOW_HEIGHT = 800;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private InsetObserver mInsetObserver;

    private DisplayMetrics mDisplayMetrics;
    private AppModalPresenter mAppModalPresenter;
    private PropertyModel mModel;
    private ObservableSupplierImpl<Boolean> mEdgeToEdgeStateSupplier;

    @Before
    public void setup() {
        Activity activity = buildActivity(Activity.class).setup().get();
        mDisplayMetrics = activity.getResources().getDisplayMetrics();
        mDisplayMetrics.density = 1;
        mAppModalPresenter = new AppModalPresenter(activity);
        mAppModalPresenter.setInsetObserver(mInsetObserver);
        mEdgeToEdgeStateSupplier = new ObservableSupplierImpl<>();
        mAppModalPresenter.setEdgeToEdgeStateSupplier(mEdgeToEdgeStateSupplier);
        mModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS).build();
    }

    @After
    public void teardown() {
        mAppModalPresenter.removeDialogView(mModel);
    }

    @Test
    public void addDialogView_NonZeroSystemBarsInsets_EdgeToEdgeActive() {
        // max(left, right, fixedMargin) = max(25, 20, 16) = 25.
        int expectedHorizontalMargin = 25;
        // max(top, bottom, fixedMargin) = max(40, 32, 16) = 40.
        int expectedVerticalMargin = 40;
        doTestAddViewWithSystemBarsInsets(
                /* left= */ 25,
                /* top= */ 40,
                /* right= */ 20,
                /* bottom= */ 32,
                expectedHorizontalMargin,
                expectedVerticalMargin,
                /* isEdgeToEdgeActive= */ true);
    }

    @Test
    public void addDialogView_NonZeroSystemBarsInsets_EdgeToEdgeInactive() {
        // expectedHorizontalMargin = expectedVerticalMargin = fixedMargin = 16.
        int expectedHorizontalMargin = 16;
        int expectedVerticalMargin = 16;
        doTestAddViewWithSystemBarsInsets(
                /* left= */ 25,
                /* top= */ 40,
                /* right= */ 20,
                /* bottom= */ 32,
                expectedHorizontalMargin,
                expectedVerticalMargin,
                /* isEdgeToEdgeActive= */ false);
    }

    @Test
    public void addDialogView_NoSystemBarsInsets_EdgeToEdgeActive() {
        // max(left, right, fixedMargin) = max(0, 0, 16) = 16.
        int expectedHorizontalMargin = 16;
        // max(top, bottom, fixedMargin) = max(0, 0, 16) = 16.
        int expectedVerticalMargin = 16;
        doTestAddViewWithSystemBarsInsets(
                /* left= */ 0,
                /* top= */ 0,
                /* right= */ 0,
                /* bottom= */ 0,
                expectedHorizontalMargin,
                expectedVerticalMargin,
                /* isEdgeToEdgeActive= */ true);
    }

    @Test
    public void addDialogView_NoSystemBarsInsets_EdgeToEdgeInactive() {
        // max(left, right, fixedMargin) = max(0, 0, 16) = 16.
        int expectedHorizontalMargin = 16;
        // max(top, bottom, fixedMargin) = max(0, 0, 16) = 16.
        int expectedVerticalMargin = 16;
        doTestAddViewWithSystemBarsInsets(
                /* left= */ 0,
                /* top= */ 0,
                /* right= */ 0,
                /* bottom= */ 0,
                expectedHorizontalMargin,
                expectedVerticalMargin,
                /* isEdgeToEdgeActive= */ false);
    }

    private void doTestAddViewWithSystemBarsInsets(
            int left,
            int top,
            int right,
            int bottom,
            int expectedHorizontalMargin,
            int expectedVerticalMargin,
            boolean isEdgeToEdgeActive) {
        // Setup window.
        setupWindow(WINDOW_WIDTH, WINDOW_HEIGHT, left, top, right, bottom, isEdgeToEdgeActive);
        // Add dialog view.
        addDialogView();
        // Verify dialog margins.
        verifyDialogMargins(expectedHorizontalMargin, expectedVerticalMargin);
    }

    @Test
    public void addDialogView_MultipleDialogs() {
        // Setup initial window.
        setupWindow(
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                /* leftInset= */ 25,
                /* topInset= */ 40,
                /* rightInset= */ 20,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ true);

        // Add dialog view.
        addDialogView();

        // Verify dialog margins.
        verifyDialogMargins(25, 40);

        // Dismiss current dialog.
        mAppModalPresenter.removeDialogView(mModel);

        // Add a new dialog view while insets remain the same.
        addDialogView();

        // Verify dialog margins.
        verifyDialogMargins(25, 40);
    }

    @Test
    public void testRemoveDialogView() {
        setupWindow(
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                /* leftInset= */ 25,
                /* topInset= */ 40,
                /* rightInset= */ 20,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ true);

        addDialogView();
        mAppModalPresenter.removeDialogView(mModel);

        // This will try to run #onEnterAnimationStarted on the dialog view, which is now null. Just
        // need to make sure it doesn't crash.
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    @Test
    public void onWindowResizedWithDialogShowing_EdgeToEdgeActive() {
        // Setup initial window.
        setupWindow(
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                /* leftInset= */ 25,
                /* topInset= */ 40,
                /* rightInset= */ 20,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ true);

        // Add dialog view.
        addDialogView();

        // Verify dialog margins.
        verifyDialogMargins(25, 40);

        // Simulate window resizing / inset change while the dialog is showing.
        setupWindow(
                /* windowWidth= */ 1600,
                /* windowHeight= */ 1600,
                /* leftInset= */ 0,
                /* topInset= */ 0,
                /* rightInset= */ 0,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ true);
        // This method will be invoked when the dialog window is resized.
        mAppModalPresenter
                .getWindowInsetsListenerForTesting()
                .onApplyWindowInsets(mock(View.class), mock(WindowInsetsCompat.class));
        verifyDialogMargins(/* expectedHorizontalMargin= */ 16, /* expectedVerticalMargin= */ 32);
    }

    @Test
    public void onWindowResizedWithDialogShowing_EdgeToEdgeInactive() {
        // Setup initial window.
        setupWindow(
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                /* leftInset= */ 25,
                /* topInset= */ 40,
                /* rightInset= */ 20,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ false);

        // Add dialog view.
        addDialogView();

        // Verify dialog margins.
        verifyDialogMargins(16, 16);

        // Simulate window resizing / inset change while the dialog is showing.
        setupWindow(
                /* windowWidth= */ 1600,
                /* windowHeight= */ 1600,
                /* leftInset= */ 0,
                /* topInset= */ 0,
                /* rightInset= */ 0,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ false);
        // This method will be invoked when the dialog window is resized.
        mAppModalPresenter
                .getWindowInsetsListenerForTesting()
                .onApplyWindowInsets(mock(View.class), mock(WindowInsetsCompat.class));
        verifyDialogMargins(/* expectedHorizontalMargin= */ 16, /* expectedVerticalMargin= */ 16);
    }

    @Test
    public void edgeToEdgeStateChangeBeforeInsetChange() {
        // Setup initial window.
        setupWindow(
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                /* leftInset= */ 25,
                /* topInset= */ 40,
                /* rightInset= */ 20,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ true);

        // Add dialog view.
        addDialogView();

        // Verify dialog margins.
        verifyDialogMargins(/* expectedHorizontalMargin= */ 25, /* expectedVerticalMargin= */ 40);

        // Simulate window resizing where edge-to-edge state change is received before inset change
        // while the dialog is showing.
        setupWindow(
                /* windowWidth= */ 1600,
                /* windowHeight= */ 1600,
                /* leftInset= */ 0,
                /* topInset= */ 36,
                /* rightInset= */ 0,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ false);
        // This method will be invoked when the dialog window is resized.
        mAppModalPresenter
                .getWindowInsetsListenerForTesting()
                .onApplyWindowInsets(mock(View.class), mock(WindowInsetsCompat.class));

        verifyDialogMargins(/* expectedHorizontalMargin= */ 16, /* expectedVerticalMargin= */ 16);
    }

    @Test
    public void edgeToEdgeStateChangeAfterInsetChange() {
        // Setup initial window.
        setupWindow(
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                /* leftInset= */ 0,
                /* topInset= */ 36,
                /* rightInset= */ 0,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ false);

        // Add dialog view.
        addDialogView();

        // Verify dialog margins.
        verifyDialogMargins(/* expectedHorizontalMargin= */ 16, /* expectedVerticalMargin= */ 16);

        // Simulate window resizing where edge-to-edge state change is received after inset change
        // while the dialog is showing.
        setupWindow(
                /* windowWidth= */ 1600,
                /* windowHeight= */ 1600,
                /* leftInset= */ 25,
                /* topInset= */ 40,
                /* rightInset= */ 20,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ false);
        // This method will be invoked when the dialog window is resized.
        mAppModalPresenter
                .getWindowInsetsListenerForTesting()
                .onApplyWindowInsets(mock(View.class), mock(WindowInsetsCompat.class));
        mEdgeToEdgeStateSupplier.set(true);

        verifyDialogMargins(/* expectedHorizontalMargin= */ 25, /* expectedVerticalMargin= */ 40);
    }

    @Test
    public void addDialogView_FullscreenDialog() {
        // Setup window.
        setupWindow(
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                /* leftInset= */ 25,
                /* topInset= */ 40,
                /* rightInset= */ 20,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ true);

        // Add dialog view.
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.DIALOG_STYLES, DialogStyles.FULLSCREEN_DIALOG)
                        .build();
        addDialogView();

        // Verify that dialog margins are not set.
        verifyDialogMargins(ModalDialogView.NOT_SPECIFIED, ModalDialogView.NOT_SPECIFIED);
    }

    @Test
    @Config(qualifiers = "small")
    public void addDialogView_DialogWhenLargeDialog_Fullscreen() {
        // Setup window.
        setupWindow(
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                /* leftInset= */ 25,
                /* topInset= */ 40,
                /* rightInset= */ 20,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ true);

        // Add dialog view.
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.DIALOG_STYLES, DialogStyles.DIALOG_WHEN_LARGE)
                        .build();
        addDialogView();

        // Verify that dialog margins are not set.
        verifyDialogMargins(ModalDialogView.NOT_SPECIFIED, ModalDialogView.NOT_SPECIFIED);
    }

    @Test
    @Config(qualifiers = "xlarge")
    public void addDialogView_DialogWhenLargeDialog_Normal() {
        // Setup window.
        setupWindow(
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                /* leftInset= */ 25,
                /* topInset= */ 40,
                /* rightInset= */ 20,
                /* bottomInset= */ 32,
                /* isEdgeToEdgeActive= */ true);

        // Add dialog view.
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.DIALOG_STYLES, DialogStyles.DIALOG_WHEN_LARGE)
                        .build();
        addDialogView();

        // Verify dialog margins.
        verifyDialogMargins(25, 40);
    }

    @Test
    public void addDialogView_NullInsetObserver() {
        mAppModalPresenter.setInsetObserver(null);

        // Setup test window dimensions.
        mDisplayMetrics.heightPixels = WINDOW_HEIGHT;
        mDisplayMetrics.widthPixels = WINDOW_WIDTH;

        // Add dialog view.
        addDialogView();

        // Verify dialog margins are set to the fixed value of 16dp, when window insets are not
        // available.
        verifyDialogMargins(16, 16);
    }

    private void setupWindow(
            int windowWidth,
            int windowHeight,
            int leftInset,
            int topInset,
            int rightInset,
            int bottomInset,
            boolean isEdgeToEdgeActive) {
        // Setup test values.
        mDisplayMetrics.heightPixels = windowHeight;
        mDisplayMetrics.widthPixels = windowWidth;

        // Setup system bars insets.
        var windowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                systemBars(),
                                Insets.of(leftInset, topInset, rightInset, bottomInset))
                        .build();
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(windowInsets);

        // Set edge-to-edge state.
        mEdgeToEdgeStateSupplier.set(isEdgeToEdgeActive);
    }

    private void addDialogView() {
        mAppModalPresenter.addDialogView(mModel, null);
        // This method will be invoked when the dialog is added.
        mAppModalPresenter
                .getWindowInsetsListenerForTesting()
                .onApplyWindowInsets(mock(View.class), mock(WindowInsetsCompat.class));
    }

    private void verifyDialogMargins(int expectedHorizontalMargin, int expectedVerticalMargin) {
        var dialogView = mAppModalPresenter.getDialogViewForTesting();
        assertEquals(
                "Dialog horizontal margin is incorrect.",
                expectedHorizontalMargin,
                dialogView.getHorizontalMarginForTesting());
        assertEquals(
                "Dialog vertical margin is incorrect.",
                expectedVerticalMargin,
                dialogView.getVerticalMarginForTesting());
    }
}
