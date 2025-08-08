// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '/strings.m.js';
import './site_permissions_list.js';

import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {navigation, Page} from '../navigation_helper.js';

import {getCss} from './site_permissions.css.js';
import {getHtml} from './site_permissions.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';

export interface ExtensionsSitePermissionsElement {
  $: {
    allSitesLink: CrLinkRowElement,
  };
}

const ExtensionsSitePermissionsElementBase = SiteSettingsMixin(CrLitElement);

export class ExtensionsSitePermissionsElement extends
    ExtensionsSitePermissionsElementBase {
  static get is() {
    return 'extensions-site-permissions';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ...super.properties,
      extensions: {type: Array},
      showPermittedSites_: {type: Boolean},
    };
  }

  extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  protected showPermittedSites_: boolean =
      loadTimeData.getBoolean('enableUserPermittedSites');

  protected onAllSitesLinkClick_() {
    navigation.navigateTo({page: Page.SITE_PERMISSIONS_ALL_SITES});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-site-permissions': ExtensionsSitePermissionsElement;
  }
}

customElements.define(
    ExtensionsSitePermissionsElement.is, ExtensionsSitePermissionsElement);
