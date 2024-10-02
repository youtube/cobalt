// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from './animation.js';
import {assert, assertInstanceof} from './assert.js';
import * as dom from './dom.js';
import {I18nString} from './i18n_string.js';
import * as loadTimeData from './models/load_time_data.js';
import * as state from './state.js';
import * as tooltip from './tooltip.js';
import {AspectRatioSet, Facing, FpsRange, Resolution} from './type.js';

/**
 * Creates a canvas element for 2D drawing.
 *
 * @param params Size of the canvas.
 * @param params.width Width of the canvas.
 * @param params.height Height of the canvas.
 * @return Returns canvas element and the context for 2D drawing.
 */
export function newDrawingCanvas(
    {width, height}: {width: number, height: number}):
    {canvas: HTMLCanvasElement, ctx: CanvasRenderingContext2D} {
  const canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  const ctx =
      assertInstanceof(canvas.getContext('2d'), CanvasRenderingContext2D);
  return {canvas, ctx};
}

/**
 * Converts canvas content to a JPEG Blob.
 */
export function canvasToJpegBlob(canvas: HTMLCanvasElement): Promise<Blob> {
  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => {
      if (blob !== null) {
        resolve(blob);
      } else {
        reject(new Error('Failed to convert canvas to jpeg blob.'));
      }
    }, 'image/jpeg');
  });
}

/**
 * Converts ImageBitmap to a JPEG Blob.
 */
export function bitmapToJpegBlob(bitmap: ImageBitmap): Promise<Blob> {
  const {canvas, ctx} =
      newDrawingCanvas({width: bitmap.width, height: bitmap.height});
  ctx.drawImage(bitmap, 0, 0);
  return canvasToJpegBlob(canvas);
}

/**
 * Types for keyboard shortcuts.
 */
const KEYBOARD_KEYS = [
  ' ',
  '-',
  '=',
  'A',
  'B',
  'C',
  'D',
  'E',
  'F',
  'G',
  'H',
  'I',
  'J',
  'K',
  'L',
  'M',
  'N',
  'O',
  'P',
  'Q',
  'R',
  'S',
  'T',
  'U',
  'V',
  'W',
  'X',
  'Y',
  'Z',
  'ArrowDown',
  'ArrowLeft',
  'ArrowRight',
  'ArrowUp',
  'AudioVolumeUp',
  'AudioVolumeDown',
  'BrowserBack',
  'Delete',  // Alt + Backspace
  'End',     // Ctrl + Alt + ArrowDown
  'Enter',
  'Escape',
  'Home',  // Ctrl + Alt + ArrowUp
  'Tab',
] as const;
const KEYBOARD_KEY_SET = new Set(KEYBOARD_KEYS);
type KeyboardKey = typeof KEYBOARD_KEYS[number];

type WithModifiers<Modifiers extends string[], Key extends string> =
    Modifiers extends [...infer Rest extends string[], infer Last extends
                           string] ?
    WithModifiers<Rest, Key|`${Last}-${Key}`>:
    Key;
export type KeyboardShortcut =
    WithModifiers<['Ctrl', 'Alt', 'Shift'], KeyboardKey>|'Unsupported';

/**
 * Returns a shortcut string, such as Ctrl-Alt-A.
 *
 * @param event Keyboard event.
 * @return Shortcut identifier.
 */
export function getKeyboardShortcut(event: KeyboardEvent): KeyboardShortcut {
  let key = event.key;
  if (key.match(/^[a-z]$/)) {
    key = key.toUpperCase();
  }
  if (!isSupportedKeyboardKey(key)) {
    return 'Unsupported';
  }
  let modifiers: WithModifiers<['Ctrl', 'Alt', 'Shift'], ''> = '';
  if (event.ctrlKey) {
    modifiers = `${modifiers}Ctrl-`;
  }
  if (event.altKey) {
    modifiers = `${modifiers}Alt-`;
  }
  if (event.shiftKey) {
    modifiers = `${modifiers}Shift-`;
  }
  return `${modifiers}${key}`;
}

function isSupportedKeyboardKey(key: string): key is KeyboardKey {
  return KEYBOARD_KEY_SET.has(key as KeyboardKey);
}

/**
 * Sets up i18n messages on DOM subtree by i18n attributes.
 *
 * @param rootElement Root of DOM subtree to be set up with.
 */
export function setupI18nElements(rootElement: DocumentFragment|Element): void {
  function getElements(attr: string) {
    const elements = [...dom.getAllFrom(rootElement, `[${attr}]`, HTMLElement)];
    if (rootElement instanceof HTMLElement && rootElement.hasAttribute(attr)) {
      elements.push(rootElement);
    }
    return elements;
  }
  function getMessage(element: HTMLElement, attr: string) {
    return loadTimeData.getI18nMessage(
        assertEnumVariant(I18nString, element.getAttribute(attr)));
  }
  function setAriaLabel(element: HTMLElement, attr: string) {
    element.setAttribute('aria-label', getMessage(element, attr));
  }

  for (const element of getElements('i18n-text')) {
    element.textContent = getMessage(element, 'i18n-text');
  }
  for (const element of getElements('i18n-tooltip-true')) {
    element.setAttribute(
        'tooltip-true', getMessage(element, 'i18n-tooltip-true'));
  }
  for (const element of getElements('i18n-tooltip-false')) {
    element.setAttribute(
        'tooltip-false', getMessage(element, 'i18n-tooltip-false'));
  }
  for (const element of getElements('i18n-aria')) {
    setAriaLabel(element, 'i18n-aria');
  }
  for (const element of tooltip.setup(getElements('i18n-label'))) {
    setAriaLabel(element, 'i18n-label');
  }
}

/**
 * Reads blob into Image.
 */
export function blobToImage(blob: Blob): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error('Failed to load unprocessed image'));
    img.src = URL.createObjectURL(blob);
  });
}

/**
 * Gets default facing according to device mode.
 */
export function getDefaultFacing(): Facing {
  return state.get(state.State.TABLET) ? Facing.ENVIRONMENT : Facing.USER;
}

/**
 * Toggle checked value of element.
 */
export function toggleChecked(
    element: HTMLInputElement, checked: boolean): void {
  element.checked = checked;
  element.dispatchEvent(new Event('change'));
}

/**
 * Binds on/off of specified state with different aria label on an element.
 */
export function bindElementAriaLabelWithState(
    {element, state: s, onLabel, offLabel}: {
      element: Element,
      state: state.State,
      onLabel: I18nString,
      offLabel: I18nString,
    }): void {
  function update(value: boolean) {
    const label = value ? onLabel : offLabel;
    element.setAttribute('i18n-label', label);
    element.setAttribute('aria-label', loadTimeData.getI18nMessage(label));
  }
  update(state.get(s));
  state.addObserver(s, update);
}

/**
 * Sets inkdrop effect on button or label in setting menu.
 */
export function setInkdropEffect(el: HTMLElement): void {
  const tpl = instantiateTemplate('#inkdrop-template');
  const ripple =
      assertInstanceof(tpl.querySelector('.inkdrop-ripple'), HTMLElement);
  el.appendChild(tpl);
  el.addEventListener('click', async (e) => {
    const tRect =
        assertInstanceof(e.target, HTMLElement).getBoundingClientRect();
    const elRect = el.getBoundingClientRect();
    const dropX = tRect.left + e.offsetX - elRect.left;
    const dropY = tRect.top + e.offsetY - elRect.top;
    const maxDx = Math.max(Math.abs(dropX), Math.abs(elRect.width - dropX));
    const maxDy = Math.max(Math.abs(dropY), Math.abs(elRect.height - dropY));
    const radius = Math.hypot(maxDx, maxDy);
    el.style.setProperty('--drop-x', `${dropX}px`);
    el.style.setProperty('--drop-y', `${dropY}px`);
    el.style.setProperty('--drop-radius', `${radius}px`);
    await animate.play(ripple);
  });
}

/**
 * Instantiates template with the target selector.
 */
export function instantiateTemplate(selector: string): DocumentFragment {
  const tpl = dom.get(selector, HTMLTemplateElement);
  const doc = assertInstanceof(
      document.importNode(tpl.content, true), DocumentFragment);
  setupI18nElements(doc);
  return doc;
}

/**
 * Sleeps for a specified time.
 *
 * @param ms Milliseconds to sleep.
 */
export function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

/**
 * Gets value in px of a property in a StylePropertyMapReadOnly.
 */
export function getStyleValueInPx(
    style: (StylePropertyMap|StylePropertyMapReadOnly), prop: string): number {
  return assertInstanceof(style.get(prop), CSSNumericValue).to('px').value;
}

/**
 * Trigger callback in fixed interval like |setInterval()| with specified delay
 * before calling the first callback.
 */
export class DelayInterval {
  private intervalId: number|null = null;

  private readonly delayTimeoutId: number;

  /**
   * @param callback Callback to be triggered in fixed interval.
   * @param delayMs Delay milliseconds at start.
   * @param intervalMs Interval in milliseconds.
   */
  constructor(callback: () => void, delayMs: number, intervalMs: number) {
    this.delayTimeoutId = setTimeout(() => {
      this.intervalId = setInterval(() => {
        callback();
      }, intervalMs);
      callback();
    }, delayMs);
  }

  /**
   * Stop the interval.
   */
  stop(): void {
    if (this.intervalId === null) {
      clearTimeout(this.delayTimeoutId);
    } else {
      clearInterval(this.intervalId);
    }
  }
}

/**
 * Share file with share API.
 */
export async function share(file: File): Promise<void> {
  const shareData = {files: [file]};
  try {
    if (!navigator.canShare(shareData)) {
      throw new Error('cannot share');
    }
    await navigator.share(shareData);
  } catch (e) {
    // TODO(b/191950622): Handles all share error case, e.g. no
    // share target, share abort... with right treatment like toast
    // message.
  }
}

/**
 * Check if a string value is a variant of an enum.
 *
 * @param enumType The enum type to be checked.
 * @param value Value to be checked.
 * @return The value if it's an enum variant, null otherwise.
 */
export function checkEnumVariant<T extends string>(
    enumType: {[key: string]: T}, value: string|null|undefined): T|null {
  if (value === null || value === undefined ||
      !Object.values<string>(enumType).includes(value)) {
    return null;
  }
  return value as T;
}

/**
 * Asserts that a string value is a variant of an enum.
 *
 * @param enumType The enum type to be checked.
 * @param value Value to be checked.
 * @return The value if it's an enum variant, throws assertion error otherwise.
 */
export function assertEnumVariant<T extends string>(
    enumType: {[key: string]: T}, value: string|null|undefined): T {
  const ret = checkEnumVariant(enumType, value);
  assert(ret !== null, `${value} is not a valid enum variant`);
  return ret;
}

/**
 * Crops out maximum possible centered square from the image blob.
 *
 * @return Promise with result cropped square image.
 */
export async function cropSquare(blob: Blob): Promise<Blob> {
  const img = await blobToImage(blob);
  try {
    const side = Math.min(img.width, img.height);
    const {canvas, ctx} = newDrawingCanvas({width: side, height: side});
    ctx.drawImage(
        img, Math.floor((img.width - side) / 2),
        Math.floor((img.height - side) / 2), side, side, 0, 0, side, side);
    // TODO(b/174190121): Patch important exif entries from input blob to
    // result blob.
    const croppedBlob = await canvasToJpegBlob(canvas);
    return croppedBlob;
  } finally {
    URL.revokeObjectURL(img.src);
  }
}

/**
 * Returns the mapped aspect ratio set according to the given resolution.
 */
export function toAspectRatioSet(resolution: Resolution|null): AspectRatioSet {
  switch (resolution?.aspectRatio) {
    case 1.3333:
      return AspectRatioSet.RATIO_4_3;
    case 1.7778:
      return AspectRatioSet.RATIO_16_9;
    default:
      return AspectRatioSet.RATIO_OTHER;
  }
}

/**
 * Extract first url from CSS background-image value if exist.
 */
export function extractBackgroundImageValueUrl(element: HTMLElement): string|
    null {
  const style = element.attributeStyleMap;
  const imageValue = style.get('background-image');
  // attributeStyleMap.get() returns null instead of undefined if the property
  // does not exist. Check undefined for type narrowing.
  if (imageValue === null || imageValue === undefined) {
    return null;
  }
  const match = imageValue.toString().match(/url\(['"](.*)['"]\)/);
  return match?.[1] ?? null;
}

/**
 * Load the image element with given blob.
 */
export async function loadImage(
    image: HTMLImageElement, data: Blob|string): Promise<void> {
  const src = typeof data === 'string' ? data : URL.createObjectURL(data);
  return new Promise((resolve, reject) => {
    image.onload = () => resolve();
    image.onerror = (e) => {
      reject(new Error(`Failed to load image: ${e}`));
      URL.revokeObjectURL(image.src);
    };
    image.src = src;
  });
}

/**
 * Gets the mapping from name to enum value for a number enum.
 *
 * Note that in TypeScript, number enum contains both mapping from name to
 * value and value to name, which most of the time isn't what we want.
 */
export function getNumberEnumMapping<T extends number>(
    enumType: {[key: string]: T|string}): {[key: string]: T} {
  return Object.fromEntries(Object.entries(enumType).flatMap(([k, v]) => {
    if (typeof v === 'string') {
      return [];
    }
    return [[k, v]];
  }));
}

/**
 * Returns FPS range from media track constraints.
 */
export function getFpsRangeFromConstraints(frameRate: ConstrainDouble|
                                           undefined): FpsRange {
  let minFps = 0;
  let maxFps = 0;
  // For devices that don't support constant frame rate, we pass {0,0} and let
  // VCD fall back to the default range.
  if (frameRate) {
    if (typeof frameRate === 'number') {
      minFps = frameRate;
      maxFps = frameRate;
    } else if (frameRate.exact) {
      minFps = frameRate.exact;
      maxFps = frameRate.exact;
    }
  }
  return {minFps, maxFps};
}
