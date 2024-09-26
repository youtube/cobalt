// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists, assertInstanceof} from '../assert.js';
import * as dom from '../dom.js';
import * as localStorage from '../models/local_storage.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as state from '../state.js';
import {Facing, Resolution} from '../type.js';
import {sleep} from '../util.js';
import {windowController} from '../window_controller.js';

import {
  SELECTOR_MAP,
  SETTING_MENU_MAP,
  SETTING_OPTION_MAP,
  SettingMenu,
  SettingOption,
  UIComponent,
} from './cca_type.js';

/**
 * Possible HTMLElement types that can have a boolean attribute "disabled".
 */
type HTMLElementWithDisabled = HTMLElement&{disabled: boolean};
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
   *
   * @param shouldSupportDeviceOperator True if the device should support
   * DeviceOperator.
   * @return The promise resolves successfully if the check passes.
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
   * Returns the number of ui elements of the specified component.
   */
  static countVisibleUI(component: UIComponent): number {
    return getVisibleElementList(component).length;
  }

  /**
   * Returns whether the UI exist in the current DOM tree.
   */
  static exists(component: UIComponent): boolean {
    const elements = getElementList(component);
    return elements.length > 0;
  }

  /**
   * Focuses the window.
   */
  static focusWindow(): Promise<void> {
    return windowController.focus();
  }

  /**
   * Makes the window fullscreen.
   */
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
    if (!deviceOperator) {
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
   * Gets number of camera devices.
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
   * Return whether the state associated to the open is checked.
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

  /**
   * Returns the screen orientation.
   */
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
   * Returns disabled attribute of the component. In case the element with
   * "disabled" attribute, always returns false.
   */
  static isDisabled(component: UIComponent, index?: number): boolean {
    const element = resolveElement(component, index);
    const withDisabledElement = element as HTMLElementWithDisabled;
    if (withDisabledElement.disabled === undefined) {
      return false;
    }
    return withDisabledElement.disabled;
  }

  /**
   * Checks whether the setting menu is opened.
   */
  static isSettingMenuOpened(menu: SettingMenu): boolean {
    assert(menu !== undefined, 'Invalid SettingMenu value');
    const view = SETTING_MENU_MAP[menu].view;
    return state.get(view);
  }

  /**
   * Checks whether the preview video stream has been set and the stream status
   * is active.
   *
   * @return Whether the preview video is active.
   */
  static isVideoActive(): boolean {
    const video = getPreviewVideo();
    return video.srcObject instanceof MediaStream && video.srcObject.active;
  }

  /**
   * Returns whether the UI component is current visible.
   */
  static isVisible(component: UIComponent, index?: number): boolean {
    const element = resolveElement(component, index);
    return isVisibleElement(element);
  }

  /**
   * Maximizes the window.
   */
  static maximizeWindow(): Promise<void> {
    return windowController.maximize();
  }

  /**
   * Minimizes the window.
   */
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
   * Sets input value of the component. Throws an error if the component is not
   * HTMLInputElement with type "range", or the value is not within [min, max]
   * range.
   */
  static setRangeInputValue(component: UIComponent, value: number): void {
    const {max, min} = CCATest.getInputRange(component);
    if (value < min || value > max) {
      new Error(`Invalid value ${value} within range ${min}-${max}`);
    }

    const element = getRangeInputComponent(component);
    element.value = value.toString();
    element.dispatchEvent(new Event('change'));
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
}
