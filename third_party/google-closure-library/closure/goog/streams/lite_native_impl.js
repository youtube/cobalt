/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A lite polyfill of the ReadableStream native API with a subset
 * of methods supported that uses the native ReadableStream.
 */
goog.module('goog.streams.liteNativeImpl');

const liteTypes = goog.require('goog.streams.liteTypes');

/**
 * @template T
 * @implements {liteTypes.ReadableStream<T>}
 */
class NativeReadableStream {
  /**
   * @param {!ReadableStream} stream
   */
  constructor(stream) {
    /** @protected @const {!ReadableStream} */
    this.stream = stream;
  }

  /** @override */
  get locked() {
    return this.stream.locked;
  }

  /** @override */
  getReader() {
    return new NativeReadableStreamDefaultReader(
        /** @type {!ReadableStreamDefaultReader} */ (this.stream.getReader()));
  }
}

/**
 * @param {!liteTypes.ReadableStreamUnderlyingSource<T>} underlyingSource
 * @return {!NativeReadableStream<T>}
 * @suppress {strictMissingProperties}
 * @template T
 */
function newReadableStream(underlyingSource) {
  /** @const {!ReadableStreamSource} */
  const source = {
    start(controller) {
      return underlyingSource.start(
          new NativeReadableStreamDefaultController(controller));
    },
  };
  const stream = new ReadableStream(source);
  return new NativeReadableStream(stream);
}

/**
 * @template T
 * @implements {liteTypes.ReadableStreamDefaultReader<T>}
 */
class NativeReadableStreamDefaultReader {
  /**
   * @param {!ReadableStreamDefaultReader} reader
   */
  constructor(reader) {
    /** @protected @const {!ReadableStreamDefaultReader} */
    this.reader = reader;
  }

  /** @override */
  get closed() {
    return this.reader.closed;
  }

  /** @override */
  read() {
    return this.reader.read();
  }

  /** @override */
  releaseLock() {
    this.reader.releaseLock();
  }
}

/**
 * @template T
 * @implements {liteTypes.ReadableStreamDefaultController<T>}
 */
class NativeReadableStreamDefaultController {
  /**
   * @param {!ReadableStreamDefaultController} controller
   */
  constructor(controller) {
    /** @protected @const {!ReadableStreamDefaultController} */
    this.controller = controller;
  }

  /** @override */
  close() {
    this.controller.close();
  }

  /** @override */
  enqueue(chunk) {
    this.controller.enqueue(chunk);
  }

  /** @override */
  error(e) {
    this.controller.error(e);
  }
}

exports = {
  NativeReadableStream,
  NativeReadableStreamDefaultController,
  NativeReadableStreamDefaultReader,
  newReadableStream,
};
