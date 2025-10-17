// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const exceptionHandler = require('uncaught_exception_handler');
const imageUtil = require('imageUtil');
const SetIconCommon = requireNative('setIcon').SetIconCommon;
const inServiceWorker = requireNative('utils').isInServiceWorker();

function loadImagePathForServiceWorker(path, callback, failureCallback) {
  let fetchPromise = fetch(path);

  let blobPromise = $Promise.then(fetchPromise, (response) => {
    if (!response.ok) {
      // This error is caught below.
      throw $Error.self('Response from fetching icon not ok.');
    }
    return response.blob();
  });

  let imagePromise = $Promise.then(blobPromise, (blob) => {
    return createImageBitmap(blob);
  });

  let imageDataPromise = $Promise.then(imagePromise, (image) => {
    var canvas = new OffscreenCanvas(image.width, image.height);
    var canvasContext = canvas.getContext('2d');
    canvasContext.clearRect(0, 0, canvas.width, canvas.height);
    canvasContext.drawImage(image, 0, 0, canvas.width, canvas.height);
    var imageData = canvasContext.getImageData(0, 0, canvas.width,
                                               canvas.height);
    callback(imageData);
  });

  $Promise.catch(imageDataPromise, function(error) {
    var message = `Failed to set icon '${path}': ` +
        exceptionHandler.safeErrorToString(error, true);
    failureCallback(message);
  });
}

function loadImagePathForNonServiceWorker(path, callback, failureCallback) {
  var img = new Image();
  img.onerror = function() {
    var message = 'Could not load action icon \'' + path + '\'.';
    console.error(message);
    failureCallback(message);
  };
  img.onload = function() {
    var canvas = document.createElement('canvas');
    canvas.width = img.width;
    canvas.height = img.height
    var canvasContext = canvas.getContext('2d');
    canvasContext.clearRect(0, 0, canvas.width, canvas.height);
    canvasContext.drawImage(img, 0, 0, canvas.width, canvas.height);
    var imageData = canvasContext.getImageData(0, 0, canvas.width,
                                               canvas.height);
    callback(imageData);
  };
  img.src = path;
}

function loadImagePath(path, callback, failureCallback) {
  if (inServiceWorker) {
    loadImagePathForServiceWorker(path, callback, failureCallback);
  } else {
    loadImagePathForNonServiceWorker(path, callback, failureCallback);
  }
}

/**
 * Normalizes |details| to a format suitable for sending to the browser,
 * for example converting ImageData to a binary representation.
 *
 * @param {ImageDetails} details
 *   The ImageDetails passed into an extension action-style API.
 * @param {Function} callback
 *   The callback function to pass processed imageData back to. Note that this
 *   callback may be called reentrantly.
 * @param {Function} failureCallback
 *   The callback function to be called in case of an error.
 */
function setIcon(details, callback, failureCallback) {
  // NOTE: |details| should already have gone through API argument validation,
  // and, as part of that, will be a null-proto'd object. As such, it's safer
  // to directly access and manipulate fields.

  // Note that iconIndex is actually deprecated, and only available to the
  // pageAction API.
  // TODO(kalman): Investigate whether this is for the pageActions API, and if
  // so, delete it.
  if ('iconIndex' in details) {
    callback(details);
    return;
  }

  if ('imageData' in details) {
    if (imageUtil.smellsLikeImageData(details.imageData)) {
      var imageData = details.imageData;
      details.imageData = {__proto__: null};
      details.imageData[imageData.width.toString()] = imageData;
    } else if (typeof details.imageData == 'object' &&
               Object.getOwnPropertyNames(details.imageData).length !== 0) {
      for (var sizeKey in details.imageData) {
        imageUtil.verifyImageData(details.imageData[sizeKey]);
      }
    } else {
      imageUtil.verifyImageData(false);
    }

    callback(SetIconCommon(details));
    return;
  }

  if ('path' in details) {
    if (typeof details.path == 'object') {
      details.imageData = {__proto__: null};
      var detailKeyCount = 0;
      for (var iconSize in details.path) {
        ++detailKeyCount;
        loadImagePath(
            details.path[iconSize],
            function(size, imageData) {
              details.imageData[size] = imageData;
              if (--detailKeyCount == 0) {
                callback(SetIconCommon(details));
              }
            }.bind(null, iconSize),
            function(errorMessage) {
              if (failureCallback) {
                failureCallback(errorMessage);
                // Only report the first error.
                failureCallback = null;
              }
            });
      }
      if (detailKeyCount == 0)
        throw new Error('The path property must not be empty.');
    } else if (typeof details.path == 'string') {
      details.imageData = {__proto__: null};
      loadImagePath(details.path, function(imageData) {
        details.imageData[imageData.width.toString()] = imageData;
        delete details.path;
        callback(SetIconCommon(details));
      }, failureCallback);
    }
    return;
  }
  throw new Error('Either the path or imageData property must be specified.');
}

// Returns a common handler function used by several extension APIs when setting
// the extension icon.
function getSetIconHandler(methodName) {
  return function(details, successCallback, failureCallback) {
    var onIconRetrieved = function(iconSpec) {
      bindingUtil.sendRequest(
          methodName, [iconSpec, successCallback],
          /*options=*/ undefined);
    };
    setIcon(details, onIconRetrieved, failureCallback);
  };
}

// TODO(crbug.com/41159896): The setIcon export is only used by the declarative
// content custom bindings and it actually has some major problems with how it
// uses it. When that is resolved we can likely remove this export.
exports.$set('setIcon', setIcon);
exports.$set('getSetIconHandler', getSetIconHandler);
