/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A wrapper for the HTML5 FileError object.
 */


// TODO(user): We're trying to migrate all ES5 subclasses of Closure
// Library to ES6. In ES6 this cannot be referenced before super is called. This
// file has at least one this before a super call (in ES5) and cannot be
// automatically upgraded to ES6 as a result. Please fix this if you have a
// chance. Note: This can sometimes be caused by not calling the super
// constructor at all. You can run the conversion tool yourself to see what it
// does on this file: blaze run //javascript/refactoring/es6_classes:convert.

goog.provide('goog.fs.DOMErrorLike');
goog.provide('goog.fs.Error');
goog.provide('goog.fs.Error.ErrorCode');

goog.require('goog.asserts');
goog.require('goog.debug.Error');
goog.require('goog.object');
goog.require('goog.string');

/** @record */
goog.fs.DOMErrorLike = function() {};

/** @type {string|undefined} */
goog.fs.DOMErrorLike.prototype.name;

/** @type {!goog.fs.Error.ErrorCode|undefined} */
goog.fs.DOMErrorLike.prototype.code;



/**
 * A filesystem error. Since the filesystem API is asynchronous, stack traces
 * are less useful for identifying where errors come from, so this includes a
 * large amount of metadata in the message.
 *
 * @param {!DOMError|!goog.fs.DOMErrorLike} error
 * @param {string} action The action being undertaken when the error was raised.
 * @constructor
 * @extends {goog.debug.Error}
 * @final
 */
goog.fs.Error = function(error, action) {
  'use strict';
  /** @type {string} */
  this.name;

  /**
   * @type {!goog.fs.Error.ErrorCode}
   * @deprecated Use the 'name' or 'message' field instead.
   */
  this.code;

  if (error.name !== undefined) {
    this.name = error.name;
    // TODO(user): Remove warning suppression after JSCompiler stops
    // firing a spurious warning here.
    /** @suppress {deprecated} */
    this.code = goog.fs.Error.getCodeFromName_(error.name);
  } else {
    const code = /** @type {!goog.fs.Error.ErrorCode} */ (
        goog.asserts.assertNumber(error.code));
    this.code = code;
    this.name = goog.fs.Error.getNameFromCode_(code);
  }
  goog.fs.Error.base(
      this, 'constructor', goog.string.subs('%s %s', this.name, action));
};
goog.inherits(goog.fs.Error, goog.debug.Error);


/**
 * Names of errors that may be thrown by the File API, the File System API, or
 * the File Writer API.
 *
 * @see http://dev.w3.org/2006/webapi/FileAPI/#ErrorAndException
 * @see http://www.w3.org/TR/file-system-api/#definitions
 * @see http://dev.w3.org/2009/dap/file-system/file-writer.html#definitions
 * @enum {string}
 */
goog.fs.Error.ErrorName = {
  ABORT: 'AbortError',
  ENCODING: 'EncodingError',
  INVALID_MODIFICATION: 'InvalidModificationError',
  INVALID_STATE: 'InvalidStateError',
  NOT_FOUND: 'NotFoundError',
  NOT_READABLE: 'NotReadableError',
  NO_MODIFICATION_ALLOWED: 'NoModificationAllowedError',
  PATH_EXISTS: 'PathExistsError',
  QUOTA_EXCEEDED: 'QuotaExceededError',
  SECURITY: 'SecurityError',
  SYNTAX: 'SyntaxError',
  TYPE_MISMATCH: 'TypeMismatchError'
};


/**
 * Error codes for file errors.
 * @see http://www.w3.org/TR/file-system-api/#idl-def-FileException
 *
 * @enum {number}
 * @deprecated Use the 'name' or 'message' attribute instead.
 */
goog.fs.Error.ErrorCode = {
  NOT_FOUND: 1,
  SECURITY: 2,
  ABORT: 3,
  NOT_READABLE: 4,
  ENCODING: 5,
  NO_MODIFICATION_ALLOWED: 6,
  INVALID_STATE: 7,
  SYNTAX: 8,
  INVALID_MODIFICATION: 9,
  QUOTA_EXCEEDED: 10,
  TYPE_MISMATCH: 11,
  PATH_EXISTS: 12
};


/**
 * @param {goog.fs.Error.ErrorCode|undefined} code
 * @return {string} name
 * @private
 */
goog.fs.Error.getNameFromCode_ = function(code) {
  'use strict';
  const name = goog.object.findKey(goog.fs.Error.NameToCodeMap_, function(c) {
    'use strict';
    return code == c;
  });
  if (name === undefined) {
    throw new Error('Invalid code: ' + code);
  }
  return name;
};


/**
 * Returns the code that corresponds to the given name.
 * @param {string} name
 * @return {goog.fs.Error.ErrorCode} code
 * @private
 */
goog.fs.Error.getCodeFromName_ = function(name) {
  'use strict';
  return goog.fs.Error.NameToCodeMap_[name];
};


/**
 * Mapping from error names to values from the ErrorCode enum.
 * @see http://www.w3.org/TR/file-system-api/#definitions.
 * @private {!Object<string, goog.fs.Error.ErrorCode>}
 */
goog.fs.Error.NameToCodeMap_ = {
  [goog.fs.Error.ErrorName.ABORT]: goog.fs.Error.ErrorCode.ABORT,
  [goog.fs.Error.ErrorName.ENCODING]: goog.fs.Error.ErrorCode.ENCODING,
  [goog.fs.Error.ErrorName.INVALID_MODIFICATION]:
      goog.fs.Error.ErrorCode.INVALID_MODIFICATION,
  [goog.fs.Error.ErrorName.INVALID_STATE]:
      goog.fs.Error.ErrorCode.INVALID_STATE,
  [goog.fs.Error.ErrorName.NOT_FOUND]: goog.fs.Error.ErrorCode.NOT_FOUND,
  [goog.fs.Error.ErrorName.NOT_READABLE]: goog.fs.Error.ErrorCode.NOT_READABLE,
  [goog.fs.Error.ErrorName.NO_MODIFICATION_ALLOWED]:
      goog.fs.Error.ErrorCode.NO_MODIFICATION_ALLOWED,
  [goog.fs.Error.ErrorName.PATH_EXISTS]: goog.fs.Error.ErrorCode.PATH_EXISTS,
  [goog.fs.Error.ErrorName.QUOTA_EXCEEDED]:
      goog.fs.Error.ErrorCode.QUOTA_EXCEEDED,
  [goog.fs.Error.ErrorName.SECURITY]: goog.fs.Error.ErrorCode.SECURITY,
  [goog.fs.Error.ErrorName.SYNTAX]: goog.fs.Error.ErrorCode.SYNTAX,
  [goog.fs.Error.ErrorName.TYPE_MISMATCH]: goog.fs.Error.ErrorCode.TYPE_MISMATCH
};
