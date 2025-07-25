// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This file is checked via TS, so we suppress Closure checks.
 * @suppress {checkTypes}
 */

import {str} from '../../../../common/js/translations.js';
import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {constants} from '../../constants.js';

import {getTemplate} from './shared_with_crostini_pluginvm_banner.html.js';
import {StateBanner} from './state_banner.js';
import {BANNER_INFINITE_TIME} from './types.js';

/**
 * The custom element tag name.
 */
export const TAG_NAME = 'shared-with-crostini-pluginvm-banner';

/**
 * A banner that shows if the current navigated directory has been shared with
 * Crostini or PluginVM.
 */
export class SharedWithCrostiniPluginVmBanner extends StateBanner {
  /**
   * Returns the HTML template for this banner.
   */
  override getTemplate() {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    return fragment;
  }

  /**
   * This banner relies on a custom trigger registered in the BannerController
   * and thus the following list are root types where sharing to Crostini or
   * PluginVM is allowed and thus a banner may appear.
   */
  override allowedVolumes() {
    return [
      {root: VolumeManagerCommon.RootType.DOWNLOADS},
      {root: VolumeManagerCommon.RootType.REMOVABLE},
      {root: VolumeManagerCommon.RootType.ANDROID_FILES},
      {root: VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT},
      {root: VolumeManagerCommon.RootType.COMPUTER},
      {root: VolumeManagerCommon.RootType.DRIVE},
      {root: VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT},
      {root: VolumeManagerCommon.RootType.SHARED_DRIVE},
      {root: VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME},
      {root: VolumeManagerCommon.RootType.CROSTINI},
      {root: VolumeManagerCommon.RootType.ARCHIVE},
      {root: VolumeManagerCommon.RootType.SMB},
    ];
  }

  /**
   * Persist the banner at all times if the folder is shared.
   */
  override timeLimit() {
    return BANNER_INFINITE_TIME;
  }

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner. This type is used to identify if this folder is
   * shared with Crostini, PluginVM or both and update the text and links
   * accordingly.
   */
  override onFilteredContext(context: {type: string}) {
    if (!context || !context.type) {
      console.warn('Context not supplied or type key missing');
      return;
    }
    const text =
        this.shadowRoot!.querySelector<HTMLSpanElement>('span[slot="text"]')!;
    const button = this.shadowRoot!.querySelector<HTMLButtonElement>(
        'cr-button[slot="extra-button"]')!;
    if (context.type ===
        (constants.DEFAULT_CROSTINI_VM + constants.PLUGIN_VM)) {
      text.innerText = str('MESSAGE_FOLDER_SHARED_WITH_CROSTINI_AND_PLUGIN_VM');
      button.setAttribute(
          'href', 'chrome://os-settings/app-management/pluginVm/sharedPaths');
      return;
    }
    if (context.type === constants.PLUGIN_VM) {
      text.innerText = str('MESSAGE_FOLDER_SHARED_WITH_PLUGIN_VM');
      button.setAttribute(
          'href', 'chrome://os-settings/app-management/pluginVm/sharedPaths');
      return;
    }
    text.innerText = str('MESSAGE_FOLDER_SHARED_WITH_CROSTINI');
    button.setAttribute('href', 'chrome://os-settings/crostini/sharedPaths');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TAG_NAME]: SharedWithCrostiniPluginVmBanner;
  }
}

customElements.define(TAG_NAME, SharedWithCrostiniPluginVmBanner);
