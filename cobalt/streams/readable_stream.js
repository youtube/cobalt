// ==ClosureCompiler==
// @output_file_name readable_stream.js
// @compilation_level SIMPLE_OPTIMIZATIONS
// @language_out ES5_STRICT
// ==/ClosureCompiler==

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global) {
  'use strict';

  const defineProperty = global.Object.defineProperty;

  // Mimic functionality provided to v8 extras.
  const v8_InternalPackedArray = global.Array;

  const v8_createPrivateSymbol = global.Symbol;

  function v8_simpleBind(fn, obj) {
    return fn.bind(obj);
  }

  const arraySlice = global.Array.prototype.slice;
  function v8_uncurryThis(fn) {
    return function(obj) {
      return fn.apply(obj, arraySlice.call(arguments, 1));
    }
  }

  const PromiseBase = global.Promise;
  const _promiseResolve = v8_createPrivateSymbol('[[Resolve]]');
  const _promiseReject = v8_createPrivateSymbol('[[Reject]]');

  function v8_createPromise() {
    let tempResolve, tempReject;
    let promise = new PromiseBase(function(resolve, reject) {
      tempResolve = resolve;
      tempReject = reject;
    });
    promise[_promiseResolve] = tempResolve;
    promise[_promiseReject] = tempReject;
    return promise;
  }

  function v8_isPromise(obj) {
    return obj instanceof PromiseBase;
  }

  function v8_resolvePromise(promise, value) {
    promise[_promiseResolve](value);
  }

  function v8_rejectPromise(promise, reason) {
    promise[_promiseReject](reason);
  }

  function v8_markPromiseAsHandled(promise) {
    try {
      thenPromise(promise, undefined, () => {});
    } catch(error) {
    }
  }

  /* SimpleQueue.js: begin */
  // Simple queue structure. Avoids scalability issues with using
  // InternalPackedArray directly by using multiple arrays in a linked list and
  // keeping the array size bounded.
  const QUEUE_MAX_ARRAY_SIZE = 16384;
  function SimpleQueue() {
    this.front = {
      elements: new v8_InternalPackedArray(),
      next: undefined,
    };
    this.back = this.front;
    // The cursor is used to avoid calling InternalPackedArray.shift().
    this.cursor = 0;
    this.size = 0;
  }

  defineProperty(SimpleQueue.prototype, 'length', {
    get: function() { return this.size; }
  });

  SimpleQueue.prototype.push = function(element) {
    ++this.size;
    if (this.back.elements.length === QUEUE_MAX_ARRAY_SIZE) {
      const oldBack = this.back;
      this.back = {
        elements: new v8_InternalPackedArray(),
        next: undefined,
      };
      oldBack.next = this.back;
    }
    this.back.elements.push(element);
  }

  SimpleQueue.prototype.shift = function() {
    // assert(this.size > 0);
    --this.size;
    if (this.front.elements.length === this.cursor) {
      // assert(this.cursor === QUEUE_MAX_ARRAY_SIZE);
      // assert(this.front.next !== undefined);
      this.front = this.front.next;
      this.cursor = 0;
    }
    const element = this.front.elements[this.cursor];
    // Permit shifted element to be garbage collected.
    this.front.elements[this.cursor] = undefined;
    ++this.cursor;

    return element;
  }

  SimpleQueue.prototype.forEach = function(callback) {
    let i = this.cursor;
    let node = this.front;
    let elements = node.elements;
    while (i !== elements.length || node.next !== undefined) {
      if (i === elements.length) {
        // assert(node.next !== undefined);
        // assert(i === QUEUE_MAX_ARRAY_SIZE);
        node = node.next;
        elements = node.elements;
        i = 0;
      }
      callback(elements[i]);
      ++i;
    }
  }

  // Return the element that would be returned if shift() was called now,
  // without modifying the queue.
  SimpleQueue.prototype.peek = function() {
    // assert(this.size > 0);
    if (this.front.elements.length === this.cursor) {
      // assert(this.cursor === QUEUE_MAX_ARRAY_SIZE)
      // assert(this.front.next !== undefined);
      return this.front.next.elements[0];
    }
    return this.front.elements[this.cursor];
  }
  /* SimpleQueue.js: end */

  /* CommonStrings.js: begin */
  const streamErrors_illegalInvocation = 'Illegal invocation';
  const streamErrors_illegalConstructor = 'Illegal constructor';
  const streamErrors_invalidType = 'Invalid type is specified';
  const streamErrors_invalidSize = 'The return value of a queuing strategy\'s size function must be a finite, non-NaN, non-negative number';
  const streamErrors_sizeNotAFunction = 'A queuing strategy\'s size property must be a function';
  const streamErrors_invalidHWM = 'A queueing strategy\'s highWaterMark property must be a nonnegative, non-NaN number';
  /* CommonStrings.js: end */

  const _reader = v8_createPrivateSymbol('[[reader]]');
  const _storedError = v8_createPrivateSymbol('[[storedError]]');
  const _controller = v8_createPrivateSymbol('[[controller]]');

  const _closedPromise = v8_createPrivateSymbol('[[closedPromise]]');
  const _ownerReadableStream =
      v8_createPrivateSymbol('[[ownerReadableStream]]');

  const _readRequests = v8_createPrivateSymbol('[[readRequests]]');

  const _readableStreamBits = v8_createPrivateSymbol('bit field for [[state]] and [[disturbed]]');
  const DISTURBED = 0b1;
  // The 2nd and 3rd bit are for [[state]].
  const STATE_MASK = 0b110;
  const STATE_BITS_OFFSET = 1;
  const STATE_READABLE = 0;
  const STATE_CLOSED = 1;
  const STATE_ERRORED = 2;

  const _underlyingSource = v8_createPrivateSymbol('[[underlyingSource]]');
  const _controlledReadableStream =
      v8_createPrivateSymbol('[[controlledReadableStream]]');
  const _queue = v8_createPrivateSymbol('[[queue]]');
  const _totalQueuedSize = v8_createPrivateSymbol('[[totalQueuedSize]]');
  const _strategySize = v8_createPrivateSymbol('[[strategySize]]');
  const _strategyHWM = v8_createPrivateSymbol('[[strategyHWM]]');

  const _readableStreamDefaultControllerBits = v8_createPrivateSymbol(
      'bit field for [[started]], [[closeRequested]], [[pulling]], [[pullAgain]]');
  const STARTED = 0b1;
  const CLOSE_REQUESTED = 0b10;
  const PULLING = 0b100;
  const PULL_AGAIN = 0b1000;

  const undefined = global.undefined;
  const Infinity = global.Infinity;

  const hasOwnProperty = v8_uncurryThis(global.Object.hasOwnProperty);
  const callFunction = v8_uncurryThis(global.Function.prototype.call);
  const applyFunction = v8_uncurryThis(global.Function.prototype.apply);

  const TypeError = global.TypeError;
  const RangeError = global.RangeError;

  const Number = global.Number;
  const Number_isNaN = Number.isNaN;
  const Number_isFinite = Number.isFinite;

  const Promise = global.Promise;
  const thenPromise = v8_uncurryThis(Promise.prototype.then);
  const Promise_resolve = v8_simpleBind(Promise.resolve, Promise);
  const Promise_reject = v8_simpleBind(Promise.reject, Promise);

  const errCancelLockedStream =
      'Cannot cancel a readable stream that is locked to a reader';
  const errEnqueueCloseRequestedStream =
      'Cannot enqueue a chunk into a readable stream that is closed or has been requested to be closed';
  const errCancelReleasedReader =
      'This readable stream reader has been released and cannot be used to cancel its previous owner stream';
  const errReadReleasedReader =
      'This readable stream reader has been released and cannot be used to read from its previous owner stream';
  const errCloseCloseRequestedStream =
      'Cannot close a readable stream that has already been requested to be closed';
  const errEnqueueClosedStream = 'Cannot enqueue a chunk into a closed readable stream';
  const errEnqueueErroredStream = 'Cannot enqueue a chunk into an errored readable stream';
  const errCloseClosedStream = 'Cannot close a closed readable stream';
  const errCloseErroredStream = 'Cannot close an errored readable stream';
  const errErrorClosedStream = 'Cannot error a close readable stream';
  const errErrorErroredStream =
      'Cannot error a readable stream that is already errored';
  const errGetReaderNotByteStream = 'This readable stream does not support BYOB readers';
  const errGetReaderBadMode = 'Invalid reader mode given: expected undefined or "byob"';
  const errReaderConstructorBadArgument =
      'ReadableStreamReader constructor argument is not a readable stream';
  const errReaderConstructorStreamAlreadyLocked =
      'ReadableStreamReader constructor can only accept readable streams that are not yet locked to a reader';
  const errReleaseReaderWithPendingRead =
      'Cannot release a readable stream reader when it still has outstanding read() calls that have not yet settled';
  const errReleasedReaderClosedPromise =
      'This readable stream reader has been released and cannot be used to monitor the stream\'s state';

  const errTmplMustBeFunctionOrUndefined = name =>
      `${name} must be a function or undefined`;

  function ReadableStream() {
    // TODO(domenic): when V8 gets default parameters and destructuring, all
    // this can be cleaned up.
    const underlyingSource = arguments[0] === undefined ? {} : arguments[0];
    const strategy = arguments[1] === undefined ? {} : arguments[1];
    const size = strategy.size;
    let highWaterMark = strategy.highWaterMark;
    if (highWaterMark === undefined) {
      highWaterMark = 1;
    }

    this[_readableStreamBits] = 0b0;
    ReadableStreamSetState(this, STATE_READABLE);
    this[_reader] = undefined;
    this[_storedError] = undefined;

    // Avoid allocating the controller if the stream is going to be controlled
    // externally (i.e. from C++) anyway. All calls to underlyingSource
    // methods will disregard their controller argument in such situations
    // (but see below).

    this[_controller] = undefined;

    const type = underlyingSource.type;
    const typeString = String(type);
    if (typeString === 'bytes') {
      throw new RangeError('bytes type is not yet implemented');
    } else if (type !== undefined) {
      throw new RangeError(streamErrors_invalidType);
    }

    this[_controller] =
        new ReadableStreamDefaultController(this, underlyingSource, size, highWaterMark, arguments[2]);
  }

  defineProperty(ReadableStream.prototype, 'locked', {
    enumerable: false,
    configurable: true,
    get: function() {
      if (IsReadableStream(this) === false) {
        throw new TypeError(streamErrors_illegalInvocation);
      }

      return IsReadableStreamLocked(this);
    }
  });

  defineProperty(ReadableStream.prototype, 'cancel', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function(reason) {
      if (IsReadableStream(this) === false) {
        return Promise_reject(new TypeError(streamErrors_illegalInvocation));
      }

      if (IsReadableStreamLocked(this) === true) {
        return Promise_reject(new TypeError(errCancelLockedStream));
      }

      return ReadableStreamCancel(this, reason);
    }
  });

  defineProperty(ReadableStream.prototype, 'getReader', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function({ mode } = {}) {
      if (IsReadableStream(this) === false) {
        throw new TypeError(streamErrors_illegalInvocation);
      }

      if (mode === 'byob') {
        // TODO(ricea): When BYOB readers are supported:
        //
        // a. If
        // ! IsReadableByteStreamController(this.[[_controller]])
        // is false, throw a TypeError exception.
        // b. Return ? AcquireReadableStreamBYOBReader(this).
        throw new TypeError(errGetReaderNotByteStream);
      }

      if (mode === undefined) {
        return AcquireReadableStreamDefaultReader(this);
      }

      throw new RangeError(errGetReaderBadMode);
    }
  });

  defineProperty(ReadableStream.prototype, 'pipeThrough', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function({writable, readable}, options) {
      throw new TypeError('pipeThrough not implemented');
    }
  });

  defineProperty(ReadableStream.prototype, 'pipeTo', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function(dest) {
      throw new TypeError('pipeTo not implemented');
    }
  });

  defineProperty(ReadableStream.prototype, 'tee', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function() {
      if (IsReadableStream(this) === false) {
        throw new TypeError(streamErrors_illegalInvocation);
      }

      return ReadableStreamTee(this);
    }
  });

  function ReadableStreamDefaultController(stream, underlyingSource, size, highWaterMark) {
    if (IsReadableStream(stream) === false) {
      throw new TypeError(streamErrors_illegalConstructor);
    }

    if (stream[_controller] !== undefined) {
      throw new TypeError(streamErrors_illegalConstructor);
    }

    this[_controlledReadableStream] = stream;

    this[_underlyingSource] = underlyingSource;

    this[_queue] = new SimpleQueue();
    this[_totalQueuedSize] = 0;

    this[_readableStreamDefaultControllerBits] = 0b0;

    const normalizedStrategy =
        ValidateAndNormalizeQueuingStrategy(size, highWaterMark);
    this[_strategySize] = normalizedStrategy.size;
    this[_strategyHWM] = normalizedStrategy.highWaterMark;

    const controller = this;

    const startResult = CallOrNoop(
        underlyingSource, 'start', this, 'underlyingSource.start');
    thenPromise(Promise_resolve(startResult),
        () => {
          controller[_readableStreamDefaultControllerBits] |= STARTED;
          ReadableStreamDefaultControllerCallPullIfNeeded(controller);
        },
        r => {
          if (ReadableStreamGetState(stream) === STATE_READABLE) {
            ReadableStreamDefaultControllerError(controller, r);
          }
        });
  }

  defineProperty(ReadableStreamDefaultController.prototype, 'desiredSize', {
    enumerable: false,
    configurable: true,
    get: function() {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors_illegalInvocation);
      }

      return ReadableStreamDefaultControllerGetDesiredSize(this);
    }
  });

  defineProperty(ReadableStreamDefaultController.prototype, 'close', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function() {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors_illegalInvocation);
      }

      const stream = this[_controlledReadableStream];

      if (this[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) {
        throw new TypeError(errCloseCloseRequestedStream);
      }

      const state = ReadableStreamGetState(stream);
      if (state === STATE_ERRORED) {
        throw new TypeError(errCloseErroredStream);
      }
      if (state === STATE_CLOSED) {
        throw new TypeError(errCloseClosedStream);
      }

      return ReadableStreamDefaultControllerClose(this);
    }
  });

  defineProperty(ReadableStreamDefaultController.prototype, 'enqueue', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function(chunk) {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors_illegalInvocation);
      }

      const stream = this[_controlledReadableStream];

      if (this[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) {
        throw new TypeError(errEnqueueCloseRequestedStream);
      }

      const state = ReadableStreamGetState(stream);
      if (state === STATE_ERRORED) {
        throw new TypeError(errEnqueueErroredStream);
      }
      if (state === STATE_CLOSED) {
        throw new TypeError(errEnqueueClosedStream);
      }

      return ReadableStreamDefaultControllerEnqueue(this, chunk);
    }
  });

  defineProperty(ReadableStreamDefaultController.prototype, 'error', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function(e) {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors_illegalInvocation);
      }

      const stream = this[_controlledReadableStream];

      const state = ReadableStreamGetState(stream);
      if (state === STATE_ERRORED) {
        throw new TypeError(errErrorErroredStream);
      }
      if (state === STATE_CLOSED) {
        throw new TypeError(errErrorClosedStream);
      }

      return ReadableStreamDefaultControllerError(this, e);
    }
  });

  function ReadableStreamDefaultControllerCancel(controller, reason) {
    controller[_queue] = new SimpleQueue();

    const underlyingSource = controller[_underlyingSource];
    return PromiseCallOrNoop(underlyingSource, 'cancel', reason, 'underlyingSource.cancel');
  }

  function ReadableStreamDefaultControllerPull(controller) {
    const stream = controller[_controlledReadableStream];

    if (controller[_queue].length > 0) {
      const chunk = DequeueValue(controller);

      if ((controller[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) &&
          controller[_queue].length === 0) {
        ReadableStreamClose(stream);
      } else {
        ReadableStreamDefaultControllerCallPullIfNeeded(controller);
      }

      return Promise_resolve(CreateIterResultObject(chunk, false));
    }

    const pendingPromise = ReadableStreamAddReadRequest(stream);
    ReadableStreamDefaultControllerCallPullIfNeeded(controller);
    return pendingPromise;
  }

  function ReadableStreamAddReadRequest(stream) {
    const promise = v8_createPromise();
    stream[_reader][_readRequests].push(promise);
    return promise;
  }

  function ReadableStreamDefaultReader(stream) {
    if (IsReadableStream(stream) === false) {
      throw new TypeError(errReaderConstructorBadArgument);
    }
    if (IsReadableStreamLocked(stream) === true) {
      throw new TypeError(errReaderConstructorStreamAlreadyLocked);
    }

    ReadableStreamReaderGenericInitialize(this, stream);

    this[_readRequests] = new SimpleQueue();
  }

  defineProperty(ReadableStreamDefaultReader.prototype, 'closed', {
    enumerable: false,
    configurable: true,
    get: function() {
      if (IsReadableStreamDefaultReader(this) === false) {
        return Promise_reject(new TypeError(streamErrors_illegalInvocation));
      }

      return this[_closedPromise];
    }
  });

  defineProperty(ReadableStreamDefaultReader.prototype, 'cancel', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function(reason) {
      if (IsReadableStreamDefaultReader(this) === false) {
        return Promise_reject(new TypeError(streamErrors_illegalInvocation));
      }

      const stream = this[_ownerReadableStream];
      if (stream === undefined) {
        return Promise_reject(new TypeError(errCancelReleasedReader));
      }

      return ReadableStreamReaderGenericCancel(this, reason);
    }
  });

  defineProperty(ReadableStreamDefaultReader.prototype, 'read', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function() {
      if (IsReadableStreamDefaultReader(this) === false) {
        return Promise_reject(new TypeError(streamErrors_illegalInvocation));
      }

      if (this[_ownerReadableStream] === undefined) {
        return Promise_reject(new TypeError(errReadReleasedReader));
      }

      return ReadableStreamDefaultReaderRead(this);
    }
  });

  defineProperty(ReadableStreamDefaultReader.prototype, 'releaseLock', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function() {
      if (IsReadableStreamDefaultReader(this) === false) {
        throw new TypeError(streamErrors_illegalInvocation);
      }

      const stream = this[_ownerReadableStream];
      if (stream === undefined) {
        return undefined;
      }

      if (this[_readRequests].length > 0) {
        throw new TypeError(errReleaseReaderWithPendingRead);
      }

      ReadableStreamReaderGenericRelease(this);
    }
  });

  function ReadableStreamReaderGenericCancel(reader, reason) {
    return ReadableStreamCancel(reader[_ownerReadableStream], reason);
  }

  //
  // Readable stream abstract operations
  //

  function AcquireReadableStreamDefaultReader(stream) {
    return new ReadableStreamDefaultReader(stream);
  }

  function ReadableStreamCancel(stream, reason) {
    stream[_readableStreamBits] |= DISTURBED;

    const state = ReadableStreamGetState(stream);
    if (state === STATE_CLOSED) {
      return Promise_resolve(undefined);
    }
    if (state === STATE_ERRORED) {
      return Promise_reject(stream[_storedError]);
    }

    ReadableStreamClose(stream);

    const sourceCancelPromise = ReadableStreamDefaultControllerCancel(stream[_controller], reason);
    return thenPromise(sourceCancelPromise, () => undefined);
  }

  function ReadableStreamDefaultControllerClose(controller) {
    const stream = controller[_controlledReadableStream];

    controller[_readableStreamDefaultControllerBits] |= CLOSE_REQUESTED;

    if (controller[_queue].length === 0) {
      ReadableStreamClose(stream);
    }
  }

  function ReadableStreamFulfillReadRequest(stream, chunk, done) {
    const reader = stream[_reader];

    const readRequest = stream[_reader][_readRequests].shift();
    v8_resolvePromise(readRequest, CreateIterResultObject(chunk, done));
  }

  function ReadableStreamDefaultControllerEnqueue(controller, chunk) {
    const stream = controller[_controlledReadableStream];

    if (IsReadableStreamLocked(stream) === true && ReadableStreamGetNumReadRequests(stream) > 0) {
      ReadableStreamFulfillReadRequest(stream, chunk, false);
    } else {
      let chunkSize = 1;

      const strategySize = controller[_strategySize];
      if (strategySize !== undefined) {
        try {
          chunkSize = strategySize(chunk);
        } catch (chunkSizeE) {
          if (ReadableStreamGetState(stream) === STATE_READABLE) {
            ReadableStreamDefaultControllerError(controller, chunkSizeE);
          }
          throw chunkSizeE;
        }
      }

      try {
        EnqueueValueWithSize(controller, chunk, chunkSize);
      } catch (enqueueE) {
        if (ReadableStreamGetState(stream) === STATE_READABLE) {
          ReadableStreamDefaultControllerError(controller, enqueueE);
        }
        throw enqueueE;
      }
    }

    ReadableStreamDefaultControllerCallPullIfNeeded(controller);
  }

  function ReadableStreamGetState(stream) {
    return (stream[_readableStreamBits] & STATE_MASK) >> STATE_BITS_OFFSET;
  }

  function ReadableStreamSetState(stream, state) {
    stream[_readableStreamBits] = (stream[_readableStreamBits] & ~STATE_MASK) |
        (state << STATE_BITS_OFFSET);
  }

  function ReadableStreamDefaultControllerError(controller, e) {
    controller[_queue] = new SimpleQueue();
    const stream = controller[_controlledReadableStream];
    ReadableStreamError(stream, e);
  }

  function ReadableStreamError(stream, e) {
    stream[_storedError] = e;
    ReadableStreamSetState(stream, STATE_ERRORED);

    const reader = stream[_reader];
    if (reader === undefined) {
      return undefined;
    }

    if (IsReadableStreamDefaultReader(reader) === true) {
      reader[_readRequests].forEach(request => v8_rejectPromise(request, e));
      reader[_readRequests] = new SimpleQueue();
    }

    v8_rejectPromise(reader[_closedPromise], e);
    v8_markPromiseAsHandled(reader[_closedPromise]);
  }

  function ReadableStreamClose(stream) {
    ReadableStreamSetState(stream, STATE_CLOSED);

    const reader = stream[_reader];
    if (reader === undefined) {
      return undefined;
    }

    if (IsReadableStreamDefaultReader(reader) === true) {
      reader[_readRequests].forEach(request =>
          v8_resolvePromise(request, CreateIterResultObject(undefined, true)));
      reader[_readRequests] = new SimpleQueue();
    }

    v8_resolvePromise(reader[_closedPromise], undefined);
  }

  function ReadableStreamDefaultControllerGetDesiredSize(controller) {
    // Cobalt: When the state is closed or errored, return particular values.
    const stream = controller[_controlledReadableStream];
    const state = ReadableStreamGetState(stream);
    if (state === STATE_CLOSED) {
      return 0;
    } else if (state === STATE_ERRORED) {
      return null;
    }

    const queueSize = GetTotalQueueSize(controller);
    return controller[_strategyHWM] - queueSize;
  }

  function IsReadableStream(x) {
    return hasOwnProperty(x, _controller);
  }

  function IsReadableStreamDisturbed(stream) {
    return stream[_readableStreamBits] & DISTURBED;
  }

  function IsReadableStreamLocked(stream) {
    return stream[_reader] !== undefined;
  }

  function IsReadableStreamDefaultController(x) {
    return hasOwnProperty(x, _controlledReadableStream);
  }

  function IsReadableStreamDefaultReader(x) {
    return hasOwnProperty(x, _readRequests);
  }

  function IsReadableStreamReadable(stream) {
    return ReadableStreamGetState(stream) === STATE_READABLE;
  }

  function IsReadableStreamClosed(stream) {
    return ReadableStreamGetState(stream) === STATE_CLOSED;
  }

  function IsReadableStreamErrored(stream) {
    return ReadableStreamGetState(stream) === STATE_ERRORED;
  }

  function ReadableStreamReaderGenericInitialize(reader, stream) {
    // TODO(yhirano): Remove this when we don't need hasPendingActivity in
    // blink::UnderlyingSourceBase.
    const controller = stream[_controller];

    reader[_ownerReadableStream] = stream;
    stream[_reader] = reader;

    switch (ReadableStreamGetState(stream)) {
      case STATE_READABLE:
        reader[_closedPromise] = v8_createPromise();
        break;
      case STATE_CLOSED:
        reader[_closedPromise] = Promise_resolve(undefined);
        break;
      case STATE_ERRORED:
        reader[_closedPromise] = Promise_reject(stream[_storedError]);
        v8_markPromiseAsHandled(reader[_closedPromise]);
        break;
    }
  }

  function ReadableStreamReaderGenericRelease(reader) {
    // TODO(yhirano): Remove this when we don't need hasPendingActivity in
    // blink::UnderlyingSourceBase.
    const controller = reader[_ownerReadableStream][_controller];

    if (ReadableStreamGetState(reader[_ownerReadableStream]) === STATE_READABLE) {
      v8_rejectPromise(reader[_closedPromise], new TypeError(errReleasedReaderClosedPromise));
    } else {
      reader[_closedPromise] = Promise_reject(new TypeError(errReleasedReaderClosedPromise));
    }
    v8_markPromiseAsHandled(reader[_closedPromise]);

    reader[_ownerReadableStream][_reader] = undefined;
    reader[_ownerReadableStream] = undefined;
  }

  function ReadableStreamDefaultReaderRead(reader) {
    const stream = reader[_ownerReadableStream];
    stream[_readableStreamBits] |= DISTURBED;

    if (ReadableStreamGetState(stream) === STATE_CLOSED) {
      return Promise_resolve(CreateIterResultObject(undefined, true));
    }

    if (ReadableStreamGetState(stream) === STATE_ERRORED) {
      return Promise_reject(stream[_storedError]);
    }

    return ReadableStreamDefaultControllerPull(stream[_controller]);
  }

  function ReadableStreamDefaultControllerCallPullIfNeeded(controller) {
    const shouldPull = ReadableStreamDefaultControllerShouldCallPull(controller);
    if (shouldPull === false) {
      return undefined;
    }

    if (controller[_readableStreamDefaultControllerBits] & PULLING) {
      controller[_readableStreamDefaultControllerBits] |= PULL_AGAIN;
      return undefined;
    }

    controller[_readableStreamDefaultControllerBits] |= PULLING;

    const underlyingSource = controller[_underlyingSource];
    const pullPromise = PromiseCallOrNoop(
        underlyingSource, 'pull', controller, 'underlyingSource.pull');

    thenPromise(pullPromise,
        () => {
          controller[_readableStreamDefaultControllerBits] &= ~PULLING;

          if (controller[_readableStreamDefaultControllerBits] & PULL_AGAIN) {
            controller[_readableStreamDefaultControllerBits] &= ~PULL_AGAIN;
            ReadableStreamDefaultControllerCallPullIfNeeded(controller);
          }
        },
        e => {
          if (ReadableStreamGetState(controller[_controlledReadableStream]) === STATE_READABLE) {
            ReadableStreamDefaultControllerError(controller, e);
          }
        });
  }

  function ReadableStreamDefaultControllerShouldCallPull(controller) {
    const stream = controller[_controlledReadableStream];

    const state = ReadableStreamGetState(stream);
    if (state === STATE_CLOSED || state === STATE_ERRORED) {
      return false;
    }

    if (controller[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) {
      return false;
    }

    if (!(controller[_readableStreamDefaultControllerBits] & STARTED)) {
      return false;
    }

    if (IsReadableStreamLocked(stream) === true && ReadableStreamGetNumReadRequests(stream) > 0) {
      return true;
    }

    const desiredSize = ReadableStreamDefaultControllerGetDesiredSize(controller);
    if (desiredSize > 0) {
      return true;
    }

    return false;
  }

  function ReadableStreamGetNumReadRequests(stream) {
    const reader = stream[_reader];
    const readRequests = reader[_readRequests];
    return readRequests.length;
  }

  // Potential future optimization: use class instances for the underlying
  // sources, so that we don't re-create
  // closures every time.

  // TODO(domenic): shouldClone argument from spec not supported yet
  function ReadableStreamTee(stream) {
    const reader = AcquireReadableStreamDefaultReader(stream);

    let closedOrErrored = false;
    let canceled1 = false;
    let canceled2 = false;
    let reason1;
    let reason2;
    let promise = v8_createPromise();

    const branch1Stream = new ReadableStream({pull, cancel: cancel1});

    const branch2Stream = new ReadableStream({pull, cancel: cancel2});

    const branch1 = branch1Stream[_controller];
    const branch2 = branch2Stream[_controller];

    thenPromise(
        reader[_closedPromise], undefined, function(r) {
          if (closedOrErrored === true) {
            return;
          }

          ReadableStreamDefaultControllerError(branch1, r);
          ReadableStreamDefaultControllerError(branch2, r);
          closedOrErrored = true;
        });

    return [branch1Stream, branch2Stream];

    function pull() {
      return thenPromise(
          ReadableStreamDefaultReaderRead(reader), function(result) {
            const value = result.value;
            const done = result.done;

            if (done === true && closedOrErrored === false) {
              if (canceled1 === false) {
                ReadableStreamDefaultControllerClose(branch1);
              }
              if (canceled2 === false) {
                ReadableStreamDefaultControllerClose(branch2);
              }
              closedOrErrored = true;
            }

            if (closedOrErrored === true) {
              return;
            }

            if (canceled1 === false) {
              ReadableStreamDefaultControllerEnqueue(branch1, value);
            }

            if (canceled2 === false) {
              ReadableStreamDefaultControllerEnqueue(branch2, value);
            }
          });
    }

    function cancel1(reason) {
      canceled1 = true;
      reason1 = reason;

      if (canceled2 === true) {
        const compositeReason = [reason1, reason2];
        const cancelResult = ReadableStreamCancel(stream, compositeReason);
        v8_resolvePromise(promise, cancelResult);
      }

      return promise;
    }

    function cancel2(reason) {
      canceled2 = true;
      reason2 = reason;

      if (canceled1 === true) {
        const compositeReason = [reason1, reason2];
        const cancelResult = ReadableStreamCancel(stream, compositeReason);
        v8_resolvePromise(promise, cancelResult);
      }

      return promise;
    }
  }

  //
  // Queue-with-sizes
  //

  function DequeueValue(controller) {
    const result = controller[_queue].shift();
    controller[_totalQueuedSize] -= result.size;
    return result.value;
  }

  function EnqueueValueWithSize(controller, value, size) {
    size = Number(size);
    if (Number_isNaN(size) || size === +Infinity || size < 0) {
      throw new RangeError(streamErrors_invalidSize);
    }

    controller[_totalQueuedSize] += size;
    controller[_queue].push({value, size});
  }

  function GetTotalQueueSize(controller) { return controller[_totalQueuedSize]; }

  //
  // Other helpers
  //

  function ValidateAndNormalizeQueuingStrategy(size, highWaterMark) {
    if (size !== undefined && typeof size !== 'function') {
      throw new TypeError(streamErrors_sizeNotAFunction);
    }

    highWaterMark = Number(highWaterMark);
    if (Number_isNaN(highWaterMark)) {
      throw new RangeError(streamErrors_invalidHWM);
    }
    if (highWaterMark < 0) {
      throw new RangeError(streamErrors_invalidHWM);
    }

    return {size, highWaterMark};
  }

  // Modified from InvokeOrNoop in spec
  function CallOrNoop(O, P, arg, nameForError) {
    const method = O[P];
    if (method === undefined) {
      return undefined;
    }
    if (typeof method !== 'function') {
      throw new TypeError(errTmplMustBeFunctionOrUndefined(nameForError));
    }

    return callFunction(method, O, arg);
  }


  // Modified from PromiseInvokeOrNoop in spec
  function PromiseCallOrNoop(O, P, arg, nameForError) {
    let method;
    try {
      method = O[P];
    } catch (methodE) {
      return Promise_reject(methodE);
    }

    if (method === undefined) {
      return Promise_resolve(undefined);
    }

    if (typeof method !== 'function') {
      return Promise_reject(new TypeError(errTmplMustBeFunctionOrUndefined(nameForError)));
    }

    try {
      return Promise_resolve(callFunction(method, O, arg));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  function CreateIterResultObject(value, done) { return {value, done}; }


  //
  // Additions to the global
  //

  defineProperty(global, 'ReadableStream', {
    value: ReadableStream,
    enumerable: false,
    configurable: true,
    writable: true
  });

  //
  // Exports to Blink
  //

  global.AcquireReadableStreamDefaultReader = AcquireReadableStreamDefaultReader;
  global.IsReadableStream = IsReadableStream;
  global.IsReadableStreamDisturbed = IsReadableStreamDisturbed;
  global.IsReadableStreamLocked = IsReadableStreamLocked;
  global.IsReadableStreamReadable = IsReadableStreamReadable;
  global.IsReadableStreamClosed = IsReadableStreamClosed;
  global.IsReadableStreamErrored = IsReadableStreamErrored;
  global.IsReadableStreamDefaultReader = IsReadableStreamDefaultReader;
  global.ReadableStreamDefaultReaderRead = ReadableStreamDefaultReaderRead;
  global.ReadableStreamTee = ReadableStreamTee;

  global.ReadableStreamDefaultControllerClose = ReadableStreamDefaultControllerClose;
  global.ReadableStreamDefaultControllerGetDesiredSize = ReadableStreamDefaultControllerGetDesiredSize;
  global.ReadableStreamDefaultControllerEnqueue = ReadableStreamDefaultControllerEnqueue;
  global.ReadableStreamDefaultControllerError = ReadableStreamDefaultControllerError;
})(this);
