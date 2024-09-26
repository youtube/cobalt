// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Overview Tracing UI root element.
 */

cr.define('cr.ArcOverviewTracing', function() {
  return {
    /**
     * Initializes internal structures.
     */
    initialize() {
      const maxTime = $('arc-overview-tracing-max-time');
      maxTime.addEventListener('change', function(event) {
        chrome.send('setMaxTime', [parseInt(maxTime.value)]);
      }, false);
      chrome.send('ready');
      chrome.send('setMaxTime', [parseInt(maxTime.value)]);
      initializeOverviewUi();
    },

    setStatus: setStatus,

    setModel(model) {
      addModel(model);
    },
  };
});

/**
 * Initializes UI.
 */
window.onload = function() {
  cr.ArcOverviewTracing.initialize();
};
