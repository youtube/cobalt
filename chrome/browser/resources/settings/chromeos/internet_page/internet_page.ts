// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-page' is the settings page containing internet
 * settings.
 */

import 'chrome://resources/ash/common/cellular_setup/cellular_setup_icons.html.js';
import 'chrome://resources/ash/common/network/sim_lock_dialogs.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import '../../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_icons.css.js';
import './cellular_setup_dialog.js';
import './esim_rename_dialog.js';
import './esim_remove_profile_dialog.js';
import './hotspot_config_dialog.js';
import './internet_config.js';
import './internet_detail_menu.js';
import './network_summary.js';

import {CellularSetupPageName} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {getNumESimProfiles} from 'chrome://resources/ash/common/cellular_setup/esim_manager_utils.js';
import {HotspotInfo} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {hasActiveCellularNetwork, isConnectedToNonCellularNetwork} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {PrefsMixin, PrefsMixinInterface} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrosNetworkConfigRemote, GlobalPolicy, NetworkStateProperties, StartConnectResult, VpnProvider} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {afterNextRender, DomRepeatEvent, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin, DeepLinkingMixinInterface} from '../deep_linking_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {ApnSubpageElement} from './apn_subpage.js';
import {InternetConfigElement} from './internet_config.js';
import {getTemplate} from './internet_page.html.js';
import {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page_browser_proxy.js';

const ESIM_PROFILE_LIMIT = 5;

declare global {
  interface HTMLElementEventMap {
    'device-enabled-toggled':
        CustomEvent<{enabled: boolean, type: NetworkType}>;
    'network-connect': CustomEvent<{
      networkState: OncMojo.NetworkStateProperties,
      bypassConnectionDialog: boolean|undefined,
    }>;
    'show-cellular-setup': CustomEvent<{pageName: CellularSetupPageName}>;
    'show-config':
        CustomEvent<{type: string, guid: string|null, name: string|null}>;
    'show-detail': CustomEvent<OncMojo.NetworkStateProperties>;
    'show-error-toast': CustomEvent<string>;
    'show-esim-profile-rename-dialog':
        CustomEvent<{networkState: NetworkStateProperties}>;
    'show-esim-remove-profile-dialog':
        CustomEvent<{networkState: NetworkStateProperties}>;
    'show-known-networks': CustomEvent<NetworkType>;
    'show-networks': CustomEvent<NetworkType>;
  }
}

interface SettingsInternetPageElement {
  $: {
    errorToast: CrToastElement,
  };
}

const SettingsInternetPageElementBase =
    mixinBehaviors(
        [
          NetworkListenerBehavior,
        ],
        DeepLinkingMixin(PrefsMixin(RouteObserverMixin(
            WebUiListenerMixin(I18nMixin(PolymerElement)))))) as {
      new (): PolymerElement & I18nMixinInterface &
          WebUiListenerMixinInterface & RouteObserverMixinInterface &
          PrefsMixinInterface & DeepLinkingMixinInterface &
          NetworkListenerBehaviorInterface,
    };

class SettingsInternetPageElement extends SettingsInternetPageElementBase {
  static get is() {
    return 'settings-internet-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The device state for each network device type, keyed by NetworkType.
       * Set by network-summary.
       */
      deviceStates: {
        type: Object,
        notify: true,
        observer: 'onDeviceStatesChanged_',
      },

      /**
       * Highest priority connected network or null. Set by network-summary.
       */
      defaultNetwork: {
        type: Object,
        notify: true,
      },

      /**
       * Hotspot information. Set by network-summary.
       */
      hotspotInfo: {
        type: Object,
        notify: true,
      },

      /**
       * Set by internet-subpage. Controls spinner visibility in subpage header.
       */
      showSpinner_: Boolean,

      /**
       * The network type for the networks subpage when shown.
       */
      subpageType_: Number,

      /**
       * The network type for the known networks subpage when shown.
       */
      knownNetworksType_: Number,

      /**
       * Whether the 'Add connection' section is expanded.
       */
      addConnectionExpanded_: {
        type: Boolean,
        value: false,
      },

      /**
       * True if VPN is prohibited by policy.
       */
      vpnIsProhibited_: {
        type: Boolean,
        value: false,
      },

      globalPolicy_: Object,

      /**
       * Whether a managed network is available in the visible network list.
       */
      managedNetworkAvailable: {
        type: Boolean,
        value: false,
      },

      /**
       * List of third party (Extension + Arc) VPN providers.
       */
      vpnProviders_: {
        type: Array,
        value() {
          return [];
        },
      },

      showInternetConfig_: {
        type: Boolean,
        value: false,
      },

      isApnRevampEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isApnRevampEnabled') &&
              loadTimeData.getBoolean('isApnRevampEnabled');
        },
      },

      /**
       * Page name, if defined, indicating that the next deviceStates update
       * should call attemptShowCellularSetupDialog_().
       */
      pendingShowCellularSetupDialogAttemptPageName_: {
        type: String,
        value: null,
      },

      showCellularSetupDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Name of cellular setup dialog page.
       */
      cellularSetupDialogPageName_: String,

      hasActiveCellularNetwork_: {
        type: Boolean,
        value: false,
      },

      isConnectedToNonCellularNetwork_: {
        type: Boolean,
        value: false,
      },

      showESimProfileRenameDialog_: {
        type: Boolean,
        value: false,
      },

      showESimRemoveProfileDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      showHotspotConfigDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Flag, if true, indicating that the next deviceStates update
       * should set showSimLockDialog_ to true.
       */
      pendingShowSimLockDialog_: {
        type: Boolean,
        value: false,
      },

      showSimLockDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * eSIM network used in internet detail menu.
       */
      eSimNetworkState_: {
        type: Object,
        value: '',
      },

      focusConfig_: {
        type: Object,
        value() {
          return new Map();
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kWifiOnOff,
          Setting.kMobileOnOff,
        ]),
      },

      errorToastMessage_: {
        type: String,
        value: '',
      },

      /**
       * Return true if hotspot feature flag is enabled.
       */
      isHotspotFeatureEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isHotspotEnabled') &&
              loadTimeData.getBoolean('isHotspotEnabled');
        },
      },

      /**
       * Whether the 'Add custom APN' button is disabled.
       */
      isCreateCustomApnButtonDisabled_: {
        type: Boolean,
      },
    };
  }

  defaultNetwork: OncMojo.NetworkStateProperties|null|undefined;
  deviceStates: Record<string, OncMojo.DeviceStateProperties>|undefined;
  hotspotInfo: HotspotInfo|undefined;
  managedNetworkAvailable: boolean;
  private addConnectionExpanded_: boolean;
  private browserProxy_: InternetPageBrowserProxy;
  private cellularSetupDialogPageName_: CellularSetupPageName|null;
  private detailType_: NetworkType|undefined;
  private errorToastMessage_: string;
  private eSimNetworkState_: NetworkStateProperties;
  private focusConfig_: Map<string, HTMLElement>;
  private globalPolicy_: GlobalPolicy|undefined;
  private hasActiveCellularNetwork_: boolean;
  private isApnRevampEnabled_: boolean;
  private isConnectedToNonCellularNetwork_: boolean;
  private isCreateCustomApnButtonDisabled_: boolean;
  private isHotspotFeatureEnabled_: boolean;
  private knownNetworksType_: NetworkType;
  private networkConfig_: CrosNetworkConfigRemote;
  private pendingShowCellularSetupDialogAttemptPageName_: CellularSetupPageName|
      null;
  private pendingShowSimLockDialog_: boolean;
  private showCellularSetupDialog_: boolean;
  private showESimProfileRenameDialog_: boolean;
  private showESimRemoveProfileDialog_: boolean;
  private showHotspotConfigDialog_: boolean;
  private showInternetConfig_: boolean;
  private showSimLockDialog_: boolean;
  private showSpinner_: boolean;
  private subpageType_: NetworkType;
  private vpnIsProhibited_: boolean;
  private vpnProviders_: VpnProvider[];

  constructor() {
    super();

    /**
     * Type of last detail page visited
     */
    this.detailType_ = undefined;

    this.browserProxy_ = InternetPageBrowserProxyImpl.getInstance();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('device-enabled-toggled', (event) => {
      this.onDeviceEnabledToggled_(event);
    });
    this.addEventListener('network-connect', (event) => {
      this.onNetworkConnect_(event);
    });
    this.addEventListener('show-cellular-setup', (event) => {
      this.onShowCellularSetupDialog_(event);
    });
    this.addEventListener('show-config', (event) => {
      this.onShowConfig_(event);
    });
    this.addEventListener('show-detail', (event) => {
      this.onShowDetail_(event);
    });
    this.addEventListener('show-known-networks', (event) => {
      this.onShowKnownNetworks_(event);
    });
    this.addEventListener('show-networks', (event) => {
      this.onShowNetworks_(event);
    });
    this.addEventListener('show-esim-profile-rename-dialog', (event) => {
      this.onShowEsimProfileRenameDialog_(event);
    });
    this.addEventListener('show-esim-remove-profile-dialog', (event) => {
      this.onShowEsimRemoveProfileDialog_(event);
    });
    this.addEventListener('show-hotspot-config-dialog', () => {
      this.onShowHotspotConfigDialog_();
    });
    this.addEventListener('show-error-toast', (event) => {
      this.onShowErrorToast_(event);
    });
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.onPoliciesApplied(/*userhash=*/ '');
    this.onVpnProvidersChanged();
    this.onNetworkStateListChanged();
  }

  /**
   * Overridden from DeepLinkingMixin.
   */
  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    // Manually show the deep links for settings nested within elements.
    let networkType: NetworkType|null = null;
    if (settingId === Setting.kWifiOnOff) {
      networkType = NetworkType.kWiFi;
    } else if (settingId === Setting.kMobileOnOff) {
      networkType = NetworkType.kCellular;
    }

    afterNextRender(this, () => {
      const networkRow =
          this.shadowRoot!.querySelector('network-summary')!.getNetworkRow(
              networkType!);
      if (networkRow) {
        const toggleEl = networkRow.getDeviceEnabledToggle();
        if (toggleEl) {
          this.showDeepLinkElement(toggleEl);
          return;
        }
      }
      console.warn(`Element with deep link id ${settingId} not focusable.`);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  }

  /**
   * RouteObserverMixin override
   */
  override currentRouteChanged(route: Route, oldRoute?: Route): void {
    if (route === routes.INTERNET_NETWORKS) {
      // Handle direct navigation to the networks page,
      // e.g. chrome://settings/internet/networks?type=WiFi
      const queryParams = Router.getInstance().getQueryParameters();
      const type = queryParams.get('type');
      if (type) {
        this.subpageType_ = OncMojo.getNetworkTypeFromString(type);
      }

      if (!oldRoute && queryParams.get('showCellularSetup') === 'true') {
        const pageName = queryParams.get('showPsimFlow') === 'true' ?
            CellularSetupPageName.PSIM_FLOW_UI :
            CellularSetupPageName.ESIM_FLOW_UI;
        // If the page just loaded, deviceStates will not be fully initialized
        // yet. Set pendingShowCellularSetupDialogAttemptPageName_ to indicate
        // showCellularSetupDialogAttempt_() should be called next deviceStates
        // update.
        this.pendingShowCellularSetupDialogAttemptPageName_ = pageName;
      }

      // If the page just loaded, deviceStates will not be fully initialized
      // yet. Set pendingShowSimLockDialog_ to indicate
      // showSimLockDialog_ should be set next deviceStates
      // update.
      this.pendingShowSimLockDialog_ = !oldRoute &&
          !!queryParams.get('showSimLockDialog') &&
          this.subpageType_ === NetworkType.kCellular;
    } else if (route === routes.KNOWN_NETWORKS) {
      // Handle direct navigation to the known networks page,
      // e.g. chrome://settings/internet/knownNetworks?type=WiFi
      const queryParams = Router.getInstance().getQueryParameters();
      const type = queryParams.get('type');
      if (type) {
        this.knownNetworksType_ = OncMojo.getNetworkTypeFromString(type);
      } else {
        this.knownNetworksType_ = NetworkType.kWiFi;
      }
    } else if (route === routes.INTERNET) {
      // Show deep links for the internet page.
      this.attemptDeepLink();
    } else if (route !== routes.BASIC) {
      // If we are navigating to a non internet section, do not set focus.
      return;
    }

    if (!oldRoute || !routes.INTERNET || !routes.INTERNET.contains(oldRoute)) {
      return;
    }

    // Focus the subpage arrow where appropriate.
    let element: HTMLElement|null = null;
    if (route === routes.INTERNET_NETWORKS) {
      // iron-list makes the correct timing to focus an item in the list
      // very complicated, and the item may not exist, so just focus the
      // entire list for now.
      const subPage =
          this.shadowRoot!.querySelector('settings-internet-subpage');
      if (subPage) {
        element = subPage.shadowRoot!.querySelector('#networkList');
      }
    } else if (this.detailType_ !== undefined) {
      const rowForDetailType =
          this.shadowRoot!.querySelector('network-summary')!.getNetworkRow(
              this.detailType_);

      // Note: It is possible that the row is no longer present in the DOM
      // (e.g., when a Cellular dongle is unplugged or when Instant Tethering
      // becomes unavailable due to the Bluetooth controller disconnecting).
      if (rowForDetailType) {
        element = rowForDetailType.shadowRoot!.querySelector('.subpage-arrow');
      }
    }

    if (element) {
      this.focusConfig_.set(oldRoute.path, element);
    } else {
      this.focusConfig_.delete(oldRoute.path);
    }
  }

  /** NetworkListenerBehavior override */
  override onNetworkStateListChanged(): void {
    hasActiveCellularNetwork().then((hasActive) => {
      this.hasActiveCellularNetwork_ = hasActive;
    });
    this.updateIsConnectedToNonCellularNetwork_();
  }

  override async onVpnProvidersChanged(): Promise<void> {
    const response = await this.networkConfig_.getVpnProviders();
    const providers = response.providers;
    providers.sort(this.compareVpnProviders_);
    this.vpnProviders_ = providers;
  }

  override async onPoliciesApplied(_userhash: string): Promise<void> {
    const response = await this.networkConfig_.getGlobalPolicy();
    this.globalPolicy_ = response.result;
  }

  private async updateIsConnectedToNonCellularNetwork_(): Promise<boolean> {
    const isConnected = await isConnectedToNonCellularNetwork();
    this.isConnectedToNonCellularNetwork_ = isConnected;
    return isConnected;
  }

  /**
   * Event triggered by a device state enabled toggle.
   */
  private onDeviceEnabledToggled_(
      event: CustomEvent<{enabled: boolean, type: NetworkType}>): void {
    this.networkConfig_.setNetworkTypeEnabledState(
        event.detail.type, event.detail.enabled);
    recordSettingChange();
  }

  private onShowConfig_(
      event: CustomEvent<{type: string, guid: string|null, name: string|null}>):
      void {
    const type = OncMojo.getNetworkTypeFromString(event.detail.type);
    if (!event.detail.guid) {
      // New configuration
      this.showConfig_(/* configAndConnect= */ true, type);
    } else {
      this.showConfig_(
          /* configAndConnect= */ false, type, event.detail.guid,
          event.detail.name);
    }
  }

  private onShowCellularSetupDialog_(
      event: CustomEvent<{pageName: CellularSetupPageName}>): void {
    this.attemptShowCellularSetupDialog_(event.detail.pageName);
  }

  /**
   * Opens the cellular setup dialog if pageName is PSIM_FLOW_UI, or if pageName
   * is ESIM_FLOW_UI and isConnectedToNonCellularNetwork_ is true. If
   * isConnectedToNonCellularNetwork_ is false, shows an error toast.
   */
  private attemptShowCellularSetupDialog_(pageName: CellularSetupPageName):
      void {
    const cellularDeviceState =
        this.getDeviceState_(NetworkType.kCellular, this.deviceStates);
    if (!cellularDeviceState ||
        cellularDeviceState.deviceState !== DeviceStateType.kEnabled) {
      this.showErrorToast_(this.i18n('eSimMobileDataNotEnabledErrorToast'));
      return;
    }

    if (pageName === CellularSetupPageName.PSIM_FLOW_UI) {
      this.showCellularSetupDialog_ = true;
      this.cellularSetupDialogPageName_ = pageName;
    } else {
      this.attemptShowEsimSetupDialog_();
    }
  }

  private async attemptShowEsimSetupDialog_(): Promise<void> {
    const numProfiles = await getNumESimProfiles();
    if (numProfiles >= ESIM_PROFILE_LIMIT) {
      this.showErrorToast_(
          this.i18n('eSimProfileLimitReachedErrorToast', ESIM_PROFILE_LIMIT));
      return;
    }

    // isConnectedToNonCellularNetwork_ may
    // not be fetched yet if the page just opened, fetch it
    // explicitly.
    const isConnected = await this.updateIsConnectedToNonCellularNetwork_();
    this.showCellularSetupDialog_ =
        isConnected || loadTimeData.getBoolean('bypassConnectivityCheck');
    if (!this.showCellularSetupDialog_) {
      this.showErrorToast_(this.i18n('eSimNoConnectionErrorToast'));
      return;
    }
    this.cellularSetupDialogPageName_ = CellularSetupPageName.ESIM_FLOW_UI;
  }

  private onShowErrorToast_(event: CustomEvent<string>): void {
    this.showErrorToast_(event.detail);
  }

  private showErrorToast_(message: string): void {
    this.errorToastMessage_ = message;
    this.$.errorToast.show();
  }

  private onCloseCellularSetupDialog_(): void {
    this.showCellularSetupDialog_ = false;
  }

  private showConfig_(
      configAndConnect: boolean, type: NetworkType, guid?: string|null,
      name?: string|null): void {
    assert(type !== NetworkType.kCellular && type !== NetworkType.kTether);
    if (this.showInternetConfig_) {
      return;
    }
    this.showInternetConfig_ = true;
    // Async call to ensure dialog is stamped.
    setTimeout(() => {
      const configDialog =
          castExists(this.shadowRoot!.querySelector<InternetConfigElement>(
              '#configDialog'));
      configDialog.type = OncMojo.getNetworkTypeString(type);
      configDialog.guid = guid || '';
      configDialog.name = name || '';
      configDialog.showConnect = configAndConnect;
      configDialog.open();
    });
  }

  private onInternetConfigClose_(): void {
    this.showInternetConfig_ = false;
  }

  private onShowDetail_(event: CustomEvent<OncMojo.NetworkStateProperties>):
      void {
    const networkState = event.detail;
    this.detailType_ = networkState.type;
    const params = new URLSearchParams();
    params.append('guid', networkState.guid);
    params.append('type', OncMojo.getNetworkTypeString(networkState.type));
    params.append('name', OncMojo.getNetworkStateDisplayName(networkState));
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);
  }

  private onShowEsimProfileRenameDialog_(
      event: CustomEvent<{networkState: NetworkStateProperties}>): void {
    this.eSimNetworkState_ = event.detail.networkState;
    this.showESimProfileRenameDialog_ = true;
  }

  private onCloseEsimProfileRenameDialog_(): void {
    this.showESimProfileRenameDialog_ = false;
  }

  private onShowEsimRemoveProfileDialog_(
      event: CustomEvent<{networkState: NetworkStateProperties}>): void {
    this.eSimNetworkState_ = event.detail.networkState;
    this.showESimRemoveProfileDialog_ = true;
  }

  private onCloseEsimRemoveProfileDialog_(): void {
    this.showESimRemoveProfileDialog_ = false;
  }

  private onShowHotspotConfigDialog_() {
    this.showHotspotConfigDialog_ = true;
  }

  private onCloseHotspotConfigDialog_(): void {
    this.showHotspotConfigDialog_ = false;
  }

  private onShowNetworks_(event: CustomEvent<NetworkType>): void {
    this.showNetworksSubpage_(event.detail);
  }

  private getNetworksPageTitle_(): string {
    // The shared Cellular/Tether subpage is referred to as "Mobile".
    // TODO(khorimoto): Remove once Cellular/Tether are split into their own
    // sections.
    if (this.subpageType_ === NetworkType.kCellular ||
        this.subpageType_ === NetworkType.kTether) {
      return this.i18n('OncTypeMobile');
    }
    return this.i18n(
        'OncType' + OncMojo.getNetworkTypeString(this.subpageType_));
  }

  private getDeviceState_(
      subpageType: NetworkType,
      deviceStates: Record<string, OncMojo.DeviceStateProperties>|
      undefined): OncMojo.DeviceStateProperties|undefined {
    if (subpageType === undefined) {
      return undefined;
    }
    // If both Tether and Cellular are enabled, use the Cellular device state
    // when directly navigating to the Tether page.
    if (subpageType === NetworkType.kTether &&
        this.deviceStates![NetworkType.kCellular]) {
      subpageType = NetworkType.kCellular;
    }
    return deviceStates![subpageType];
  }

  private getTetherDeviceState_(
      deviceStates: Record<string, OncMojo.DeviceStateProperties>|
      undefined): OncMojo.DeviceStateProperties|undefined {
    return deviceStates![NetworkType.kTether];
  }

  private onDeviceStatesChanged_(
      newValue: Record<string, OncMojo.DeviceStateProperties>|undefined,
      _oldValue: Record<string, OncMojo.DeviceStateProperties>|
      undefined): void {
    const wifiDeviceState = this.getDeviceState_(NetworkType.kWiFi, newValue);
    let managedNetworkAvailable = false;
    if (wifiDeviceState) {
      managedNetworkAvailable = !!wifiDeviceState.managedNetworkAvailable;
    }

    if (this.managedNetworkAvailable !== managedNetworkAvailable) {
      this.managedNetworkAvailable = managedNetworkAvailable;
    }

    assert(this.deviceStates);
    const vpn = this.deviceStates[NetworkType.kVPN];
    this.vpnIsProhibited_ =
        !!vpn && vpn.deviceState === DeviceStateType.kProhibited;

    if (this.detailType_ && !this.deviceStates[this.detailType_]) {
      // If the device type associated with the current network has been
      // removed (e.g., due to unplugging a Cellular dongle), the details page,
      // if visible, displays controls which are no longer functional. If this
      // case occurs, close the details page.
      const detailPage =
          this.shadowRoot!.querySelector('settings-internet-detail-subpage');
      if (detailPage) {
        detailPage.close();
      }
    }

    if (this.pendingShowCellularSetupDialogAttemptPageName_) {
      this.attemptShowCellularSetupDialog_(
          this.pendingShowCellularSetupDialogAttemptPageName_);
      this.pendingShowCellularSetupDialogAttemptPageName_ = null;
    }

    if (this.pendingShowSimLockDialog_) {
      this.showSimLockDialog_ = true;
      this.pendingShowSimLockDialog_ = false;
    }
  }

  private onShowKnownNetworks_(event: CustomEvent<NetworkType>): void {
    const type = event.detail;
    this.detailType_ = type;
    this.knownNetworksType_ = type;
    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(type));
    Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
  }

  private onAddWiFiClick_(): void {
    this.showConfig_(true /* configAndConnect */, NetworkType.kWiFi);
  }

  private onAddVpnClick_(): void {
    if (!this.vpnIsProhibited_) {
      this.showConfig_(true /* configAndConnect */, NetworkType.kVPN);
    }
  }

  private onAddThirdPartyVpnClick_(event: DomRepeatEvent<VpnProvider>): void {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.appId);
    recordSettingChange();
  }

  private showNetworksSubpage_(type: NetworkType): void {
    this.detailType_ = type;
    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(type));
    this.subpageType_ = type;
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
  }

  private compareVpnProviders_(
      vpnProvider1: VpnProvider, vpnProvider2: VpnProvider): number {
    // Show Extension VPNs before Arc VPNs.
    if (vpnProvider1.type < vpnProvider2.type) {
      return -1;
    }
    if (vpnProvider1.type > vpnProvider2.type) {
      return 1;
    }
    // Show VPNs of the same type by lastLaunchTime.
    if (vpnProvider1.lastLaunchTime.internalValue >
        vpnProvider2.lastLaunchTime.internalValue) {
      return -1;
    }
    if (vpnProvider1.lastLaunchTime.internalValue <
        vpnProvider2.lastLaunchTime.internalValue) {
      return 1;
    }
    return 0;
  }

  private wifiIsEnabled_(deviceStates: OncMojo.DeviceStateProperties[]):
      boolean {
    const wifi = deviceStates[NetworkType.kWiFi];
    return !!wifi && wifi.deviceState === DeviceStateType.kEnabled;
  }

  private shouldShowAddWiFiRow_(
      globalPolicy: GlobalPolicy, managedNetworkAvailable: boolean,
      deviceStates: OncMojo.DeviceStateProperties[]): boolean {
    return this.allowAddWiFiConnection_(
               globalPolicy, managedNetworkAvailable) &&
        this.wifiIsEnabled_(deviceStates);
  }

  private allowAddWiFiConnection_(
      globalPolicy: GlobalPolicy, managedNetworkAvailable: boolean): boolean {
    if (!globalPolicy) {
      return true;
    }

    return !globalPolicy.allowOnlyPolicyWifiNetworksToConnect &&
        (!globalPolicy.allowOnlyPolicyWifiNetworksToConnectIfAvailable ||
         !managedNetworkAvailable);
  }

  private allowAddConnection_(
      globalPolicy: GlobalPolicy, managedNetworkAvailable: boolean): boolean {
    if (!this.vpnIsProhibited_) {
      return true;
    }
    return this.allowAddWiFiConnection_(globalPolicy, managedNetworkAvailable);
  }

  private getAddThirdPartyVpnLabel_(provider: VpnProvider): string {
    return this.i18n('internetAddThirdPartyVPN', provider.providerName || '');
  }

  /**
   * Handles UI requests to connect to a network.
   * TODO(stevenjb): Handle Cellular activation, etc.
   */
  private async onNetworkConnect_(event: CustomEvent<{
    networkState: OncMojo.NetworkStateProperties,
    bypassConnectionDialog: boolean|undefined,
  }>): Promise<void> {
    const networkState = event.detail.networkState;
    const type = networkState.type;
    const displayName = OncMojo.getNetworkStateDisplayName(networkState);

    if (!event.detail.bypassConnectionDialog && type === NetworkType.kTether &&
        !networkState.typeState.tether!.hasConnectedToHost) {
      const params = new URLSearchParams();
      params.append('guid', networkState.guid);
      params.append('type', OncMojo.getNetworkTypeString(type));
      params.append('name', displayName);
      params.append('showConfigure', true.toString());

      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);
      return;
    }

    if (OncMojo.networkTypeHasConfigurationFlow(type) &&
        (!OncMojo.isNetworkConnectable(networkState) ||
         !!networkState.errorState)) {
      this.showConfig_(
          true /* configAndConnect */, type, networkState.guid, displayName);
      return;
    }

    const response = await this.networkConfig_.startConnect(networkState.guid);
    switch (response.result) {
      case StartConnectResult.kSuccess:
        return;
      case StartConnectResult.kInvalidGuid:
      case StartConnectResult.kInvalidState:
      case StartConnectResult.kCanceled:
        // TODO(stevenjb/khorimoto): Consider handling these cases.
        return;
      case StartConnectResult.kNotConfigured:
        if (OncMojo.networkTypeHasConfigurationFlow(type)) {
          this.showConfig_(
              true /* configAndConnect */, type, networkState.guid,
              displayName);
        }
        return;
      case StartConnectResult.kBlocked:
        // This shouldn't happen, the UI should prevent this, fall through and
        // show the error.
      case StartConnectResult.kUnknown:
        console.warn(
            'startConnect failed for: ' + networkState.guid +
            ' Error: ' + response.message);
        return;
    }
    assertNotReached();
  }

  /**
   * Handles UI requests to add new APN.
   */
  private onCreateCustomApnClicked_(): void {
    if (this.isCreateCustomApnButtonDisabled_) {
      return;
    }
    const apnSubpage = castExists(
        this.shadowRoot!.querySelector<ApnSubpageElement>('#apnSubpage'));
    apnSubpage.openApnDetailDialogInCreateMode();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsInternetPageElement.is]: SettingsInternetPageElement;
  }
}

customElements.define(
    SettingsInternetPageElement.is, SettingsInternetPageElement);
