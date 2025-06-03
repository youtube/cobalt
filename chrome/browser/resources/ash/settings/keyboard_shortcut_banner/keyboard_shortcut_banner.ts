// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'keyboard-shortcut-banner' is an element to display reminders
 * for keyboard shortcuts.
 *
 * The body of the reminder should be specified as an array of strings given by
 * the "body" property. Use a <kbd> element to wrap entire shortcuts, and use
 * nested <kbd> elements to signify keys. Do not add spaces around the + between
 * keyboard shortcuts. For example, "Press Ctrl + Space" should be passed in as
 * "Press <kbd><kbd>Ctrl</kbd>+<kbd>Space</kbd></kbd>".
 */
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './keyboard_shortcut_banner.html.js';

const KeyboardShortcutBannerBase = I18nMixin(PolymerElement);

export class KeyboardShortcutBanner extends KeyboardShortcutBannerBase {
  static get is() {
    return 'keyboard-shortcut-banner';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      header: {
        type: String,
      },

      body: {
        type: Array,
      },
    };
  }

  header: string;
  body: TrustedHTML[];

  private onDismissClick_(): void {
    getAnnouncerInstance().announce(this.i18n('shortcutBannerDismissed'));
    this.dispatchEvent(
        new CustomEvent('dismiss', {bubbles: true, composed: true}));
  }

  private getIdForIndex_(index: number): string {
    return `id${index}`;
  }

  private getBodyIds_(): string {
    const ids: string[] = [];
    for (let i = 0; i < this.body.length; i++) {
      ids.push(this.getIdForIndex_(i));
    }
    return ids.join(' ');
  }
}

customElements.define(KeyboardShortcutBanner.is, KeyboardShortcutBanner);

declare global {
  interface HTMLElementTagNameMap {
    'keyboard-shortcut-banner': KeyboardShortcutBanner;
  }
}
