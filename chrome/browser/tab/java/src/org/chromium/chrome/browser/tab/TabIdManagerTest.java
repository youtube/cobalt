// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/** Tests for the TabIdManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabIdManagerTest {
    @Mock
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        TabIdManager.resetInstanceForTesting();
    }

    @After
    public void tearDown() {
        TabIdManager.resetInstanceForTesting();
    }

    /** Tests that IDs are stored and generated properly. */
    @Test
    public void testBasic() {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.writeInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, 11684);

        TabIdManager manager = TabIdManager.getInstance(mContext);
        Assert.assertEquals(
                "Wrong Tab ID was generated", 11684, manager.generateValidId(Tab.INVALID_TAB_ID));

        Assert.assertEquals("Wrong next Tab ID", 11685,
                prefs.readInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, -1));
    }

    /** Tests that the max ID is updated properly. */
    @Test
    public void testIncrementIdCounterTo() {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.writeInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, 11684);

        TabIdManager manager = TabIdManager.getInstance(mContext);
        Assert.assertEquals(
                "Wrong Tab ID was generated", 11684, manager.generateValidId(Tab.INVALID_TAB_ID));

        Assert.assertEquals("Wrong next Tab ID", 11685,
                prefs.readInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, -1));

        manager.incrementIdCounterTo(100);
        Assert.assertEquals("Didn't stay the same", 11685,
                prefs.readInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, -1));

        manager.incrementIdCounterTo(1000000);
        Assert.assertEquals("Didn't increase", 1000000,
                prefs.readInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, -1));
    }
}
