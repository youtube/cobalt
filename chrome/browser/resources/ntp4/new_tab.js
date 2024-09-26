// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {$, appendParam, getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {AppInfo} from './app_info.js';
import {APP_LAUNCH, AppsPage} from './apps_page.js';
import {PageListView} from './page_list_view.js';

/**
 * @fileoverview New tab page 4
 * This is the main code for a previous version of the Chrome NTP ("NTP4").
 * Some parts of this are still used for the chrome://apps page.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome

/** @type {!NewTabView|undefined} */
let newTabView;

/**
 * The time when all sections are ready.
 * @type {number|undefined}
 * @private
 */
let startTime;

/**
 * The number of sections to wait on.
 * @type {number}
 */
let sectionsToWaitFor = -1;

/**
 * The time in milliseconds for most transitions.  This should match what's
 * in new_tab.css.  Unfortunately there's no better way to try to time
 * something to occur until after a transition has completed.
 * @type {number}
 * @const
 */
const DEFAULT_TRANSITION_TIME = 500;

/**
 * Creates a NewTabView object. NewTabView extends PageListView with
 * new tab UI specific logics.
 * @constructor
 * @extends {PageListView}
 */
function NewTabView() {
  const pageSwitcherStart =
      /** @type {!Element} */ (getRequiredElement('page-switcher-start'));
  const pageSwitcherEnd =
      /** @type {!Element} */ (getRequiredElement('page-switcher-end'));
  this.initialize(
      getRequiredElement('page-list'), getRequiredElement('dot-list'),
      getRequiredElement('card-slider-frame'), getRequiredElement('trash'),
      pageSwitcherStart, pageSwitcherEnd);
}

// TODO(dbeam): NewTabView is now the only extender of PageListView; these
// classes should be merged.
NewTabView.prototype = {
  __proto__: PageListView.prototype,
};

/**
 * Invoked at startup once the DOM is available to initialize the app.
 */
function onLoad() {
  sectionsToWaitFor = 1;
  measureNavDots();

  newTabView = new NewTabView();

  if (!loadTimeData.getBoolean('showWebStoreIcon')) {
    const webStoreIcon = $('chrome-web-store-link');
    // Not all versions of the NTP have a footer, so this may not exist.
    if (webStoreIcon) {
      webStoreIcon.hidden = true;
    }
  } else {
    const webStoreLink = loadTimeData.getString('webStoreLink');
    const url = appendParam(webStoreLink, 'utm_source', 'chrome-ntp-launcher');
    $('chrome-web-store-link').href = url;
    $('chrome-web-store-link')
        .addEventListener('auxclick', onChromeWebStoreButtonClick);
    $('chrome-web-store-link')
        .addEventListener('click', onChromeWebStoreButtonClick);
  }

  // We need to wait for all the footer menu setup to be completed before
  // we can compute its layout.
  layoutFooter();

  doWhenAllSectionsReady(function() {
    // Tell the slider about the pages.
    newTabView.updateSliderCards();
    // Mark the current page.
    newTabView.cardSlider.currentCardValue.navigationDot.classList.add(
        'selected');

    document.documentElement.classList.remove('starting-up');

    startTime = Date.now();

    addWebUiListener('theme-changed', () => {
      $('themecss').href = 'chrome://theme/css/new_tab_theme.css?' + Date.now();
    });
    chrome.send('observeThemeChanges');
  });
}

/**
 * Launches the chrome web store app with the chrome-ntp-launcher
 * source.
 * @param {!Event} e The click/auxclick event.
 */
function onChromeWebStoreButtonClick(e) {
  if (e.button > 1) {
    return;
  }  // Ignore buttons other than left and middle.
  chrome.send(
      'recordAppLaunchByURL',
      [e.currentTarget.href, APP_LAUNCH.NTP_WEBSTORE_FOOTER]);
}

/**
 * Launches the deprecated apps deletion dialog on click.
 * @param {!Event} e The click/auxclick event.
 */
function onChromeDeprecatedAppsDeletionLinkClick(e) {
  if (/** @type {MouseEvent} */ (e).button > 1) {
    return;
  }
  e.preventDefault();
  chrome.send('deprecatedDialogLinkClicked');
}

/**
 * Queued callbacks which lie in wait for all sections to be ready.
 * @type {Array}
 */
const readyCallbacks = [];

/**
 * Fired as each section of pages becomes ready.
 */
document.addEventListener('sectionready', function(e) {
  if (--sectionsToWaitFor <= 0) {
    while (readyCallbacks.length) {
      readyCallbacks.shift()();
    }
  }
});

/**
 * This is used to simulate a fire-once event (i.e. $(document).ready() in
 * jQuery or Y.on('domready') in YUI. If all sections are ready, the callback
 * is fired right away. If all pages are not ready yet, the function is queued
 * for later execution.
 * @param {Function} callback The work to be done when ready.
 */
function doWhenAllSectionsReady(callback) {
  assert(typeof callback === 'function');
  if (sectionsToWaitFor > 0) {
    readyCallbacks.push(callback);
  } else {
    window.setTimeout(callback, 0);
  }  // Do soon after, but asynchronously.
}

/**
 * Measure the width of a nav dot with a given title.
 * @param {string} id The loadTimeData ID of the desired title.
 * @return {number} The width of the nav dot.
 */
function measureNavDot(id) {
  const measuringDiv = $('fontMeasuringDiv');
  measuringDiv.textContent = loadTimeData.getString(id);
  // The 4 is for border and padding.
  return Math.max(measuringDiv.clientWidth * 1.15 + 4, 80);
}

/**
 * Fills in an invisible div with the longest dot title string so that
 * its length may be measured and the nav dots sized accordingly.
 */
function measureNavDots() {
  const styleElement = document.createElement('style');
  styleElement.type = 'text/css';
  // max-width is used because if we run out of space, the nav dots will be
  // shrunk.
  const pxWidth = measureNavDot('appDefaultPageName');
  styleElement.textContent = '.dot { max-width: ' + pxWidth + 'px; }';
  document.querySelector('head').appendChild(styleElement);
}

/**
 * Layout the footer so that the nav dots stay centered.
 */
function layoutFooter() {
  // We need the image to be loaded.
  const logo = $('logo-img');
  const logoImg = logo.querySelector('img');

  // Only compare the width after the footer image successfully loaded.
  if (!logoImg.complete || logoImg.width === 0) {
    logoImg.onload = layoutFooter;
    return;
  }

  const menu = $('footer-menu-container');
  if (menu.clientWidth > logoImg.width) {
    logo.style.WebkitFlex = '0 1 ' + menu.clientWidth + 'px';
  } else {
    menu.style.WebkitFlex = '0 1 ' + logoImg.width + 'px';
  }
}

/**
 * Set the dominant color for a node. This will be called in response to
 * getFaviconDominantColor. The node represented by |id| better have a setter
 * for stripeColor.
 * @param {string} id The ID of a node.
 * @param {string} color The color represented as a CSS string.
 */
function setFaviconDominantColor(id, color) {
  const node = $(id);
  if (node) {
    node.stripeColor = color;
  }
}

/**
 * Wrappers to forward the callback to corresponding PageListView member.
 */

/**
 * Called by chrome when a new app has been added to chrome or has been
 * enabled if previously disabled.
 * @param {AppInfo} appData A data structure full of relevant information for
 *     the app.
 * @param {boolean=} opt_highlight Whether the app about to be added should
 *     be highlighted.
 */
function appAdded(appData, opt_highlight) {
  newTabView.appAdded(appData, opt_highlight);
}

/**
 * Called by chrome when an app has changed positions.
 * @param {AppInfo} appData The data for the app. This contains page and
 *     position indices.
 */
function appMoved(appData) {
  newTabView.appMoved(appData);
}

/**
 * Called by chrome when an existing app has been disabled or
 * removed/uninstalled from chrome.
 * @param {AppInfo} appData A data structure full of relevant information for
 *     the app.
 * @param {boolean} isUninstall True if the app is being uninstalled;
 *     false if the app is being disabled.
 * @param {boolean} fromPage True if the removal was from the current page.
 */
function appRemoved(appData, isUninstall, fromPage) {
  newTabView.appRemoved(appData, isUninstall, fromPage);
}

/**
 * Callback invoked by chrome whenever an app preference changes.
 * @param {Object} data An object with all the data on available
 *     applications.
 */
function appsPrefChangeCallback(data) {
  newTabView.appsPrefChangedCallback(data);
}

/**
 * Called whenever tiles should be re-arranging themselves out of the way
 * of a moving or insert tile.
 */
export function enterRearrangeMode() {
  newTabView.enterRearrangeMode();
}

/**
 * Callback invoked by chrome with the apps available.
 *
 * Note that calls to this function can occur at any time, not just in
 * response to a getApps request. For example, when a user
 * installs/uninstalls an app on another synchronized devices.
 * @param {{apps: Array<AppInfo>, appPageNames: Array<string>,
 *     deprecatedAppsDialogLinkText: string}} data An object with all the data
 *     on available applications.
 */
function getAppsCallback(data) {
  newTabView.getAppsCallback(data);
  setUpDeprecatedAppsDialogLink(data.deprecatedAppsDialogLinkText);
}

/**
 * Called whenever there are deprecated apps on the page, to set up the link
 * to trigger the deprecated apps dialog.
 * @param {string} linkText The link text to trigger the deprecated apps dialog.
 */
function setUpDeprecatedAppsDialogLink(linkText) {
  if (linkText) {
    $('deprecated-apps-link').textContent = linkText;
    $('deprecated-apps-link')
        .addEventListener('click', onChromeDeprecatedAppsDeletionLinkClick);
    $('deprecated-apps-link-container').hidden = false;
  } else {
    $('deprecated-apps-link-container').hidden = true;
  }
}

/**
 * Return the index of the given apps page.
 * @param {AppsPage} page The AppsPage we wish to find.
 * @return {number} The index of |page| or -1 if it is not in the collection.
 */
export function getAppsPageIndex(page) {
  return newTabView.getAppsPageIndex(page);
}

export function getCardSlider() {
  return newTabView.cardSlider;
}

/**
 * Invoked whenever some app is released
 */
export function leaveRearrangeMode() {
  newTabView.leaveRearrangeMode();
}

/**
 * Save the name of an apps page.
 * Store the apps page name into the preferences store.
 * @param {AppsPage} appPage The app page for which we wish to save.
 * @param {string} name The name of the page.
 */
export function saveAppPageName(appPage, name) {
  newTabView.saveAppPageName(appPage, name);
}

// Return an object with all the exports
const exports = {
  appAdded,
  appMoved,
  appRemoved,
  appsPrefChangeCallback,
  getAppsCallback,
  setFaviconDominantColor,
};

window['ntp'] = window['ntp'] || {};
for (const key of Object.keys(exports)) {
  window['ntp'][key] = exports[key];
}

document.addEventListener('DOMContentLoaded', onLoad);
