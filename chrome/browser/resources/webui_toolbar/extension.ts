// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '/shared/icon_from_table.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExtensionActionInfo} from '/shared/extensions_bar_data_model.mojom-webui.js';

import {getHtml} from './extension.html.js';
import {getCss} from './toolbar_button.css.js';

export class ExtensionElement extends CrLitElement {
  static get is() {
    return 'webui-toolbar-extension';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Object},
    };
  }

  accessor state: ExtensionActionInfo = {
    id: '',
    accessibleName: '',
    tooltip: '',
    isVisible: false,
    icon: {handleId: 0n},
  };

  protected onClick_() {
    // TODO: implement click action
  }

  protected onContextmenu_(e: Event) {
    e.preventDefault();
    // TODO: implement context menu
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-toolbar-extension': ExtensionElement;
  }
}

customElements.define(ExtensionElement.is, ExtensionElement);
