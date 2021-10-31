// ==ClosureCompiler==
// @output_file_name count_queuing_strategy.js
// @compilation_level SIMPLE_OPTIMIZATIONS
// @language_out ES5_STRICT
// ==/ClosureCompiler==

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global) {
  'use strict';

  const defineProperty = global.Object.defineProperty;

  class CountQueuingStrategy {
    constructor(options) {
      defineProperty(this, 'highWaterMark', {
        value: options.highWaterMark,
        enumerable: true,
        configurable: true,
        writable: true
      });
    }

    size(chunk) { return 1; }
  }

  defineProperty(global, 'CountQueuingStrategy', {
    value: CountQueuingStrategy,
    enumerable: false,
    configurable: true,
    writable: true
  });
})(this);
