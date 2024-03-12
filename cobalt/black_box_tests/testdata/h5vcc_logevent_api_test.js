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

function canCallH5vccLogEventTest() {
  if (!h5vcc.crashLog) {
    console.log("h5vcc.crashLog does not exist");
    return;
  }

  if (!h5vcc.crashLog.logEvent) {
    console.log("h5vcc.crashLog.logEvent does not exist");
    return;
  }

  h5vcc.crashLog.logEvent("test-frame");
}

function canCallH5vccGetLogTraceTest() {
  h5vcc.crashLog.logEvent("frame");
}

function getLogTraceReturnsLastLoggedEventTest() {
  if (!h5vcc.crashLog) {
    console.log("h5vcc.crashLog does not exist");
    return;
  }

  if (!h5vcc.crashLog.logEvent) {
    console.log("h5vcc.crashLog.logEvent does not exist");
    return;
  }

  h5vcc.crashLog.logEvent(
      "yt.ui.uix.Abc.efG_ (http://www.youtube.com/yts/jsbin/a-b-c.js:14923:34)");
  h5vcc.crashLog.logEvent(
      "yy.ttt.Aaa.b_ (http://www.youtube.com/yts/jsbin/a-b-c.js:123:45)");
  h5vcc.crashLog.logEvent(
      "yy.ttt.Ccc.d_ (http://www.youtube.com/yts/jsbin/a-b-c.js:678:90)");

  let logTrace = h5vcc.crashLog.getLogTrace();
  if (logTrace[logTrace.length - 1]
      !== "yy.ttt.Ccc.d_ (http://www.youtube.com/yts/jsbin/a-b-c.js:678:90)") {
    failTest();
  }
}

function clearLogWorks() {
  if (!h5vcc.crashLog.clearLog) {
    console.log("h5vcc.crashLog.clearLog does not exist");
    return;
  }

  h5vcc.crashLog.clearLog();

  h5vcc.crashLog.logEvent("1");
  h5vcc.crashLog.logEvent("2");
  h5vcc.crashLog.logEvent("3");

  h5vcc.crashLog.clearLog();

  h5vcc.crashLog.logEvent("4");
  h5vcc.crashLog.logEvent("5");

  let logTrace = h5vcc.crashLog.getLogTrace();

  if (logTrace.length !== 2) {
    failTest();
  }

  if (logTrace[0] !== "4") {
    failTest();
  }
  if (logTrace[1] !== "5") {
    failTest();
  }
}

function canReadLogtraceFromWatchdogViolation() {
  h5vcc.crashLog.register("test-client", "", "started", 500, 0, 'all',);
  h5vcc.crashLog.ping("test-client", "");

  h5vcc.crashLog.clearLog();
  h5vcc.crashLog.logEvent("1");
  h5vcc.crashLog.logEvent("2");

  // sleep to generate violation
  let start = Date.now();
  while ((Date.now() - start) < 2000) {
  }

  let violationsJson = h5vcc.crashLog.getWatchdogViolations();
  if (!violationsJson) {
    failTest();
  }

  let violations = JSON.parse(violationsJson);
  if (violations['test-client']['violations'][0]['logTrace'].length !== 2) {
    failTest();
  }
}

canCallH5vccLogEventTest();
canCallH5vccGetLogTraceTest();
getLogTraceReturnsLastLoggedEventTest();
clearLogWorks();
canReadLogtraceFromWatchdogViolation();
onEndTest();
