// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {File} from '../../drive.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../i18n_setup.js';
import {InfoDialogElement} from '../info_dialog.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {DriveProxy} from './drive_module_proxy.js';
import {getTemplate} from './module.html.js';

export interface DriveModuleElement {
  $: {
    files: HTMLElement,
    fileRepeat: DomRepeat,
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
  };
}

/**
 * The Drive module, which serves as an inside look in to recent activity within
 * a user's Google Drive.
 */
export class DriveModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-drive-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      files: Array,
    };
  }

  files: File[];

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onDismissButtonClick_() {
    DriveProxy.getHandler().dismissModule();
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage',
            loadTimeData.getString('modulesDriveFilesSentence')),
        restoreCallback: () => DriveProxy.getHandler().restoreModule(),
      },
    }));
  }

  private onDisableButtonClick_() {
    this.dispatchEvent(new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesDriveSentence2')),
      },
    }));
  }

  private getImageSrc_(file: File): string {
    return 'https://drive-thirdparty.googleusercontent.com/32/type/' +
        file.mimeType;
  }

  private onFileClick_(e: DomRepeatEvent<File>) {
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
    const index = e.model.index;
    chrome.metricsPrivate.recordSmallCount('NewTabPage.Drive.FileClick', index);
  }
}

customElements.define(DriveModuleElement.is, DriveModuleElement);

async function createDriveElement(): Promise<DriveModuleElement|null> {
  const {files} = await DriveProxy.getHandler().getFiles();
  if (files.length === 0) {
    return null;
  }
  const element = new DriveModuleElement();
  element.files = files;
  return element;
}

export const driveDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'drive', createDriveElement);
