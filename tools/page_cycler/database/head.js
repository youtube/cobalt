// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var totalTime;
var fudgeTime;
var elapsedTime;
var endTime;
var iterations;
var cycle;
var results = false;
var TIMEOUT = 15;

/**
 * Returns the value of the given property stored in the cookie.
 * @param {string} name The property name.
 * @return {string} The value of the given property, or empty string
 *     if the property was not found.
 */
function getPropertyValue(name) {
  var cookies = document.cookie.split('; ');
  for (var i = 0; i < cookies.length; ++i) {
    var t = cookies[i].split('=');
    if ((t[0] == name) && t[1])
      return t[1];
  }
  return '';
}

/**
 * Returns the value of the '__pc_timings' property.
 * @return {string} The list of all timings we got so far.
 */
function getTimings() {
  return getPropertyValue('__pc_timings');
}

/**
 * Sets the value of the '__pc_timings' property in the cookie.
 * @param {string} timings A comma-separated list of timings.
 */
function setTimings(timings) {
  document.cookie = '__pc_timings=' + timings + '; path=/';
}

/**
 * Starts the next test cycle or redirects the browser to the results page.
 */
function nextCycleOrResults() {
  // Call GC twice to cleanup JS heap before starting a new test.
  if (window.gc) {
    window.gc();
    window.gc();
  }

  var tLag = Date.now() - endTime - TIMEOUT;
  if (tLag > 0)
    fudgeTime += tLag;

  var doc;
  if (cycle == iterations) {
    document.cookie = '__pc_done=1; path=/';
    doc = '../../common/report.html';
  } else {
    doc = 'index.html';
  }

  var timings = elapsedTime;
  var oldTimings = getTimings();
  if (oldTimings != '')
    timings = oldTimings + ',' + timings;
  setTimings(timings);

  var url = doc + '?n=' + iterations + '&i=' + cycle +
      '&td=' + totalTime + '&tf=' + fudgeTime;
  document.location.href = url;
}

/**
 * Computes various running times and updates the stats reported at the end.
 * @param {!number} cycleTime The running time of the test cycle.
 */
function testComplete(cycleTime) {
  if (results)
    return;

  var oldTotalTime = 0;
  var cycleEndTime = Date.now();
  var cycleFudgeTime = 0;

  var s = document.location.search;
  if (s) {
    var params = s.substring(1).split('&');
    for (var i = 0; i < params.length; i++) {
      var f = params[i].split('=');
      switch (f[0]) {
        case 'skip':
          return;  // No calculation, just viewing
        case 'n':
          iterations = f[1];
          break;
        case 'i':
          cycle = f[1] - 0 + 1;
          break;
        case 'td':
          oldTotalTime = f[1] - 0;
          break;
        case 'tf':
          cycleFudgeTime = f[1] - 0;
          break;
      }
    }
  }
  elapsedTime = cycleTime;
  totalTime = oldTotalTime + elapsedTime;
  endTime = cycleEndTime;
  fudgeTime = cycleFudgeTime;

  setTimeout(nextCycleOrResults, TIMEOUT);
}
