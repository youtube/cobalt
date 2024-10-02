// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './emoji_category_button.html.js';
import {CATEGORY_BUTTON_CLICK, CategoryButtonClickEvent, createCustomEvent} from './events.js';

export class EmojiCategoryButton extends PolymerElement {
  static get is() {
    return 'emoji-category-button' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      name: {type: String, readonly: true},
      icon: {type: String, readonly: true},
      active: {type: Boolean, value: false},
      searchActive: {type: Boolean, value: false},
      gifSupport: {type: Boolean, value: false},
    };
  }
  name: string;
  icon: string;
  active: boolean;
  private gifSupport: boolean;


  private handleClick(): void {
    this.dispatchEvent(
        createCustomEvent(CATEGORY_BUTTON_CLICK, {categoryName: this.name}));
  }

  private calculateClassName(active: boolean, searchActive: boolean): string {
    if (searchActive) {
      return 'category-button-primary';
    }
    return active ? 'category-button-active' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiCategoryButton.is]: EmojiCategoryButton;
  }
  interface HTMLElementEventMap {
    [CATEGORY_BUTTON_CLICK]: CategoryButtonClickEvent;
  }
}


customElements.define(EmojiCategoryButton.is, EmojiCategoryButton);
