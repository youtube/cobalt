// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './component.html.js';

interface Component {
  id: string;
  name: string;
  status: string;
  version: string;
}

export interface ComponentsData {
  components: Component[];
}

/**
 * An array of the latest component data including ID, name, status and
 * version. This is populated in returnComponentsData() for the convenience of
 * tests.
 */
let currentComponentsData: Component[]|null = null;

/**
 * Takes the |componentsData| input argument which represents data about the
 * currently installed components and populates the Lit HTML template with
 * that data. It expects an object structure like the above.
 * @param componentsData Detailed info about installed components.
 *      Same expected format as returnComponentsData().
 */
function renderTemplate(componentsData: ComponentsData) {
  render(getHtml(componentsData), getRequiredElement('component-placeholder'));
}

/**
 * Asks the C++ ComponentsDOMHandler to get details about the installed
 * components.
 */
function requestComponentsData() {
  sendWithPromise('requestComponentsData').then(returnComponentsData);
}

/**
 * Called by the WebUI to re-populate the page with data representing the
 * current state of installed components. The componentsData will also be
 * stored in currentComponentsData to be available to JS for testing purposes.
 * @param componentsData Detailed info about installed components.
 */
function returnComponentsData(componentsData: ComponentsData) {
  const bodyContainer = getRequiredElement('body-container');
  const body = document.body;

  bodyContainer.style.visibility = 'hidden';
  body.className = '';

  // Initialize |currentComponentsData|, which can also be updated in
  // onComponentEvent() later.
  currentComponentsData = componentsData.components;

  renderTemplate(componentsData);

  // Add handlers to dynamically created HTML elements.
  const links =
      document.body.querySelectorAll<HTMLButtonElement>('.button-check-update');
  for (const link of links) {
    link.onclick = function(e) {
      handleCheckUpdate(link);
      e.preventDefault();
    };
  }

  // Disable some controls for Guest mode in ChromeOS.
  if (isChromeOS && loadTimeData.getBoolean('isGuest')) {
    document.body.querySelectorAll<HTMLButtonElement>('[guest-disabled]')
        .forEach(function(element) {
          element.disabled = true;
        });
  }

  bodyContainer.style.visibility = 'visible';
  body.className = 'show-tmi-mode-initial';
}

interface ComponentEvent {
  event: string;
  id?: string;
  version?: string;
}

/**
 * Listener called when state of component updater service changes.
 * @param event Contains event and component ID. Component ID is
 *     optional.
 */
function onComponentEvent(event: ComponentEvent) {
  if (!event.id) {
    return;
  }

  const id = event.id;

  assert(currentComponentsData);
  const filteredComponents = currentComponentsData.filter(function(entry) {
    return entry.id === id;
  });

  // A component may be added from another page so the status and version
  // should only be updated if the component is listed on this page.
  if (filteredComponents.length === 0) {
    return;
  }

  const component = filteredComponents[0];
  assert(component);

  const status = event.event;
  getRequiredElement('status-' + id).textContent = status;
  component.status = status;

  if (event.version) {
    const version = event.version;
    getRequiredElement('version-' + id).textContent = version;
    component.version = version;
  }
}

/**
 * Handles an 'enable' or 'disable' button getting clicked.
 * @param node The HTML element representing the component being checked for
 *     update.
 */
function handleCheckUpdate(node: HTMLElement) {
  getRequiredElement('status-' + String(node.id)).textContent =
      loadTimeData.getString('checkingLabel');

  // Tell the C++ ComponentssDOMHandler to check for update.
  chrome.send('checkUpdate', [String(node.id)]);
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', function() {
  addWebUiListener('component-event', onComponentEvent);
  requestComponentsData();
});
