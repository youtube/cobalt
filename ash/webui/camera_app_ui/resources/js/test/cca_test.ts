// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertEnumVariant,
  assertExists,
  assertInstanceof,
} from '../assert.js';
import {TIME_LAPSE_INITIAL_SPEED} from '../device/mode/video.js';
import * as dom from '../dom.js';
import * as localStorage from '../models/local_storage.js';
import {
  TIME_LAPSE_MAX_DURATION,
  TimeLapseSaver,
} from '../models/video_saver.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as state from '../state.js';
import {Facing, Mode, Resolution} from '../type.js';
import * as untrustedScripts from '../untrusted_scripts.js';
import {FpsObserver, sleep} from '../util.js';
import {windowController} from '../window_controller.js';

import {
  SELECTOR_MAP,
  SETTING_MENU_MAP,
  SETTING_OPTION_MAP,
  SettingMenu,
  SettingOption,
  UIComponent,
} from './cca_type.js';

interface Coordinate {
  x: number;
  y: number;
}

interface InputRange {
  max: number;
  min: number;
}

/**
 * Get HTMLInputElement from the specified component and ensure that the type of
 * the input element is "range".
 */
function getRangeInputComponent(component: UIComponent): HTMLInputElement {
  const element = resolveElement(component);
  const inputElement = assertInstanceof(
      element, HTMLInputElement,
      'The provided element is not an input element');
  assert(
      inputElement.type === 'range',
      'The provided element is not an input with type range');
  return inputElement;
}

/**
 * Returns HTMLVideoElement for the preview video.
 */
function getPreviewVideo(): HTMLVideoElement {
  const previewVideo = resolveElement('previewVideo');
  return assertInstanceof(previewVideo, HTMLVideoElement);
}

/**
 * Returns MediaStream from the preview video.
 */
function getPreviewVideoStream(): MediaStream {
  return assertInstanceof(getPreviewVideo().srcObject, MediaStream);
}

/**
 * Returns MediaStreamTrack from the preview video stream.
 */
function getPreviewVideoTrack(): MediaStreamTrack {
  const track = getPreviewVideoStream().getVideoTracks()[0];
  return assertInstanceof(track, MediaStreamTrack);
}

/**
 * Resolves selector of the component and returns a list of HTML elements with
 * that selector.
 */
function getElementList(component: UIComponent): HTMLElement[] {
  const selector = SELECTOR_MAP[component];
  // Value from Tast may not be UIComponent and results in undefined.
  assert(selector !== undefined, 'Invalid UIComponent value.');

  const elements = Array.from(dom.getAll(selector, HTMLElement));
  return elements;
}


/**
 * Returns a list of HTML elements which are visible.
 */
function getVisibleElementList(component: UIComponent): HTMLElement[] {
  const elements = getElementList(component);
  const visibleElements =
      elements.filter((element) => isVisibleElement(element));
  return visibleElements;
}

/**
 * Returns whether the given element is current visible.
 */
function isVisibleElement(element: HTMLElement): boolean {
  const style = window.getComputedStyle(element);
  const opacity = Number(style.opacity);
  return style.visibility !== 'hidden' && element.getClientRects().length > 0 &&
      opacity > 0;
}

/**
 * Resolves HTMLElement of the specified ui |component|. If |index| is
 * specified, returns the |index|'th element, else returns the first element
 * found. This will throw an error if it cannot be resolved.
 */
function resolveElement(component: UIComponent, index = 0): HTMLElement {
  const elements = getElementList(component);
  assert(
      index < elements.length,
      `Cannot access index ${index} from array of size ${elements.length}`);
  return elements[index];
}

/**
 * Resolves the |index|'th visible HTMLElement of the specified ui |component|.
 */
function resolveVisibleElement(component: UIComponent, index = 0): HTMLElement {
  const elements = getVisibleElementList(component);
  assert(
      index < elements.length,
      `Cannot access index ${index} from array of size ${elements.length}`);
  return elements[index];
}

/**
 * Return test functionalities to be used in Tast automation test.
 */
export class CCATest {
  /**
   * Checks if mojo connection could be constructed without error. In this check
   * we only check if the path works and does not check for the correctness of
   * each mojo calls.
   */
  static async checkMojoConnection(shouldSupportDeviceOperator: boolean):
      Promise<void> {
    // Checks if ChromeHelper works. It should work on all devices.
    const chromeHelper = ChromeHelper.getInstance();
    await chromeHelper.isTabletMode();

    const isDeviceOperatorSupported = DeviceOperator.isSupported();
    if (shouldSupportDeviceOperator !== isDeviceOperatorSupported) {
      throw new Error(`DeviceOperator support mismatch. Expected: ${
          shouldSupportDeviceOperator} Actual: ${isDeviceOperatorSupported}`);
    }

    // Checks if DeviceOperator works on v3 devices.
    if (isDeviceOperatorSupported) {
      const deviceOperator = DeviceOperator.getInstance();
      assert(deviceOperator !== null, 'Failed to get deviceOperator instance.');
      const devices = (await navigator.mediaDevices.enumerateDevices())
                          .filter(({kind}) => kind === 'videoinput');
      await deviceOperator.getCameraFacing(devices[0].deviceId);
    }
  }

  static visitedFocusedElementSet = new Set();

  /**
   * Checks if the focused element is in `visitedFocusedElementSet`.
   */
  static checkFocusedElementVisited(): boolean {
    const focused = document.activeElement;
    if (this.visitedFocusedElementSet.has(focused)) {
      return true;
    }

    this.visitedFocusedElementSet.add(focused);
    return false;
  }

  /**
   * Returns aria-label of the focused element. Throws an error if a focused
   * element is null.
   */
  static getFocusedElementAriaLabel(): string|null {
    if (document.activeElement === null) {
      throw new Error(`There is no active element`);
    }
    return document.activeElement.ariaLabel;
  }

  /**
   * Clicks on the UI component if it's visible.
   */
  static click(component: UIComponent, index?: number): void {
    const element = resolveVisibleElement(component, index);
    element.click();
  }

  /**
   * Clicks on the back button to close the setting menu.
   */
  static closeSettingMenu(menu: SettingMenu): void {
    assert(menu !== undefined, 'Invalid SettingMenu value');
    const view = SETTING_MENU_MAP[menu].view;
    const selector = `#${view} .menu-header button`;
    const closeButton = dom.get(selector, HTMLButtonElement);
    assert(
        isVisibleElement(closeButton),
        `Close button for settings menu ${menu} is not visible.`);
    closeButton.click();
  }

  /**
   * Returns the number of ui elements of the specified component.
   */
  static countUI(component: UIComponent): number {
    return getElementList(component).length;
  }

  /**
   * Returns the number of visible ui elements of the specified component.
   */
  static countVisibleUI(component: UIComponent): number {
    return getVisibleElementList(component).length;
  }

  /**
   * Returns whether the UI exists in the current DOM tree.
   */
  static exists(component: UIComponent): boolean {
    const elements = getElementList(component);
    return elements.length > 0;
  }

  static focusWindow(): Promise<void> {
    return windowController.focus();
  }

  static fullscreenWindow(): Promise<void> {
    return windowController.fullscreen();
  }

  /**
   * Returns the attribute |attr| of the |index|'th ui.
   */
  static getAttribute(component: UIComponent, attr: string, index?: number):
      string|null {
    const element = resolveElement(component, index);
    return element.getAttribute(attr);
  }

  /**
   * Gets device id of current active camera device.
   */
  static getDeviceId(): string {
    const deviceId = getPreviewVideoTrack().getSettings().deviceId;
    return assertExists(deviceId, 'Invalid deviceId');
  }

  /**
   * Gets facing of current active camera device.
   *
   * @return The facing string 'user', 'environment', 'external'. Returns
   *     'unknown' if current device does not support device operator.
   */
  static async getFacing(): Promise<string> {
    const track = getPreviewVideoTrack();
    const deviceOperator = DeviceOperator.getInstance();
    if (deviceOperator === null) {
      const facing = track.getSettings().facingMode;
      return facing ?? 'unknown';
    }

    const deviceId = CCATest.getDeviceId();
    const facing = await deviceOperator.getCameraFacing(deviceId);
    switch (facing) {
      case Facing.USER:
      case Facing.ENVIRONMENT:
      case Facing.EXTERNAL:
        return facing;
      default:
        throw new Error(`Unexpected CameraFacing value: ${facing}`);
    }
  }

  /**
   * Get [min, max] range of the component. Throws an error if the component is
   * not HTMLInputElement with type "range".
   */
  static getInputRange(component: UIComponent): InputRange {
    const element = getRangeInputComponent(component);
    const max = Number(element.max);
    const min = Number(element.min);
    assert(!isNaN(max) && !isNaN(min), 'Min or max is not a number.');
    return {max, min};
  }

  /**
   * Gets the number of camera devices.
   */
  static async getNumOfCameras(): Promise<number> {
    const devices = await navigator.mediaDevices.enumerateDevices();
    return devices
        .filter(
            (d) => d.kind === 'videoinput' &&
                !d.label.startsWith('Virtual Camera'))
        .length;
  }

  /**
   * Return whether the state associated to the option is checked.
   */
  static getOptionState(option: SettingOption): boolean {
    assert(option !== undefined, 'Invalid SettingOption value.');
    const optionState = SETTING_OPTION_MAP[option].state;
    return state.get(optionState);
  }

  /**
   * Creates a canvas which renders a frame from the preview video.
   *
   * @return Context from the created canvas.
   */
  static getPreviewFrame(): OffscreenCanvasRenderingContext2D {
    const video = getPreviewVideo();
    const canvas = new OffscreenCanvas(video.videoWidth, video.videoHeight);
    const ctx = canvas.getContext('2d');
    assert(ctx !== null, 'Failed to get canvas context.');
    ctx.drawImage(video, 0, 0);
    return ctx;
  }

  static getScreenOrientation(): OrientationType {
    return window.screen.orientation.type;
  }

  /**
   * Gets screen x, y of the center of |index|'th ui component.
   */
  static getScreenXY(component: UIComponent, index?: number): Coordinate {
    const element = resolveVisibleElement(component, index);
    const rect = element.getBoundingClientRect();
    const actionBarH = window.outerHeight - window.innerHeight;
    return {
      x: Math.round(rect.x + window.screenX),
      y: Math.round(rect.y + actionBarH + window.screenY),
    };
  }

  /**
   * Gets rounded numbers of width and height of the specified ui component.
   */
  static getSize(component: UIComponent, index?: number): Resolution {
    const element = resolveVisibleElement(component, index);
    const {width, height} = element.getBoundingClientRect();
    return new Resolution(Math.round(width), Math.round(height));
  }

  /**
   * Gets current boolean value of |key|.
   */
  static getState(key: string): boolean {
    const stateKey = state.assertState(key);
    return state.get(stateKey);
  }

  /**
   * Calculates the expected duration of the time-lapse video recorded for
   * |recordDuration| seconds.
   */
  static getTimeLapseDuration(recordDuration: number): number {
    let speed = TIME_LAPSE_INITIAL_SPEED;
    let duration = recordDuration / speed;
    while (duration >= TIME_LAPSE_MAX_DURATION) {
      speed = TimeLapseSaver.getNextSpeed(speed);
      duration = recordDuration / speed;
    }
    return duration;
  }

  /**
   * Performs mouse hold by sending pointerdown and pointerup events.
   */
  static async hold(component: UIComponent, ms: number, index?: number):
      Promise<void> {
    const element = resolveVisibleElement(component, index);
    element.dispatchEvent(new Event('pointerdown'));
    await sleep(ms);
    element.dispatchEvent(new Event('pointerup'));
  }

  /**
   * Returns checked attribute of component. Throws an error if the component is
   * not HTMLInputElement.
   */
  static isChecked(component: UIComponent, index?: number): boolean {
    const element = resolveElement(component, index);
    const inputElement = assertInstanceof(element, HTMLInputElement);
    return inputElement.checked;
  }

  /**
   * Returns disabled attribute of the component. In case the element without
   * "disabled" attribute, always returns false.
   */
  static isDisabled(component: UIComponent, index?: number): boolean {
    const element = resolveElement(component, index);
    if ('disabled' in element && typeof element.disabled === 'boolean') {
      return element.disabled;
    }
    return false;
  }

  static isSettingMenuOpened(menu: SettingMenu): boolean {
    assert(menu !== undefined, 'Invalid SettingMenu value');
    const view = SETTING_MENU_MAP[menu].view;
    return state.get(view);
  }

  /**
   * Checks whether the preview video stream has been set and the stream status
   * is active.
   */
  static isVideoActive(): boolean {
    const video = getPreviewVideo();
    return video.srcObject instanceof MediaStream && video.srcObject.active;
  }

  /**
   * Returns whether the UI component is currently visible.
   */
  static isVisible(component: UIComponent, index?: number): boolean {
    const element = resolveElement(component, index);
    return isVisibleElement(element);
  }

  static maximizeWindow(): Promise<void> {
    return windowController.maximize();
  }

  static minimizeWindow(): Promise<void> {
    return windowController.minimize();
  }

  /**
   * Clicks on the component to open setting menu.
   */
  static openSettingMenu(menu: SettingMenu): void {
    assert(menu !== undefined, 'Invalid SettingMenu value');
    const component = SETTING_MENU_MAP[menu].component;
    CCATest.click(component);
  }

  /**
   * Selects the select component with the option with the provided value.
   */
  static selectOption(component: UIComponent, value: string): void {
    const element = resolveElement(component);
    const selectElement = assertInstanceof(element, HTMLSelectElement);
    const option =
        Array.from(selectElement.options).find((opt) => opt.value === value);
    assert(
        option !== undefined,
        `There is no ${value} option in the select element`);

    selectElement.value = value;
    selectElement.dispatchEvent(new Event('change'));
  }

  /**
   * Hides all toasts, nudges and tooltips.
   */
  static hideFloatingUI(): void {
    state.set(state.State.HIDE_FLOATING_UI_FOR_TESTING, true);
  }

  /**
   * Sets input value of the component. Throws an error if the component is not
   * HTMLInputElement with type "range", or the value is not within [min, max]
   * range.
   */
  static setRangeInputValue(component: UIComponent, value: number): void {
    const {max, min} = CCATest.getInputRange(component);
    if (value < min || value > max) {
      throw new Error(`Invalid value ${value} within range ${min}-${max}`);
    }

    const element = getRangeInputComponent(component);
    element.value = value.toString();
    element.dispatchEvent(new Event('change'));
  }

  /**
   * Switches to the specified camera mode.
   */
  static switchMode(mode: Mode): void {
    assertEnumVariant(Mode, mode);
    const modeSelector =
        dom.get(`.mode-item>input[data-mode="${mode}"]`, HTMLInputElement);
    assert(isVisibleElement(modeSelector), 'Mode selector is not visible');
    modeSelector.click();
  }

  /**
   * Removes all the cached data in chrome.storage.local.
   */
  static removeCacheData(): void {
    return localStorage.clear();
  }

  /**
   * Restores the window and leaves maximized/minimized/fullscreen state.
   */
  static restoreWindow(): Promise<void> {
    return windowController.restore();
  }

  /**
   * Toggles expert mode by simulating the activation key press.
   */
  static toggleExpertMode(): void {
    document.body.dispatchEvent(new KeyboardEvent(
        'keydown', {ctrlKey: true, shiftKey: true, key: 'E'}));
  }

  /**
   * Toggles the settings option.
   */
  static toggleOption(option: SettingOption): void {
    assert(option !== undefined, 'Invalid SettingOption value.');
    const component = SETTING_OPTION_MAP[option].component;
    CCATest.click(component);
  }

  static getFpsObserver(): FpsObserver {
    return new FpsObserver(getPreviewVideo());
  }

  /**
   * Waits until the state |key| is changed to |expected| and resolves the
   * millisecond unix timestamp of the state change.
   */
  static waitStateChange(key: string, expected: boolean): Promise<number> {
    const stateKey = state.assertState(key);
    const current = state.get(stateKey);
    if (current === expected) {
      throw new Error(`Cannot start observing because the state of ${
          stateKey} is already ${expected}`);
    }
    return new Promise((resolve, reject) => {
      function onChange(newState: boolean) {
        state.removeObserver(stateKey, onChange);
        if (newState !== expected) {
          reject(
              new Error(`The changed "${stateKey}" state is not ${expected}`));
        }
        resolve(Date.now());
      }
      state.addObserver(stateKey, onChange);
    });
  }

  /**
   * Sets measurement protocol's URL.
   */
  static async setMeasurementProtocolUrl(url: string): Promise<void> {
    const helper = await untrustedScripts.getGaHelper();
    return helper.setMeasurementProtocolUrl(url);
  }

  /**
   * Enables GA4 metrics.
   */
  static async enableGa4Metrics(): Promise<void> {
    const helper = await untrustedScripts.getGaHelper();
    return helper.setGa4Enabled(true);
  }
}
