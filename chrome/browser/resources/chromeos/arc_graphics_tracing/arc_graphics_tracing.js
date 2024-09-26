// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Graphics Tracing UI root element.
 */

cr.define('cr.ArcGraphicsTracing', function() {
  return {
    /**
     * Initializes internal structures.
     */
    initialize() {
      const stopOnJank = $('arc-graphics-tracing-stop-on-jank');
      stopOnJank.addEventListener('click', function(event) {
        chrome.send('setStopOnJank', [stopOnJank.checked]);
      }, false);
      chrome.send('ready');
      chrome.send('setStopOnJank', [stopOnJank.checked]);
      initializeGraphicsUi();
    },

    setStatus: setStatus,

    setModel(model) {
      setGraphicBuffersModel(model);
    },
  };
});

/**
 * Initializes UI.
 */
window.onload = function() {
  cr.ArcGraphicsTracing.initialize();
};
