/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utilities for adding, removing and setting classes.  Prefer
 * {@link goog.dom.classlist} over these utilities since goog.dom.classlist
 * conforms closer to the semantics of Element.classList, is faster (uses
 * native methods rather than parsing strings on every call) and compiles
 * to smaller code as a result.
 *
 * Note: these utilities are meant to operate on HTMLElements and
 * will not work on elements with differing interfaces (such as SVGElements).
 */


goog.provide('goog.dom.classes');

goog.require('goog.array');


/**
 * Sets the entire class name of an element.
 * @param {Node} element DOM node to set class of.
 * @param {string} className Class name(s) to apply to element.
 * @deprecated Use goog.dom.classlist.set instead.
 */
goog.dom.classes.set = function(element, className) {
  'use strict';
  /** @type {!HTMLElement} */ (element).className = className;
};


/**
 * Gets an array of class names on an element
 * @param {Node} element DOM node to get class of.
 * @return {!Array<?>} Class names on `element`. Some browsers add extra
 *     properties to the array. Do not depend on any of these!
 * @deprecated Use goog.dom.classlist.get instead.
 */
goog.dom.classes.get = function(element) {
  'use strict';
  var className = /** @type {!Element} */ (element).className;
  // Some types of elements don't have a className in IE (e.g. iframes).
  // Furthermore, in Firefox, className is not a string when the element is
  // an SVG element.
  return typeof className === 'string' && className.match(/\S+/g) || [];
};


/**
 * Adds a class or classes to an element. Does not add multiples of class names.
 * @param {Node} element DOM node to add class to.
 * @param {...string} var_args Class names to add.
 * @return {boolean} Whether class was added (or all classes were added).
 * @deprecated Use goog.dom.classlist.add or goog.dom.classlist.addAll instead.
 */
goog.dom.classes.add = function(element, var_args) {
  'use strict';
  var classes = goog.dom.classes.get(element);
  var args = Array.prototype.slice.call(arguments, 1);
  var expectedCount = classes.length + args.length;
  goog.dom.classes.add_(classes, args);
  goog.dom.classes.set(element, classes.join(' '));
  return classes.length == expectedCount;
};


/**
 * Removes a class or classes from an element.
 * @param {Node} element DOM node to remove class from.
 * @param {...string} var_args Class name(s) to remove.
 * @return {boolean} Whether all classes in `var_args` were found and
 *     removed.
 * @deprecated Use goog.dom.classlist.remove or goog.dom.classlist.removeAll
 *     instead.
 */
goog.dom.classes.remove = function(element, var_args) {
  'use strict';
  var classes = goog.dom.classes.get(element);
  var args = Array.prototype.slice.call(arguments, 1);
  var newClasses = goog.dom.classes.getDifference_(classes, args);
  goog.dom.classes.set(element, newClasses.join(' '));
  return newClasses.length == classes.length - args.length;
};


/**
 * Helper method for {@link goog.dom.classes.add} and
 * {@link goog.dom.classes.addRemove}. Adds one or more classes to the supplied
 * classes array.
 * @param {Array<string>} classes All class names for the element, will be
 *     updated to have the classes supplied in `args` added.
 * @param {Array<string>} args Class names to add.
 * @private
 */
goog.dom.classes.add_ = function(classes, args) {
  'use strict';
  for (var i = 0; i < args.length; i++) {
    if (!goog.array.contains(classes, args[i])) {
      classes.push(args[i]);
    }
  }
};


/**
 * Helper method for {@link goog.dom.classes.remove} and
 * {@link goog.dom.classes.addRemove}. Calculates the difference of two arrays.
 * @param {!Array<string>} arr1 First array.
 * @param {!Array<string>} arr2 Second array.
 * @return {!Array<string>} The first array without the elements of the second
 *     array.
 * @private
 */
goog.dom.classes.getDifference_ = function(arr1, arr2) {
  'use strict';
  return arr1.filter(function(item) {
    'use strict';
    return !goog.array.contains(arr2, item);
  });
};


/**
 * Switches a class on an element from one to another without disturbing other
 * classes. If the fromClass isn't removed, the toClass won't be added.
 * @param {Node} element DOM node to swap classes on.
 * @param {string} fromClass Class to remove.
 * @param {string} toClass Class to add.
 * @return {boolean} Whether classes were switched.
 * @deprecated Use goog.dom.classlist.swap instead.
 */
goog.dom.classes.swap = function(element, fromClass, toClass) {
  'use strict';
  var classes = goog.dom.classes.get(element);

  var removed = false;
  for (var i = 0; i < classes.length; i++) {
    if (classes[i] == fromClass) {
      classes.splice(i--, 1);
      removed = true;
    }
  }

  if (removed) {
    classes.push(toClass);
    goog.dom.classes.set(element, classes.join(' '));
  }

  return removed;
};


/**
 * Adds zero or more classes to an element and removes zero or more as a single
 * operation. Unlike calling {@link goog.dom.classes.add} and
 * {@link goog.dom.classes.remove} separately, this is more efficient as it only
 * parses the class property once.
 *
 * If a class is in both the remove and add lists, it will be added. Thus,
 * you can use this instead of {@link goog.dom.classes.swap} when you have
 * more than two class names that you want to swap.
 *
 * @param {Node} element DOM node to swap classes on.
 * @param {?(string|Array<string>)} classesToRemove Class or classes to
 *     remove, if null no classes are removed.
 * @param {?(string|Array<string>)} classesToAdd Class or classes to add, if
 *     null no classes are added.
 * @deprecated Use goog.dom.classlist.addRemove instead.
 */
goog.dom.classes.addRemove = function(element, classesToRemove, classesToAdd) {
  'use strict';
  var classes = goog.dom.classes.get(element);
  if (typeof classesToRemove === 'string') {
    goog.array.remove(classes, classesToRemove);
  } else if (Array.isArray(classesToRemove)) {
    classes = goog.dom.classes.getDifference_(classes, classesToRemove);
  }

  if (typeof classesToAdd === 'string' &&
      !goog.array.contains(classes, classesToAdd)) {
    classes.push(classesToAdd);
  } else if (Array.isArray(classesToAdd)) {
    goog.dom.classes.add_(classes, classesToAdd);
  }

  goog.dom.classes.set(element, classes.join(' '));
};


/**
 * Returns true if an element has a class.
 * @param {Node} element DOM node to test.
 * @param {string} className Class name to test for.
 * @return {boolean} Whether element has the class.
 * @deprecated Use goog.dom.classlist.contains instead.
 */
goog.dom.classes.has = function(element, className) {
  'use strict';
  return goog.array.contains(goog.dom.classes.get(element), className);
};


/**
 * Adds or removes a class depending on the enabled argument.
 * @param {Node} element DOM node to add or remove the class on.
 * @param {string} className Class name to add or remove.
 * @param {boolean} enabled Whether to add or remove the class (true adds,
 *     false removes).
 * @deprecated Use goog.dom.classlist.enable or goog.dom.classlist.enableAll
 *     instead.
 */
goog.dom.classes.enable = function(element, className, enabled) {
  'use strict';
  if (enabled) {
    goog.dom.classes.add(element, className);
  } else {
    goog.dom.classes.remove(element, className);
  }
};


/**
 * Removes a class if an element has it, and adds it the element doesn't have
 * it.  Won't affect other classes on the node.
 * @param {Node} element DOM node to toggle class on.
 * @param {string} className Class to toggle.
 * @return {boolean} True if class was added, false if it was removed
 *     (in other words, whether element has the class after this function has
 *     been called).
 * @deprecated Use goog.dom.classlist.toggle instead.
 */
goog.dom.classes.toggle = function(element, className) {
  'use strict';
  var add = !goog.dom.classes.has(element, className);
  goog.dom.classes.enable(element, className, add);
  return add;
};
