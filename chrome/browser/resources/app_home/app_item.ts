// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppInfo, ClickEvent, RunOnOsLoginMode} from './app_home.mojom-webui.js';
import {AppHomeUserAction, recordUserAction} from './app_home_utils.js';
import {getTemplate} from './app_item.html.js';
import {BrowserProxy} from './browser_proxy.js';
import {UserDisplayMode} from './user_display_mode.mojom-webui.js';

export interface AppItemElement {
  $: {
    menu: CrActionMenuElement,
    iconContainer: HTMLElement,
  };
}

export class AppItemElement extends PolymerElement {
  static get is() {
    return 'app-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      appInfo: Object,
    };
  }

  appInfo: AppInfo;

  override ready() {
    super.ready();
    this.addEventListener('contextmenu', this.handleContextMenu_);
    this.addEventListener('click', this.handleClick_);
    this.addEventListener('auxclick', this.handleClick_);
  }

  closeContextMenu() {
    if (!this.$.menu.open) {
      return;
    }
    this.$.menu.close();
    this.fire_('on-menu-closed', {appItem: this});
  }

  private handleContextMenu_(e: Event) {
    const position = this.getPositionForEvent_(e);
    if (this.isValidPosition(position)) {
      // Show custom context menu only if it is inside the area of the item that
      // triggered it.
      this.fire_('on-menu-open-triggered', {
        appItem: this,
      });
      this.$.menu.showAtPosition(position);
      recordUserAction(AppHomeUserAction.CONTEXT_MENU_TRIGGERED);
    }

    e.preventDefault();
    e.stopPropagation();
  }

  private isValidPosition(position: any) {
    const rect =
        this.shadowRoot!.getElementById(
                            'objectContainer')!.getBoundingClientRect();
    if (!rect) {
      return false;
    }

    return (
        position.top >= rect.top && position.top <= rect.bottom &&
        position.left >= rect.left && position.left <= rect.right);
  }

  private getPositionForEvent_(e: Event) {
    if (e instanceof MouseEvent) {
      return {top: e.clientY, left: e.clientX};
    } else {
      // Events other than a MouseEvent do not have locations specified, so
      // automatically default to the middle of the icon for the context menu to
      // show up.
      const rect =
          this.shadowRoot!.getElementById(
                              'iconContainer')!.getBoundingClientRect();
      if (rect) {
        return {
          top: rect.top + (rect.height / 2),
          left: rect.left + (rect.width / 2),
        };
      } else {
        return {top: 0, left: 0};
      }
    }
  }

  private handleClick_(e: MouseEvent) {
    // We want to capture left-click `0` and aux-click `1` (aka
    // middle-mouse-button click). Other clicks (right-click, etc) should not
    // trigger a launch.
    if (e.button > 1) {
      return;
    }

    const clickEvent: ClickEvent = {
      button: e.button,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    };

    if (this.appInfo.isDeprecatedApp) {
      recordUserAction(AppHomeUserAction.LAUNCH_DEPRECATED_APP);
    } else {
      recordUserAction(AppHomeUserAction.LAUNCH_WEB_APP);
    }
    BrowserProxy.getInstance().handler.launchApp(this.appInfo.id, clickEvent);

    e.preventDefault();
    e.stopPropagation();
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  // The CrActionMenuElement is a modal that does not listen to any other
  // events other than mousedown on right click when it is open. This allows
  // us to listen to changes on the dom even when the menu is showing.
  private onMenuMousedown_(e: MouseEvent) {
    // Actions that are not a right click on a different app are handled
    // correctly by the cr-action-menu. Listening to them here causes
    // errors.
    if (e.button !== 2) {
      return;
    }

    // Do not listen to the mousedown event if not triggered from a
    // CrActionMenuElement, i.e. one without a dialog element covering the dom.
    if ((e.composedPath()[0] as HTMLElement).tagName !== 'DIALOG') {
      return;
    }

    this.closeContextMenu();
  }

  // Methods to hide menu items:
  private isWebStoreLinkHidden_() {
    return !this.appInfo.storePageUrl;
  }
  private isOpenInWindowHidden_() {
    return this.appInfo.isLocallyInstalled || this.appInfo.isDeprecatedApp;
  }
  private isLaunchOnStartupDisabled_() {
    return !this.appInfo.mayToggleRunOnOsLoginMode;
  }
  private isLaunchOnStartupHidden_() {
    return !this.appInfo.mayShowRunOnOsLoginMode ||
        this.appInfo.isDeprecatedApp;
  }
  private isCreateShortcutHidden_() {
    return !this.appInfo.isLocallyInstalled || this.appInfo.isDeprecatedApp;
  }
  private isInstallLocallyHidden_() {
    return this.appInfo.isLocallyInstalled || this.appInfo.isDeprecatedApp;
  }
  private isUninstallHidden_() {
    return !this.appInfo.isLocallyInstalled;
  }
  private isRemoveFromChromeHidden_() {
    return this.appInfo.isLocallyInstalled;
  }
  private isAppSettingsHidden_() {
    return !this.appInfo.isLocallyInstalled;
  }

  private isLocallyInstalled_() {
    return this.appInfo.isLocallyInstalled;
  }

  private isLaunchOnStartUp_() {
    return (this.appInfo.runOnOsLoginMode !== RunOnOsLoginMode.kNotRun);
  }

  private onMenuClick_(event: Event) {
    // The way the menu works, it's inside of a dialog which covers the whole
    // screen. Because our element uses a click listener on the entire element,
    // any clicks anywhere while the context menu is open will then bubble up to
    // our launch listener. So this makes sure that we stop those clicks here.
    event.stopPropagation();
  }

  private openStorePage_() {
    if (!this.appInfo.storePageUrl) {
      return;
    }
    window.open(new URL(this.appInfo.storePageUrl.url), '_blank');
    this.closeContextMenu();
  }

  private onOpenInWindowItemClick_() {
    if (this.appInfo.openInWindow) {
      BrowserProxy.getInstance().handler.setUserDisplayMode(
          this.appInfo.id, UserDisplayMode.kBrowser);
      recordUserAction(AppHomeUserAction.OPEN_IN_WINDOW_UNCHECKED);
    } else {
      BrowserProxy.getInstance().handler.setUserDisplayMode(
          this.appInfo.id, UserDisplayMode.kStandalone);
      recordUserAction(AppHomeUserAction.OPEN_IN_WINDOW_CHECKED);
    }
  }

  // Changing the app's launch mode.
  private onLaunchOnStartupItemClick_() {
    if (this.isLaunchOnStartupDisabled_()) {
      return;
    }

    if (this.isLaunchOnStartUp_()) {
      BrowserProxy.getInstance().handler.setRunOnOsLoginMode(
          this.appInfo.id, RunOnOsLoginMode.kNotRun);
      recordUserAction(AppHomeUserAction.LAUNCH_AT_STARTUP_UNCHECKED);
    } else {
      BrowserProxy.getInstance().handler.setRunOnOsLoginMode(
          this.appInfo.id, RunOnOsLoginMode.kWindowed);
      recordUserAction(AppHomeUserAction.LAUNCH_AT_STARTUP_CHECKED);
    }
  }

  private onCreateShortcutItemClick_() {
    if (this.appInfo.id) {
      BrowserProxy.getInstance().handler.createAppShortcut(this.appInfo.id);
      recordUserAction(AppHomeUserAction.CREATE_SHORTCUT);
    }
    this.closeContextMenu();
  }

  private onInstallLocallyItemClick_() {
    if (this.appInfo.id) {
      BrowserProxy.getInstance().handler.installAppLocally(this.appInfo.id);
      recordUserAction(AppHomeUserAction.INSTALL_APP_LOCALLY);
    }
    this.closeContextMenu();
  }

  private onUninstallItemClick_() {
    if (this.appInfo.id) {
      BrowserProxy.getInstance().handler.uninstallApp(this.appInfo.id);
      recordUserAction(AppHomeUserAction.UNINSTALL);
    }
    this.closeContextMenu();
  }

  private onAppSettingsItemClick_() {
    if (this.appInfo.id) {
      BrowserProxy.getInstance().handler.showAppSettings(this.appInfo.id);
      recordUserAction(AppHomeUserAction.OPEN_APP_SETTINGS);
    }
    this.closeContextMenu();
  }

  private getIconUrl_() {
    const url = new URL(this.appInfo.iconUrl.url);
    // For web app, the backend serves grayscale image when the app is not
    // locally installed automatically and doesn't recognize this query param,
    // but we add a query param here to force browser to refetch the image.
    if (!this.isLocallyInstalled_()) {
      url.searchParams.append('grayscale', 'true');
    }
    return url;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-item': AppItemElement;
  }
}

customElements.define(AppItemElement.is, AppItemElement);