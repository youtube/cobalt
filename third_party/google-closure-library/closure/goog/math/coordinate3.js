/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A utility class for representing three-dimensional points.
 *
 * Based heavily on coordinate.js by:
 */

goog.provide('goog.math.Coordinate3');



/**
 * Class for representing coordinates and positions in 3 dimensions.
 *
 * @param {number=} opt_x X coordinate, defaults to 0.
 * @param {number=} opt_y Y coordinate, defaults to 0.
 * @param {number=} opt_z Z coordinate, defaults to 0.
 * @struct
 * @constructor
 */
goog.math.Coordinate3 = function(opt_x, opt_y, opt_z) {
  'use strict';
  /**
   * X-value
   * @type {number}
   */
  this.x = (opt_x !== undefined) ? opt_x : 0;

  /**
   * Y-value
   * @type {number}
   */
  this.y = (opt_y !== undefined) ? opt_y : 0;

  /**
   * Z-value
   * @type {number}
   */
  this.z = (opt_z !== undefined) ? opt_z : 0;
};


/**
 * Returns a new copy of the coordinate.
 *
 * @return {!goog.math.Coordinate3} A clone of this coordinate.
 */
goog.math.Coordinate3.prototype.clone = function() {
  'use strict';
  return new goog.math.Coordinate3(this.x, this.y, this.z);
};


if (goog.DEBUG) {
  /**
   * Returns a nice string representing the coordinate.
   *
   * @return {string} In the form (50, 73, 31).
   * @override
   */
  goog.math.Coordinate3.prototype.toString = function() {
    'use strict';
    return '(' + this.x + ', ' + this.y + ', ' + this.z + ')';
  };
}


/**
 * Compares coordinates for equality.
 *
 * @param {goog.math.Coordinate3} a A Coordinate3.
 * @param {goog.math.Coordinate3} b A Coordinate3.
 * @return {boolean} True iff the coordinates are equal, or if both are null.
 */
goog.math.Coordinate3.equals = function(a, b) {
  'use strict';
  if (a == b) {
    return true;
  }
  if (!a || !b) {
    return false;
  }
  return a.x == b.x && a.y == b.y && a.z == b.z;
};


/**
 * Returns the distance between two coordinates.
 *
 * @param {goog.math.Coordinate3} a A Coordinate3.
 * @param {goog.math.Coordinate3} b A Coordinate3.
 * @return {number} The distance between `a` and `b`.
 */
goog.math.Coordinate3.distance = function(a, b) {
  'use strict';
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  const dz = a.z - b.z;
  return Math.sqrt(dx * dx + dy * dy + dz * dz);
};


/**
 * Returns the squared distance between two coordinates. Squared distances can
 * be used for comparisons when the actual value is not required.
 *
 * Performance note: eliminating the square root is an optimization often used
 * in lower-level languages, but the speed difference is not nearly as
 * pronounced in JavaScript (only a few percent.)
 *
 * @param {goog.math.Coordinate3} a A Coordinate3.
 * @param {goog.math.Coordinate3} b A Coordinate3.
 * @return {number} The squared distance between `a` and `b`.
 */
goog.math.Coordinate3.squaredDistance = function(a, b) {
  'use strict';
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  const dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
};


/**
 * Returns the difference between two coordinates as a new
 * goog.math.Coordinate3.
 *
 * @param {goog.math.Coordinate3} a A Coordinate3.
 * @param {goog.math.Coordinate3} b A Coordinate3.
 * @return {!goog.math.Coordinate3} A Coordinate3 representing the difference
 *     between `a` and `b`.
 */
goog.math.Coordinate3.difference = function(a, b) {
  'use strict';
  return new goog.math.Coordinate3(a.x - b.x, a.y - b.y, a.z - b.z);
};


/**
 * Returns the contents of this coordinate as a 3 value Array.
 *
 * @return {!Array<number>} A new array.
 */
goog.math.Coordinate3.prototype.toArray = function() {
  'use strict';
  return [this.x, this.y, this.z];
};


/**
 * Converts a three element array into a Coordinate3 object.  If the value
 * passed in is not an array, not array-like, or not of the right length, an
 * error is thrown.
 *
 * @param {Array<number>} a Array of numbers to become a coordinate.
 * @return {!goog.math.Coordinate3} A new coordinate from the array values.
 * @throws {Error} When the oject passed in is not valid.
 */
goog.math.Coordinate3.fromArray = function(a) {
  'use strict';
  if (a.length <= 3) {
    return new goog.math.Coordinate3(a[0], a[1], a[2]);
  }

  throw new Error('Conversion from an array requires an array of length 3');
};
