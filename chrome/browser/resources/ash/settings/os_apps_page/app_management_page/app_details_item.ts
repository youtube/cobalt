// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import './app_management_cros_shared_style.css.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType, InstallReason, InstallSource} from 'chrome://resources/cr_components/app_management/constants.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_details_item.html.js';
import {AppManagementBrowserProxy} from './browser_proxy.js';
import {AppManagementStoreMixin} from './store_mixin.js';

const AppManagementAppDetailsItemBase =
    AppManagementStoreMixin(I18nMixin(PolymerElement));

export class AppManagementAppDetailsItem extends
    AppManagementAppDetailsItemBase {
  static get is() {
    return 'app-management-app-details-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: {
        type: Object,
      },

      hidden: {
        type: Boolean,
        computed: 'isHidden_()',
        reflectToAttribute: true,
      },

      appId_: {
        type: String,
        observer: 'appIdChanged_',
      },
    };
  }

  app: App;
  private appId_: string;

  override connectedCallback(): void {
    super.connectedCallback();
    this.watch('appId_', state => state.selectedAppId);
    this.updateFromStore();
  }

  override hidden: boolean;

  private isHidden_(): boolean {
    return !loadTimeData.getBoolean('appManagementAppDetailsEnabled');
  }

  private appIdChanged_(appId: string): void {
    if (appId && this.app) {
      AppManagementBrowserProxy.getInstance().handler.updateAppSize(appId);
    }
  }

  /**
   * The version is shown for every app apart from System apps.
   */
  private shouldShowVersion_(app: App): boolean {
    if (app.installReason === InstallReason.kSystem) {
      return false;
    }
    return Boolean(app.version);
  }

  /**
   * Storage information is shown for every app apart from System apps.
   */
  private shouldShowStorage_(app: App): boolean {
    if (app.installReason === InstallReason.kSystem) {
      return false;
    }
    return true;
  }

  private shouldShowAppSize_(app: App): boolean {
    return Boolean(app.appSize);
  }

  private shouldShowDataSize_(app: App): boolean {
    return Boolean(app.dataSize);
  }

  private shouldShowInfoIcon_(app: App): boolean {
    return app.type === AppType.kWeb &&
        (app.installSource === InstallSource.kBrowser ||
         app.installSource === InstallSource.kSync);
  }

  /**
   * The launch icon is show for apps installed from the Chrome Web
   * Store and Google Play Store.
   */
  private shouldShowLaunchIcon_(app: App): boolean {
    return app.installSource === InstallSource.kChromeWebStore ||
        app.installSource === InstallSource.kPlayStore;
  }

  private getTypeString_(app: App): string {
    // When installReason = kSystem, the system has determined that the app
    // needs to be installed. This includes apps such as Chrome and the Play
    // Store.
    if (app.installReason === InstallReason.kSystem) {
      return this.i18n('appManagementAppDetailsTypeCrosSystem');
    }
    switch (app.type) {
      case AppType.kArc:
        return this.i18n('appManagementAppDetailsTypeAndroid');
      case AppType.kChromeApp:
      case AppType.kStandaloneBrowserChromeApp:
        return this.i18n('appManagementAppDetailsTypeChrome');
      case AppType.kWeb:
      case AppType.kExtension:
      case AppType.kStandaloneBrowserExtension:
        return this.i18n('appManagementAppDetailsTypeWeb');
      default:
        console.error('App type not handled by app management.');
        return '';
    }
  }

  private getInstallSourceString_(app: App): string {
    switch (app.installSource) {
      case InstallSource.kChromeWebStore:
        return this.i18n('appManagementAppDetailsInstallSourceWebStore');
      case InstallSource.kPlayStore:
        return this.i18n('appManagementAppDetailsInstallSourcePlayStore');
      default:
        console.error('Install source not recognised.');
        return '';
    }
  }

  private getTypeAndSourceString_(app: App): string {
    if (app.type === AppType.kWeb &&
        (app.installSource === InstallSource.kBrowser ||
         app.installSource === InstallSource.kSync)) {
      return this.i18n('appManagementAppDetailsInstallSourceBrowser');
    }
    if (app.installSource === InstallSource.kPlayStore ||
        app.installSource === InstallSource.kChromeWebStore) {
      return this
          .i18nAdvanced('appManagementAppDetailsTypeAndSourceCombined', {
            substitutions: [
              this.getTypeString_(app),
              this.getInstallSourceString_(app),
            ],
          })
          .toString();
    }
    if (app.installSource === InstallSource.kSystem) {
      return this
          .i18nAdvanced('appManagementAppDetailsTypeAndSourcePreinstalledApp', {
            substitutions: [
              this.getTypeString_(app),
              loadTimeData.getString('appManagementDeviceName'),
            ],
          })
          .toString();
    }
    return this.getTypeString_(app);
  }

  private onStoreLinkClicked_(e: CustomEvent<{event: Event}>): void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    if (e.detail.event) {  // When the store link is clicked
      e.detail.event.preventDefault();
      e.stopPropagation();
    }

    if (this.app !== null) {
      AppManagementBrowserProxy.getInstance().handler.openStorePage(
          this.app.id);
    }
  }

  /**
   * Returns the sanitized URL for apps downloaded from the Chrome browser, to
   * be shown in the tooltip.
   */
  private getTooltipText_(app: App): string {
    switch (app.installSource) {
      case InstallSource.kBrowser:
      case InstallSource.kSync:
        return app.publisherId.replace(/\?.*$/g, '');
      default:
        return '';
    }
  }

  private getTooltipA11yText_(app: App): string {
    return this.i18n(
        'appManagementAppDetailsTooltipWebA11y', this.getTooltipText_(app));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-app-details-item': AppManagementAppDetailsItem;
  }
}

customElements.define(
    AppManagementAppDetailsItem.is, AppManagementAppDetailsItem);
