// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var googleResponseReceived = false;
var googleRequestSent = false;
var nonGoogleResponseReceived = false;
var nonGoogleRequestSent = false;

function initGlobals() {
  googleResponseReceived = false;
  googleRequestSent = false;
  nonGoogleResponseReceived = false;
  nonGoogleRequestSent = false;
}

// Starts XHR requests - one for google.com and one later for non-google.
function startXHRRequests(googlePageUrl, googlePageCheckCallback,
                          nonGooglePageUrl, nonGooglePageCheckCallback,
                          async, should_throttle) {
  // Kick off google XHR first.
  var xhr = new XMLHttpRequest();

  var validateResponse = function() {
    if (xhr.status == 200 &&
        xhr.responseText.indexOf('Hello Google') != -1) {
      googleResponseReceived = true;
      googlePageCheckCallback();
    }
  };

  xhr.onreadystatechange = function() {
    console.warn("xhr.onreadystatechange: " + xhr.readyState);
    switch (xhr.readyState) {
      case XMLHttpRequest.OPENED:
        startNonGoogleXHRRequests(nonGooglePageUrl, nonGooglePageCheckCallback,
                                  async, should_throttle);
        break;
      case XMLHttpRequest.DONE:
        validateResponse();
        break;
    }
  };
  chrome.test.sendMessage("opening " + googlePageUrl);
  xhr.open("GET", googlePageUrl, async);
  xhr.send();
  googleRequestSent = true;
  if (!async) {
    validateResponse();
  }
}

function startNonGoogleXHRRequests(nonGooglePageUrl,
                                   nonGooglePageCheckCallback,
                                   async, should_throttle) {
  // Kick off non-google XHR next.
  var xhr = new XMLHttpRequest();

  var validateResponse = function() {
    if (xhr.status == 200 &&
        xhr.responseText.indexOf('SomethingElse') != -1) {
      chrome.test.sendMessage('non-google-xhr-received');
      nonGoogleResponseReceived = true;
      nonGooglePageCheckCallback();
    }
  };

  xhr.onreadystatechange = function() {
    console.warn("xhr.onreadystatechange: " + xhr.readyState);
    chrome.test.sendMessage("xhr.onreadystatechange: " + xhr.readyState);
    switch (xhr.readyState) {
      case XMLHttpRequest.OPENED:
        chrome.test.sendMessage("Both XHR's Opened");
        break;
      case XMLHttpRequest.DONE:
        validateResponse();
        break;
    }
  };
  xhr.open("GET", nonGooglePageUrl, async);
  xhr.send();
  if (!async) {
    validateResponse();
  }
}

function googlePageCheck() {
  // Responses may be reordered.
  if (nonGoogleResponseReceived)
    chrome.test.succeed();
  else
    console.info('non-Google response still pending');
}

function nonGooglePageCheck() {
  // Responses may be reordered.
  if (googleResponseReceived)
    chrome.test.succeed();
  else
    console.info('Google response still pending');
}

// Performs test that will verify if XHR request had completed prematurely.
function startThrottledTests(googlePageUrl, nonGooglePageUrl, async,
                             should_throttle) {
  chrome.test.runTests([function testXHRThrottle() {
    initGlobals();
    startXHRRequests(googlePageUrl, googlePageCheck,
                     nonGooglePageUrl, nonGooglePageCheck, async,
                     should_throttle);
  }]);
  return true;
}
