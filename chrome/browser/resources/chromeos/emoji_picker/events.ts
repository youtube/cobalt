// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CategoryEnum, Emoji, VisualContent} from './types';

export type CategoryButtonClickEvent =
    CustomEvent<{categoryName: CategoryEnum}>;

export const CATEGORY_BUTTON_CLICK = 'category-button-click';

export type GroupButtonClickEvent = CustomEvent<{group: string}>;

export const GROUP_BUTTON_CLICK = 'group-button-click';

export type EmojiTextButtonClickEvent = CustomEvent<{
  emoji: string,
  isVariant: boolean,
  baseEmoji: string,
  allVariants: Emoji[],
  name: string,
  text: string,
  category: CategoryEnum,
}>;

export const EMOJI_TEXT_BUTTON_CLICK = 'emoji-text-button-click';

export type EmojiImgButtonClickEvent = CustomEvent<{
  name: string,
  visualContent: VisualContent,
  category: CategoryEnum,
}>;

export const EMOJI_IMG_BUTTON_CLICK = 'emoji-img-button-click';

/**
 * TODO(b/233130994): Update the type after removing emoji-button.
 * The current event type is used as an intermediate step for a refactor
 * leading to the removal of emoji-button. Therefore, its current state allows
 * keeping variants events for both emoji-button and emoji-group valid at the
 * same time. It will be be improved after removing emoji-button.
 */
export type EmojiVariantsShownEvent =
    CustomEvent<{owner?: Element, variants?: HTMLElement, baseEmoji: string}>;

export const EMOJI_VARIANTS_SHOWN = 'emoji-variants-shown';

export type CategoryDataLoadEvent = CustomEvent<{category: string}>;

/**
 * The event that data of a category is fetched and processed for rendering.
 * Note: this event does not indicate if rendering of the category data is
 * completed or not.
 */
export const CATEGORY_DATA_LOADED = 'category-data-loaded';

export type EmojiPickerReadyEvent = CustomEvent;

/**
 * The event that the user clicks try again when there is either a network or
 * http error when trying to fetch for gifs.
 */
export const GIF_ERROR_TRY_AGAIN = 'gif-error-try-again';

export type GifErrorTryAgainEvent = CustomEvent;

/**
 * The event that all the data are loaded and rendered and all the
 * emoji-picker functionalities are ready to use.
 */
export const EMOJI_PICKER_READY = 'emoji-picker-ready';

export type EmojiClearRecentClickEvent = CustomEvent;

export const EMOJI_CLEAR_RECENTS_CLICK = 'emoji-clear-recents-click';

/**
 * Constructs a CustomEvent with the given event type and details.
 * The event will bubble up through elements and components.
 *
 * @param {string} type event type
 * @param {T=} detail event details
 * @return {!CustomEvent<T>} custom event
 * @template T event detail type
 */
export function createCustomEvent<T>(type: string, detail: T) {
  return new CustomEvent(type, {bubbles: true, composed: true, detail});
}
