/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A driver for testing renderers.
 */

goog.setTestOnly('goog.testing.ui.RendererHarness');
goog.provide('goog.testing.ui.RendererHarness');

goog.require('goog.Disposable');
goog.require('goog.dom.NodeType');
goog.require('goog.testing.asserts');
goog.require('goog.testing.dom');
goog.require('goog.ui.Control');
goog.require('goog.ui.ControlRenderer');



/**
 * A driver for testing renderers.
 *
 * @param {goog.ui.ControlRenderer} renderer A renderer to test.
 * @param {Element} renderParent The parent of the element where controls will
 *     be rendered.
 * @param {Element} decorateParent The parent of the element where controls will
 *     be decorated.
 * @constructor
 * @extends {goog.Disposable}
 * @final
 */
goog.testing.ui.RendererHarness = function(
    renderer, renderParent, decorateParent) {
  'use strict';
  goog.Disposable.call(this);

  /**
   * The renderer under test.
   * @type {goog.ui.ControlRenderer}
   * @private
   */
  this.renderer_ = renderer;

  /**
   * The parent of the element where controls will be rendered.
   * @type {Element}
   * @private
   */
  this.renderParent_ = renderParent;

  /**
   * The original HTML of the render element.
   * @type {string}
   * @private
   */
  this.renderHtml_ = renderParent.innerHTML;

  /**
   * The parent of the element where controls will be decorated.
   * @type {Element}
   * @private
   */
  this.decorateParent_ = decorateParent;

  /**
   * The original HTML of the decorated element.
   * @type {string}
   * @private
   */
  this.decorateHtml_ = decorateParent.innerHTML;
};
goog.inherits(goog.testing.ui.RendererHarness, goog.Disposable);


/**
 * A control to create by decoration.
 * @type {goog.ui.Control}
 * @private
 */
goog.testing.ui.RendererHarness.prototype.decorateControl_;


/**
 * A control to create by rendering.
 * @type {goog.ui.Control}
 * @private
 */
goog.testing.ui.RendererHarness.prototype.renderControl_;


/**
 * Whether all the necessary assert methods have been called.
 * @type {boolean}
 * @private
 */
goog.testing.ui.RendererHarness.prototype.verified_ = false;


/**
 * Attach a control and render its DOM.
 * @param {goog.ui.Control} control A control.
 * @return {Element} The element created.
 */
goog.testing.ui.RendererHarness.prototype.attachControlAndRender = function(
    control) {
  'use strict';
  this.renderControl_ = control;

  control.setRenderer(this.renderer_);
  control.render(this.renderParent_);
  return control.getElement();
};


/**
 * Attach a control and decorate the element given in the constructor.
 * @param {goog.ui.Control} control A control.
 * @return {Element} The element created.
 */
goog.testing.ui.RendererHarness.prototype.attachControlAndDecorate = function(
    control) {
  'use strict';
  this.decorateControl_ = control;

  control.setRenderer(this.renderer_);

  const child = this.decorateParent_.firstChild;
  assertEquals(
      'The decorated node must be an element', goog.dom.NodeType.ELEMENT,
      child.nodeType);
  control.decorate(/** @type {!Element} */ (child));
  return control.getElement();
};


/**
 * Assert that the rendered element and the decorated element match.
 */
goog.testing.ui.RendererHarness.prototype.assertDomMatches = function() {
  'use strict';
  assert(
      'Both elements were not generated',
      !!(this.renderControl_ && this.decorateControl_));
  goog.testing.dom.assertHtmlMatches(
      this.renderControl_.getElement().innerHTML,
      this.decorateControl_.getElement().innerHTML);
  this.verified_ = true;
};


/**
 * Destroy the harness, verifying that all assertions had been checked.
 * @override
 * @protected
 */
goog.testing.ui.RendererHarness.prototype.disposeInternal = function() {
  'use strict';
  // If the harness was not verified appropriately, throw an exception.
  assert(
      'Expected assertDomMatches to be called',
      this.verified_ || !this.renderControl_ || !this.decorateControl_);

  if (this.decorateControl_) {
    this.decorateControl_.dispose();
  }
  if (this.renderControl_) {
    this.renderControl_.dispose();
  }

  this.renderParent_.innerHTML = this.renderHtml_;
  this.decorateParent_.innerHTML = this.decorateHtml_;

  goog.testing.ui.RendererHarness.superClass_.disposeInternal.call(this);
};
