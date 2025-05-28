// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../info_dialog.js';
import '../../module_header.js';
import './file_suggestion.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {File} from '../../../file_suggestion.mojom-webui.js';
import {I18nMixinLit, loadTimeData} from '../../../i18n_setup.js';
import type {MicrosoftFilesPageHandlerRemote} from '../../../microsoft_files.mojom-webui.js';
import {ParentTrustedDocumentProxy} from '../../microsoft_auth_frame_connector.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import type {FileSuggestionElement} from './file_suggestion.js';
import {getHtml} from './microsoft_files_module.html.js';
import {MicrosoftFilesProxyImpl} from './microsoft_files_proxy.js';

export interface MicrosoftFilesModuleElement {
  $: {
    fileSuggestion: FileSuggestionElement,
    moduleHeaderElementV2: ModuleHeaderElement,
  };
}

const MicrosoftFilesModuleElementBase = I18nMixinLit(CrLitElement);

/**
 * The SharePoint/OneDrive module, which serves as an inside look to recent
 * activity within a user's Microsoft SharePoint and OneDrive.
 */
export class MicrosoftFilesModuleElement extends
    MicrosoftFilesModuleElementBase {
  static get is() {
    return 'ntp-microsoft-files-module';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      files_: {type: Array},
      showInfoDialog_: {type: Boolean},
    };
  }

  protected files_: File[] = [];
  protected showInfoDialog_: boolean = false;

  private handler_: MicrosoftFilesPageHandlerRemote;

  constructor(files: File[]) {
    super();
    this.handler_ = MicrosoftFilesProxyImpl.getInstance().handler;
    this.files_ = files;
  }

  protected getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'dismiss',
          icon: 'modules:visibility_off',
          text: this.i18nRecursive(
              '', 'modulesDismissForHoursButtonText',
              'fileSuggestionDismissHours'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18n('modulesMicrosoftFilesDisableButtonText'),
        },
        {
          action: 'signout',
          icon: 'modules:logout',
          text: this.i18n('modulesMicrosoftSignOutButtonText'),
        },
        {
          action: 'info',
          icon: 'modules:info',
          text: this.i18n('moduleInfoButtonTitle'),
        },
      ],
      [
        {
          action: 'customize-module',
          icon: 'modules:tune',
          text: this.i18n('modulesCustomizeButtonText'),
        },
      ],
    ];
  }

  protected onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesMicrosoftFilesName')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  protected onDismissButtonClick_() {
    // TODO(crbug.com/372724129): Update dismiss message.
    this.handler_.dismissModule();
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage',
            loadTimeData.getString('modulesFilesSentence')),
        restoreCallback: () => this.handler_.restoreModule(),
      },
    }));
  }

  protected onInfoButtonClick_() {
    this.showInfoDialog_ = true;
  }

  protected onInfoDialogClose_() {
    this.showInfoDialog_ = false;
  }

  protected onSignOutButtonClick_() {
    ParentTrustedDocumentProxy.getInstance()?.getChildDocument().signOut();
  }
}

customElements.define(
    MicrosoftFilesModuleElement.is, MicrosoftFilesModuleElement);

async function createMicrosoftFilesElement():
    Promise<MicrosoftFilesModuleElement|null> {
  const {files} =
      await MicrosoftFilesProxyImpl.getInstance().handler.getFiles();
  return files.length > 0 ? new MicrosoftFilesModuleElement(files) : null;
}

export const microsoftFilesModuleDescriptor: ModuleDescriptor =
    new ModuleDescriptor(
        /*id*/ 'microsoft_files', createMicrosoftFilesElement);
