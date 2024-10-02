/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A class that can be used to listen to font size changes.
 */

goog.provide('goog.dom.FontSizeMonitor');
goog.provide('goog.dom.FontSizeMonitor.EventType');

goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.events');
goog.require('goog.events.EventTarget');
goog.require('goog.events.EventType');
goog.require('goog.userAgent');
goog.requireType('goog.events.BrowserEvent');


// TODO(arv): Move this to goog.events instead.



/**
 * This class can be used to monitor changes in font size.  Instances will
 * dispatch a `goog.dom.FontSizeMonitor.EventType.CHANGE` event.
 * Example usage:
 * <pre>
 * var fms = new goog.dom.FontSizeMonitor();
 * goog.events.listen(fms, goog.dom.FontSizeMonitor.EventType.CHANGE,
 *     function(e) {
 *       alert('Font size was changed');
 *     });
 * </pre>
 * @param {goog.dom.DomHelper=} opt_domHelper DOM helper object that is used to
 *     determine where to insert the DOM nodes used to determine when the font
 *     size changes.
 * @constructor
 * @extends {goog.events.EventTarget}
 * @final
 */
goog.dom.FontSizeMonitor = function(opt_domHelper) {
  'use strict';
  goog.events.EventTarget.call(this);

  var dom = opt_domHelper || goog.dom.getDomHelper();

  /**
   * Offscreen iframe which we use to detect resize events.
   * @type {HTMLElement}
   * @private
   */
  this.sizeElement_ = /** @type {!HTMLElement} */ (
      dom.createDom(
          // The size of the iframe is expressed in em, which are font size
          // relative
          // which will cause the iframe to be resized when the font size
          // changes.
          // The actual values are not relevant as long as we can ensure that
          // the
          // iframe has a non zero size and is completely off screen.
          goog.userAgent.IE ? goog.dom.TagName.DIV : goog.dom.TagName.IFRAME, {
            'style': 'position:absolute;width:9em;height:9em;top:-99em',
            'tabIndex': -1,
            'aria-hidden': 'true'
          }));
  var p = dom.getDocument().body;
  p.insertBefore(this.sizeElement_, p.firstChild);

  /**
   * The object that we listen to resize events on.
   * @type {Element|Window}
   * @private
   */
  var resizeTarget = this.resizeTarget_ = goog.userAgent.IE ?
      this.sizeElement_ :
      goog.dom.getFrameContentWindow(
          /** @type {HTMLIFrameElement} */ (this.sizeElement_));

  // We need to open and close the document to get Firefox 2 to work.  We must
  // not do this for IE in case we are using HTTPS since accessing the document
  // on an about:blank iframe in IE using HTTPS raises a Permission Denied
  // error.
  if (goog.userAgent.GECKO) {
    var doc = resizeTarget.document;
    doc.open();
    doc.close();
  }

  // Listen to resize event on the window inside the iframe.
  goog.events.listen(
      resizeTarget, goog.events.EventType.RESIZE, this.handleResize_, false,
      this);

  /**
   * Last measured width of the iframe element.
   * @type {number}
   * @private
   */
  this.lastWidth_ = this.sizeElement_.offsetWidth;
};
goog.inherits(goog.dom.FontSizeMonitor, goog.events.EventTarget);


/**
 * The event types that the FontSizeMonitor fires.
 * @enum {string}
 */
goog.dom.FontSizeMonitor.EventType = {
  // TODO(arv): Change value to 'change' after updating the callers.
  CHANGE: 'fontsizechange'
};


/**
 * Constant for the change event.
 * @type {string}
 * @deprecated Use `goog.dom.FontSizeMonitor.EventType.CHANGE` instead.
 */
goog.dom.FontSizeMonitor.CHANGE_EVENT =
    goog.dom.FontSizeMonitor.EventType.CHANGE;


/** @override */
goog.dom.FontSizeMonitor.prototype.disposeInternal = function() {
  'use strict';
  goog.dom.FontSizeMonitor.superClass_.disposeInternal.call(this);

  goog.events.unlisten(
      this.resizeTarget_, goog.events.EventType.RESIZE, this.handleResize_,
      false, this);
  this.resizeTarget_ = null;

  goog.dom.removeNode(this.sizeElement_);
  delete this.sizeElement_;
};


/**
 * Handles the onresize event of the iframe and dispatches a change event in
 * case its size really changed.
 * @param {goog.events.BrowserEvent} e The event object.
 * @private
 */
goog.dom.FontSizeMonitor.prototype.handleResize_ = function(e) {
  'use strict';
  // Only dispatch the event if the size really changed.  Some newer browsers do
  // not really change the font-size,  instead they zoom the whole page.  This
  // does trigger window resize events on the iframe but the logical pixel size
  // remains the same (the device pixel size changes but that is irrelevant).
  var currentWidth = this.sizeElement_.offsetWidth;
  if (this.lastWidth_ != currentWidth) {
    this.lastWidth_ = currentWidth;
    this.dispatchEvent(goog.dom.FontSizeMonitor.EventType.CHANGE);
  }
};
