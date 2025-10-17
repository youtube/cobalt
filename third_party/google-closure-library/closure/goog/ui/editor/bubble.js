/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Bubble component - handles display, hiding, etc. of the
 * actual bubble UI.
 *
 * This is used exclusively by code within the editor package, and should not
 * be used directly.
 */

goog.provide('goog.ui.editor.Bubble');

goog.require('goog.asserts');
goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.dom.ViewportSizeMonitor');
goog.require('goog.dom.classlist');
goog.require('goog.dom.safe');
goog.require('goog.editor.style');
goog.require('goog.events.EventHandler');
goog.require('goog.events.EventTarget');
goog.require('goog.events.EventType');
goog.require('goog.functions');
goog.require('goog.log');
goog.require('goog.math.Box');
goog.require('goog.object');
goog.require('goog.positioning');
goog.require('goog.positioning.Corner');
goog.require('goog.positioning.Overflow');
goog.require('goog.positioning.OverflowStatus');
goog.require('goog.string');
goog.require('goog.string.Const');
goog.require('goog.style');
goog.require('goog.ui.Component');
goog.require('goog.ui.PopupBase');
goog.require('goog.userAgent');



/**
 * Property bubble UI element.
 * @param {Element} parent The parent element for this bubble.
 * @param {number} zIndex The z index to draw the bubble at.
 * @constructor
 * @extends {goog.events.EventTarget}
 */
goog.ui.editor.Bubble = function(parent, zIndex) {
  'use strict';
  goog.ui.editor.Bubble.base(this, 'constructor');

  /**
   * Dom helper for the document the bubble should be shown in.
   * @type {!goog.dom.DomHelper}
   * @private
   */
  this.dom_ = goog.dom.getDomHelper(parent);

  /**
   * Event handler for this bubble.
   * @type {goog.events.EventHandler<!goog.ui.editor.Bubble>}
   * @private
   */
  this.eventHandler_ = new goog.events.EventHandler(this);

  /**
   * Object that monitors the application window for size changes.
   * @type {goog.dom.ViewportSizeMonitor}
   * @private
   */
  this.viewPortSizeMonitor_ =
      new goog.dom.ViewportSizeMonitor(this.dom_.getWindow());

  /**
   * Maps panel ids to panels.
   * @type {Object<goog.ui.editor.Bubble.Panel_>}
   * @private
   */
  this.panels_ = {};

  /**
   * Container element for the entire bubble.  This may contain elements related
   * to look and feel or styling of the bubble.
   * @type {Element}
   * @private
   */
  this.bubbleContainer_ = this.dom_.createDom(
      goog.dom.TagName.DIV,
      {'className': goog.ui.editor.Bubble.BUBBLE_CLASSNAME});

  goog.style.setElementShown(this.bubbleContainer_, false);
  goog.dom.appendChild(parent, this.bubbleContainer_);
  goog.style.setStyle(this.bubbleContainer_, 'zIndex', zIndex);

  /**
   * Container element for the bubble panels - this should be some inner element
   * within (or equal to) bubbleContainer.
   * @type {Element}
   * @private
   */
  this.bubbleContents_ = this.createBubbleDom(this.dom_, this.bubbleContainer_);

  /**
   * Element showing the close box.
   * @type {!Element}
   * @private
   */
  this.closeBox_ = this.dom_.createDom(goog.dom.TagName.DIV, {
    'className': goog.getCssName('tr_bubble_closebox'),
  });
  goog.dom.safe.setInnerHtmlFromConstant(
      this.closeBox_, goog.string.Const.from('&nbsp;'));
  this.bubbleContents_.appendChild(this.closeBox_);

  // We make bubbles unselectable so that clicking on them does not steal focus
  // or move the cursor away from the element the bubble is attached to.
  goog.editor.style.makeUnselectable(this.bubbleContainer_, this.eventHandler_);

  /**
   * Popup that controls showing and hiding the bubble at the appropriate
   * position.
   * @type {goog.ui.PopupBase}
   * @private
   */
  this.popup_ = new goog.ui.PopupBase(this.bubbleContainer_);
};
goog.inherits(goog.ui.editor.Bubble, goog.events.EventTarget);


/**
 * The css class name of the bubble container element.
 * @type {string}
 */
goog.ui.editor.Bubble.BUBBLE_CLASSNAME = goog.getCssName('tr_bubble');


/**
 * Creates and adds DOM for the bubble UI to the given container.  This default
 * implementation just returns the container itself.
 * @param {!goog.dom.DomHelper} dom DOM helper to use.
 * @param {!Element} container Element to add the new elements to.
 * @return {!Element} The element where bubble content should be added.
 * @protected
 */
goog.ui.editor.Bubble.prototype.createBubbleDom = function(dom, container) {
  'use strict';
  return container;
};


/**
 * A logger for goog.ui.editor.Bubble.
 * @type {goog.log.Logger}
 * @protected
 */
goog.ui.editor.Bubble.prototype.logger =
    goog.log.getLogger('goog.ui.editor.Bubble');


/** @override */
goog.ui.editor.Bubble.prototype.disposeInternal = function() {
  'use strict';
  goog.ui.editor.Bubble.base(this, 'disposeInternal');

  goog.dom.removeNode(this.bubbleContainer_);
  this.bubbleContainer_ = null;

  this.eventHandler_.dispose();
  this.eventHandler_ = null;

  this.viewPortSizeMonitor_.dispose();
  this.viewPortSizeMonitor_ = null;
};


/**
 * @return {Element} The element that where the bubble's contents go.
 */
goog.ui.editor.Bubble.prototype.getContentElement = function() {
  'use strict';
  return this.bubbleContents_;
};


/**
 * @return {Element} The element that contains the bubble.
 * @protected
 */
goog.ui.editor.Bubble.prototype.getContainerElement = function() {
  'use strict';
  return this.bubbleContainer_;
};


/**
 * @return {goog.events.EventHandler<T>} The event handler.
 * @protected
 * @this {T}
 * @template T
 */
goog.ui.editor.Bubble.prototype.getEventHandler = function() {
  'use strict';
  return this.eventHandler_;
};


/**
 * Handles user resizing of window.
 * @private
 */
goog.ui.editor.Bubble.prototype.handleWindowResize_ = function() {
  'use strict';
  if (this.isVisible()) {
    this.reposition();
  }
};


/**
 * Sets whether the bubble dismisses itself when the user clicks outside of it.
 * @param {boolean} autoHide Whether to autohide on an external click.
 */
goog.ui.editor.Bubble.prototype.setAutoHide = function(autoHide) {
  'use strict';
  this.popup_.setAutoHide(autoHide);
};


/**
 * Returns whether there is already a panel of the given type.
 * @param {string} type Type of panel to check.
 * @return {boolean} Whether there is already a panel of the given type.
 */
goog.ui.editor.Bubble.prototype.hasPanelOfType = function(type) {
  'use strict';
  return goog.object.some(this.panels_, function(panel) {
    'use strict';
    return panel.type == type;
  });
};


/**
 * Adds a panel to the bubble.
 * @param {string} type The type of bubble panel this is.  Should usually be
 *     the same as the tagName of the targetElement.  This ensures multiple
 *     bubble panels don't appear for the same element.
 * @param {string} title The title of the panel.
 * @param {Element} targetElement The target element of the bubble.
 * @param {function(Element): void} contentFn Function that when called with
 *     a container element, will add relevant panel content to it.
 * @param {boolean=} opt_preferTopPosition Whether to prefer placing the bubble
 *     above the element instead of below it.  Defaults to preferring below.
 *     If any panel prefers the top position, the top position is used.
 * @return {string} The id of the panel.
 */
goog.ui.editor.Bubble.prototype.addPanel = function(
    type, title, targetElement, contentFn, opt_preferTopPosition) {
  'use strict';
  const id = goog.string.createUniqueString();
  const panel = new goog.ui.editor.Bubble.Panel_(
      this.dom_, id, type, title, targetElement, !opt_preferTopPosition);
  this.panels_[id] = panel;

  // Insert the panel in string order of type.  Technically we could use binary
  // search here but n is really small (probably 0 - 2) so it's not worth it.
  // The last child of bubbleContents_ is the close box so we take care not
  // to treat it as a panel element, and we also ensure it stays as the last
  // element.  The intention here is not to create any artificial order, but
  // just to ensure that it is always consistent.
  let nextElement;
  for (let i = 0, len = this.bubbleContents_.childNodes.length - 1; i < len;
       i++) {
    const otherChild = this.bubbleContents_.childNodes[i];
    /** @suppress {strictMissingProperties} Added to tighten compiler checks */
    const otherPanel = this.panels_[otherChild.id];
    if (otherPanel.type > type) {
      nextElement = otherChild;
      break;
    }
  }
  goog.dom.insertSiblingBefore(
      panel.element, nextElement || this.bubbleContents_.lastChild);

  contentFn(panel.getContentElement());
  goog.editor.style.makeUnselectable(panel.element, this.eventHandler_);

  const numPanels = goog.object.getCount(this.panels_);
  if (numPanels == 1) {
    this.openBubble_();
  } else if (numPanels == 2) {
    goog.dom.classlist.add(
        goog.asserts.assert(this.bubbleContainer_),
        goog.getCssName('tr_multi_bubble'));
  }
  this.reposition();

  return id;
};


/**
 * Removes the panel with the given id.
 * @param {string} id The id of the panel.
 */
goog.ui.editor.Bubble.prototype.removePanel = function(id) {
  'use strict';
  const panel = this.panels_[id];
  goog.dom.removeNode(panel.element);
  delete this.panels_[id];

  const numPanels = goog.object.getCount(this.panels_);
  if (numPanels <= 1) {
    goog.dom.classlist.remove(
        goog.asserts.assert(this.bubbleContainer_),
        goog.getCssName('tr_multi_bubble'));
  }

  if (numPanels == 0) {
    this.closeBubble_();
  } else {
    this.reposition();
  }
};


/**
 * Opens the bubble.
 * @private
 */
goog.ui.editor.Bubble.prototype.openBubble_ = function() {
  'use strict';
  this.eventHandler_
      .listen(this.closeBox_, goog.events.EventType.CLICK, this.closeBubble_)
      .listen(
          this.viewPortSizeMonitor_, goog.events.EventType.RESIZE,
          this.handleWindowResize_)
      .listen(
          this.popup_, goog.ui.PopupBase.EventType.HIDE, this.handlePopupHide);

  this.popup_.setVisible(true);
  this.reposition();
};


/**
 * Closes the bubble.
 * @private
 */
goog.ui.editor.Bubble.prototype.closeBubble_ = function() {
  'use strict';
  this.popup_.setVisible(false);
};


/**
 * Handles the popup's hide event by removing all panels and dispatching a
 * HIDE event.
 * @protected
 */
goog.ui.editor.Bubble.prototype.handlePopupHide = function() {
  'use strict';
  // Remove the panel elements.
  for (let panelId in this.panels_) {
    goog.dom.removeNode(this.panels_[panelId].element);
  }

  // Update the state to reflect no panels.
  this.panels_ = {};
  goog.dom.classlist.remove(
      goog.asserts.assert(this.bubbleContainer_),
      goog.getCssName('tr_multi_bubble'));

  this.eventHandler_.removeAll();
  this.dispatchEvent(goog.ui.Component.EventType.HIDE);
};


/**
 * Returns the visibility of the bubble.
 * @return {boolean} True if visible false if not.
 */
goog.ui.editor.Bubble.prototype.isVisible = function() {
  'use strict';
  return this.popup_.isVisible();
};


/**
 * The vertical clearance in pixels between the bottom of the targetElement
 * and the edge of the bubble.
 * @type {number}
 * @private
 */
goog.ui.editor.Bubble.VERTICAL_CLEARANCE_ = goog.userAgent.IE ? 4 : 2;


/**
 * Bubble's margin box to be passed to goog.positioning.
 * @type {goog.math.Box}
 * @private
 */
goog.ui.editor.Bubble.MARGIN_BOX_ = new goog.math.Box(
    goog.ui.editor.Bubble.VERTICAL_CLEARANCE_, 0,
    goog.ui.editor.Bubble.VERTICAL_CLEARANCE_, 0);


/**
 * Returns the margin box.
 * @return {goog.math.Box}
 * @protected
 */
goog.ui.editor.Bubble.prototype.getMarginBox = function() {
  'use strict';
  return goog.ui.editor.Bubble.MARGIN_BOX_;
};


/**
 * Positions and displays this bubble below its targetElement. Assumes that
 * the bubbleContainer is already contained in the document object it applies
 * to.
 */
goog.ui.editor.Bubble.prototype.reposition = function() {
  'use strict';
  let targetElement = null;
  let preferBottomPosition = true;
  for (let panelId in this.panels_) {
    const panel = this.panels_[panelId];
    // We don't care which targetElement we get, so we just take the last one.
    targetElement = panel.targetElement;
    preferBottomPosition = preferBottomPosition && panel.preferBottomPosition;
  }
  let status = goog.positioning.OverflowStatus.FAILED;

  // Fix for bug when bubbleContainer and targetElement have
  // opposite directionality, the bubble should anchor to the END of
  // the targetElement instead of START.
  const reverseLayout =
      (goog.style.isRightToLeft(this.bubbleContainer_) !=
       goog.style.isRightToLeft(targetElement));

  // Try to put the bubble at the bottom of the target unless the plugin has
  // requested otherwise.
  if (preferBottomPosition) {
    status = this.positionAtAnchor_(
        reverseLayout ? goog.positioning.Corner.BOTTOM_END :
                        goog.positioning.Corner.BOTTOM_START,
        goog.positioning.Corner.TOP_START,
        goog.positioning.Overflow.ADJUST_X | goog.positioning.Overflow.FAIL_Y);
  }

  if (status & goog.positioning.OverflowStatus.FAILED) {
    // Try to put it at the top of the target if there is not enough
    // space at the bottom.
    status = this.positionAtAnchor_(
        reverseLayout ? goog.positioning.Corner.TOP_END :
                        goog.positioning.Corner.TOP_START,
        goog.positioning.Corner.BOTTOM_START,
        goog.positioning.Overflow.ADJUST_X | goog.positioning.Overflow.FAIL_Y);
  }

  if (status & goog.positioning.OverflowStatus.FAILED) {
    // Put it at the bottom again with adjustment if there is no
    // enough space at the top.
    status = this.positionAtAnchor_(
        reverseLayout ? goog.positioning.Corner.BOTTOM_END :
                        goog.positioning.Corner.BOTTOM_START,
        goog.positioning.Corner.TOP_START, goog.positioning.Overflow.ADJUST_X |
            goog.positioning.Overflow.ADJUST_Y);
    if (status & goog.positioning.OverflowStatus.FAILED) {
      goog.log.warning(
          this.logger,
          'reposition(): positionAtAnchor() failed with ' + status);
    }
  }
};


/**
 * A helper for reposition() - positions the bubble in regards to the position
 * of the elements the bubble is attached to.
 * @param {goog.positioning.Corner} targetCorner The corner of
 *     the target element.
 * @param {goog.positioning.Corner} bubbleCorner The corner of the bubble.
 * @param {number} overflow Overflow handling mode bitmap,
 *     {@see goog.positioning.Overflow}.
 * @return {number} Status bitmap, {@see goog.positioning.OverflowStatus}.
 * @private
 */
goog.ui.editor.Bubble.prototype.positionAtAnchor_ = function(
    targetCorner, bubbleCorner, overflow) {
  'use strict';
  let targetElement = null;
  for (let panelId in this.panels_) {
    // For now, we use the outermost element.  This assumes the multiple
    // elements this panel is showing for contain each other - in the event
    // that is not generally the case this may need to be updated to pick
    // the lowest or highest element depending on targetCorner.
    const candidate = this.panels_[panelId].targetElement;
    if (!targetElement || goog.dom.contains(candidate, targetElement)) {
      targetElement = this.panels_[panelId].targetElement;
    }
  }
  return goog.positioning.positionAtAnchor(
      targetElement, targetCorner, this.bubbleContainer_, bubbleCorner, null,
      this.getMarginBox(), overflow, null, this.getViewportBox());
};


/**
 * Returns the viewport box to use when positioning the bubble.
 * @return {goog.math.Box}
 * @protected
 */
goog.ui.editor.Bubble.prototype.getViewportBox = goog.functions.NULL;



/**
 * Private class used to describe a bubble panel.
 * @param {goog.dom.DomHelper} dom DOM helper used to create the panel.
 * @param {string} id ID of the panel.
 * @param {string} type Type of the panel.
 * @param {string} title Title of the panel.
 * @param {Element} targetElement Element the panel is showing for.
 * @param {boolean} preferBottomPosition Whether this panel prefers to show
 *     below the target element.
 * @constructor
 * @private
 */
goog.ui.editor.Bubble.Panel_ = function(
    dom, id, type, title, targetElement, preferBottomPosition) {
  'use strict';
  /**
   * The type of bubble panel.
   * @type {string}
   */
  this.type = type;

  /**
   * The target element of this bubble panel.
   * @type {Element}
   */
  this.targetElement = targetElement;

  /**
   * Whether the panel prefers to be placed below the target element.
   * @type {boolean}
   */
  this.preferBottomPosition = preferBottomPosition;

  /**
   * The element containing this panel.
   * @type {!Element}
   */
  this.element = dom.createDom(
      goog.dom.TagName.DIV,
      {className: goog.getCssName('tr_bubble_panel'), id: id},
      dom.createDom(
          goog.dom.TagName.DIV,
          {className: goog.getCssName('tr_bubble_panel_title')},
          title ? title + ':' : ''),  // TODO(robbyw): Does this work in bidi?
      dom.createDom(
          goog.dom.TagName.DIV,
          {className: goog.getCssName('tr_bubble_panel_content')}));
};


/**
 * @return {Element} The element in the panel where content should go.
 */
goog.ui.editor.Bubble.Panel_.prototype.getContentElement = function() {
  'use strict';
  return /** @type {Element} */ (this.element.lastChild);
};
