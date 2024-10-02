// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util_ts.js';

/**
 * Generate the page content.
 * @param {Array<Object>} errorCodes Error codes array consisting of a
 *    numerical error ID and error code string.
 */
function listErrorCodes(errorCodes) {
  const errorPageUrl = 'chrome://network-error/';
  const errorCodesList = document.createElement('ul');
  for (let i = 0; i < errorCodes.length; i++) {
    const listEl = document.createElement('li');
    const errorCodeLinkEl = document.createElement('a');
    errorCodeLinkEl.href = errorPageUrl + errorCodes[i].errorId;
    errorCodeLinkEl.textContent =
        errorCodes[i].errorCode + ' (' + errorCodes[i].errorId + ')';
    listEl.appendChild(errorCodeLinkEl);
    errorCodesList.appendChild(listEl);
  }
  $('pages').appendChild(errorCodesList);
}

function initialize() {
  const xhr = new XMLHttpRequest();
  xhr.open('GET', 'network-error-data.json');
  xhr.addEventListener('load', function(e) {
    if (xhr.status === 200) {
      try {
        const data = JSON.parse(xhr.responseText);
        listErrorCodes(data['errorCodes']);
      } catch (e) {
        $('pages').innerText = 'Could not parse the error codes data. ' +
            'Try reloading the page.';
      }
    }
  });
  xhr.send();
}

document.addEventListener('DOMContentLoaded', initialize);
