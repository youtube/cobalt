// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Installs Autofill management functions on the __gCrWeb object.
 *
 * It scans the DOM, extracting and storing forms and returns a JSON string
 * representing an array of objects, each of which represents an Autofill form
 * with information about a form to be filled and/or submitted and it can be
 * translated to struct FormData
 * (chromium/src/components/autofill/core/common/form_data.h) for further
 * processing.
 */

/**
 * The autofill data for a form.
 * @typedef {{
 *   formName: string,
 *   formRendererID: number,
 *   fields: !Object<string, !Object<string, string>>,
 * }}
 */
// eslint-disable-next-line no-var
var FormData;

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.autofill = {};

// Store autofill namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['autofill'] = __gCrWeb.autofill;

/**
 * The delay between filling two fields
 *
 * Page need time to propagate the events after setting one field. Add a delay
 * between filling two fields. In milliseconds.
 *
 * @type {number}
 */
__gCrWeb.autofill.delayBetweenFieldFillingMs = 50;

/**
 * The last element that was autofilled.
 *
 * @type {Element}
 */
__gCrWeb.autofill.lastAutoFilledElement = null;

/**
 * Whether CSS for autofilled elements has been injected into the page.
 *
 * @type {boolean}
 */
__gCrWeb.autofill.styleInjected = false;

/**
 * Sets the delay between fields when autofilling forms.
 *
 * @param {number} delay The new delay in milliseconds.
 */
__gCrWeb.autofill.setDelay = function(delay) {
  __gCrWeb.autofill.delayBetweenFieldFillingMs = delay;
};

/**
 * Determines whether the form is interesting enough to send to the browser for
 * further operations.
 *
 * Unlike the C++ version, this version takes a required field count param,
 * instead of using a hard coded value.
 *
 * It is based on the logic in
 *     bool IsFormInteresting(const FormData& form,
 *                            size_t num_editable_elements);
 * in chromium/src/components/autofill/content/renderer/form_cache.cc
 *
 * @param {AutofillFormData} form Form to examine.
 * @param {number} numEditableElements number of editable elements.
 * @param {number} numFieldsRequired number of fields required.
 * @return {boolean} Whether the form is sufficiently interesting.
 */
function isFormInteresting_(form, numEditableElements, numFieldsRequired) {
  if (form.fields.length === 0) {
    return false;
  }

  // If the form has at least one field with an autocomplete attribute, it is a
  // candidate for autofill.
  for (let i = 0; i < form.fields.length; ++i) {
    if (form.fields[i]['autocomplete_attribute'] != null &&
        form.fields[i]['autocomplete_attribute'].length > 0) {
      return true;
    }
  }

  // If there are no autocomplete attributes, the form needs to have at least
  // the required number of editable fields for the prediction routines to be a
  // candidate for autofill.
  return numEditableElements >= numFieldsRequired;
}

/**
 * Scans |control_elements| and returns the number of editable elements.
 *
 * Unlike the C++ version, this version does not take the
 * log_deprecation_messages parameter, and it does not save any state since
 * there is no caching.
 *
 * It is based on the logic in:
 *     size_t FormCache::ScanFormControlElements(
 *         const std::vector<WebFormControlElement>& control_elements,
 *         bool log_deprecation_messages);
 * in chromium/src/components/autofill/content/renderer/form_cache.cc.
 *
 * @param {Array<FormControlElement>} controlElements The elements to scan.
 * @return {number} The number of editable elements.
 */
function scanFormControlElements_(controlElements) {
  let numEditableElements = 0;
  for (let elementIndex = 0; elementIndex < controlElements.length;
       ++elementIndex) {
    const element = controlElements[elementIndex];
    if (!__gCrWeb.fill.isCheckableElement(element)) {
      ++numEditableElements;
    }
  }
  return numEditableElements;
}

/**
 * Scans DOM and returns a JSON string representation of forms and form
 * extraction results. This is just a wrapper around extractNewForms() to JSON
 * encode the forms, for convenience.
 *
 * @param {number} requiredFields The minimum number of fields forms must have
 *     to be extracted.
 * @param {bool} restrictUnownedFieldsToFormlessCheckout whether forms made of
 *     unowned fields (i.e., not within a <form> tag) should be restricted to
 *     those that appear to be in a checkout flow.
 * @return {string} A JSON encoded an array of the forms data.
 */
__gCrWeb.autofill['extractForms'] = function(
    requiredFields, restrictUnownedFieldsToFormlessCheckout) {
  const forms = __gCrWeb.autofill.extractNewForms(
      requiredFields, restrictUnownedFieldsToFormlessCheckout);
  return __gCrWeb.stringify(forms);
};

/**
 * Fills data into the active form field.
 *
 * @param {AutofillFormFieldData} data The data to fill in.
 * @return {boolean} Whether the field was filled successfully.
 */
__gCrWeb.autofill['fillActiveFormField'] = function(data) {
  const activeElement = document.activeElement;
  const fieldID = data['unique_renderer_id'];
  if (typeof fieldID === 'undefined' ||
      fieldID.toString() !== __gCrWeb.fill.getUniqueID(activeElement)) {
    return false;
  }
  __gCrWeb.autofill.lastAutoFilledElement = activeElement;
  return __gCrWeb.autofill.fillFormField(data, activeElement);
};

// Remove Autofill styling when control element is edited by the user.
function controlElementInputListener_(evt) {
  if (evt.isTrusted) {
    evt.target.removeAttribute('chrome-autofilled');
    evt.target.isAutofilled = false;
    evt.target.removeEventListener('input', controlElementInputListener_);
  }
}

/**
 * Fills a number of fields in the same named form for full-form Autofill.
 * Applies Autofill CSS (i.e. yellow background) to filled elements.
 * Only empty fields will be filled, except that field named
 * |forceFillFieldName| will always be filled even if non-empty.
 *
 * @param {!FormData} data Autofill data to fill in.
 * @param {number} forceFillFieldID Identified field will always be
 *     filled even if non-empty. May be __gCrWeb.fill.RENDERER_ID_NOT_SET.
 * @return {string} JSON encoded list of renderer IDs of filled elements.
 */
__gCrWeb.autofill['fillForm'] = function(data, forceFillFieldID) {
  // Inject CSS to style the autofilled elements with a yellow background.
  if (!__gCrWeb.autofill.styleInjected) {
    const style = document.createElement('style');
    style.textContent = '[chrome-autofilled] {' +
        'background-color:#E8F0FE !important;' +
        'background-image:none !important;' +
        'color:#000000 !important;' +
        '}';
    document.head.appendChild(style);
    __gCrWeb.autofill.styleInjected = true;
  }
  const filledElements = {};

  const form =
      __gCrWeb.form.getFormElementFromUniqueFormId(data.formRendererID);
  const controlElements = form ?
      __gCrWeb.form.getFormControlElements(form) :
      __gCrWeb.fill.getUnownedAutofillableFormFieldElements(
          document.all,
          /*fieldsets=*/[]);

  for (let i = 0, delay = 0; i < controlElements.length; ++i) {
    const element = controlElements[i];
    if (!__gCrWeb.fill.isAutofillableElement(element)) {
      continue;
    }

    // TODO(crbug.com/836013): Investigate autofilling checkable elements.
    if (__gCrWeb.fill.isCheckableElement(element)) {
      continue;
    }

    // Skip fields for which autofill data is missing.
    const fieldRendererID = __gCrWeb.fill.getUniqueID(element);
    const fieldData = data.fields[fieldRendererID];

    if (!fieldData) {
      continue;
    }

    // Skip non-empty fields unless:
    // a) The element's identifier matches |forceFillFieldIdentifier|; or
    // b) The element is a 'select-one' element. 'select-one' elements are
    //    always autofilled; see AutofillManager::FillOrPreviewDataModelForm().
    // c) The "value" or "placeholder" attributes match the value, if any; or
    // d) The value has not been set by the user.
    const shouldBeForceFilled = fieldRendererID === forceFillFieldID.toString();
    if (element.value && __gCrWeb.form.fieldWasEditedByUser(element) &&
        !__gCrWeb.autofill.sanitizedFieldIsEmpty(element.value) &&
        !shouldBeForceFilled && !__gCrWeb.fill.isSelectElement(element) &&
        !((element.hasAttribute('value') &&
           element.getAttribute('value') === element.value) ||
          (element.hasAttribute('placeholder') &&
           element.getAttribute('placeholder').toLowerCase() ==
               element.value.toLowerCase()))) {
      continue;
    }

    (function(_element, _value, _section, _delay) {
      window.setTimeout(function() {
        __gCrWeb.fill.setInputElementValue(_value, _element, function() {
          _element.setAttribute('chrome-autofilled', '');
          _element.isAutofilled = true;
          _element.autofillSection = _section;
          _element.addEventListener('input', controlElementInputListener_);
        });
      }, _delay);
    })(element, fieldData.value, fieldData.section, delay);
    delay += __gCrWeb.autofill.delayBetweenFieldFillingMs;
    filledElements[__gCrWeb.fill.getUniqueID(element)] = fieldData.value;
  }

  if (form) {
    // Remove Autofill styling when form receives 'reset' event.
    // Individual control elements may be left with 'input' event listeners but
    // they are harmless.
    const formResetListener = function(evt) {
      const controlElements = __gCrWeb.form.getFormControlElements(evt.target);
      for (let i = 0; i < controlElements.length; ++i) {
        controlElements[i].removeAttribute('chrome-autofilled');
        controlElements[i].isAutofilled = false;
      }
      evt.target.removeEventListener('reset', formResetListener);
    };
    form.addEventListener('reset', formResetListener);
  }

  return __gCrWeb.stringify(filledElements);
};

/**
 * Clear autofilled fields of the specified form section. Fields that are not
 * currently autofilled or do not belong to the same section as that of the
 * field with |fieldIdentifier| are not modified. If the field identified by
 * |fieldIdentifier| cannot be found all autofilled form fields get cleared.
 * Field contents are cleared, and Autofill flag and styling are removed.
 * 'change' events are sent for fields whose contents changed.
 * Based on FormCache::ClearSectionWithElement().
 *
 * @param {string} formUniqueID Unique ID of the form element.
 * @param {string} fieldUniqueID Unique ID of the field initiating the
 *     clear action.
 * @return {string} JSON encoded list of renderer IDs of cleared elements.
 */
__gCrWeb.autofill['clearAutofilledFields'] = function(
    formUniqueID, fieldUniqueID) {
  const clearedElements = [];

  const form = __gCrWeb.form.getFormElementFromUniqueFormId(formUniqueID);

  const controlElements = form ?
      __gCrWeb.form.getFormControlElements(form) :
      __gCrWeb.fill.getUnownedAutofillableFormFieldElements(
          document.all,
          /*fieldsets=*/[]);

  let formField = null;
  for (let i = 0; i < controlElements.length; ++i) {
    if (__gCrWeb.fill.getUniqueID(controlElements[i]) ==
        fieldUniqueID.toString()) {
      formField = controlElements[i];
      break;
    }
  }

  for (let i = 0, delay = 0; i < controlElements.length; ++i) {
    const element = controlElements[i];
    if (!element.isAutofilled || element.disabled) {
      continue;
    }

    if (formField && formField.autofillSection !== element.autofillSection) {
      continue;
    }

    let value = null;
    if (__gCrWeb.fill.isTextInput(element) ||
        __gCrWeb.fill.isTextAreaElement(element)) {
      value = '';
    } else if (__gCrWeb.fill.isSelectElement(element)) {
      // Reset to the first index.
      // TODO(bondd): Store initial values and reset to the correct one here.
      value = element.options[0].value;
    } else if (__gCrWeb.fill.isCheckableElement(element)) {
      // TODO(crbug.com/836013): Investigate autofilling checkable elements.
    }
    if (value !== null) {
      (function(_element, _value, _delay) {
        window.setTimeout(function() {
          __gCrWeb.fill.setInputElementValue(
              _value, _element, function(changed) {
                _element.removeAttribute('chrome-autofilled');
                _element.isAutofilled = false;
                _element.removeEventListener(
                    'input', controlElementInputListener_);
              });
        }, _delay);
      })(element, value, delay);
      delay += __gCrWeb.autofill.delayBetweenFieldFillingMs;
      clearedElements.push(__gCrWeb.fill.getUniqueID(element));
    }
  }
  return __gCrWeb.stringify(clearedElements);
};

/**
 * Scans the DOM in |frame| extracting and storing forms. Fills |forms| with
 * extracted forms.
 *
 * This method is based on the logic in method:
 *
 *     std::vector<FormData> ExtractNewForms();
 *
 * in chromium/src/components/autofill/content/renderer/form_cache.cc.
 *
 * The difference is in this implementation, the cache is not considered.
 * Initial values of select and checkable elements are not recorded at the
 * moment.
 *
 * This version still takes the minimumRequiredFields parameters. Whereas the
 * C++ version does not.
 *
 * @param {number} minimumRequiredFields The minimum number of fields a form
 *     should contain for autofill.
 * @param {bool} restrictUnownedFieldsToFormlessCheckout whether forms made of
 *     unowned fields (i.e., not within a <form> tag) should be restricted to
 *     those that appear to be in a checkout flow.
 * @return {Array<AutofillFormData>} The extracted forms.
 */
__gCrWeb.autofill.extractNewForms = function(
    minimumRequiredFields, restrictUnownedFieldsToFormlessCheckout) {
  const forms = [];
  // Protect against custom implementation of Array.toJSON in host pages.
  (function() {
    forms.toJSON = null;
  })();

  /** @type {HTMLCollection} */
  const webForms = document.forms;

  const extractMask =
      __gCrWeb.fill.EXTRACT_MASK_VALUE | __gCrWeb.fill.EXTRACT_MASK_OPTIONS;
  let numFieldsSeen = 0;
  for (let formIndex = 0; formIndex < webForms.length; ++formIndex) {
    /** @type {HTMLFormElement} */
    const formElement = webForms[formIndex];
    const controlElements =
        __gCrWeb.autofill.extractAutofillableElementsInForm(formElement);
    const numEditableElements = scanFormControlElements_(controlElements);

    if (numEditableElements === 0) {
      continue;
    }

    const form = new __gCrWeb['common'].JSONSafeObject();
    if (!__gCrWeb.fill.webFormElementToFormData(
            window, formElement, null, extractMask, form, null /* field */)) {
      continue;
    }

    numFieldsSeen += form['fields'].length;
    if (numFieldsSeen > __gCrWeb.fill.MAX_PARSEABLE_FIELDS) {
      break;
    }

    if (isFormInteresting_(form, numEditableElements, minimumRequiredFields)) {
      forms.push(form);
    }
  }

  // Look for more parseable fields outside of forms.
  const fieldsets = [];
  const unownedControlElements =
      __gCrWeb.fill.getUnownedAutofillableFormFieldElements(
          document.all, fieldsets);
  const numEditableUnownedElements =
      scanFormControlElements_(unownedControlElements);
  if (numEditableUnownedElements > 0) {
    const unownedForm = new __gCrWeb['common'].JSONSafeObject();
    const hasUnownedForm =
        __gCrWeb.fill.unownedFormElementsAndFieldSetsToFormData(
            window, fieldsets, unownedControlElements, extractMask,
            restrictUnownedFieldsToFormlessCheckout, unownedForm);
    if (hasUnownedForm) {
      numFieldsSeen += unownedForm['fields'].length;
      if (numFieldsSeen <= __gCrWeb.fill.MAX_PARSEABLE_FIELDS) {
        const interesting = isFormInteresting_(
            unownedForm, numEditableUnownedElements, minimumRequiredFields);
        if (interesting) {
          forms.push(unownedForm);
        }
      }
    }
  }
  return forms;
};

/**
 * Sets the |field|'s value to the value in |data|.
 * Also sets the "autofilled" attribute.
 *
 * It is based on the logic in
 *     void FillFormField(const FormFieldData& data,
 *                        bool is_initiating_node,
 *                        blink::WebFormControlElement* field)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * Different from FillFormField(), is_initiating_node is not considered in
 * this implementation.
 *
 * @param {AutofillFormFieldData} data Data that will be filled into field.
 * @param {FormControlElement} field The element to which data will be filled.
 * @return {boolean} Whether the field was filled successfully.
 */
__gCrWeb.autofill.fillFormField = function(data, field) {
  // Nothing to fill.
  if (!data['value'] || data['value'].length === 0) {
    return false;
  }

  let filled = false;
  if (__gCrWeb.fill.isTextInput(field) ||
      __gCrWeb.fill.isTextAreaElement(field)) {
    let sanitizedValue = data['value'];

    if (__gCrWeb.fill.isTextInput(field)) {
      // If the 'max_length' attribute contains a negative value, the default
      // maxlength value is used.
      let maxLength = data['max_length'];
      if (maxLength < 0) {
        maxLength = __gCrWeb.fill.MAX_DATA_LENGTH;
      }
      sanitizedValue = data['value'].substr(0, maxLength);
    }

    filled = __gCrWeb.fill.setInputElementValue(sanitizedValue, field);
    field.isAutofilled = true;
  } else if (__gCrWeb.fill.isSelectElement(field)) {
    filled = __gCrWeb.fill.setInputElementValue(data['value'], field);
  } else if (__gCrWeb.fill.isCheckableElement(field)) {
    filled = __gCrWeb.fill.setInputElementValue(data['is_checked'], field);
  }
  return filled;
};

/**
 * Returns the auto-fillable form control elements in |formElement|.
 *
 * It is based on the logic in:
 *     std::vector<blink::WebFormControlElement>
 *     ExtractAutofillableElementsFromSet(
 *         const WebVector<WebFormControlElement>& control_elements);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {Array<FormControlElement>} controlElements Set of control elements.
 * @return {Array<FormControlElement>} The array of autofillable elements.
 */
__gCrWeb.autofill.extractAutofillableElementsFromSet = function(
    controlElements) {
  const autofillableElements = [];
  for (let i = 0; i < controlElements.length; ++i) {
    const element = controlElements[i];
    if (!__gCrWeb.fill.isAutofillableElement(element)) {
      continue;
    }
    autofillableElements.push(element);
  }
  return autofillableElements;
};

/**
 * Returns all the auto-fillable form control elements in |formElement|.
 *
 * It is based on the logic in
 *     void ExtractAutofillableElementsInForm(
 *         const blink::WebFormElement& form_element);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {HTMLFormElement} formElement A form element to be processed.
 * @return {Array<FormControlElement>} The array of autofillable elements.
 */
__gCrWeb.autofill.extractAutofillableElementsInForm = function(formElement) {
  const controlElements = __gCrWeb.form.getFormControlElements(formElement);
  return __gCrWeb.autofill.extractAutofillableElementsFromSet(controlElements);
};

/**
 * For debugging purposes, annotate forms on the page with prediction data using
 * the placeholder attribute.
 *
 * @param {Object<AutofillFormData>} data The form and field identifiers with
 *     their prediction data.
 */
__gCrWeb.autofill['fillPredictionData'] = function(data) {
  for (const formName in data) {
    const form = __gCrWeb.form.getFormElementFromIdentifier(formName);
    const formData = data[formName];
    const controlElements = __gCrWeb.form.getFormControlElements(form);
    for (let i = 0; i < controlElements.length; ++i) {
      const element = controlElements[i];
      if (!__gCrWeb.fill.isAutofillableElement(element)) {
        continue;
      }
      const elementID = __gCrWeb.fill.getUniqueID(element);
      const value = formData[elementID];
      if (value) {
        element.placeholder = value;
      }
    }
  }
};

/**
 * Returns whether |value| contains only formating characters.
 *
 * It is based on the logic in
 *     void SanitizedFieldIsEmpty(const std::u16string& value);
 * in chromium/src/components/autofill/common/autofill_util.h.
 *
 * @param {HTMLFormElement} formElement A form element to be processed.
 * @return {Array<FormControlElement>} The array of autofillable elements.
 */
__gCrWeb.autofill['sanitizedFieldIsEmpty'] = function(value) {
  // Some sites enter values such as ____-____-____-____ or (___)-___-____ in
  // their fields. Check if the field value is empty after the removal of the
  // formatting characters.
  return __gCrWeb.common.trim(value.replace(/[-_()/|]/g, '')) === '';
};
