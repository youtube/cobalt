// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {ImageCache} from './cache.js';
import {ImageOrientation} from './image_orientation.js';
import {ImageRequestTask} from './image_request_task.js';
import {LoadImageRequest, LoadImageResponse} from './load_image_request.js';
import {Scheduler} from './scheduler.js';

/**
 * Loads and resizes an image.
 * @constructor
 */
export function ImageLoader() {
  /**
   * Persistent cache object.
   * @type {ImageCache}
   * @private
   */
  this.cache_ = new ImageCache();

  /**
   * Manages pending requests and runs them in order of priorities.
   * @type {Scheduler}
   * @private
   */
  this.scheduler_ = new Scheduler();

  // Grant permissions to all volumes, initialize the cache and then start the
  // scheduler.
  chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    // Listen for mount events, and grant permissions to volumes being mounted.
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        function(event) {
          if (event.eventType === 'mount' && event.status === 'success') {
            chrome.fileSystem.requestFileSystem(
                {volumeId: event.volumeMetadata.volumeId}, function() {});
          }
        });
    const initPromises = volumeMetadataList.map(function(volumeMetadata) {
      const requestPromise = new Promise(function(callback) {
        chrome.fileSystem.requestFileSystem(
            {volumeId: volumeMetadata.volumeId},
            /** @type {function(FileSystem=)} */(callback));
      });
      return requestPromise;
    });
    initPromises.push(new Promise(function(resolve, reject) {
      this.cache_.initialize(resolve);
    }.bind(this)));

    // After all initialization promises are done, start the scheduler.
    Promise.all(initPromises).then(this.scheduler_.start.bind(this.scheduler_));
  }.bind(this));

  // Listen for incoming requests.
  chrome.runtime.onMessageExternal.addListener((msg, sender, sendResponse) => {
    if (!sender.origin || !msg) {
      return;
    }

    if (ImageLoader.ALLOWED_CLIENT_ORIGINS.indexOf(sender.origin) === -1) {
      return;
    }

    this.onIncomingRequest_(msg, sender.origin, sendResponse);
  });

  chrome.runtime['onConnectNative'].addListener((port) => {
    if (port.sender.nativeApplication != 'com.google.ash_thumbnail_loader') {
      port.disconnect();
      return;
    }

    port.onMessage.addListener((msg) => {
      // Each connection is expected to handle a single request only.
      const started = this.onIncomingRequest_(
          msg, port.sender.nativeApplication, response => {
            port.postMessage(response);
            port.disconnect();
          });

      if (!started) {
        port.disconnect();
      }
    });
  });
}

/**
 * List of extensions allowed to perform image requests.
 *
 * @const
 * @type {Array<string>}
 */
ImageLoader.ALLOWED_CLIENT_ORIGINS = [
  'chrome://file-manager',  // File Manager SWA
];

/**
 * Handler for incoming requests.
 *
 * @param {*} request_data A LoadImageRequest (received untyped).
 * @param {!string} senderOrigin
 * @param {function(*): void} sendResponse
 */
ImageLoader.prototype.onIncomingRequest_ = function(
    request_data, senderOrigin, sendResponse) {
  const request = /** @type {!LoadImageRequest} */ (request_data);

  // Sending a response may fail if the receiver already went offline.
  // This is not an error, but a normal and quite common situation.
  const failSafeSendResponse = function(response) {
    try {
      sendResponse(response);
    } catch (e) {
      // Ignore the error.
    }
  };
  // Incoming requests won't have the full type.
  assert(!(request.orientation instanceof ImageOrientation));
  assert(!(typeof request.orientation === 'number'));

  if (request.orientation) {
    request.orientation =
        ImageOrientation.fromRotationAndScale(request.orientation);
  } else {
    request.orientation = new ImageOrientation(1, 0, 0, 1);
  }
  return this.onMessage_(senderOrigin, request, failSafeSendResponse);
};

/**
 * Handles a request. Depending on type of the request, starts or stops
 * an image task.
 *
 * @param {string} senderOrigin Sender's origin.
 * @param {!LoadImageRequest} request Pre-processed request.
 * @param {function(!LoadImageResponse)} callback Callback to be called to
 *     return response.
 * @return {boolean} True if the message channel should stay alive until the
 *     callback is called.
 * @private
 */
ImageLoader.prototype.onMessage_ = function(senderOrigin, request, callback) {
  const requestId = senderOrigin + ':' + request.taskId;
  if (request.cancel) {
    // Cancel a task.
    this.scheduler_.remove(requestId);
    return false;  // No callback calls.
  } else {
    // Create a request task and add it to the scheduler (queue).
    const requestTask =
        new ImageRequestTask(requestId, this.cache_, request, callback);
    this.scheduler_.add(requestTask);
    return true;  // Request will call the callback.
  }
};

/**
 * Returns the singleton instance.
 * @return {ImageLoader} ImageLoader object.
 */
ImageLoader.getInstance = function() {
  if (!ImageLoader.instance_) {
    ImageLoader.instance_ = new ImageLoader();
  }
  return ImageLoader.instance_;
};
