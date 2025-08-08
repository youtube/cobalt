// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';

// </if>

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util_ts.js';

// By default this page only records metrics for a given period of time in order
// to not waste too much memory. This constant defines the default period until
// recording ceases.
const kDefaultLoggingPeriodInSeconds = 300;

// Indicates whether logs should be recorded at the moment.
let recordLogs = true;

// Renders a simple dialog with |text| as a message and a close button.
function showModalDialog(text) {
  const dialog = document.createElement('div');
  dialog.className = 'modal-dialog';

  const content = document.createElement('div');
  content.className = 'modal-dialog-content';

  const closeButton = document.createElement('span');
  closeButton.className = 'modal-dialog-close-button fake-button';
  closeButton.innerText = 'Close';

  const textContent = document.createElement('p');
  textContent.className = 'modal-dialog-text';
  textContent.innerText = text;

  content.appendChild(closeButton);
  content.appendChild(textContent);
  dialog.appendChild(content);
  window.document.body.append(dialog);

  closeButton.addEventListener('click', () => {
    window.document.body.removeChild(dialog);
  });
}

// Autoscrolling keeps the page scrolled down. Intended usage is as follows:
// before modifying the DOM, check needsScrollDown(), and afterwards invoke
// scrollDown() if needsScrollDown() was true.

function isScrolledDown() {
  return window.innerHeight + window.scrollY >= document.body.offsetHeight;
}

let autoScrollActive = false;  // True iff autoscroll is currently scrolling.
let autoScrollTimer = null;    // Timer for resetting |autoScrollActive|.

function needsScrollDown() {
  const checkbox = document.getElementById('enable-autoscroll');
  return autoScrollActive || (isScrolledDown() && checkbox && checkbox.checked);
}

function scrollDown() {
  autoScrollActive = true;
  window.scrollTo(0, document.body.scrollHeight);
  (function unsetAutoScrollActiveAfterIdletime() {
    if (isScrolledDown()) {
      autoScrollActive = false;
    } else {
      clearTimeout(autoScrollTimer);
      autoScrollTimer = setTimeout(unsetAutoScrollActiveAfterIdletime, 50);
    }
  })();
}

// The configuration of log display can be represented in the URI fragment.
// Below are utility functions for setting/getting these parameters.

function makeKeyValueRegExp(key) {
  return new RegExp(`\\b${key}=([^&]*)`);
}

function setUrlHashParam(key, value) {
  key = encodeURIComponent(key);
  value = encodeURIComponent(value);
  const keyValueRegExp = makeKeyValueRegExp(key);
  const keyValue = `${key}=${value}`;
  if (keyValueRegExp.test(window.location.hash)) {
    const replaced = window.location.hash.replace(keyValueRegExp, keyValue);
    window.location.hash = replaced;
  } else {
    window.location.hash +=
        (window.location.hash.length > 0 ? '&' : '') + keyValue;
  }
}

function getUrlHashParam(key) {
  key = encodeURIComponent(key);
  const match = window.location.hash.match(makeKeyValueRegExp(key));
  if (!match || match[1] === undefined) {
    return undefined;
  }
  return decodeURIComponent(match[1]);
}

// Converts an internal representation of nodes to actual DOM nodes that can
// be attached to the DOM. The internal representation has the following
// properties for each node:
// - type: 'element' | 'text'
// - value: name of tag | text content
// - children (opt): list of child nodes
// - attributes (opt): dictionary of name/value pairs
function nodeToDomNode(node) {
  if (node.type === 'text') {
    return document.createTextNode(node.value);
  }
  // Else the node is of type 'element'.
  const domNode = document.createElement(node.value);
  if ('children' in node) {
    node.children.forEach((child) => {
      domNode.appendChild(nodeToDomNode(child));
    });
  }
  if ('attributes' in node) {
    for (const attribute in node.attributes) {
      domNode.setAttribute(attribute, node.attributes[attribute]);
    }
  }
  return domNode;
}

function addStructuredLog(node, ignoreRecordLogs = false) {
  if (!recordLogs && !ignoreRecordLogs) {
    return;
  }
  const logDiv = $('log-entries');
  if (!logDiv) {
    return;
  }
  const scrollAfterInsert = needsScrollDown();
  logDiv.appendChild(document.createElement('hr'));
  if (node.type === 'fragment') {
    if ('children' in node) {
      node.children.forEach((child) => {
        logDiv.appendChild(nodeToDomNode(child));
      });
    }
  } else {
    logDiv.appendChild(nodeToDomNode(node));
  }
  if (scrollAfterInsert) {
    scrollDown();
  }
}

// Sets up a couple of event handlers and interval handlers for automatically
// stopping the recording of autofill events. We stop the recording because
// you may forget an internals page in some tab and don't want it to keep
// growing it's memory consumption forever.
// We have two checkboxes
// [x] Record new events
// [x] Automatically stop recording in 0:30
// with the following behavior:
// - While the first checkbox is checked, log entries are recorded.
// - While both checkboxes are checked, the countdown decreases.
// - If the countdown reaches 0:00, the first checkbox gets unchecked.
// - If any checkbox is toggled, we reset the countdown time to
//   kDefaultLoggingPeriodInSeconds.
function setUpStopRecording() {
  // Timestamp (in ms after epoch), when the countdown to stop recording should
  // happen.
  let stopRecordingLogsAt = undefined;
  // Interval ID generated by setInterval, which is called every second to
  // update the remaining time.
  let countdown = undefined;

  const currentlyRecordingChkBox =
      document.getElementById('currently-recording');
  const autoStopRecordingChkBox =
      document.getElementById('automatically-stop-recording');

  // Formats a number of seconds into a [M]M:SS format.
  const secondsToString = (seconds) => {
    const minutes = Math.floor(seconds / 60);
    seconds = seconds % 60;
    return `${minutes}:${seconds < 10 ? '0' : ''}${seconds}`;
  };

  // Updates the time label and reacts to the countdown reaching 0.
  const countdownHandler = () => {
    const remainingSeconds = Math.round(
        Math.max((stopRecordingLogsAt - new Date().getTime()) / 1000, 0));
    document.getElementById('stop-recording-time').innerText =
        secondsToString(remainingSeconds);

    if (remainingSeconds == 0) {
      recordLogs = false;
      currentlyRecordingChkBox.checked = false;
      resetTimeout();
    }
  };
  const startCountDown = () => {
    if (!countdown) {
      countdown = window.setInterval(countdownHandler, 1000);
    }
  };
  const stopCountDown = () => {
    if (countdown) {
      window.clearInterval(countdown);
      countdown = undefined;
    }
  };
  const startOrStopCountDown = () => {
    if (currentlyRecordingChkBox.checked && autoStopRecordingChkBox.checked) {
      startCountDown();
    } else {
      stopCountDown();
    }
  };
  const resetTimeout = () => {
    stopRecordingLogsAt =
        new Date().getTime() + kDefaultLoggingPeriodInSeconds * 1000;
    countdownHandler();  // Update the string shown to the user.
    startOrStopCountDown();
  };

  currentlyRecordingChkBox.addEventListener('click', () => {
    recordLogs = currentlyRecordingChkBox.checked;
    resetTimeout();
  });
  autoStopRecordingChkBox.addEventListener('click', () => {
    resetTimeout();
  });

  resetTimeout();
}

function setUpAutofillInternals() {
  document.title = 'Autofill Internals';
  document.getElementById('h1-title').textContent = 'Autofill Internals';
  document.getElementById('logging-note').innerText =
      'Captured autofill logs are listed below. Logs are cleared and no longer \
      captured when all autofill-internals pages are closed.';
  document.getElementById('logging-note-incognito').innerText =
      'Captured autofill logs are not available in Incognito.';
  setUpLogDisplayConfig();
  setUpMarker();
  setUpDownload('autofill');
  setUpStopRecording();
}

function setUpPasswordManagerInternals() {
  document.title = 'Password Manager Internals';
  document.getElementById('h1-title').textContent =
      'Password Manager Internals';
  document.getElementById('logging-note').innerText =
      'Captured password manager logs are listed below. Logs are cleared and \
      no longer captured when all password-manager-internals pages are closed.';
  document.getElementById('logging-note-incognito').innerText =
      'Captured password manager logs are not available in Incognito.';
  setUpMarker();
  setUpDownload('password-manager');
  setUpStopRecording();
  addWebUiListener(
      'enable-reset-upm-eviction-button', enableResetUpmEvictionButton);
}

function enableResetCacheButton() {
  document.getElementById('reset-cache-fake-button').style.display = 'inline';
}

function enableResetUpmEvictionButton(isEnabled) {
  document.getElementById('reset-upm-eviction-fake-button').style.display =
      isEnabled ? 'inline' : 'none';
}

function notifyAboutIncognito(isIncognito) {
  document.body.dataset.incognito = isIncognito;
}

function notifyAboutVariations(variations) {
  const list = document.createElement('div');
  for (const item of variations) {
    list.appendChild(document.createTextNode(item));
    list.appendChild(document.createElement('br'));
  }
  const variationsList = document.getElementById('variations-list');
  variationsList.appendChild(list);
}

// Setup a (fake) button to add visual markers
// (it's fake to keep Autofill from parsing the form).
function setUpMarker() {
  // Initialize marker field: when pressed, add fake log event.
  let markerCounter = 0;
  const markerFakeButton = document.getElementById('marker-fake-button');
  markerFakeButton.addEventListener('click', () => {
    ++markerCounter;
    const scrollAfterInsert = needsScrollDown();
    addStructuredLog(
        {
          type: 'element',
          value: 'div',
          attributes: {'class': 'marker', 'contenteditable': 'true'},
          children: [{type: 'text', value: `#${markerCounter} `}],
        },
        /*ignoreRecordLogs=*/ true);
    if (scrollAfterInsert) {
      scrollDown();
      // Focus marker div, set caret at end of line.
      const logDiv = document.getElementById('log-entries');
      if (!logDiv) {
        return;
      }
      const markerNode = logDiv.lastChild;
      const textNode = markerNode.lastChild;
      markerNode.focus();
      window.getSelection().collapse(textNode, textNode.length);
    }
  });
}

// Setup a (fake) download button to download html content of the page.
function setUpDownload(moduleName) {
  const downloadFakeButton = document.getElementById('download-fake-button');
  downloadFakeButton.addEventListener('click', () => {
    const html = document.documentElement.outerHTML;
    const blob = new Blob([html], {type: 'text/html'});
    const url = window.URL.createObjectURL(blob);
    const dateString = new Date()
                           .toISOString()
                           .replace(/T/g, '_')
                           .replace(/\..+/, '')
                           .replace(/:/g, '-');
    const filename = `${moduleName}-internals-${dateString}.html`;

    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.style.display = 'none';
    document.body.appendChild(a);
    a.click();
    window.URL.revokeObjectURL(url);
    a.remove();
  });
  // <if expr="is_ios">
  // Hide this until downloading a file works on iOS, see
  // https://bugs.webkit.org/show_bug.cgi?id=167341
  // https://bugs.chromium.org/p/chromium/issues/detail?id=1252380
  downloadFakeButton.style = 'display: none';
  // </if>
}

// Sets up the top bar with checkboxes to show/hide the different sorts of log
// event types, a checkbox to enable/disable autoscroll.
function setUpLogDisplayConfig() {
  const FAST_CHECKOUT = 'FastCheckout';
  const SCOPES = [
    'Context',
    'Parsing',
    'AbortParsing',
    'Filling',
    'Submission',
    'AutofillServer',
    'Metrics',
    'AddressProfileFormImport',
    'WebsiteModifiedFieldValue',
    FAST_CHECKOUT,
  ];
  const DEFAULT_UNCHECKED_SCOPES = new Set([
    FAST_CHECKOUT,
  ]);
  const logDiv = document.getElementById('log-entries');
  const autoScrollInput = document.getElementById('enable-autoscroll');
  const checkboxPlaceholder = document.getElementById('checkbox-placeholder');

  // Initialize the auto-scroll checkbox.
  autoScrollInput.checked = getUrlHashParam('autoscroll') !== 'n';
  autoScrollInput.addEventListener('change', (event) => {
    setUrlHashParam('autoscroll', autoScrollInput.checked ? 'y' : 'n');
  });

  // Create and initialize filter checkboxes: remove/add hide-<Scope> class to
  // |logDiv| when (un)checked.
  for (const scope of SCOPES) {
    const input = document.createElement('input');
    input.setAttribute('type', 'checkbox');
    input.setAttribute('id', `checkbox-${scope}`);
    const urlHashParam = getUrlHashParam(scope);
    if (DEFAULT_UNCHECKED_SCOPES.has(scope) && urlHashParam === undefined) {
      input.checked = false;
    } else {
      input.checked = getUrlHashParam(scope) !== 'n';
    }
    function changeHandler() {
      setUrlHashParam(scope, input.checked ? 'y' : 'n');
      const cls = `hide-${scope}`;
      const scrollAfterInsert = needsScrollDown();
      if (!input.checked) {
        logDiv.classList.add(cls);
      } else {
        logDiv.classList.remove(cls);
      }
      if (scrollAfterInsert) {
        scrollDown();
      }
    }
    input.addEventListener('change', changeHandler);
    changeHandler();  // Call once to initialize |logDiv|'s classes.
    const label = document.createElement('label');
    label.appendChild(input);
    label.appendChild(document.createTextNode(' ' + scope));
    checkboxPlaceholder.appendChild(label);
  }
}

document.addEventListener('DOMContentLoaded', function(event) {
  addWebUiListener('enable-reset-cache-button', enableResetCacheButton);
  addWebUiListener('notify-about-incognito', notifyAboutIncognito);
  addWebUiListener('notify-about-variations', notifyAboutVariations);
  addWebUiListener('notify-reset-done', message => showModalDialog(message));
  addWebUiListener('add-structured-log', addStructuredLog);
  addWebUiListener('setup-autofill-internals', setUpAutofillInternals);
  addWebUiListener(
      'setup-password-manager-internals', setUpPasswordManagerInternals);

  chrome.send('loaded');

  const resetCacheFakeButton =
      document.getElementById('reset-cache-fake-button');
  resetCacheFakeButton.addEventListener('click', () => {
    chrome.send('resetCache');
  });

  const resetUpmEvictionButton =
      document.getElementById('reset-upm-eviction-fake-button');
  resetUpmEvictionButton.addEventListener('click', () => {
    chrome.send('resetUpmEviction');
    showModalDialog('UPM re-enabled. Please, restart Chrome.');
  });
});
