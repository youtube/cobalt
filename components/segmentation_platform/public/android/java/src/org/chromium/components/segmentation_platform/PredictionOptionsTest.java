// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for PredictionOptions. */
@RunWith(BaseRobolectricTestRunner.class)
public class PredictionOptionsTest {

    @Mock PredictionOptions.Natives mNativeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PredictionOptionsJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    public void testPredictionOptionsForOndemand() {
        PredictionOptions predictionOptions = PredictionOptions.forOndemand(true);
        predictionOptions.fillNativePredictionOptions(0x12345678);
        Mockito.verify(mNativeMock).fillNative(0x12345678, true, false, true);
    }

    @Test
    public void testPredictionOptionsForCached() {
        PredictionOptions predictionOptions = PredictionOptions.forCached(true);
        predictionOptions.fillNativePredictionOptions(0x12345678);
        Mockito.verify(mNativeMock).fillNative(0x12345678, false, true, true);
    }
}
