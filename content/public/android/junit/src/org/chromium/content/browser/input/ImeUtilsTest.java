// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;

import android.view.inputmethod.EditorInfo;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.HtmlMetadata;
import org.chromium.ui.base.ime.TextInputType;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.util.Base64;

@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ImeUtilsTest {
    @Test
    @SmallTest
    public void testGetDataUrlFromValidInputStream() throws Throwable {
        String base64EncodedData = "UHVDWERNMm4=";
        byte[] imageData = Base64.getDecoder().decode(base64EncodedData);
        InputStream inputStream = new ByteArrayInputStream(imageData);

        PostTask.postTask(
                TaskTraits.USER_BLOCKING_MAY_BLOCK,
                () ->
                        assertEquals(
                                "data:image/png;base64," + base64EncodedData,
                                ImeUtils.getDataUrlFromContentUri(inputStream, "image/png")));
    }

    @Test
    @SmallTest
    public void testGetDataUrlFromNullInputStream() throws Throwable {
        PostTask.postTask(
                TaskTraits.USER_BLOCKING_MAY_BLOCK,
                () -> assertEquals("", ImeUtils.getDataUrlFromContentUri(null, "image/png")));
    }

    @Test
    @SmallTest
    public void testComputeEditorInfo_Metadata() {
        EditorInfo outAttrs = new EditorInfo();
        ImeUtils.computeEditorInfo(
                TextInputType.TEXT,
                /* inputFlags= */ 0,
                /* inputMode= */ 0,
                /* inputAction= */ 0,
                /* initialSelStart= */ 0,
                /* initialSelEnd= */ 0,
                /* lastText= */ "",
                HtmlMetadata.create(
                        /* label= */ "test_label",
                        /* fieldName= */ "test_name",
                        /* placeholder= */ "test_placeholder"),
                outAttrs);

        assertEquals("test_label", outAttrs.label);
        assertEquals("test_name", outAttrs.fieldName);
        assertEquals("test_placeholder", outAttrs.hintText);
    }

    @Test
    @SmallTest
    public void testComputeEditorInfo_MetadataFieldName() {
        EditorInfo outAttrs = new EditorInfo();
        ImeUtils.computeEditorInfo(
                TextInputType.TEXT,
                /* inputFlags= */ 0,
                /* inputMode= */ 0,
                /* inputAction= */ 0,
                /* initialSelStart= */ 0,
                /* initialSelEnd= */ 0,
                /* lastText= */ "",
                HtmlMetadata.create(
                        /* label= */ null, /* fieldName= */ "test_id", /* placeholder= */ null),
                outAttrs);

        assertEquals(null, outAttrs.label);
        assertEquals("test_id", outAttrs.fieldName);
        assertEquals(null, outAttrs.hintText);
    }

    @Test
    @SmallTest
    public void testComputeEditorInfo_MetadataEmpty() {
        EditorInfo outAttrs = new EditorInfo();
        ImeUtils.computeEditorInfo(
                TextInputType.TEXT,
                /* inputFlags= */ 0,
                /* inputMode= */ 0,
                /* inputAction= */ 0,
                /* initialSelStart= */ 0,
                /* initialSelEnd= */ 0,
                /* lastText= */ "",
                HtmlMetadata.EMPTY,
                outAttrs);

        assertEquals(null, outAttrs.label);
        assertEquals(null, outAttrs.fieldName);
        assertEquals(null, outAttrs.hintText);
    }
}
