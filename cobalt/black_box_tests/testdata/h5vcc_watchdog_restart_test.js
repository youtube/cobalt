// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
'use strict';

function failTest() {
  notReached();
  onEndTest();
}

function doBusyLoopFor(msToSleep) {
  let start = Date.now();
  while ((Date.now() - start) < msToSleep) {
  }
}
function generaTeViolationAndCrash() {
  h5vcc.crashLog.register("test-client", "", "started", 1000, 0, 'all');
  // clear whatever violations are in the cache
  h5vcc.crashLog.getWatchdogViolations();

  h5vcc.crashLog.ping("test-client", "");
  doBusyLoopFor(1500);
  h5vcc.crashLog.ping("test-client", "");

  // wait for periodic write(every 500ms) to save violation
  doBusyLoopFor(600);

  h5vcc.crashLog.triggerCrash('null_dereference');
}

function canReadViolationAfterCrash() {
  h5vcc.crashLog.register("test-client", "", "started", 500, 0, 'all');

  let violationsJson = h5vcc.crashLog.getWatchdogViolations();
  if (!violationsJson) {
    failTest();
  }

  let violations = JSON.parse(violationsJson);
  if (violations['test-client']['violations'].length !== 1) {
    failTest();
  }
}

window.onkeydown = function (event) {
  if (event.keyCode === 97) {
    generaTeViolationAndCrash();
  } else if (event.keyCode === 98) {
    canReadViolationAfterCrash();
  }
  onEndTest();
}
setupFinished();
