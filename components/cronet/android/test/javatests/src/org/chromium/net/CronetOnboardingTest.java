// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.List;

/**
 * A test file which is used to teach people how to build, test and submit changes to Cronet. Do not
 * move, change or delete unless you modify the onboarding instructions as well.
 */
@RunWith(AndroidJUnit4.class)
public class CronetOnboardingTest {
    // TODO(noogler): STEP 1 - add your name here
    private static final List<String> CRONET_CONTRIBUTORS =
            Arrays.asList("colibie", "danstahr", "edechamps", "sporeba", "stefanoduo");

    // TODO(noogler): STEP 2 - run the test suite and see it fail
    @Test
    @SmallTest
    public void testNumberOfCronetContributors() throws Exception {
        // TODO(noogler): STEP 3 - fix the test, rerun it and see it pass
        assertEquals(5, CRONET_CONTRIBUTORS.size());
    }
}
