// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-nearby-share-subpage' is the settings subpage for managing the
 * Nearby Share feature.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './nearby_share_contact_visibility_dialog.js';
import './nearby_share_device_name_dialog.js';
import './nearby_share_data_usage_dialog.js';
import './nearby_share_receive_dialog.js';

import {getContactManager} from '/shared/nearby_contact_manager.js';
import type {ReceiveObserverReceiver, ShareTarget, TransferMetadata} from '/shared/nearby_share.mojom-webui.js';
import type {NearbySettings} from '/shared/nearby_share_settings_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DataUsage, FastInitiationNotificationState, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {Route} from '../router.js';
import {Router, routes} from '../router.js';

import {NearbyAccountManagerBrowserProxyImpl} from './nearby_account_manager_browser_proxy.js';
import type {NearbyShareReceiveDialogElement} from './nearby_share_receive_dialog.js';
import {observeReceiveManager} from './nearby_share_receive_manager.js';
import {getTemplate} from './nearby_share_subpage.html.js';
import {dataUsageStringToEnum} from './types.js';

const DEFAULT_HIGH_VISIBILITY_TIMEOUT_LEGACY_S = 300;
const DEFAULT_HIGH_VISIBILITY_TIMEOUT_S = 600;

const SettingsNearbyShareSubpageElementBase =
    DeepLinkingMixin(PrefsMixin(RouteObserverMixin(I18nMixin(PolymerElement))));

export class SettingsNearbyShareSubpageElement extends
    SettingsNearbyShareSubpageElementBase {
  static get is() {
    return 'settings-nearby-share-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      profileName_: {
        type: String,
        value: '',
      },

      profileLabel_: {
        type: String,
        value: '',
      },

      settings: {
        type: Object,
        notify: true,
        value: {},
      },

      isSettingsRetreived: {
        type: Boolean,
        value: false,
      },

      showDeviceNameDialog_: {
        type: Boolean,
        value: false,
      },

      showVisibilityDialog_: {
        type: Boolean,
        value: false,
      },

      showDataUsageDialog_: {
        type: Boolean,
        value: false,
      },

      showReceiveDialog_: {
        type: Boolean,
        value: false,
      },

      manageContactsUrl_: {
        type: String,
        value: () => loadTimeData.getString('nearbyShareManageContactsUrl'),
      },

      inHighVisibility_: {
        type: Boolean,
        value: false,
      },

      /**
       * Determines whether the QuickShareV2 flag is enabled.
       */
      isQuickShareV2Enabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isQuickShareV2Enabled'),
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kNearbyShareOnOff,
          Setting.kNearbyShareDeviceName,
          Setting.kNearbyShareDeviceVisibility,
          Setting.kNearbyShareContacts,
          Setting.kNearbyShareDataUsage,
          Setting.kDevicesNearbyAreSharingNotificationOnOff,
        ]),
      },

      shouldShowFastInititationNotificationToggle_: {
        type: Boolean,
        computed: `computeShouldShowFastInititationNotificationToggle_(
                settings.isFastInitiationHardwareSupported)`,
      },

      yourDevicesLabel_: {
        type: String,
        value: 'Your devices',
      },

      contactsLabel_: {
        type: String,
        value: 'Contacts',
      },
    };
  }

  static get observers() {
    return [
      'enabledChange_(settings.enabled)',
    ];
  }

  isSettingsRetreived: boolean;
  settings: NearbySettings;
  private inHighVisibility_: boolean;
  private isQuickShareV2Enabled_: boolean;
  private manageContactsUrl_: string;
  private profileLabel_: string;
  private profileName_: string;
  private receiveObserver_: ReceiveObserverReceiver|null;
  private shouldShowFastInititationNotificationToggle_: boolean;
  private showDataUsageDialog_: boolean;
  private showDeviceNameDialog_: boolean;
  private showReceiveDialog_: boolean;
  private showVisibilityDialog_: boolean;
  private yourDevicesLabel_: string;
  private contactsLabel_: string;

  constructor() {
    super();

    this.receiveObserver_ = null;
  }

  override ready(): void {
    super.ready();

    this.addEventListener('onboarding-cancelled', this.onOnboardingCancelled_);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    // TODO(b/166779043): Check whether the Account Manager is enabled and fall
    // back to profile name, or just hide the row. This is not urgent because
    // the Account Manager should be available whenever Nearby Share is enabled.
    NearbyAccountManagerBrowserProxyImpl.getInstance().getAccounts().then(
        accounts => {
          if (accounts.length === 0) {
            return;
          }

          this.profileName_ = accounts[0].fullName;
          this.profileLabel_ = accounts[0].email;
        });
    this.receiveObserver_ = observeReceiveManager(this);
  }

  private enabledChange_(newValue: boolean, oldValue: boolean|undefined): void {
    if (oldValue === undefined && newValue) {
      // Trigger a contact sync whenever the Nearby subpage is opened and
      // nearby is enabled complete to improve consistency. This should help
      // avoid scenarios where a share is attempted and contacts are stale on
      // the receiver.
      getContactManager().downloadContacts();
    }
  }

  private onDeviceNameClick_(): void {
    if (this.showDeviceNameDialog_) {
      return;
    }
    this.showDeviceNameDialog_ = true;
  }

  private onVisibilityClick_(): void {
    this.showVisibilityDialog_ = true;
  }

  private onDataUsageClick_(): void {
    this.showDataUsageDialog_ = true;
  }

  private onDeviceNameDialogClose_(): void {
    this.showDeviceNameDialog_ = false;
  }

  private onVisibilityDialogClose_(): void {
    this.showVisibilityDialog_ = false;
  }

  private onDataUsageDialogClose_(): void {
    this.showDataUsageDialog_ = false;
  }

  private onReceiveDialogClose_(): void {
    this.showReceiveDialog_ = false;
    this.inHighVisibility_ = false;
  }

  private onManageContactsClick_(): void {
    window.open(this.manageContactsUrl_);
  }

  private getManageContactsSubLabel_(): string {
    // Remove the protocol part of the contacts url.
    return this.manageContactsUrl_.replace(/(^\w+:|^)\/\//, '');
  }

  /**
   * Mojo callback when high visibility changes.
   */
  onHighVisibilityChanged(inHighVisibility: boolean): void {
    this.inHighVisibility_ = inHighVisibility;
  }

  /**
   * Mojo callback when transfer status changes.
   */
  onTransferUpdate(_shareTarget: ShareTarget, _metadata: TransferMetadata):
      void {
    // Note: Intentionally left empty.
  }

  /**
   * Mojo callback when the Nearby utility process stops.
   */
  onNearbyProcessStopped(): void {
    this.inHighVisibility_ = false;
  }

  /**
   * Mojo callback when advertising fails to start.
   */
  onStartAdvertisingFailure(): void {
    this.inHighVisibility_ = false;
  }

  private onInHighVisibilityToggledByUser_(): void {
    if (this.inHighVisibility_) {
      this.showHighVisibilityPage_();
    }
  }

  /**
   * @param state boolean state that determines which string to show
   * @param onstr string to show when state is true
   * @param offstr string to show when state is false
   * @return localized string
   */
  private getOnOffString_(state: boolean, onstr: string, offstr: string):
      string {
    return state ? onstr : offstr;
  }

  private getEditNameButtonAriaDescription_(name: string): string {
    return this.i18n('nearbyShareDeviceNameAriaDescription', name);
  }

  private getVisibilityText_(visibility: Visibility|undefined): string {
    if (visibility === undefined) {
      return '';
    }

    if (this.isQuickShareV2Enabled_) {
      switch (visibility) {
        case Visibility.kAllContacts:
          return this.i18n('nearbyShareContactVisiblityContactsButton');
        case Visibility.kNoOne:
          return this.i18n('nearbyShareContactVisibilityNone');
        case Visibility.kUnknown:
          return this.i18n('nearbyShareContactVisibilityUnknown');
        case Visibility.kYourDevices:
          return this.i18n('nearbyShareContactVisibilityYourDevices');
        default:
          assertNotReached();
      }
    }

    switch (visibility) {
      case Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAll');
      case Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySome');
      case Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNone');
      case Visibility.kUnknown:
        return this.i18n('nearbyShareContactVisibilityUnknown');
      case Visibility.kYourDevices:
        return this.i18n('nearbyShareContactVisibilityYourDevices');
      default:
        assertNotReached();
    }
  }

  private getVisibilityDescription_(visibility: Visibility|undefined): string {
    if (visibility === undefined) {
      return '';
    }
    switch (visibility) {
      case Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAllDescription');
      case Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySomeDescription');
      case Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNoneDescription');
      case Visibility.kUnknown:
        return this.i18n('nearbyShareContactVisibilityUnknownDescription');
      case Visibility.kYourDevices:
        return this.i18n('nearbyShareContactVisibilityYourDevicesDescription');
      default:
        assertNotReached();
    }
  }

  private getHighVisibilityToggleText_(inHighVisibility: boolean): TrustedHTML
      |string {
    // TODO(crbug.com/40159645): Add logic to show how much time the user
    // actually has left.
    return inHighVisibility ?
        this.i18n(
            'nearbyShareHighVisibilityOn',
            this.isQuickShareV2Enabled_ ? 10 : 5) :
        this.i18nAdvanced(
            'nearbyShareHighVisibilityOff',
            {substitutions: [this.isQuickShareV2Enabled_ ? '10' : '5']});
  }

  private getDataUsageLabel_(dataUsageValue: string): string {
    if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOnline) {
      return this.i18n('nearbyShareDataUsageDataLabel');
    } else if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOffline) {
      return this.i18n('nearbyShareDataUsageOfflineLabel');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyLabel');
    }
  }

  private getDataUsageSubLabel_(dataUsageValue: string): string {
    if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOnline) {
      return this.i18n('nearbyShareDataUsageDataDescription');
    } else if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOffline) {
      return this.i18n('nearbyShareDataUsageOfflineDescription');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyDescription');
    }
  }

  private getEditDataUsageButtonAriaDescription_(dataUsageValue: string):
      string {
    if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOnline) {
      return this.i18n('nearbyShareDataUsageDataEditButtonDescription');
    } else if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOffline) {
      return this.i18n('nearbyShareDataUsageOfflineEditButtonDescription');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyEditButtonDescription');
    }
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.NEARBY_SHARE) {
      return;
    }

    const router = Router.getInstance();
    const queryParams = router.getQueryParameters();

    if (queryParams.has('deviceName')) {
      this.showDeviceNameDialog_ = true;
    }

    if (queryParams.has('visibility')) {
      this.showVisibilityDialog_ = true;
    }

    if (queryParams.has('receive')) {
      this.showHighVisibilityPage_(Number(queryParams.get('timeout')));
    }

    if (queryParams.has('confirm')) {
      this.showReceiveDialog_ = true;
      flush();
      this.shadowRoot!
          .querySelector<NearbyShareReceiveDialogElement>(
              '#receiveDialog')!.showConfirmPage();
    }

    if (queryParams.has('onboarding')) {
      this.showOnboarding_();
    }

    this.attemptDeepLink();
  }

  private showHighVisibilityPage_(timeoutInSeconds?: number): void {
    const defaultTimeout = this.isQuickShareV2Enabled_ ?
        DEFAULT_HIGH_VISIBILITY_TIMEOUT_S :
        DEFAULT_HIGH_VISIBILITY_TIMEOUT_LEGACY_S;
    const shutoffTimeoutInSeconds = timeoutInSeconds || defaultTimeout;

    this.showReceiveDialog_ = true;
    flush();
    this.shadowRoot!
        .querySelector<NearbyShareReceiveDialogElement>(
            '#receiveDialog')!.showHighVisibilityPage(shutoffTimeoutInSeconds);
  }

  private getAccountRowLabel(profileName: string, profileLabel: string):
      string {
    return this.i18n(
        'nearbyShareAccountRowLabel', this.i18n('nearbyShareFeatureName'),
        profileName, profileLabel);
  }

  private onOnboardingCancelled_(): void {
    // Return to main settings page multidevice section
    Router.getInstance().navigateTo(routes.MULTIDEVICE);
  }

  private onFastInitiationNotificationToggledByUser_(): void {
    this.set(
        'settings.fastInitiationNotificationState',
        this.isFastInitiationNotificationEnabled_() ?
            FastInitiationNotificationState.kDisabledByUser :
            FastInitiationNotificationState.kEnabled);
  }

  private isFastInitiationNotificationEnabled_(): boolean {
    return this.get('settings.fastInitiationNotificationState') ===
        FastInitiationNotificationState.kEnabled;
  }

  private shouldShowSubpageContent_(
      isNearbySharingEnabled: boolean, isOnboardingComplete: boolean,
      shouldShowFastInititationNotificationToggle: boolean): boolean {
    if (!isOnboardingComplete) {
      return false;
    }
    return isNearbySharingEnabled ||
        shouldShowFastInititationNotificationToggle;
  }

  private showOnboarding_(): void {
    this.showReceiveDialog_ = true;
    flush();
    this.shadowRoot!
        .querySelector<NearbyShareReceiveDialogElement>(
            '#receiveDialog')!.showOnboarding();
  }

  private computeShouldShowFastInititationNotificationToggle_(
      isHardwareSupported: boolean): boolean {
    return isHardwareSupported;
  }

  private setVisibility_(visibility: Visibility): void {
    this.set('settings.visibility', visibility);
  }

  private getVisibilityByLabel_(visibilityString: string): Visibility {
    switch (visibilityString) {
      case this.yourDevicesLabel_:
        return Visibility.kYourDevices;
      case this.contactsLabel_:
        return Visibility.kAllContacts;
      default:
        return Visibility.kUnknown;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsNearbyShareSubpageElement.is]: SettingsNearbyShareSubpageElement;
  }
}

customElements.define(
    SettingsNearbyShareSubpageElement.is, SettingsNearbyShareSubpageElement);
