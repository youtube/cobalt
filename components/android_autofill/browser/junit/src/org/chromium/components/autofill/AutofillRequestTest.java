// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.Rect;
import android.graphics.RectF;
import android.util.SparseArray;
import android.view.View;
import android.view.autofill.AutofillValue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;

import java.util.Arrays;

/**
 * Unit test for {@link AutofillRequest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures(
        {AndroidAutofillFeatures.ANDROID_AUTOFILL_VIEW_STRUCTURE_WITH_FORM_HIERARCHY_LAYER_NAME})
public class AutofillRequestTest {
    private static final String FORM_DOMAIN = "https://example.com";
    private static final String FORM_NAME = "sample-form-name";

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    private static FormFieldDataBuilder createTextFieldBuilder() {
        FormFieldDataBuilder builder = new FormFieldDataBuilder();
        builder.mValue = "current value";
        builder.mMaxLength = 15;
        return builder;
    }

    private static FormFieldDataBuilder createDatalistFieldBuilder() {
        FormFieldDataBuilder builder = new FormFieldDataBuilder();
        builder.mValue = "current value";
        builder.mDatalistValues = new String[] {"entry1", "entry2"};
        return builder;
    }

    private static FormFieldDataBuilder createCheckboxFieldBuilder() {
        FormFieldDataBuilder builder = new FormFieldDataBuilder();
        builder.mIsCheckField = true;
        builder.mIsChecked = true;
        return builder;
    }

    private static FormFieldDataBuilder createListFieldBuilder() {
        FormFieldDataBuilder builder = new FormFieldDataBuilder();
        builder.mOptionValues = new String[] {"value1", "value2"};
        builder.mOptionContents = new String[] {"content1", "content2"};
        builder.mValue = "value2";
        return builder;
    }

    private static AutofillRequest createRequest(FormFieldData... fields) {
        FormData formData = new FormData(FORM_NAME, FORM_DOMAIN, Arrays.asList(fields));

        return new AutofillRequest(formData, null, /*hasServerPrediction=*/false);
    }

    private static AutofillRequest createSampleRequest() {
        FormFieldDataBuilder fieldBuilder1 = new FormFieldDataBuilder();
        FormFieldDataBuilder fieldBuilder2 = new FormFieldDataBuilder();
        return createRequest(fieldBuilder1.build(), fieldBuilder2.build());
    }

    private static TestViewStructure fillStructureForRequest(AutofillRequest request) {
        TestViewStructure structure = new TestViewStructure();
        request.fillViewStructure(structure);
        return structure;
    }

    @Test
    // Tests that the information that deals with form level data (host, form name) is set
    // correctly.
    public void testFormInformationIsSet() {
        TestViewStructure structure = fillStructureForRequest(createSampleRequest());

        assertEquals(FORM_DOMAIN, structure.getWebDomain());
        TestViewStructure.TestHtmlInfo htmlInfoForm = structure.getHtmlInfo();
        assertEquals("form", htmlInfoForm.getTag());
        assertEquals(FORM_NAME, htmlInfoForm.getAttribute("name"));
    }

    @Test
    // Tests that forms with multiple children result in ViewStructures with multiple leaf nodes.
    public void testMultipleChildrenAreAddedToViewStructure() {
        AutofillRequest request = createSampleRequest();
        assertEquals(2, request.getFieldCount());
        TestViewStructure structure = fillStructureForRequest(request);
        assertEquals(2, structure.getChildCount());
    }

    @Features.
    EnableFeatures({AndroidAutofillFeatures
                            .ANDROID_AUTOFILL_VIEW_STRUCTURE_WITH_FORM_HIERARCHY_LAYER_NAME})
    @Test
    // Tests that there is an additional hierarchy level if the feature
    // ANDROID_AUTOFILL_VIEW_STRUCTURE_WITH_FORM_HIERARCHY_LAYER is enabled, i.e. the form
    // is a child node of the ViewStructure filled by the AutofillRequest.
    public void
    testFormInformationIsInSeparateNode() {
        TestViewStructure structure = fillStructureForRequest(createSampleRequest());

        assertEquals(1, structure.getChildCount());
        TestViewStructure childForm = structure.getChild(0);

        assertEquals(FORM_DOMAIN, childForm.getWebDomain());
        TestViewStructure.TestHtmlInfo htmlInfoForm = childForm.getHtmlInfo();
        assertEquals("form", htmlInfoForm.getTag());
        assertEquals(FORM_NAME, htmlInfoForm.getAttribute("name"));

        assertEquals(2, childForm.getChildCount());
    }

    @Test
    // Tests that the form field level data (bounds, visibility, labels, etc.) apart from the
    // control type is set correctly
    public void testFormFieldInformationIsSet() {
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        fieldBuilder.mAutocompleteAttr = "username";
        fieldBuilder.mPlaceholder = "placeholder";
        fieldBuilder.mBoundsInContainerViewCoordinates = new RectF(0, 0, 40, 60);
        fieldBuilder.mName = "username-field";
        fieldBuilder.mType = "username";
        fieldBuilder.mLabel = "Username";
        fieldBuilder.mHeuristicType = "PASSWORD";
        fieldBuilder.mId = "username-id";
        fieldBuilder.mServerType = "USERNAME";
        fieldBuilder.mComputedType = "USERNAME";

        TestViewStructure structure = fillStructureForRequest(createRequest(fieldBuilder.build()));

        assertEquals(1, structure.getChildCount());
        TestViewStructure child = structure.getChild(0);

        assertArrayEquals(new String[] {"username"}, child.getAutofillHints());
        assertEquals("placeholder", child.getHint());
        assertEquals(new Rect(0, 0, 40, 60), child.getDimensRect());
        TestViewStructure.TestHtmlInfo htmlInfoField = child.getHtmlInfo();
        assertEquals("input", htmlInfoField.getTag());
        assertEquals("username-field", htmlInfoField.getAttribute("name"));
        assertEquals("username", htmlInfoField.getAttribute("type"));
        assertEquals("Username", htmlInfoField.getAttribute("label"));
        assertEquals("PASSWORD", htmlInfoField.getAttribute("ua-autofill-hints"));
        assertEquals("username-id", htmlInfoField.getAttribute("id"));
        assertEquals("USERNAME", htmlInfoField.getAttribute("crowdsourcing-autofill-hints"));
        assertEquals("USERNAME", htmlInfoField.getAttribute("computed-autofill-hints"));
    }

    @Test
    // Tests that the control-type specific data of a text field is set correctly.
    public void testControlTypeSpecificInformationIsSetForTextFields() {
        FormFieldDataBuilder fieldBuilder = createTextFieldBuilder();
        TestViewStructure structure = fillStructureForRequest(createRequest(fieldBuilder.build()));

        assertEquals(1, structure.getChildCount());
        TestViewStructure child = structure.getChild(0);

        // The default type is a text field.
        assertEquals(View.AUTOFILL_TYPE_TEXT, child.getAutofillType());
        assertEquals(AutofillValue.forText(fieldBuilder.mValue), child.getAutofillValue());
        assertEquals(Integer.toString(fieldBuilder.mMaxLength),
                child.getHtmlInfo().getAttribute("maxlength"));
    }

    @Test
    // Tests that the control-type specific data of a data list field is set correctly.
    public void testControlTypeSpecificInformationIsSetForDatalistFields() {
        FormFieldDataBuilder fieldBuilder = createDatalistFieldBuilder();
        TestViewStructure structure = fillStructureForRequest(createRequest(fieldBuilder.build()));

        assertEquals(1, structure.getChildCount());
        TestViewStructure child = structure.getChild(0);

        // Datalists also have text type.
        assertEquals(View.AUTOFILL_TYPE_TEXT, child.getAutofillType());
        assertEquals(AutofillValue.forText(fieldBuilder.mValue), child.getAutofillValue());
        assertEquals(fieldBuilder.mDatalistValues.length, child.getAutofillOptions().length);
        assertEquals(fieldBuilder.mDatalistValues[0], child.getAutofillOptions()[0]);
        assertEquals(fieldBuilder.mDatalistValues[1], child.getAutofillOptions()[1]);
    }

    @Test
    // Tests that the control-type specific data of a checkbox field is set correctly.
    public void testControlTypeSpecificInformationIsSetForCheckboxFields() {
        FormFieldDataBuilder fieldBuilder = createCheckboxFieldBuilder();
        TestViewStructure structure = fillStructureForRequest(createRequest(fieldBuilder.build()));

        assertEquals(1, structure.getChildCount());
        TestViewStructure child = structure.getChild(0);

        assertEquals(View.AUTOFILL_TYPE_TOGGLE, child.getAutofillType());
        assertEquals(AutofillValue.forToggle(fieldBuilder.mIsChecked), child.getAutofillValue());
    }

    @Test
    // Tests that the control-type specific data of a list field is set correctly.
    public void testControlTypeSpecificInformationIsSetForListFields() {
        FormFieldDataBuilder fieldBuilder = createListFieldBuilder();
        TestViewStructure structure = fillStructureForRequest(createRequest(fieldBuilder.build()));

        assertEquals(1, structure.getChildCount());
        TestViewStructure child = structure.getChild(0);

        assertEquals(View.AUTOFILL_TYPE_LIST, child.getAutofillType());
        // The field's value matches the option entry with index 1.
        assertEquals(AutofillValue.forList(1), child.getAutofillValue());

        fieldBuilder.mValue = "value3";
        structure = fillStructureForRequest(createRequest(fieldBuilder.build()));
        assertEquals(1, structure.getChildCount());
        child = structure.getChild(0);
        assertEquals(View.AUTOFILL_TYPE_LIST, child.getAutofillType());
        // If there is no matching entry, the AutofillValue is not set.
        assertEquals(null, child.getAutofillValue());
    }

    @Test
    // Tests that autofill() updates the underlying FormFieldData for a text field.
    public void testAutofillUpdatesTextField() {
        FormFieldDataBuilder fieldBuilder = createTextFieldBuilder();
        AutofillRequest request = createRequest(fieldBuilder.build());
        TestViewStructure structure = fillStructureForRequest(request);
        assertEquals(1, structure.getChildCount());
        TestViewStructure child = structure.getChild(0);

        SparseArray<AutofillValue> valuesToFill = new SparseArray<AutofillValue>();
        valuesToFill.append(child.getId(), AutofillValue.forText("new value"));

        // The autofill requests succeeds.
        assertTrue(request.autofill(valuesToFill));
        // The underlying FormFieldData object is updated.
        assertEquals("new value", request.getField((short) 0).getValue());
    }

    @Test
    // Tests that autofill() updates the underlying FormFieldData for a datalist field.
    public void testAutofillUpdatesDatalistField() {
        FormFieldDataBuilder fieldBuilder = createDatalistFieldBuilder();
        AutofillRequest request = createRequest(fieldBuilder.build());
        TestViewStructure structure = fillStructureForRequest(request);
        assertEquals(1, structure.getChildCount());
        TestViewStructure child = structure.getChild(0);

        SparseArray<AutofillValue> valuesToFill = new SparseArray<AutofillValue>();
        valuesToFill.append(child.getId(), AutofillValue.forText("entry2"));

        // The autofill requests succeeds.
        assertTrue(request.autofill(valuesToFill));
        // The underlying FormFieldData object is updated.
        assertEquals("entry2", request.getField((short) 0).getValue());
    }

    @Test
    // Tests that autofill() updates the underlying FormFieldData for a checkbox field.
    public void testAutofillUpdatesCheckboxField() {
        FormFieldDataBuilder fieldBuilder = createCheckboxFieldBuilder();
        AutofillRequest request = createRequest(fieldBuilder.build());
        TestViewStructure structure = fillStructureForRequest(request);
        assertEquals(1, structure.getChildCount());
        TestViewStructure child = structure.getChild(0);

        SparseArray<AutofillValue> valuesToFill = new SparseArray<AutofillValue>();
        valuesToFill.append(child.getId(), AutofillValue.forToggle(false));

        // The autofill requests succeeds.
        assertTrue(request.autofill(valuesToFill));
        // The underlying FormFieldData object is updated.
        assertEquals(false, request.getField((short) 0).isChecked());
    }

    @Test
    // Tests that autofill() updates the underlying FormFieldData for a list field.
    public void testAutofillUpdatesListField() {
        FormFieldDataBuilder fieldBuilder = createListFieldBuilder();
        AutofillRequest request = createRequest(fieldBuilder.build());
        TestViewStructure structure = fillStructureForRequest(request);
        assertEquals(1, structure.getChildCount());
        TestViewStructure child = structure.getChild(0);

        SparseArray<AutofillValue> valuesToFill = new SparseArray<AutofillValue>();
        valuesToFill.append(child.getId(), AutofillValue.forList(0));

        // The autofill requests succeeds.
        assertTrue(request.autofill(valuesToFill));
        // The underlying FormFieldData object is updated.
        assertEquals("value1", request.getField((short) 0).getValue());

        // Invalid list indices are ignored.
        valuesToFill.put(child.getId(), AutofillValue.forList(3));
        assertTrue(request.autofill(valuesToFill));
        assertEquals("value1", request.getField((short) 0).getValue());
        valuesToFill.put(child.getId(), AutofillValue.forList(-1));
        assertTrue(request.autofill(valuesToFill));
        assertEquals("value1", request.getField((short) 0).getValue());
    }

    @Test
    // Tests that autofill() returns false if the session id does not match that of the
    // AutofillRequest.
    public void testAutofillDoesNotFillDifferentForm() {
        AutofillRequest request1 = createRequest(createTextFieldBuilder().build());
        TestViewStructure structure1 = fillStructureForRequest(request1);
        assertEquals(1, structure1.getChildCount());

        // Create a separate request to simulate a different session id.
        AutofillRequest request2 = createRequest(createTextFieldBuilder().build());

        // Use the id from the old request for the autofill call.
        SparseArray<AutofillValue> valuesToFill = new SparseArray<AutofillValue>();
        valuesToFill.append(structure1.getChild(0).getId(), AutofillValue.forText("new text"));

        assertFalse(request2.autofill(valuesToFill));
    }

    @Test
    // Tests that autofill() returns false if the session id does not match that of the
    // AutofillRequest.
    public void testAutofillDoesNotFillUnknownField() {
        AutofillRequest request = createRequest(createTextFieldBuilder().build());
        TestViewStructure structure = fillStructureForRequest(request);
        assertEquals(1, structure.getChildCount());

        // Increment the id by 1 to generate an invalid id for the autofill call.
        SparseArray<AutofillValue> valuesToFill = new SparseArray<AutofillValue>();
        valuesToFill.append(structure.getChild(0).getId() + 1, AutofillValue.forText("new text"));

        assertFalse(request.autofill(valuesToFill));
    }
}
