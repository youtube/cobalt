// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordLoadDuration, recordOccurence, recordPerdecage} from '../metrics_utils.js';
import {WindowProxy} from '../window_proxy.js';

import {Module, ModuleHeight} from './module_descriptor.js';
import {getTemplate} from './module_wrapper.html.js';

/** @fileoverview Element that implements the common module UI. */

export interface ModuleWrapperElement {
  $: {
    moduleElement: HTMLElement,
    impressionProbe: HTMLElement,
  };
}

export class ModuleWrapperElement extends PolymerElement {
  static get is() {
    return 'ntp-module-wrapper';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      module: {
        observer: 'onModuleChange_',
        type: Object,
      },
    };
  }

  module: Module;

  private onModuleChange_(_newValue: Module, oldValue?: Module) {
    assert(!oldValue);
    this.$.moduleElement.appendChild(this.module.element);
    if (this.module.descriptor.height !== ModuleHeight.DYNAMIC) {
      this.style.height = `${this.module.descriptor.height}px`;
    }

    // Log at most one usage per module per NTP page load. This is possible,
    // if a user opens a link in a new tab.
    this.module.element.addEventListener('usage', () => {
      recordOccurence('NewTabPage.Modules.Usage');
      recordOccurence(`NewTabPage.Modules.Usage.${this.module.descriptor.id}`);
    }, {once: true});

    // Log module's id when module's info button is clicked.
    this.module.element.addEventListener('info-button-click', () => {
      chrome.metricsPrivate.recordSparseValueWithPersistentHash(
          'NewTabPage.Modules.InfoButtonClicked', this.module.descriptor.id);
    }, {once: true});

    // Install observer to log module header impression.
    const headerObserver = new IntersectionObserver(([{intersectionRatio}]) => {
      if (intersectionRatio >= 1.0) {
        headerObserver.disconnect();
        const time = WindowProxy.getInstance().now();
        recordLoadDuration('NewTabPage.Modules.Impression', time);
        recordLoadDuration(
            `NewTabPage.Modules.Impression.${this.module.descriptor.id}`, time);
        this.dispatchEvent(new Event('detect-impression'));
        this.module.element.dispatchEvent(new Event('detect-impression'));
      }
    }, {threshold: 1.0});

    // Install observer to track max perdecage (x/10th) of the module visible on
    // the page.
    let intersectionPerdecage = 0;
    const moduleObserver = new IntersectionObserver(([{intersectionRatio}]) => {
      intersectionPerdecage =
          Math.floor(Math.max(intersectionPerdecage, intersectionRatio * 10));
    }, {threshold: [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1]});
    window.addEventListener('unload', () => {
      recordPerdecage(
          'NewTabPage.Modules.ImpressionRatio', intersectionPerdecage);
      recordPerdecage(
          `NewTabPage.Modules.ImpressionRatio.${this.module.descriptor.id}`,
          intersectionPerdecage);
    });

    // Calling observe will immediately invoke the callback. If the module is
    // fully shown when the page loads, the first callback invocation will
    // happen before the elements have dimensions. For this reason, we start
    // observing after the elements have had a chance to be rendered.
    microTask.run(() => {
      headerObserver.observe(this.$.impressionProbe);
      moduleObserver.observe(this);
    });

    // Track whether the user hovered on the module.
    this.addEventListener('mouseover', () => {
      chrome.metricsPrivate.recordSparseValueWithPersistentHash(
          'NewTabPage.Modules.Hover', this.module.descriptor.id);
    }, {
      capture: true,  // So that modules cannot swallow event.
      once: true,     // Only one log per NTP load.
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-module-wrapper': ModuleWrapperElement;
  }
}

customElements.define(ModuleWrapperElement.is, ModuleWrapperElement);
