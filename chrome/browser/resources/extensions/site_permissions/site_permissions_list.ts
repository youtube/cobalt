// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';
import './site_permissions_edit_permissions_dialog.js';
import './site_permissions_edit_url_dialog.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getFaviconUrl} from '../url_util.js';

import {getCss} from './site_permissions_list.css.js';
import {getHtml} from './site_permissions_list.html.js';
import type {SiteSettingsDelegate} from './site_settings_mixin.js';
import {DummySiteSettingsDelegate} from './site_settings_mixin.js';

export interface ExtensionsSitePermissionsListElement {
  $: {
    addSite: CrButtonElement,
    siteActionMenu: CrActionMenuElement,
  };
}

export class ExtensionsSitePermissionsListElement extends CrLitElement {
  static get is() {
    return 'site-permissions-list';
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
      header: {type: String},
      siteSet: {type: String},
      sites: {type: Array},
      showEditSiteUrlDialog_: {type: Boolean},
      showEditSitePermissionsDialog_: {type: Boolean},

      /**
       * The site currently being edited if the user has opened the action menu
       * for a given site.
       */
      siteToEdit_: {type: String},
    };
  }

  delegate: SiteSettingsDelegate = new DummySiteSettingsDelegate();
  extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  header: string = '';
  siteSet: chrome.developerPrivate.SiteSet =
      chrome.developerPrivate.SiteSet.USER_PERMITTED;
  sites: string[] = [];
  protected showEditSiteUrlDialog_: boolean = false;
  protected showEditSitePermissionsDialog_: boolean = false;
  protected siteToEdit_: string|null = null;

  // The element to return focus to once the site input dialog closes. If
  // specified, this is the 3 dots menu for the site just edited, otherwise it's
  // the add site button.
  private siteToEditAnchorElement_: HTMLElement|null = null;

  protected hasSites_(): boolean {
    return !!this.sites.length;
  }

  protected getFaviconUrl_(url: string): string {
    return getFaviconUrl(url);
  }

  private focusOnAnchor_() {
    // Return focus to the three dots menu once a site has been edited.
    // TODO(crbug.com/40215499): If the edited site is the only site in the
    // list, focus is not on the three dots menu.
    assert(this.siteToEditAnchorElement_, 'Site Anchor');
    focusWithoutInk(this.siteToEditAnchorElement_);
    this.siteToEditAnchorElement_ = null;
  }

  protected onAddSiteClick_() {
    assert(!this.showEditSitePermissionsDialog_);
    this.siteToEdit_ = null;
    this.showEditSiteUrlDialog_ = true;
  }

  protected onEditSiteUrlDialogClose_() {
    this.showEditSiteUrlDialog_ = false;
    if (this.siteToEdit_ !== null) {
      this.focusOnAnchor_();
    }
    this.siteToEdit_ = null;
  }

  protected onEditSitePermissionsDialogClose_() {
    this.showEditSitePermissionsDialog_ = false;
    assert(this.siteToEdit_, 'Site To Edit');
    this.focusOnAnchor_();
    this.siteToEdit_ = null;
  }

  protected onDotsClick_(e: Event) {
    const target = e.target as HTMLElement;
    this.siteToEdit_ = target.dataset['site']!;
    assert(!this.showEditSitePermissionsDialog_);
    this.$.siteActionMenu.showAt(target);
    this.siteToEditAnchorElement_ = target;
  }

  protected onEditSitePermissionsClick_() {
    this.closeActionMenu_();
    assert(this.siteToEdit_ !== null);
    this.showEditSitePermissionsDialog_ = true;
  }

  protected onEditSiteUrlClick_() {
    this.closeActionMenu_();
    assert(this.siteToEdit_ !== null);
    this.showEditSiteUrlDialog_ = true;
  }

  protected onRemoveSiteClick_() {
    assert(this.siteToEdit_, 'Site To Edit');
    this.delegate.removeUserSpecifiedSites(this.siteSet, [this.siteToEdit_])
        .then(() => {
          this.closeActionMenu_();
          this.siteToEdit_ = null;
        });
  }

  private closeActionMenu_() {
    const menu = this.$.siteActionMenu;
    assert(menu.open);
    menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-permissions-list': ExtensionsSitePermissionsListElement;
  }
}

customElements.define(
    ExtensionsSitePermissionsListElement.is,
    ExtensionsSitePermissionsListElement);
