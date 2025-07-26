/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Event observer.
 *
 * Provides an event observer that holds onto events that it handles.  This
 * can be used in unit testing to verify an event target's events --
 * that the order count, types, etc. are correct.
 *
 * Example usage:
 * <pre>
 * var observer = new goog.testing.events.EventObserver();
 * var widget = new foo.Widget();
 * goog.events.listen(widget, ['select', 'submit'], observer);
 * // Simulate user action of 3 select events and 2 submit events.
 * assertEquals(3, observer.getEvents('select').length);
 * assertEquals(2, observer.getEvents('submit').length);
 * </pre>
 */

goog.setTestOnly('goog.testing.events.EventObserver');
goog.provide('goog.testing.events.EventObserver');

goog.require('goog.array');
goog.require('goog.events.Event');
goog.requireType('goog.events.EventId');



/**
 * Event observer.  Implements a handleEvent interface so it may be used as
 * a listener in listening functions and methods.
 * @see goog.events.listen
 * @see goog.events.EventHandler
 * @constructor
 * @final
 */
goog.testing.events.EventObserver = function() {
  'use strict';
  /**
   * A list of events handled by the observer in order of handling, oldest to
   * newest.
   * @type {!Array<!goog.events.Event>}
   * @private
   */
  this.events_ = [];
};


/**
 * Handles an event and remembers it.  Event listening functions and methods
 * will call this method when this observer is used as a listener.
 * @see goog.events.listen
 * @see goog.events.EventHandler
 * @param {!goog.events.Event} e Event to handle.
 */
goog.testing.events.EventObserver.prototype.handleEvent = function(e) {
  'use strict';
  this.events_.push(e);
};


/**
 * @param {string|!goog.events.EventId=} opt_type If given, only return events
 *     of this type.
 * @return {!Array<!goog.events.Event>} The events handled, oldest to newest.
 */
goog.testing.events.EventObserver.prototype.getEvents = function(opt_type) {
  'use strict';
  let events = goog.array.clone(this.events_);

  if (opt_type) {
    events = events.filter(function(event) {
      'use strict';
      return event.type == String(opt_type);
    });
  }

  return events;
};


/** Clears the list of events seen by this observer. */
goog.testing.events.EventObserver.prototype.clear = function() {
  'use strict';
  this.events_ = [];
};
