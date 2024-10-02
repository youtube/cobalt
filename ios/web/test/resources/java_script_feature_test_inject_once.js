// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Setup functions used in JavaScriptFeature inttests. This file
 * will be executed once for a given |window| JS object.
 */

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.javaScriptFeatureTest = {};

// Store namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['javaScriptFeatureTest'] = __gCrWeb.javaScriptFeatureTest;

// A counter which is incremented on error.
__gCrWeb.javaScriptFeatureTest.errorReceivedCount = 0;

__gCrWeb.javaScriptFeatureTest.getErrorCount = function() {
  return __gCrWeb.javaScriptFeatureTest.errorReceivedCount;
}

__gCrWeb.javaScriptFeatureTest.replaceDivContents = function() {
  document.getElementById('div').innerHTML = 'updated';
};

__gCrWeb.javaScriptFeatureTest.replyWithPostMessage = function(messageBody) {
  sendWebKitMessage('FakeHandlerName', messageBody);
};

// This function offers the same functionality as `replyWithPostMessage`, but
// uses the sendWebKitMessage implementation in __gCrWeb.common.
__gCrWeb.javaScriptFeatureTest.replyWithPostMessageCommonHelper = function(
    messageBody) {
  __gCrWeb.common.sendWebKitMessage('FakeHandlerName', messageBody);
};

document.getElementsByTagName('body')[0].appendChild(
    document.createTextNode('injected_script_loaded'));
