// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-list' shows a list of Allowed and Blocked sites for a given
 * category.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import '../settings_shared.css.js';
import './add_site_dialog.js';
import './edit_exception_dialog.js';
import './site_list_entry.js';

import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TooltipMixin} from '../tooltip_mixin.js';

import {ContentSetting, ContentSettingsTypes, CookiesExceptionType, INVALID_CATEGORY_SUBTYPE, SITE_EXCEPTION_WILDCARD} from './constants.js';
import {getTemplate} from './site_list.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import {RawSiteException, SiteException, SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';

export interface SiteListElement {
  $: {
    addSite: HTMLElement,
    category: HTMLElement,
    listContainer: HTMLElement,
    tooltip: PaperTooltipElement,
  };
}

const SiteListElementBase = TooltipMixin(ListPropertyUpdateMixin(
    SiteSettingsMixin(WebUiListenerMixin(PolymerElement))));

export class SiteListElement extends SiteListElementBase {
  static get is() {
    return 'site-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Some content types (like Location) do not allow the user to manually
       * edit the exception list from within Settings.
       */
      readOnlyList: {
        type: Boolean,
        value: false,
      },

      categoryHeader: String,

      /**
       * The site serving as the model for the currently open action menu.
       */
      actionMenuSite_: Object,

      /**
       * Whether the "edit exception" dialog should be shown.
       */
      showEditExceptionDialog_: Boolean,

      /**
       * Array of sites to display in the widget.
       */
      sites: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The type of category this widget is displaying data for. Normally
       * either 'allow' or 'block', representing which sites are allowed or
       * blocked respectively.
       */
      categorySubtype: {
        type: String,
        value: INVALID_CATEGORY_SUBTYPE,
      },

      /**
       * Filters cookies exceptions based on the type (CookiesExceptionType):
       * - THIRD_PARTY: Only show cookies exceptions that have primary pattern
       * as wildcard (third-party cookies exceptions).
       * - SITE_DATA: Only show cookies exceptions that have primary pattern
       * set. This includes site data exceptions (secondary pattern is wildcard)
       * and exceptions with both patterns set (currently possible only via
       * exceptions API).
       * - COMBINED: Doesn't apply any filters, will show exceptions with both
       * pattern types.
       */
      cookiesExceptionType: String,

      hasIncognito_: Boolean,

      /**
       * Whether to show the Add button next to the header.
       */
      showAddSiteButton_: {
        type: Boolean,
        computed: 'computeShowAddSiteButton_(readOnlyList, category, ' +
            'categorySubtype)',
      },

      showAddSiteDialog_: Boolean,

      /**
       * Whether to show the Allow action in the action menu.
       */
      showAllowAction_: Boolean,

      /**
       * Whether to show the Block action in the action menu.
       */
      showBlockAction_: Boolean,

      /**
       * Whether to show the 'Clear on exit' action in the action
       * menu.
       */
      showSessionOnlyAction_: Boolean,

      /**
       * All possible actions in the action menu.
       */
      actions_: {
        readOnly: true,
        type: Object,
        values: {
          ALLOW: 'Allow',
          BLOCK: 'Block',
          RESET: 'Reset',
          SESSION_ONLY: 'SessionOnly',
        },
      },

      lastFocused_: Object,
      listBlurred_: Boolean,
      tooltipText_: String,
      searchFilter: String,
    };
  }

  static get observers() {
    return ['configureWidget_(category, categorySubtype)'];
  }

  readOnlyList: boolean;
  categoryHeader: string;
  private actionMenuSite_: SiteException|null;
  private showEditExceptionDialog_: boolean;
  sites: SiteException[];
  categorySubtype: ContentSetting;
  private hasIncognito_: boolean;
  private showAddSiteButton_: boolean;
  private showAddSiteDialog_: boolean;
  private showAllowAction_: boolean;
  private showBlockAction_: boolean;
  private showSessionOnlyAction_: boolean;
  private lastFocused_: HTMLElement;
  private listBlurred_: boolean;
  private tooltipText_: string;
  searchFilter: string;
  cookiesExceptionType: CookiesExceptionType;

  private activeDialogAnchor_: HTMLElement|null;
  private browserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();

  constructor() {
    super();

    /**
     * The element to return focus to, when the currently active dialog is
     * closed.
     */
    this.activeDialogAnchor_ = null;
  }

  override ready() {
    super.ready();

    this.addWebUiListener(
        'contentSettingSitePermissionChanged',
        (category: ContentSettingsTypes) =>
            this.siteWithinCategoryChanged_(category));
    this.addWebUiListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) =>
            this.siteWithinCategoryChanged_(category));
    this.addWebUiListener(
        'onIncognitoStatusChanged',
        (hasIncognito: boolean) =>
            this.onIncognitoStatusChanged_(hasIncognito));
    this.browserProxy.updateIncognitoStatus();
  }

  /**
   * Called when a site changes permission.
   * @param category The category of the site that changed.
   */
  private siteWithinCategoryChanged_(category: ContentSettingsTypes) {
    if (category === this.category) {
      this.configureWidget_();
    }
  }

  /**
   * Called for each site list when incognito is enabled or disabled. Only
   * called on change (opening N incognito windows only fires one message).
   * Another message is sent when the *last* incognito window closes.
   */
  private onIncognitoStatusChanged_(hasIncognito: boolean) {
    this.hasIncognito_ = hasIncognito;

    // The SESSION_ONLY list won't have any incognito exceptions. (Minor
    // optimization, not required).
    if (this.categorySubtype === ContentSetting.SESSION_ONLY) {
      return;
    }

    // A change notification is not sent for each site. So we repopulate the
    // whole list when the incognito profile is created or destroyed.
    this.populateList_();
  }

  /**
   * Configures the action menu, visibility of the widget and shows the list.
   */
  private configureWidget_() {
    if (this.category === undefined) {
      return;
    }

    this.setUpActionMenu_();
    this.populateList_();

    // The Session permissions are only for cookies.
    if (this.categorySubtype === ContentSetting.SESSION_ONLY) {
      this.$.category.hidden = this.category !== ContentSettingsTypes.COOKIES;
    }
  }

  /**
   * Whether there are any site exceptions added for this content setting.
   */
  private hasSites_(): boolean {
    return this.sites.length > 0;
  }

  /**
   * Whether the Add Site button is shown in the header for the current category
   * and category subtype.
   */
  private computeShowAddSiteButton_(): boolean {
    return !(
        this.readOnlyList ||
        (this.category === ContentSettingsTypes.FILE_SYSTEM_WRITE &&
         this.categorySubtype === ContentSetting.ALLOW));
  }

  private showNoSearchResults_(): boolean {
    return this.sites.length > 0 && this.getFilteredSites_().length === 0;
  }

  /**
   * A handler for the Add Site button.
   */
  private onAddSiteClick_() {
    assert(!this.readOnlyList);
    this.showAddSiteDialog_ = true;
  }

  private onAddSiteDialogClosed_() {
    this.showAddSiteDialog_ = false;
    focusWithoutInk(this.$.addSite);
  }

  /**
   * Need to use common tooltip since the tooltip in the entry is cut off from
   * the iron-list.
   */
  private onShowTooltip_(e: CustomEvent<{target: HTMLElement, text: string}>) {
    this.tooltipText_ = e.detail.text;
    // paper-tooltip normally determines the target from the |for| property,
    // which is a selector. Here paper-tooltip is being reused by multiple
    // potential targets.
    this.showTooltipAtTarget(this.$.tooltip, e.detail.target);
  }

  /**
   * Populate the sites list for display.
   */
  private populateList_() {
    this.browserProxy_.getExceptionList(this.category).then(exceptionList => {
      this.processExceptions_(exceptionList);
      this.closeActionMenu_();
    });
  }

  /**
   * Process the exception list returned from the native layer.
   */
  private processExceptions_(exceptionList: RawSiteException[]) {
    const sites = exceptionList
                      .filter(
                          site => site.setting !== ContentSetting.DEFAULT &&
                              site.setting === this.categorySubtype)
                      .filter(site => {
                        if (this.category !== ContentSettingsTypes.COOKIES) {
                          return true;
                        }
                        assert(this.cookiesExceptionType !== undefined);
                        switch (this.cookiesExceptionType) {
                          case CookiesExceptionType.THIRD_PARTY:
                            return site.origin === SITE_EXCEPTION_WILDCARD;
                          case CookiesExceptionType.SITE_DATA:
                            // Site data exceptions include all exceptions that
                            // have `origin` set. This includes site data
                            // exceptions and exceptions with both patterns set
                            // (currently possible only via exceptions API).
                            return site.origin !== SITE_EXCEPTION_WILDCARD;
                          case CookiesExceptionType.COMBINED:
                            // For cookies exception type COMBINED, don't apply
                            // any filters and show exceptions with both pattern
                            // types.
                            return true;
                        }
                      })
                      .map(site => this.expandSiteException(site));
    this.updateList('sites', x => x.origin, sites);
  }

  /**
   * Set up the values to use for the action menu.
   */
  private setUpActionMenu_() {
    this.showAllowAction_ = this.categorySubtype !== ContentSetting.ALLOW;
    this.showBlockAction_ = this.categorySubtype !== ContentSetting.BLOCK;
    this.showSessionOnlyAction_ =
        this.categorySubtype !== ContentSetting.SESSION_ONLY &&
        this.category === ContentSettingsTypes.COOKIES;
  }

  /**
   * @return Whether to show the "Session Only" menu item for the currently
   *     active site.
   */
  private showSessionOnlyActionForSite_(): boolean {
    // It makes no sense to show "clear on exit" for exceptions that only apply
    // to incognito. It gives the impression that they might under some
    // circumstances not be cleared on exit, which isn't true.
    if (!this.actionMenuSite_ || this.actionMenuSite_.incognito) {
      return false;
    }

    return this.showSessionOnlyAction_;
  }

  private setContentSettingForActionMenuSite_(contentSetting: ContentSetting) {
    assert(this.actionMenuSite_);
    this.browserProxy.setCategoryPermissionForPattern(
        this.actionMenuSite_!.origin, this.actionMenuSite_!.embeddingOrigin,
        this.category, contentSetting, this.actionMenuSite_!.incognito);
  }

  private onAllowClick_() {
    this.setContentSettingForActionMenuSite_(ContentSetting.ALLOW);
    this.closeActionMenu_();
  }

  private onBlockClick_() {
    this.setContentSettingForActionMenuSite_(ContentSetting.BLOCK);
    this.closeActionMenu_();
  }

  private onSessionOnlyClick_() {
    this.setContentSettingForActionMenuSite_(ContentSetting.SESSION_ONLY);
    this.closeActionMenu_();
  }

  private onEditClick_() {
    // Close action menu without resetting |this.actionMenuSite_| since it is
    // bound to the dialog.
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    this.showEditExceptionDialog_ = true;
  }

  private onEditExceptionDialogClosed_() {
    this.showEditExceptionDialog_ = false;
    this.actionMenuSite_ = null;
    if (this.activeDialogAnchor_) {
      this.activeDialogAnchor_.focus();
      this.activeDialogAnchor_ = null;
    }
  }

  private onResetClick_() {
    assert(this.actionMenuSite_);
    this.browserProxy.resetCategoryPermissionForPattern(
        this.actionMenuSite_.origin, this.actionMenuSite_.embeddingOrigin,
        this.category, this.actionMenuSite_.incognito);
    this.closeActionMenu_();
  }

  private onShowActionMenu_(
      e: CustomEvent<{anchor: HTMLElement, model: SiteException}>) {
    this.activeDialogAnchor_ = e.detail.anchor;
    this.actionMenuSite_ = e.detail.model;
    this.shadowRoot!.querySelector('cr-action-menu')!.showAt(
        this.activeDialogAnchor_);
  }

  private closeActionMenu_() {
    this.actionMenuSite_ = null;
    this.activeDialogAnchor_ = null;
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu')!;
    if (actionMenu.open) {
      actionMenu.close();
    }
  }

  private getFilteredSites_(): SiteException[] {
    if (!this.searchFilter) {
      return this.sites.slice();
    }

    type SearchableProperty = 'displayName'|'origin'|'embeddingOrigin';
    const propNames: SearchableProperty[] =
        ['displayName', 'origin', 'embeddingOrigin'];
    const searchFilter = this.searchFilter.toLowerCase();
    return this.sites.filter(
        site => propNames.some(
            propName => site[propName].toLowerCase().includes(searchFilter)));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-list': SiteListElement;
  }
}

customElements.define(SiteListElement.is, SiteListElement);
