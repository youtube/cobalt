// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {ComputedStyleModel, Events} from './ComputedStyleModel.js';

/**
 * @unrestricted
 */
<<<<<<< HEAD
Elements.ElementsSidebarPane = class extends UI.VBox {
=======
export class ElementsSidebarPane extends UI.VBox {
>>>>>>> bc9bfbcd01448b108b9f2f03cc440b2b92016b02
  /**
   * @param {boolean=} delegatesFocus
   */
  constructor(delegatesFocus) {
    super(true, delegatesFocus);
    this.element.classList.add('flex-none');
    this._computedStyleModel = new ComputedStyleModel();
    this._computedStyleModel.addEventListener(Events.ComputedStyleChanged, this.onCSSModelChanged, this);

    this._updateThrottler = new Common.Throttler(100);
    this._updateWhenVisible = false;
  }

  /**
   * @return {?SDK.DOMNode}
   */
  node() {
    return this._computedStyleModel.node();
  }

  /**
   * @return {?SDK.CSSModel}
   */
  cssModel() {
    return this._computedStyleModel.cssModel();
  }

  /**
   * @protected
   * @return {!Promise.<?>}
   */
  doUpdate() {
    return Promise.resolve();
  }

  update() {
    this._updateWhenVisible = !this.isShowing();
    if (this._updateWhenVisible) {
      return;
    }
    this._updateThrottler.schedule(innerUpdate.bind(this));

    /**
     * @return {!Promise.<?>}
     * @this {ElementsSidebarPane}
     */
    function innerUpdate() {
      return this.isShowing() ? this.doUpdate() : Promise.resolve();
    }
  }

  /**
   * @override
   */
  wasShown() {
    super.wasShown();
    if (this._updateWhenVisible) {
      this.update();
    }
  }

  /**
   * @param {!Common.Event} event
   */
  onCSSModelChanged(event) {
  }
}
