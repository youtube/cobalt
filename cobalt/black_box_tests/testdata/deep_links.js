// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Fail if the deep link is not received within 15 seconds.
var kTimeout = 15 * 1000;
var failTimer = setTimeout(fail, kTimeout);
var linksReceived = 0;
var kNavigateDelayed = "navigate_delayed"
var kNavigateImmediate = "navigate_immediate"
var kNavigated = "navigated"
var kConsume = "consume"
var kConsumed = "consumed"
var deepLinkExpected = true;
var navigationExpected = false;

function fail() {
  console.log("Failing due to timeout!");
  assertTrue(false);
}

// The test sends "link 1", "link 2", "link 3" before load & so only "link 3"
// should be handled.
function listener(link) {
  linksReceived += 1;

  // Since the test harness is loaded async for this test, its functions may
  // not yet exist when the listener is called. Add a delay before using
  // the harness functions.
  window.checkTimer = setTimeout(function () {
    // At most one deep link is expected.
    assertEqual(1, linksReceived);

    assertTrue(deepLinkExpected);
    assertEqual("link 3", link);
    clearTimeout(window.checkTimer);
    if (!navigationExpected) {
      console.log("Ending test.");
      onEndTest();
    }
    clearTimeout(failTimer);
  }, 250);
  console.log("Received link: " + link.toString());
}

window.onload = function () {
  console.log("Window loads after " + linksReceived + " deep links.");
  setupFinished();

  if (!params.has('delayedlink')) {
    // Test that undelayed links are received before the onload handler.
    assertEqual(deepLinkExpected ? 1 : 0, linksReceived);
  }

  if (!deepLinkExpected) {
    // Add a delay before ending the test to ensure that events are not
    // received right after onload.
    setTimeout(function () {
      console.log("Ending test.");
      onEndTest();
      clearTimeout(failTimer);
    }, 1500);
  }
}

const params = new Map(
  window.location.search.slice(1).split('&').map(kv => kv.split('=')))

// A link expected if we got 'consume' and did not get 'consumed'.
deepLinkExpected = params.has(kConsume) && !params.has(kConsumed);

// Navigate if we got 'navigate' and did not get 'navigated'.
if (!params.has(kNavigated)) {
  var newLocation = window.location.href + '&' + kNavigated;

  if (!params.has(kConsume)) {
    // Add a 'consume' when we navigate.
    newLocation += "&" + kConsume
  } else if (!params.has(kConsumed)) {
    // Add a 'consumed' if we have consumed the deep link.
    newLocation += "&" + kConsumed
  }

  if (params.has(kNavigateDelayed)) {
    console.log("Navigating page after a delay.");
    navigationExpected = true;

    setTimeout(function () {
      console.log("Navigating now.");
      window.location.href = newLocation;
    }, 500);
  } else if (params.has(kNavigateImmediate)) {
    console.log("Navigating page immediately.");
    navigationExpected = true;
    window.location.href = newLocation;
  }
}

// Consume the deep link if we got 'consume'.
if (params.has(kConsume)) {
  // If the query string has this value, read the initialDeepLink instead of
  // waiting for onDeepList calls.
  if (params.has("initial")) {
    console.log("Accessing initialDeepLink.");
    var link = h5vcc.runtime.initialDeepLink;
    if (link != "") {
      listener(link);
    }
  }

  console.log("Adding onDeepLink listener.");
  h5vcc.runtime.onDeepLink.addListener(listener);
} else {
  console.log("Not consuming deep link.");
}
