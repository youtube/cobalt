// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrContainerShadowBehavior} from 'chrome://resources/ash/common/cr_container_shadow_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// clang-format on

suite('CrContainerShadowBehavior', function() {
  const TestElementBase =
      mixinBehaviors([CrContainerShadowBehavior], PolymerElement) as
      {new (): PolymerElement & CrContainerShadowBehavior};

  class TestElement extends TestElementBase {
    static get is() {
      return 'test-element';
    }

    static get template() {
      return html`
        <style>
          #container {
            height: 50px;
          }
        </style>
        <div id="before"></div>
        <div id="container" show-bottom-shadow$="[[showBottomShadow]]"></div>
        <div id="after"></div>
      `;
    }

    static get properties() {
      return {
        showBottomShadow: Boolean,
      };
    }

    showBottomShadow: boolean = false;
  }
  customElements.define(TestElement.is, TestElement);

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('no bottom shadow', function() {
    const element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);

    // Should not have a bottom shadow div.
    assertFalse(
        !!element.shadowRoot!.querySelector('#cr-container-shadow-bottom'));
    assertTrue(!!element.shadowRoot!.querySelector('#cr-container-shadow-top'));

    element.showBottomShadow = true;

    // Still no bottom shadow since this is only checked in attached();
    assertFalse(
        !!element.shadowRoot!.querySelector('#cr-container-shadow-bottom'));
    assertTrue(!!element.shadowRoot!.querySelector('#cr-container-shadow-top'));
  });

  test('show bottom shadow', function() {
    const element = document.createElement('test-element') as TestElement;
    element.showBottomShadow = true;
    document.body.appendChild(element);

    // Has both shadows.
    assertTrue(
        !!element.shadowRoot!.querySelector('#cr-container-shadow-bottom'));
    assertTrue(!!element.shadowRoot!.querySelector('#cr-container-shadow-top'));
  });
});
