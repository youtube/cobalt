/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Animated zippy widget implementation.
 *
 * @see ../demos/zippy.html
 */

goog.provide('goog.ui.AnimatedZippy');

goog.require('goog.a11y.aria.Role');
goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.events');
goog.require('goog.fx.Animation');
goog.require('goog.fx.Transition');
goog.require('goog.fx.easing');
goog.require('goog.ui.Zippy');
goog.require('goog.ui.ZippyEvent');
goog.requireType('goog.events.Event');



/**
 * Zippy widget. Expandable/collapsible container, clicking the header toggles
 * the visibility of the content.
 *
 * @param {Element|string|null} header Header element, either element
 *     reference, string id or null if no header exists.
 * @param {Element|string} content Content element, either element reference or
 *     string id.
 * @param {boolean=} opt_expanded Initial expanded/visibility state. Defaults to
 *     false.
 * @param {goog.dom.DomHelper=} opt_domHelper An optional DOM helper.
 * @param {goog.a11y.aria.Role<string>=} opt_role ARIA role, default TAB.
 * @constructor
 * @extends {goog.ui.Zippy}
 */
goog.ui.AnimatedZippy = function(
    header, content, opt_expanded, opt_domHelper, opt_role) {
  'use strict';
  var domHelper = opt_domHelper || goog.dom.getDomHelper();

  // Create wrapper element and move content into it.
  var elWrapper =
      domHelper.createDom(goog.dom.TagName.DIV, {'style': 'overflow:hidden'});
  var elContent = domHelper.getElement(content);
  elContent.parentNode.replaceChild(elWrapper, elContent);
  elWrapper.appendChild(elContent);

  /**
   * Content wrapper, used for animation.
   * @type {Element}
   * @private
   */
  this.elWrapper_ = elWrapper;

  /**
   * Reference to animation or null if animation is not active.
   * @type {?goog.fx.Animation}
   * @private
   */
  this.anim_ = null;

  // Call constructor of super class.
  goog.ui.Zippy.call(
      this, header, elContent, opt_expanded, undefined, domHelper, opt_role);

  // Set initial state.
  // NOTE: Set the class names as well otherwise animated zippys
  // start with empty class names.
  var expanded = this.isExpanded();
  this.elWrapper_.style.display = expanded ? '' : 'none';
  this.updateHeaderClassName(expanded);
};
goog.inherits(goog.ui.AnimatedZippy, goog.ui.Zippy);


/**
 * Constants for event names.
 *
 * @const
 */
goog.ui.AnimatedZippy.Events = {
  // The beginning of the animation when the zippy state toggles.
  TOGGLE_ANIMATION_BEGIN: goog.events.getUniqueId('toggleanimationbegin'),
  // The end of the animation when the zippy state toggles.
  TOGGLE_ANIMATION_END: goog.events.getUniqueId('toggleanimationend')
};


/**
 * Duration of expand/collapse animation, in milliseconds.
 * @type {number}
 */
goog.ui.AnimatedZippy.prototype.animationDuration = 500;


/**
 * Acceleration function for expand/collapse animation.
 * @type {!Function}
 */
goog.ui.AnimatedZippy.prototype.animationAcceleration = goog.fx.easing.easeOut;


/**
 * @return {boolean} Whether the zippy is in the process of being expanded or
 *     collapsed.
 */
goog.ui.AnimatedZippy.prototype.isBusy = function() {
  'use strict';
  return this.anim_ != null;
};


/**
 * Sets expanded state.
 *
 * @param {boolean} expanded Expanded/visibility state.
 * @override
 */
goog.ui.AnimatedZippy.prototype.setExpanded = function(expanded) {
  'use strict';
  if (this.isExpanded() == expanded && !this.anim_) {
    return;
  }

  // Reset display property of wrapper to allow content element to be
  // measured.
  if (this.elWrapper_.style.display == 'none') {
    this.elWrapper_.style.display = '';
  }

  // Measure content element.
  var h = this.getContentElement().offsetHeight;

  // Stop active animation (if any) and determine starting height.
  var startH = 0;
  if (this.anim_) {
    goog.events.removeAll(this.anim_);
    this.anim_.stop(false);

    var marginTop = parseInt(this.getContentElement().style.marginTop, 10);
    startH = h - Math.abs(marginTop);
  } else {
    startH = expanded ? 0 : h;
  }

  // Updates header class name after the animation has been stopped.
  this.updateHeaderClassName(expanded);

  // Set up expand/collapse animation.
  this.anim_ = new goog.fx.Animation(
      [0, startH], [0, expanded ? h : 0], this.animationDuration,
      this.animationAcceleration);

  var events = [
    goog.fx.Transition.EventType.BEGIN, goog.fx.Animation.EventType.ANIMATE,
    goog.fx.Transition.EventType.END
  ];
  goog.events.listen(this.anim_, events, this.onAnimate_, false, this);
  goog.events.listen(
      this.anim_, goog.fx.Transition.EventType.BEGIN,
      goog.bind(this.onAnimationBegin_, this, expanded));
  goog.events.listen(
      this.anim_, goog.fx.Transition.EventType.END,
      goog.bind(this.onAnimationCompleted_, this, expanded));

  // Start animation.
  this.anim_.play(false);
};


/**
 * Called during animation
 *
 * @param {goog.events.Event} e The event.
 * @private
 */
goog.ui.AnimatedZippy.prototype.onAnimate_ = function(e) {
  'use strict';
  var contentElement = this.getContentElement();
  var h = contentElement.offsetHeight;
  contentElement.style.marginTop = (e.y - h) + 'px';
};


/**
 * Called once the expand/collapse animation has started.
 *
 * @param {boolean} expanding Expanded/visibility state.
 * @private
 */
goog.ui.AnimatedZippy.prototype.onAnimationBegin_ = function(expanding) {
  'use strict';
  this.dispatchEvent(new goog.ui.ZippyEvent(
      goog.ui.AnimatedZippy.Events.TOGGLE_ANIMATION_BEGIN, this, expanding));
};


/**
 * Called once the expand/collapse animation has completed.
 *
 * @param {boolean} expanded Expanded/visibility state.
 * @private
 */
goog.ui.AnimatedZippy.prototype.onAnimationCompleted_ = function(expanded) {
  'use strict';
  // Fix wrong end position if the content has changed during the animation.
  if (expanded) {
    this.getContentElement().style.marginTop = '0';
  }

  goog.events.removeAll(/** @type {!goog.fx.Animation} */ (this.anim_));
  this.setExpandedInternal(expanded);
  this.anim_ = null;

  if (!expanded) {
    this.elWrapper_.style.display = 'none';
  }

  // Fire toggle event.
  this.dispatchEvent(
      new goog.ui.ZippyEvent(goog.ui.Zippy.Events.TOGGLE, this, expanded));
  this.dispatchEvent(new goog.ui.ZippyEvent(
      goog.ui.AnimatedZippy.Events.TOGGLE_ANIMATION_END, this, expanded));
};
