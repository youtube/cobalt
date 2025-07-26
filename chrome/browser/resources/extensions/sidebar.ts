// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import './icons.html.js';

import type {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {navigation, Page} from './navigation_helper.js';
import {getCss} from './sidebar.css.js';
import {getHtml} from './sidebar.html.js';

export interface ExtensionsSidebarElement {
  $: {
    sectionMenu: CrMenuSelector,
    sectionsExtensions: HTMLElement,
    sectionsShortcuts: HTMLElement,
    sectionsSitePermissions: HTMLElement,
    moreExtensions: HTMLElement,
  };
}

const ExtensionsSidebarElementBase = I18nMixinLit(CrLitElement);

export class ExtensionsSidebarElement extends ExtensionsSidebarElementBase {
  static get is() {
    return 'extensions-sidebar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enableEnhancedSiteControls: {type: Boolean},

      /**
       * The data path/page that identifies the entry to be selected in the
       * sidebar. Note that this may not match the page that's actually
       * displayed.
       */
      selectedPath_: {type: String},
    };
  }

  enableEnhancedSiteControls: boolean = false;
  protected selectedPath_: Page = Page.LIST;

  /**
   * The ID of the listener on |navigation|. Stored so that the
   * listener can be removed when this element is detached (happens in tests).
   */
  private navigationListener_: number|null = null;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    this.setAttribute('role', 'navigation');
    this.computeSelectedPath_(navigation.getCurrentPage().page);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.navigationListener_ = navigation.addListener(newPage => {
      this.computeSelectedPath_(newPage.page);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.navigationListener_);
    assert(navigation.removeListener(this.navigationListener_));
    this.navigationListener_ = null;
  }

  private computeSelectedPath_(page: Page) {
    switch (page) {
      case Page.SITE_PERMISSIONS:
      case Page.SITE_PERMISSIONS_ALL_SITES:
        this.selectedPath_ = Page.SITE_PERMISSIONS;
        break;
      case Page.SHORTCUTS:
        this.selectedPath_ = Page.SHORTCUTS;
        break;
      default:
        this.selectedPath_ = Page.LIST;
    }
  }

  protected onLinkClick_(e: Event) {
    e.preventDefault();
    navigation.navigateTo(
        {page: ((e.target as HTMLElement).dataset['path'] as Page)});
    this.fire('close-drawer');
  }

  protected onMoreExtensionsClick_(e: Event) {
    if ((e.target as HTMLElement).tagName === 'A') {
      chrome.metricsPrivate.recordUserAction('Options_GetMoreExtensions');
    }
  }

  protected computeDiscoverMoreText_(): TrustedHTML {
    return this.i18nAdvanced('sidebarDiscoverMore', {
      tags: ['a'],
      attrs: ['target'],
      substitutions: [loadTimeData.getString('getMoreExtensionsUrl')],
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-sidebar': ExtensionsSidebarElement;
  }
}

customElements.define(ExtensionsSidebarElement.is, ExtensionsSidebarElement);
