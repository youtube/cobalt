/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Defines the goog.module.ModuleInfo class.
 */

goog.provide('goog.module.ModuleInfo');

goog.require('goog.Disposable');
goog.require('goog.async.throwException');
goog.require('goog.dispose');
goog.require('goog.functions');
goog.require('goog.html.TrustedResourceUrl');
/** @suppress {extraRequire} */
goog.require('goog.module');
goog.require('goog.module.BaseModule');
goog.require('goog.module.ModuleLoadCallback');
goog.requireType('goog.module.ModuleLoadFailure');



/**
 * A ModuleInfo object is used by the ModuleManager to hold information about a
 * module of js code that may or may not yet be loaded into the environment.
 *
 * @param {!Array<string>} deps Ids of the modules that must be loaded before
 *     this one. The ids must be in dependency order (i.e. if the ith module
 *     depends on the jth module, then i > j).
 * @param {string} id The module's ID.
 * @constructor
 * @extends {goog.Disposable}
 * @final
 */
goog.module.ModuleInfo = function(deps, id) {
  'use strict';
  goog.Disposable.call(this);

  /**
   * A list of the ids of the modules that must be loaded before this module.
   * @type {!Array<string>}
   * @private @const
   */
  this.deps_ = deps;

  /**
   * The module's ID.
   * @type {string}
   * @private
   */
  this.id_ = id;

  /**
   * Callbacks to execute once this module is loaded.
   * @type {Array<goog.module.ModuleLoadCallback>}
   * @private
   */
  this.onloadCallbacks_ = [];

  /**
   * Callbacks to execute if the module load errors.
   * @type {Array<goog.module.ModuleLoadCallback>}
   * @private
   */
  this.onErrorCallbacks_ = [];

  /**
   * Early callbacks to execute once this module is loaded. Called after
   * module initialization but before regular onload callbacks.
   * @type {Array<goog.module.ModuleLoadCallback>}
   * @private
   */
  this.earlyOnloadCallbacks_ = [];
};
goog.inherits(goog.module.ModuleInfo, goog.Disposable);


/**
 * The uris that can be used to retrieve this module's code.
 * @type {?Array<!goog.html.TrustedResourceUrl>}
 * @private
 */
goog.module.ModuleInfo.prototype.uris_ = null;


/**
 * The constructor to use to instantiate the module object after the module
 * code is loaded. This must be either goog.module.BaseModule or a subclass of
 * it.
 * @type {Function}
 * @private
 */
goog.module.ModuleInfo.prototype.moduleConstructor_ = goog.module.BaseModule;


/**
 * The module object. This will be null until the module is loaded.
 * @type {goog.module.BaseModule?}
 * @private
 */
goog.module.ModuleInfo.prototype.module_ = null;


/**
 * Gets the dependencies of this module.
 * @return {!Array<string>} The ids of the modules that this module depends on.
 */
goog.module.ModuleInfo.prototype.getDependencies = function() {
  'use strict';
  return this.deps_;
};


/**
 * Gets the ID of this module.
 * @return {string} The ID.
 */
goog.module.ModuleInfo.prototype.getId = function() {
  'use strict';
  return this.id_;
};


/**
 * Sets the uris of this module.
 * @param {!Array<!goog.html.TrustedResourceUrl>} uris Uris for this module's
 *     code.
 */
goog.module.ModuleInfo.prototype.setTrustedUris = function(uris) {
  'use strict';
  this.uris_ = uris;
};


/**
 * Gets the uris of this module.
 * @return {!Array<!goog.html.TrustedResourceUrl>} Uris for this module's code.
 */
goog.module.ModuleInfo.prototype.getUris = function() {
  'use strict';
  if (!this.uris_) {
    this.uris_ = [];
  }
  return this.uris_;
};


/**
 * Sets the constructor to use to instantiate the module object after the
 * module code is loaded.
 * @param {Function} constructor The constructor of a goog.module.BaseModule
 *     subclass.
 */
goog.module.ModuleInfo.prototype.setModuleConstructor = function(constructor) {
  'use strict';
  if (this.moduleConstructor_ === goog.module.BaseModule) {
    this.moduleConstructor_ = constructor;
  } else {
    throw new Error('Cannot set module constructor more than once.');
  }
};


/**
 * Registers a function that should be called after the module is loaded. These
 * early callbacks are called after {@link Module#initialize} is called but
 * before the other callbacks are called.
 * @param {Function} fn A callback function that takes a single argument which
 *    is the module context.
 * @param {Object=} opt_handler Optional handler under whose scope to execute
 *     the callback.
 * @return {!goog.module.ModuleLoadCallback} Reference to the callback
 *     object.
 */
goog.module.ModuleInfo.prototype.registerEarlyCallback = function(
    fn, opt_handler) {
  'use strict';
  return this.registerCallback_(this.earlyOnloadCallbacks_, fn, opt_handler);
};


/**
 * Registers a function that should be called after the module is loaded.
 * @param {Function} fn A callback function that takes a single argument which
 *    is the module context.
 * @param {Object=} opt_handler Optional handler under whose scope to execute
 *     the callback.
 * @return {!goog.module.ModuleLoadCallback} Reference to the callback
 *     object.
 */
goog.module.ModuleInfo.prototype.registerCallback = function(fn, opt_handler) {
  'use strict';
  return this.registerCallback_(this.onloadCallbacks_, fn, opt_handler);
};


/**
 * Registers a function that should be called if the module load fails.
 * @param {Function} fn A callback function that takes a single argument which
 *    is the failure type.
 * @param {Object=} opt_handler Optional handler under whose scope to execute
 *     the callback.
 * @return {!goog.module.ModuleLoadCallback} Reference to the callback
 *     object.
 */
goog.module.ModuleInfo.prototype.registerErrback = function(fn, opt_handler) {
  'use strict';
  return this.registerCallback_(this.onErrorCallbacks_, fn, opt_handler);
};


/**
 * Registers a function that should be called after the module is loaded.
 * @param {Array<goog.module.ModuleLoadCallback>} callbacks The array to
 *     add the callback to.
 * @param {Function} fn A callback function that takes a single argument which
 *     is the module context.
 * @param {Object=} opt_handler Optional handler under whose scope to execute
 *     the callback.
 * @return {!goog.module.ModuleLoadCallback} Reference to the callback
 *     object.
 * @private
 */
goog.module.ModuleInfo.prototype.registerCallback_ = function(
    callbacks, fn, opt_handler) {
  'use strict';
  var callback = new goog.module.ModuleLoadCallback(fn, opt_handler);
  callbacks.push(callback);
  return callback;
};


/**
 * Determines whether the module has been loaded.
 * @return {boolean} Whether the module has been loaded.
 */
goog.module.ModuleInfo.prototype.isLoaded = function() {
  'use strict';
  return !!this.module_;
};


/**
 * Marks the current module as loaded. This is useful for subtractive module
 * loading, where occasionally we need to fallback to normal module loading,
 * and re-fetch the module graph. In this case, we need a way to tell the module
 * manager to mark all modules that are already loaded.
 */
goog.module.ModuleInfo.prototype.setLoaded = function() {
  'use strict';
  this.module_ = new goog.module.BaseModule();
};


/**
 * Gets the module.
 * @return {goog.module.BaseModule?} The module if it has been loaded.
 *     Otherwise, null.
 */
goog.module.ModuleInfo.prototype.getModule = function() {
  'use strict';
  return this.module_;
};


/**
 * Sets this module as loaded.
 * @param {function() : Object} contextProvider A function that provides the
 *     module context.
 * @return {boolean} Whether any errors occurred while executing the onload
 *     callbacks.
 */
goog.module.ModuleInfo.prototype.onLoad = function(contextProvider) {
  'use strict';
  // Instantiate and initialize the module object.
  var module = new this.moduleConstructor_;
  module.initialize(contextProvider());

  // Keep an internal reference to the module.
  this.module_ = module;

  // Fire any early callbacks that were waiting for the module to be loaded.
  var errors =
      !!this.callCallbacks_(this.earlyOnloadCallbacks_, contextProvider());

  // Fire any callbacks that were waiting for the module to be loaded.
  errors =
      errors || !!this.callCallbacks_(this.onloadCallbacks_, contextProvider());

  if (!errors) {
    // Clear the errbacks.
    this.onErrorCallbacks_.length = 0;
  }

  return errors;
};


/**
 * Calls the error callbacks for the module.
 * @param {!goog.module.ModuleLoadFailure} cause What caused the
 *     error.
 */
goog.module.ModuleInfo.prototype.onError = function(cause) {
  'use strict';
  var result = this.callCallbacks_(this.onErrorCallbacks_, cause);
  if (result) {
    // Throw an exception asynchronously. Do not let the exception leak
    // up to the caller, or it will blow up the module loading framework.

    // Call setTimeout on global object so that it can be called from within
    // webworkers.
    goog.global.setTimeout(
        goog.functions.error('Module errback failures: ' + result), 0);
  }
  this.earlyOnloadCallbacks_.length = 0;
  this.onloadCallbacks_.length = 0;
};


/**
 * Helper to call the callbacks after module load.
 * @param {Array<goog.module.ModuleLoadCallback>} callbacks The callbacks
 *     to call and then clear.
 * @param {*} context The module context.
 * @return {Array<*>} Any errors encountered while calling the callbacks,
 *     or null if there were no errors.
 * @private
 */
goog.module.ModuleInfo.prototype.callCallbacks_ = function(callbacks, context) {
  'use strict';
  // NOTE(nicksantos):
  // In practice, there are two error-handling scenarios:
  // 1) The callback does some mandatory initialization of the module.
  // 2) The callback is for completion of some optional UI event.
  // There's no good way to handle both scenarios.
  //
  // Our strategy here is to protect module manager from exceptions, so that
  // the failure of one module doesn't affect the loading of other modules.
  // Errors are thrown outside of the current stack frame, so they still
  // get reported but don't interrupt execution.

  // Call each callback in the order they were registered
  var errors = [];
  for (var i = 0; i < callbacks.length; i++) {
    try {
      callbacks[i].execute(context);
    } catch (e) {
      goog.async.throwException(e);
      errors.push(e);
    }
  }

  // Clear the list of callbacks.
  callbacks.length = 0;
  return errors.length ? errors : null;
};


/** @override */
goog.module.ModuleInfo.prototype.disposeInternal = function() {
  'use strict';
  goog.module.ModuleInfo.superClass_.disposeInternal.call(this);
  goog.dispose(this.module_);
};
