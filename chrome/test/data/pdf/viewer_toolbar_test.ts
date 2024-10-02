// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType, ViewerToolbarElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

function createToolbar() {
  document.body.innerHTML = '';
  const toolbar = document.createElement('viewer-toolbar');
  document.body.appendChild(toolbar);
  return toolbar;
}

/**
 * Returns the cr-icon-buttons in |toolbar|'s shadowRoot under |parentId|.
 */
function getCrIconButtons(toolbar: ViewerToolbarElement, parentId: string) {
  return toolbar.shadowRoot!.querySelector(`#${parentId}`)!.querySelectorAll(
      'cr-icon-button');
}

function assertCheckboxMenuButton(button: HTMLElement, enabled: boolean) {
  chrome.test.assertEq(
      enabled ? 'true' : 'false', button.getAttribute('aria-checked'));
  chrome.test.assertEq(enabled, !button.querySelector('iron-icon')!.hidden);
}

// Unit tests for the viewer-toolbar element.
const tests = [
  /**
   * Test that the toolbar toggles between showing the fit-to-page and
   * fit-to-width buttons.
   */
  function testFitButton() {
    const toolbar = createToolbar();
    const fitButton = getCrIconButtons(toolbar, 'center')[2]!;
    const fitWidthIcon = 'pdf:fit-to-width';
    const fitHeightIcon = 'pdf:fit-to-height';

    let lastFitType = '';
    let numEvents = 0;
    toolbar.addEventListener('fit-to-changed', e => {
      lastFitType = e.detail;
      numEvents++;
    });

    // Initially FIT_TO_WIDTH, show FIT_TO_PAGE.
    chrome.test.assertEq(fitHeightIcon, fitButton.ironIcon);

    // Tap 1: Fire fit-to-changed(FIT_TO_PAGE), show fit-to-width.
    fitButton.click();
    chrome.test.assertEq(FittingType.FIT_TO_PAGE, lastFitType);
    chrome.test.assertEq(1, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);

    // Tap 2: Fire fit-to-changed(FIT_TO_WIDTH), show fit-to-page.
    fitButton.click();
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, lastFitType);
    chrome.test.assertEq(2, numEvents);
    chrome.test.assertEq(fitHeightIcon, fitButton.ironIcon);

    // Do the same as above, but with fitToggle().
    toolbar.fitToggle();
    chrome.test.assertEq(FittingType.FIT_TO_PAGE, lastFitType);
    chrome.test.assertEq(3, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);
    toolbar.fitToggle();
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, lastFitType);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitHeightIcon, fitButton.ironIcon);

    // Test forceFit(FIT_TO_PAGE): Updates the icon, does not fire an event.
    toolbar.forceFit(FittingType.FIT_TO_PAGE);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);

    // Force fitting the same fit as the existing fit should do nothing.
    toolbar.forceFit(FittingType.FIT_TO_PAGE);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);

    // Force fit width.
    toolbar.forceFit(FittingType.FIT_TO_WIDTH);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitHeightIcon, fitButton.ironIcon);

    // Force fit height.
    toolbar.forceFit(FittingType.FIT_TO_HEIGHT);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);

    chrome.test.succeed();
  },

  function testZoomButtons() {
    const toolbar = createToolbar();
    toolbar.zoomBounds = {min: 25, max: 500};
    toolbar.viewportZoom = 1;

    let zoomInCount = 0;
    let zoomOutCount = 0;
    toolbar.addEventListener('zoom-in', () => zoomInCount++);
    toolbar.addEventListener('zoom-out', () => zoomOutCount++);

    const zoomButtons = getCrIconButtons(toolbar, 'zoom-controls');
    chrome.test.assertEq(2, zoomButtons.length);
    chrome.test.assertFalse(zoomButtons[0]!.disabled);
    chrome.test.assertFalse(zoomButtons[1]!.disabled);

    // Zoom out
    chrome.test.assertEq('pdf:remove', zoomButtons[0]!.ironIcon);
    zoomButtons[0]!.click();
    chrome.test.assertEq(0, zoomInCount);
    chrome.test.assertEq(1, zoomOutCount);

    // Set zoom to min. Zoom out is disabled.
    toolbar.viewportZoom = .25;
    chrome.test.assertTrue(zoomButtons[0]!.disabled);
    chrome.test.assertFalse(zoomButtons[1]!.disabled);

    // Zoom in
    chrome.test.assertEq('pdf:add', zoomButtons[1]!.ironIcon);
    zoomButtons[1]!.click();
    chrome.test.assertEq(1, zoomInCount);
    chrome.test.assertEq(1, zoomOutCount);

    // Set zoom to max. Zoom in is disabled.
    toolbar.zoomBounds = {min: 25, max: 500};
    toolbar.viewportZoom = 5;
    chrome.test.assertFalse(zoomButtons[0]!.disabled);
    chrome.test.assertTrue(zoomButtons[1]!.disabled);

    chrome.test.succeed();
  },

  async function testRotateButton() {
    const toolbar = createToolbar();
    const rotateButton = getCrIconButtons(toolbar, 'center')[3]!;
    chrome.test.assertEq('pdf:rotate-left', rotateButton.ironIcon);

    const whenRotateLeft = eventToPromise('rotate-left', toolbar);
    rotateButton.click();
    await whenRotateLeft;
    chrome.test.succeed();
  },

  async function testZoomField() {
    const toolbar = createToolbar();
    toolbar.viewportZoom = .8;
    toolbar.zoomBounds = {min: 25, max: 500};
    const zoomField = toolbar.shadowRoot!.querySelector<HTMLInputElement>(
        '#zoom-controls input')!;
    chrome.test.assertEq('80%', zoomField.value);

    // Value is set based on viewport zoom.
    toolbar.viewportZoom = .533;
    chrome.test.assertEq('53%', zoomField.value);

    // Setting a non-number value resets to viewport zoom.
    zoomField.value = 'abc';
    zoomField.dispatchEvent(new CustomEvent('change'));
    chrome.test.assertEq('53%', zoomField.value);

    // Setting a value that is over the max zoom clips to the max value.
    const whenSent = eventToPromise('zoom-changed', toolbar);
    zoomField.value = '90000%';
    zoomField.dispatchEvent(new CustomEvent('change'));
    let event = await whenSent;
    chrome.test.assertEq(500, event.detail);

    // This happens in the parent.
    toolbar.viewportZoom = 5;
    chrome.test.assertEq('500%', zoomField.value);

    // Setting a value that is over the maximum again restores the max
    // value, even though no event is sent.
    zoomField.value = '80000%';
    zoomField.dispatchEvent(new CustomEvent('change'));
    chrome.test.assertEq('500%', zoomField.value);

    // Setting a new value sends the value in a zoom-changed event.
    const whenSentNew = eventToPromise('zoom-changed', toolbar);
    zoomField.value = '110%';
    zoomField.dispatchEvent(new CustomEvent('change'));
    event = await whenSentNew;
    chrome.test.assertEq(110, event.detail);

    // Setting a new value and blurring sends the value in a zoom-changed
    // event. If the value is below the minimum, this sends the minimum
    // zoom.
    const whenSentFromBlur = eventToPromise('zoom-changed', toolbar);
    zoomField.value = '18%';
    zoomField.dispatchEvent(new CustomEvent('blur'));
    event = await whenSentFromBlur;
    chrome.test.assertEq(25, event.detail);
    chrome.test.succeed();
  },

  // Test that the overflow menu closes when an action is triggered.
  function testOverflowMenuCloses() {
    const toolbar = createToolbar();
    const menu = toolbar.$.menu;
    chrome.test.assertFalse(menu.open);

    const more = toolbar.shadowRoot!.querySelector<HTMLElement>('#more')!;
    const buttons = menu.querySelectorAll<HTMLElement>('.dropdown-item');
    chrome.test.assertTrue(buttons.length > 0);

    for (const button of buttons) {
      // Open overflow menu.
      more.click();
      chrome.test.assertTrue(menu.open);
      button.click();
      chrome.test.assertFalse(menu.open);
    }
    chrome.test.succeed();
  },

  async function testTwoPageViewToggle() {
    const toolbar = createToolbar();
    toolbar.twoUpViewEnabled = false;
    const button = toolbar.shadowRoot!.querySelector<HTMLElement>(
        '#two-page-view-button')!;
    assertCheckboxMenuButton(button, false);

    let whenChanged = eventToPromise('two-up-view-changed', toolbar);
    button.click();
    let event = await whenChanged;

    // Happens in the parent.
    toolbar.twoUpViewEnabled = true;
    chrome.test.assertEq(true, event.detail);
    assertCheckboxMenuButton(button, true);
    whenChanged = eventToPromise('two-up-view-changed', toolbar);
    button.click();
    event = await whenChanged;

    // Happens in the parent.
    toolbar.twoUpViewEnabled = false;
    chrome.test.assertEq(false, event.detail);
    assertCheckboxMenuButton(button, false);
    chrome.test.succeed();
  },

  async function testShowAnnotationsToggle() {
    const toolbar = createToolbar();
    const button = toolbar.shadowRoot!.querySelector<HTMLElement>(
        '#show-annotations-button')!;
    assertCheckboxMenuButton(button, true);

    let whenChanged = eventToPromise('display-annotations-changed', toolbar);
    button.click();
    let event = await whenChanged;

    chrome.test.assertEq(false, event.detail);
    assertCheckboxMenuButton(button, false);
    whenChanged = eventToPromise('display-annotations-changed', toolbar);
    button.click();
    event = await whenChanged;

    chrome.test.assertEq(true, event.detail);
    assertCheckboxMenuButton(button, true);
    chrome.test.succeed();
  },

  function testSidenavToggleButton() {
    const toolbar = createToolbar();
    chrome.test.assertFalse(toolbar.sidenavCollapsed);

    const toggleButton = toolbar.$.sidenavToggle;
    chrome.test.assertTrue(toggleButton.hasAttribute('aria-label'));
    chrome.test.assertTrue(toggleButton.hasAttribute('title'));
    chrome.test.assertEq('true', toggleButton.getAttribute('aria-expanded'));

    toolbar.sidenavCollapsed = true;
    chrome.test.assertEq('false', toggleButton.getAttribute('aria-expanded'));

    toolbar.addEventListener(
        'sidenav-toggle-click', () => chrome.test.succeed());
    toggleButton.click();
  },

  async function testPresentButton() {
    const toolbar = createToolbar();
    const button =
        toolbar.shadowRoot!.querySelector<HTMLElement>('#present-button');
    chrome.test.assertTrue(!!button);

    const whenFired = eventToPromise('present-click', toolbar);
    button!.click();
    await whenFired;
    chrome.test.succeed();
  },

  async function testPropertiesButton() {
    const toolbar = createToolbar();
    const button =
        toolbar.shadowRoot!.querySelector<HTMLElement>('#properties-button');
    chrome.test.assertTrue(!!button);

    const whenFired = eventToPromise('properties-click', toolbar);
    button!.click();
    await whenFired;
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
