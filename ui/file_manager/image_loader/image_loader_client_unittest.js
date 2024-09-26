// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {reportPromise} from 'chrome://file-manager/common/js/test_error_reporting.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {ImageLoaderClient} from './image_loader_client.js';
import {LoadImageRequest, LoadImageResponse, LoadImageResponseStatus} from './load_image_request.js';

/** @suppress {const|checkTypes} */
export function setUp() {
  chrome.metricsPrivate = {
    MetricTypeType:
        {HISTOGRAM_LOG: 'histogram-log', HISTOGRAM_LINEAR: 'histogram-linear'},
    recordPercentage: function() {},
    recordValue: function() {},
  };

  chrome.i18n = {
    getMessage: function() {},
  };
}

/**
 * Lets the client to load URL and returns the local cache (not caches in the
 * image loader extension) is used or not.
 *
 * @param {ImageLoaderClient} client
 * @param {string} url URL
 * @param {boolean} cache Whether to request caching on the request.
 * @return {Promise<boolean>} True if the local cache is used.
 */
function loadAndCheckCacheUsed(client, url, cache) {
  let cacheUsed = true;

  /** @suppress {accessControls} */
  ImageLoaderClient.sendMessage_ = function(message, callback) {
    cacheUsed = false;
    if (callback) {
      callback(new LoadImageResponse(
          LoadImageResponseStatus.SUCCESS, message.taskId || -1,
          {width: 100, height: 100, ifd: null, data: 'ImageData'}));
    }
  };

  const request = LoadImageRequest.createForUrl(url);
  request.cache = cache;

  return new Promise(function(fulfill) {
    client.load(request, function() {
      fulfill(cacheUsed);
    });
  });
}

export function testCache(callback) {
  const client = new ImageLoaderClient();
  reportPromise(
      loadAndCheckCacheUsed(client, 'http://example.com/image.jpg', true)
          .then(function(cacheUsed) {
            assertFalse(cacheUsed);
            return loadAndCheckCacheUsed(
                client, 'http://example.com/image.jpg', true);
          })
          .then(function(cacheUsed) {
            assertTrue(cacheUsed);
          }),
      callback);
}

export function testNoCache(callback) {
  const client = new ImageLoaderClient();
  reportPromise(
      loadAndCheckCacheUsed(client, 'http://example.com/image.jpg', false)
          .then(function(cacheUsed) {
            assertFalse(cacheUsed);
            return loadAndCheckCacheUsed(
                client, 'http://example.com/image.jpg', false);
          })
          .then(function(cacheUsed) {
            assertFalse(cacheUsed);
          }),
      callback);
}

export function testDataURLCache(callback) {
  const client = new ImageLoaderClient();
  reportPromise(
      loadAndCheckCacheUsed(client, 'data:URI', true)
          .then(function(cacheUsed) {
            assertFalse(cacheUsed);
            return loadAndCheckCacheUsed(client, 'data:URI', true);
          })
          .then(function(cacheUsed) {
            assertFalse(cacheUsed);
          }),
      callback);
}
