// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './prefs/pref_toggle_button.js';
import './user_utils_mixin.js';
import '/shared/settings/controls/extension_controlled_indicator.js';

import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// <if expr="is_win or is_macosx">
import {PasskeysBrowserProxyImpl} from './passkeys_browser_proxy.js';
// </if>
import {BlockedSite, BlockedSitesListChangedListener, CredentialsChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';
import {PrefToggleButtonElement} from './prefs/pref_toggle_button.js';
import {Route, RouteObserverMixin, Router, UrlParam} from './router.js';
import {getTemplate} from './settings_section.html.js';
import {SyncBrowserProxyImpl, TrustedVaultBannerState} from './sync_browser_proxy.js';
import {UserUtilMixin} from './user_utils_mixin.js';

export interface SettingsSectionElement {
  $: {
    autosigninToggle: PrefToggleButtonElement,
    blockedSitesList: HTMLElement,
    passwordToggle: PrefToggleButtonElement,
    trustedVaultBanner: CrLinkRowElement,
  };
}

const PASSWORD_MANAGER_ADD_SHORTCUT_ELEMENT_ID =
    'PasswordManagerUI::kAddShortcutElementId';
const PASSWORD_MANAGER_ADD_SHORTCUT_CUSTOM_EVENT_ID =
    'PasswordManagerUI::kAddShortcutCustomEventId';

const SettingsSectionElementBase = HelpBubbleMixin(RouteObserverMixin(
    PrefsMixin(UserUtilMixin(WebUiListenerMixin(I18nMixin(PolymerElement))))));

export class SettingsSectionElement extends SettingsSectionElementBase {
  static get is() {
    return 'settings-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** An array of blocked sites to display. */
      blockedSites_: {
        type: Array,
        value: () => [],
      },

      // <if expr="is_win or is_macosx">
      isBiometricAuthenticationForFillingToggleVisible_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'biometricAuthenticationForFillingToggleVisible');
        },
      },
      // </if>

      hasPasswordsToExport_: {
        type: Boolean,
        value: false,
      },

      // <if expr="is_macosx">
      createPasskeysInICloudKeychainToggleVisible_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'createPasskeysInICloudKeychainToggleVisible');
        },
      },
      // </if>

      hasPasskeys_: {
        type: Boolean,
        value: false,
      },

      passwordManagerDisabled_: {
        type: Boolean,
        computed: 'computePasswordManagerDisabled_(' +
            'prefs.credentials_enable_service.enforcement, ' +
            'prefs.credentials_enable_service.value)',
      },

      /** The visibility state of the trusted vault banner. */
      trustedVaultBannerState_: {
        type: Object,
        value: TrustedVaultBannerState.NOT_SHOWN,
      },

      canAddShortcut_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('canAddShortcut');
        },
      },
    };
  }

  private blockedSites_: BlockedSite[];
  private hasPasskeys_: boolean;
  private hasPasswordsToExport_: boolean;
  private showPasswordsImporter_: boolean;
  private trustedVaultBannerState_: TrustedVaultBannerState;

  private setBlockedSitesListListener_: BlockedSitesListChangedListener|null =
      null;
  private setCredentialsChangedListener_: CredentialsChangedListener|null =
      null;

  override ready() {
    super.ready();

    chrome.metricsPrivate.recordBoolean(
        'PasswordManager.OpenedAsShortcut',
        window.matchMedia('(display-mode: standalone)').matches);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setBlockedSitesListListener_ = blockedSites => {
      this.blockedSites_ = blockedSites;
    };
    PasswordManagerImpl.getInstance().getBlockedSitesList().then(
        blockedSites => this.blockedSites_ = blockedSites);
    PasswordManagerImpl.getInstance().addBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);

    this.setCredentialsChangedListener_ =
        (passwords: chrome.passwordsPrivate.PasswordUiEntry[]) => {
          this.hasPasswordsToExport_ = passwords.length > 0;
        };
    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        this.setCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setCredentialsChangedListener_);

    const trustedVaultStateChanged = (state: TrustedVaultBannerState) => {
      this.trustedVaultBannerState_ = state;
    };
    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();
    syncBrowserProxy.getTrustedVaultBannerState().then(
        trustedVaultStateChanged);
    this.addWebUiListener(
        'trusted-vault-banner-state-changed', trustedVaultStateChanged);

    // <if expr="is_win or is_macosx">
    PasskeysBrowserProxyImpl.getInstance().hasPasskeys().then(hasPasskeys => {
      this.hasPasskeys_ = hasPasskeys;
    });
    // </if>
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setBlockedSitesListListener_);
    PasswordManagerImpl.getInstance().removeBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);
    this.setBlockedSitesListListener_ = null;

    assert(this.setCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setCredentialsChangedListener_);
    this.setCredentialsChangedListener_ = null;
  }

  override currentRouteChanged(route: Route): void {
    const param = route.queryParameters.get(UrlParam.START_IMPORT) || '';
    if (param === 'true') {
      const importer = this.shadowRoot!.querySelector('passwords-importer');
      assert(importer);
      importer.launchImport();
      const params = new URLSearchParams();
      Router.getInstance().updateRouterParams(params);
    }
  }

  private onShortcutBannerDomChanged_() {
    const addShortcutBanner = this.root!.querySelector('#addShortcutBanner');
    if (addShortcutBanner) {
      this.registerHelpBubble(
          PASSWORD_MANAGER_ADD_SHORTCUT_ELEMENT_ID, addShortcutBanner);
    }
  }

  private onAddShortcutClick_() {
    this.notifyHelpBubbleAnchorCustomEvent(
        PASSWORD_MANAGER_ADD_SHORTCUT_ELEMENT_ID,
        PASSWORD_MANAGER_ADD_SHORTCUT_CUSTOM_EVENT_ID,
    );
    // TODO(crbug.com/1358448): Record metrics on all entry points usage.
    // TODO(crbug.com/1358448): Hide the button for users after the shortcut is
    // installed.
    PasswordManagerImpl.getInstance().showAddShortcutDialog();
  }

  /**
   * Fires an event that should delete the blocked password entry.
   */
  private onRemoveBlockedSiteClick_(
      event: DomRepeatEvent<chrome.passwordsPrivate.ExceptionEntry>) {
    PasswordManagerImpl.getInstance().removeBlockedSite(event.model.item.id);
  }

  // <if expr="is_win or is_macosx">
  private switchBiometricAuthBeforeFillingState_(e: Event) {
    const biometricAuthenticationForFillingToggle =
        e!.target as PrefToggleButtonElement;
    assert(biometricAuthenticationForFillingToggle);
    PasswordManagerImpl.getInstance().switchBiometricAuthBeforeFillingState();
  }
  // </if>

  private onTrustedVaultBannerClick_() {
    switch (this.trustedVaultBannerState_) {
      case TrustedVaultBannerState.OPTED_IN:
        OpenWindowProxyImpl.getInstance().openUrl(
            loadTimeData.getString('trustedVaultLearnMoreUrl'));
        break;
      case TrustedVaultBannerState.OFFER_OPT_IN:
        OpenWindowProxyImpl.getInstance().openUrl(
            loadTimeData.getString('trustedVaultOptInUrl'));
        break;
      case TrustedVaultBannerState.NOT_SHOWN:
      default:
        assertNotReached();
    }
  }

  private getTrustedVaultBannerTitle_(): string {
    switch (this.trustedVaultBannerState_) {
      case TrustedVaultBannerState.OPTED_IN:
        return this.i18n('trustedVaultBannerLabelOptedIn');
      case TrustedVaultBannerState.OFFER_OPT_IN:
        return this.i18n('trustedVaultBannerLabelOfferOptIn');
      case TrustedVaultBannerState.NOT_SHOWN:
        return '';
      default:
        assertNotReached();
    }
  }

  private getTrustedVaultBannerDescription_(): string {
    switch (this.trustedVaultBannerState_) {
      case TrustedVaultBannerState.OPTED_IN:
        return this.i18n('trustedVaultBannerSubLabelOptedIn');
      case TrustedVaultBannerState.OFFER_OPT_IN:
        return this.i18n('trustedVaultBannerSubLabelOfferOptIn');
      case TrustedVaultBannerState.NOT_SHOWN:
        return '';
      default:
        assertNotReached();
    }
  }

  private shouldHideTrustedVaultBanner_(): boolean {
    return this.trustedVaultBannerState_ === TrustedVaultBannerState.NOT_SHOWN;
  }

  private getAriaLabelForBlockedSite_(
      blockedSite: chrome.passwordsPrivate.ExceptionEntry): string {
    return this.i18n('removeBlockedAriaDescription', blockedSite.urls.shown);
  }

  private changeAccountStorageOptIn_() {
    if (this.isOptedInForAccountStorage) {
      this.optOutFromAccountStorage();
    } else {
      this.optInForAccountStorage();
    }
  }

  // <if expr="is_win or is_macosx">
  private onManagePasskeysClick_() {
    PasskeysBrowserProxyImpl.getInstance().managePasskeys();
  }
  // </if>

  private computePasswordManagerDisabled_(): boolean {
    const pref = this.getPref('credentials_enable_service');
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
        !pref.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-section': SettingsSectionElement;
  }
}

customElements.define(SettingsSectionElement.is, SettingsSectionElement);
