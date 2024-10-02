// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SelectMixin, SelectMixinInterface} from 'chrome://print/print_preview.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const select_mixin_test = {
  suiteName: 'SelectMixinTest',
  TestNames: {
    CallProcessSelectChange: 'call process select change',
  },
};

Object.assign(window, {select_mixin_test: select_mixin_test});

suite(select_mixin_test.suiteName, function() {
  let testSelect: SelectMixinInterface&HTMLElement;

  let settingValue: string = '0';

  suiteSetup(function() {
    const TestSelectElementBase = SelectMixin(PolymerElement);

    class TestSelectElement extends TestSelectElementBase {
      static get is() {
        return 'test-select';
      }

      static get template() {
        return html`
          <select value="{{selectedValue::change}}">
            <option value="0" selected>0</option>
            <option value="1">1</option>
            <option value="2">2</option>
          </select>
        `;
      }

      override onProcessSelectChange(value: string) {
        settingValue = value;
        this.dispatchEvent(new CustomEvent(
            'process-select-change-called',
            {bubbles: true, composed: true, detail: value}));
      }
    }

    customElements.define(TestSelectElement.is, TestSelectElement);
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testSelect = document.createElement('test-select') as (
                     HTMLElement & SelectMixinInterface);
    document.body.appendChild(testSelect);
    testSelect.selectedValue = '0';
  });

  // Tests that onProcessSelectChange() is called when the select value is
  // set programmatically or by changing the select element.
  test(select_mixin_test.TestNames.CallProcessSelectChange, function() {
    const select = testSelect.shadowRoot!.querySelector('select')!;
    assertEquals('0', testSelect.selectedValue);
    assertEquals('0', select.value);
    let whenProcessSelectCalled =
        eventToPromise('process-select-change-called', testSelect);
    testSelect.selectedValue = '1';
    // Should be debounced so settingValue has not changed yet.
    assertEquals('0', settingValue);
    return whenProcessSelectCalled
        .then((e) => {
          assertEquals('1', e.detail);
          assertEquals('1', select.value);
          whenProcessSelectCalled =
              eventToPromise('process-select-change-called', testSelect);
          select.value = '0';
          select.dispatchEvent(new CustomEvent('change'));
          assertEquals('1', settingValue);
          return whenProcessSelectCalled;
        })
        .then((e) => {
          assertEquals('0', e.detail);
          assertEquals('0', testSelect.selectedValue);
        });
  });
});
