// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link PrepopulatedAndRecentlyVisitedTemplateURLs}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PrepopulatedAndRecentlyVisitedTemplateURLsUnitTest {
    @Test
    public void testConstructorAndGetters() {
        TemplateUrl t1 = mock(TemplateUrl.class);
        TemplateUrl t2 = mock(TemplateUrl.class);
        TemplateUrl t3 = mock(TemplateUrl.class);

        PrepopulatedAndRecentlyVisitedTemplateURLs container =
                new PrepopulatedAndRecentlyVisitedTemplateURLs(List.of(t1, t2), List.of(t3));

        assertEquals(2, container.getPrepopulatedUrls().size());
        assertEquals(t1, container.getPrepopulatedUrls().get(0));
        assertEquals(t2, container.getPrepopulatedUrls().get(1));

        assertEquals(1, container.getRecentlyVisitedUrls().size());
        assertEquals(t3, container.getRecentlyVisitedUrls().get(0));
    }
}
