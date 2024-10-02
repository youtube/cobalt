// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be used throughout personalization app.
 */

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {AmbientModeAlbum, BacklightColor, BLUE_COLOR, GooglePhotosAlbum, GREEN_COLOR, INDIGO_COLOR, PURPLE_COLOR, RED_COLOR, WHITE_COLOR, YELLOW_COLOR} from './../personalization_app.mojom-webui.js';
import {isPersonalizationJellyEnabled} from './load_time_booleans.js';

export interface ColorInfo {
  hexVal: string;
  enumVal: BacklightColor;
}

export const WALLPAPER: string = 'wallpaperColor';
export const WHITE: string = 'whiteColor';
export const RED: string = 'redColor';
export const YELLOW: string = 'yellowColor';
export const GREEN: string = 'greenColor';
export const BLUE: string = 'blueColor';
export const INDIGO: string = 'indigoColor';
export const PURPLE: string = 'purpleColor';
export const RAINBOW: string = 'rainbowColor';

export const staticColorIds =
    [WALLPAPER, WHITE, RED, YELLOW, GREEN, BLUE, INDIGO, PURPLE];

export type PersonalizationAppSelectionEvent =
    MouseEvent&{type: 'click'}|KeyboardEvent&{key: 'Enter'};

/** Returns true if this event is a user action to select an item. */
export function isSelectionEvent(event: Event):
    event is PersonalizationAppSelectionEvent {
  return (event instanceof MouseEvent && event.type === 'click') ||
      (event instanceof KeyboardEvent && event.key === 'Enter');
}

/** Returns the text to display for a number of images. */
export function getCountText(x: number|null|undefined): string {
  switch (x) {
    case null:
    case undefined:
      return '';
    case 0:
      return loadTimeData.getString('zeroImages');
    case 1:
      return loadTimeData.getString('oneImage');
    default:
      if ('number' !== typeof x || x < 0) {
        console.error('Received an impossible value');
        return '';
      }
      return loadTimeData.getStringF('multipleImages', x);
  }
}

/**
 * Returns the number of grid items to render per row given the current inner
 * width of the |window|.
 */
export function getNumberOfGridItemsPerRow(): number {
  return window.innerWidth > 720 ? 4 : 3;
}

/**
 * Checks if argument is an array with non-zero length.
 */
export function isNonEmptyArray(maybeArray: unknown): maybeArray is unknown[] {
  return Array.isArray(maybeArray) && maybeArray.length > 0;
}

/**
 * Checks if argument is a string with non-zero length.
 */
export function isNonEmptyString(maybeString: unknown): maybeString is string {
  return typeof maybeString === 'string' && maybeString.length > 0;
}

/**
 * Checks if a number is within a range.
 */
export function inBetween(
    num: number, minVal: number, maxVal: number): boolean {
  return minVal <= num && num <= maxVal;
}

/** Converts a String16 to a JavaScript String. */
export function decodeString16(str: String16|null): string {
  return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
}

export function isImageDataUrl(maybeDataUrl: Url|null|
                               undefined): maybeDataUrl is Url {
  return !!maybeDataUrl && typeof maybeDataUrl.url === 'string' &&
      (maybeDataUrl.url.startsWith('data:image/png;base64') ||
       maybeDataUrl.url.startsWith('data:image/jpeg;base64'));
}

/** Returns the RGB hex in #ffffff format. */
export function convertToRgbHexStr(hexVal: number): string {
  const PADDING_LENGTH = 6;
  const STRING_LENGTH = 16;
  return `#${
      (hexVal & 0x0FFFFFF)
          .toString(STRING_LENGTH)
          .padStart(PADDING_LENGTH, '0')}`;
}

/**
 * Returns the mapping of preset colors to their hex value and enum value in
 * BacklightColor.
 */
export function getPresetColors(): Record<string, ColorInfo> {
  return {
    [WHITE]: {
      hexVal: convertToRgbHexStr(WHITE_COLOR),
      enumVal: BacklightColor.kWhite,
    },
    [RED]: {
      hexVal: convertToRgbHexStr(RED_COLOR),
      enumVal: BacklightColor.kRed,
    },
    [YELLOW]: {
      hexVal: convertToRgbHexStr(YELLOW_COLOR),
      enumVal: BacklightColor.kYellow,
    },
    [GREEN]: {
      hexVal: convertToRgbHexStr(GREEN_COLOR),
      enumVal: BacklightColor.kGreen,
    },
    [BLUE]: {
      hexVal: convertToRgbHexStr(BLUE_COLOR),
      enumVal: BacklightColor.kBlue,
    },
    [INDIGO]: {
      hexVal: convertToRgbHexStr(INDIGO_COLOR),
      enumVal: BacklightColor.kIndigo,
    },
    [PURPLE]: {
      hexVal: convertToRgbHexStr(PURPLE_COLOR),
      enumVal: BacklightColor.kPurple,
    },
  };
}

/**
 * Returns whether the given album is Recent Highlights.
 */
export function isRecentHighlightsAlbum(album: AmbientModeAlbum|
                                        GooglePhotosAlbum): boolean {
  return album.id === 'RecentHighlights';
}

/**
 * Returns the icon string for the checkmark.
 */
export function getCheckmarkIcon(): string {
  return isPersonalizationJellyEnabled() ? 'personalization:circle_checkmark' :
                                           'personalization:checkmark';
}
