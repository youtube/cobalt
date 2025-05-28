// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './site_permissions_site_group.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ItemDelegate} from '../item.js';
import {navigation, Page} from '../navigation_helper.js';

import {getCss} from './site_permissions_by_site.css.js';
import {getHtml} from './site_permissions_by_site.html.js';
import type {SiteSettingsDelegate} from './site_settings_mixin.js';

export interface ExtensionsSitePermissionsBySiteElement {
  $: {
    closeButton: CrIconButtonElement,
  };
}

export class ExtensionsSitePermissionsBySiteElement extends CrLitElement {
  static get is() {
    return 'extensions-site-permissions-by-site';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},
      extensions: {type: Array},
      siteGroups_: {type: Array},
    };
  }

  delegate?: ItemDelegate&SiteSettingsDelegate;
  extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  protected siteGroups_: chrome.developerPrivate.SiteGroup[] = [];

  override firstUpdated() {
    assert(this.delegate);
    this.refreshUserAndExtensionSites_();
    this.delegate.getUserSiteSettingsChangedTarget().addListener(
        this.refreshUserAndExtensionSites_.bind(this));
    this.delegate.getItemStateChangedTarget().addListener(
        this.refreshUserAndExtensionSites_.bind(this));
  }

  private refreshUserAndExtensionSites_() {
    assert(this.delegate);
    this.delegate.getUserAndExtensionSitesByEtld().then(sites => {
      this.siteGroups_ = sites;
    });
  }

  protected onCloseButtonClick_() {
    navigation.navigateTo({page: Page.SITE_PERMISSIONS});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-site-permissions-by-site':
        ExtensionsSitePermissionsBySiteElement;
  }
}

customElements.define(
    ExtensionsSitePermissionsBySiteElement.is,
    ExtensionsSitePermissionsBySiteElement);
