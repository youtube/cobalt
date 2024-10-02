// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-subpage' is the settings subpage for managing Crostini.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import './crostini_confirmation_dialog.js';
import './crostini_disk_resize_dialog.js';
import './crostini_disk_resize_confirmation_dialog.js';
import './crostini_port_forwarding.js';
import './crostini_extra_containers.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {TERMINA_VM_TYPE} from '../guest_os/guest_os_browser_proxy.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniDiskInfo} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_subpage.html.js';

/**
 * The current confirmation state.
 */
enum ConfirmationState {
  NOT_CONFIRMED = 'notConfirmed',
  CONFIRMED = 'confirmed',
}

const SettingsCrostiniSubpageElementBase = DeepLinkingMixin(
    RouteOriginMixin(PrefsMixin(WebUiListenerMixin(PolymerElement))));

class SettingsCrostiniSubpageElement extends
    SettingsCrostiniSubpageElementBase {
  static get is() {
    return 'settings-crostini-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether export / import UI should be displayed.
       */
      showCrostiniExportImport_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showCrostiniExportImport');
        },
      },

      showArcAdbSideloading_: {
        type: Boolean,
        computed: 'and_(isArcAdbSideloadingSupported_, isAndroidEnabled_)',
      },

      isArcAdbSideloadingSupported_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('arcAdbSideloadingSupported');
        },
      },

      showCrostiniPortForwarding_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showCrostiniPortForwarding');
        },
      },

      showCrostiniExtraContainers_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showCrostiniExtraContainers');
        },
      },

      isAndroidEnabled_: {
        type: Boolean,
      },

      /**
       * Whether the uninstall options should be displayed.
       */
      hideCrostiniUninstall_: {
        type: Boolean,
        computed: 'or_(installerShowing_, upgraderDialogShowing_)',
      },

      /**
       * Whether the button to launch the Crostini container upgrade flow should
       * be shown.
       */
      showCrostiniContainerUpgrade_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showCrostiniContainerUpgrade');
        },
      },

      showDiskResizeConfirmationDialog_: {
        type: Boolean,
        value: false,
      },

      installerShowing_: {
        type: Boolean,
      },

      upgraderDialogShowing_: {
        type: Boolean,
      },

      /**
       * Whether the button to launch the Crostini container upgrade flow should
       * be disabled.
       */
      disableUpgradeButton_: {
        type: Boolean,
        computed: 'or_(installerShowing_, upgraderDialogShowing_)',
      },

      /**
       * Whether the disk resizing dialog is visible or not
       */
      showDiskResizeDialog_: {
        type: Boolean,
        value: false,
      },

      showCrostiniMicPermissionDialog_: {
        type: Boolean,
        value: false,
      },

      diskSizeLabel_: {
        type: String,
        value: loadTimeData.getString('crostiniDiskSizeCalculating'),
      },

      diskResizeButtonLabel_: {
        type: String,
        value: loadTimeData.getString('crostiniDiskResizeShowButton'),
      },

      diskResizeButtonAriaLabel_: {
        type: String,
        value: loadTimeData.getString('crostiniDiskResizeShowButtonAriaLabel'),
      },

      canDiskResize_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kUninstallCrostini,
          Setting.kCrostiniDiskResize,
          Setting.kCrostiniMicAccess,
          Setting.kCrostiniContainerUpgrade,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'onCrostiniEnabledChanged_(prefs.crostini.enabled.value)',
      'onArcEnabledChanged_(prefs.arc.enabled.value)',
    ];
  }

  private browserProxy_: CrostiniBrowserProxy;
  private canDiskResize_: boolean;
  private diskResizeButtonAriaLabel_: string;
  private diskResizeButtonLabel_: string;
  private diskResizeConfirmationState_: ConfirmationState;
  private diskSizeLabel_: string;
  private installerShowing_: boolean;
  private isAndroidEnabled_: boolean;
  private isDiskUserChosenSize_: boolean;
  private route_: Route;
  private showCrostiniContainerUpgrade_: boolean;
  private showCrostiniMicPermissionDialog_: boolean;
  private showDiskResizeConfirmationDialog_: boolean;
  private showDiskResizeDialog_: boolean;
  private upgraderDialogShowing_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route_ = routes.CROSTINI_DETAILS;

    this.isDiskUserChosenSize_ = false;

    this.diskResizeConfirmationState_ = ConfirmationState.NOT_CONFIRMED;

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'crostini-installer-status-changed', (status: boolean) => {
          this.installerShowing_ = status;
        });
    this.addWebUiListener(
        'crostini-upgrader-status-changed', (status: boolean) => {
          this.upgraderDialogShowing_ = status;
        });
    this.addWebUiListener(
        'crostini-container-upgrade-available-changed',
        (canUpgrade: boolean) => {
          this.showCrostiniContainerUpgrade_ = canUpgrade;
        });
    this.browserProxy_.requestCrostiniInstallerStatus();
    this.browserProxy_.requestCrostiniUpgraderDialogStatus();
    this.browserProxy_.requestCrostiniContainerUpgradeAvailable();
    this.loadDiskInfo_();
  }

  override ready() {
    super.ready();

    const r = routes;
    this.addFocusConfig(r.CROSTINI_SHARED_PATHS, '#crostini-shared-paths');
    this.addFocusConfig(r.BRUSCHETTA_SHARED_PATHS, '#bruschetta-shared-paths');
    this.addFocusConfig(
        r.CROSTINI_SHARED_USB_DEVICES, '#crostini-shared-usb-devices');
    this.addFocusConfig(
        r.BRUSCHETTA_SHARED_USB_DEVICES, '#bruschetta-shared-usb-devices');
    this.addFocusConfig(r.CROSTINI_EXPORT_IMPORT, '#crostini-export-import');
    this.addFocusConfig(r.CROSTINI_ANDROID_ADB, '#crostini-enable-arc-adb');
    this.addFocusConfig(
        r.CROSTINI_PORT_FORWARDING, '#crostini-port-forwarding');
    this.addFocusConfig(
        r.CROSTINI_EXTRA_CONTAINERS, '#crostini-extra-containers');
  }

  override currentRouteChanged(route: Route) {
    // Does not apply to this page.
    if (route !== routes.CROSTINI_DETAILS) {
      return;
    }

    this.attemptDeepLink();
  }

  private onCrostiniEnabledChanged_(enabled: boolean) {
    if (!enabled &&
        Router.getInstance().currentRoute === routes.CROSTINI_DETAILS) {
      Router.getInstance().navigateToPreviousRoute();
    }
    if (enabled) {
      // The disk size or type could have changed due to the user reinstalling
      // Crostini, update our info.
      this.loadDiskInfo_();
    }
  }

  private onArcEnabledChanged_(enabled: boolean) {
    this.isAndroidEnabled_ = enabled;
  }

  private onExportImportClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_EXPORT_IMPORT);
  }

  private onEnableArcAdbClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_ANDROID_ADB);
  }

  private loadDiskInfo_() {
    this.browserProxy_
        .getCrostiniDiskInfo(TERMINA_VM_TYPE, /*requestFullInfo=*/ false)
        .then(
            diskInfo => {
              if (diskInfo.succeeded) {
                this.setResizeLabels_(diskInfo);
              }
            },
            reason => {
              console.warn(`Unable to get info: ${reason}`);
            });
  }

  private setResizeLabels_(diskInfo: CrostiniDiskInfo) {
    this.canDiskResize_ = diskInfo.canResize;
    if (!this.canDiskResize_) {
      this.diskSizeLabel_ =
          loadTimeData.getString('crostiniDiskResizeNotSupportedSubtext');
      return;
    }
    this.isDiskUserChosenSize_ = diskInfo.isUserChosenSize;
    if (this.isDiskUserChosenSize_) {
      if (diskInfo.ticks) {
        this.diskSizeLabel_ = diskInfo.ticks[diskInfo.defaultIndex].label;
      }
      this.diskResizeButtonLabel_ =
          loadTimeData.getString('crostiniDiskResizeShowButton');
      this.diskResizeButtonAriaLabel_ =
          loadTimeData.getString('crostiniDiskResizeShowButtonAriaLabel');
    } else {
      this.diskSizeLabel_ = loadTimeData.getString(
          'crostiniDiskResizeDynamicallyAllocatedSubtext');
      this.diskResizeButtonLabel_ =
          loadTimeData.getString('crostiniDiskReserveSizeButton');
      this.diskResizeButtonAriaLabel_ =
          loadTimeData.getString('crostiniDiskReserveSizeButtonAriaLabel');
    }
  }

  private onDiskResizeClick_() {
    if (!this.isDiskUserChosenSize_ &&
        this.diskResizeConfirmationState_ !== ConfirmationState.CONFIRMED) {
      this.showDiskResizeConfirmationDialog_ = true;
      return;
    }
    this.showDiskResizeDialog_ = true;
  }

  private onDiskResizeDialogClose_() {
    this.showDiskResizeDialog_ = false;
    this.diskResizeConfirmationState_ = ConfirmationState.NOT_CONFIRMED;
    // DiskInfo could have changed.
    this.loadDiskInfo_();
  }

  private onDiskResizeConfirmationDialogClose_() {
    // The on_cancel is followed by on_close, so check cancel didn't happen
    // first.
    if (this.showDiskResizeConfirmationDialog_) {
      this.diskResizeConfirmationState_ = ConfirmationState.CONFIRMED;
      this.showDiskResizeConfirmationDialog_ = false;
      this.showDiskResizeDialog_ = true;
    }
  }

  private onDiskResizeConfirmationDialogCancel_() {
    this.showDiskResizeConfirmationDialog_ = false;
  }

  /**
   * Shows a confirmation dialog when removing crostini.
   */
  private onRemoveClick_() {
    this.browserProxy_.requestRemoveCrostini();
    recordSettingChange();
  }

  /**
   * Shows the upgrade flow dialog.
   */
  private onContainerUpgradeClick_() {
    this.browserProxy_.requestCrostiniContainerUpgradeView();
  }

  private onSharedPathsClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_SHARED_PATHS);
  }

  private onSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_SHARED_USB_DEVICES);
  }

  private onBruschettaSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(routes.BRUSCHETTA_SHARED_USB_DEVICES);
  }

  private onPortForwardingClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_PORT_FORWARDING);
  }

  private onExtraContainersClick_() {
    Router.getInstance().navigateTo(routes.CROSTINI_EXTRA_CONTAINERS);
  }

  private getMicToggle_(): SettingsToggleButtonElement {
    return castExists(
        this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crostini-mic-permission-toggle'));
  }

  /**
   * If a change to the mic settings requires Crostini to be restarted, a
   * dialog is shown.
   */
  private async onMicPermissionChange_() {
    if (await this.browserProxy_.checkCrostiniIsRunning()) {
      this.showCrostiniMicPermissionDialog_ = true;
    } else {
      this.getMicToggle_().sendPrefChange();
    }
  }

  private onCrostiniMicPermissionDialogClose_(e: CustomEvent) {
    const toggle = this.getMicToggle_();
    if (e.detail.accepted) {
      toggle.sendPrefChange();
      this.browserProxy_.shutdownCrostini();
    } else {
      toggle.resetToPrefValue();
    }

    this.showCrostiniMicPermissionDialog_ = false;
  }

  private and_(a: boolean, b: boolean): boolean {
    return a && b;
  }

  private or_(a: boolean, b: boolean): boolean {
    return a || b;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-subpage': SettingsCrostiniSubpageElement;
  }
}

customElements.define(
    SettingsCrostiniSubpageElement.is, SettingsCrostiniSubpageElement);
