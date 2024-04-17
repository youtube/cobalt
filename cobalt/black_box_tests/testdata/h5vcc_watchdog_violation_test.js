// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
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

function canDetectViolation() {
  h5vcc.crashLog.register('test-client', '', 'started', 500, 0, 'all');
  // clear whatever violations are in the cache
  h5vcc.crashLog.getWatchdogViolations();

  h5vcc.crashLog.ping('test-client', '');
  doBusyLoopFor(2000)

  let violationsJson = h5vcc.crashLog.getWatchdogViolations();
  if (!violationsJson) {
    failTest();
  }

  let violations = JSON.parse(violationsJson);
  if (violations['test-client']['violations'].length !== 1) {
    failTest();
  }
}

function reportsRepitativeViolations() {
  h5vcc.crashLog.register('test-client', '', 'started', 500, 0, 'all');
  // clear whatever violations are in the cache
  h5vcc.crashLog.getWatchdogViolations();

  h5vcc.crashLog.ping('test-client', '');
  doBusyLoopFor(2000);
  h5vcc.crashLog.ping('test-client', '');
  doBusyLoopFor(2000);

  let violationsJson = h5vcc.crashLog.getWatchdogViolations();
  if (!violationsJson) {
    failTest();
  }

  let violations = JSON.parse(violationsJson);
  if (violations['test-client']['violations'].length !== 2) {
    failTest();
  }
}

function reportsLogtraceWithViolations() {
  h5vcc.crashLog.register('test-client', '', 'started', 500, 0, 'all');
  // clear whatever violations are in the cache
  h5vcc.crashLog.getWatchdogViolations();
  h5vcc.crashLog.logEvent('frame_1');
  h5vcc.crashLog.logEvent('frame_2');

  h5vcc.crashLog.ping('test-client', '');
  h5vcc.crashLog.logEvent('frame_3');
  h5vcc.crashLog.logEvent('frame_4');

  doBusyLoopFor(2000);

  let violationsJson = h5vcc.crashLog.getWatchdogViolations();
  if (!violationsJson) {
    failTest();
  }

  let violations = JSON.parse(violationsJson);
  let logTrace = violations['test-client']['violations'][0]['logTrace'];
  if (logTrace.length !== 4) {
    failTest();

  }

  if (logTrace[0] !== 'frame_1' || logTrace[1] !== 'frame_2' || logTrace[2]
      !== 'frame_3' || logTrace[3] !== 'frame_4') {
    failTest();
  }
}

canDetectViolation();
reportsRepitativeViolations();
reportsLogtraceWithViolations();

onEndTest();
