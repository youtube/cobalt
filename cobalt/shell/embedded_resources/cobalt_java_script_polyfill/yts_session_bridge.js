/**
 * @license
 * Copyright The Cobalt Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

// Intercepts localStorage writes to yts.prior_connection so the browser process
// can stash the value in C++ memory and re-inject it after a cross-origin
// sticky-loader round trip (where the storage partition changes and the
// original localStorage entry is lost).
//
// Communication is via console.info messages matched by
// Shell::DidAddMessageToConsole on the browser side.
(function () {
  if (typeof window === 'undefined' || typeof Storage === 'undefined') {
    return;
  }

  var host = window.location && window.location.hostname || '';
  if (!/(^|\.)youtube\.com$|(^|\.)devicecertification\.youtube$/.test(host)) {
    return;
  }

  if (window.__cobaltYtsSessionBridgeInstalled) {
    return;
  }
  window.__cobaltYtsSessionBridgeInstalled = true;

  var KEY = 'yts.prior_connection';
  var TAG = '[cobalt-yts-bridge] ';
  var originalSetItem = Storage.prototype.setItem;

  Storage.prototype.setItem = function (key, value) {
    var result = originalSetItem.apply(this, arguments);
    if (String(key) === KEY) {
      console.info(TAG + 'setItem key=' + KEY + ' value=' + value);
    }
    return result;
  };
})();
