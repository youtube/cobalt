// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-users-subpage' is the settings page for managing user
 * accounts on the device.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/js/action_link.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import '../os_people_page/user_list.js';
import '../os_people_page/add_user_dialog.js';

import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {SettingsUsersAddUserDialogElement} from '../os_people_page/add_user_dialog.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {getTemplate} from './manage_users_subpage.html.js';

const SettingsManageUsersSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(PolymerElement));

interface SettingsManageUsersSubpageElement {
  $: {
    addUserDialog: SettingsUsersAddUserDialogElement,
  };
}

class SettingsManageUsersSubpageElement extends
    SettingsManageUsersSubpageElementBase {
  static get is() {
    return 'settings-manage-users-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      isOwner_: {
        type: Boolean,
        value: true,
      },

      isUserListManaged_: {
        type: Boolean,
        value: false,
      },

      isChild_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isChildAccount');
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kGuestBrowsingV2,
          Setting.kShowUsernamesAndPhotosAtSignInV2,
          Setting.kRestrictSignInV2,
          Setting.kAddToUserAllowlistV2,
          Setting.kRemoveFromUserAllowlistV2,
        ]),
      },
    };
  }

  private isOwner_: boolean;
  private isUserListManaged_: boolean;
  private isChild_: boolean;

  constructor() {
    super();

    chrome.usersPrivate.getCurrentUser().then(
        (user: chrome.usersPrivate.User) => {
          this.isOwner_ = user.isOwner;
        });

    chrome.usersPrivate.isUserListManaged().then(
        (isUserListManaged: boolean) => {
          this.isUserListManaged_ = isUserListManaged;
        });
  }

  override ready() {
    super.ready();

    this.addEventListener(
        'all-managed-users-removed', this.focusAddUserButton_);
  }

  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    if (settingId !== Setting.kRemoveFromUserAllowlistV2) {
      // Continue with deep linking attempt.
      return true;
    }

    // Wait for element to load.
    afterNextRender(this, () => {
      const userList = this.shadowRoot!.querySelector('settings-user-list');
      const removeButton =
          userList!.shadowRoot!.querySelector('cr-icon-button');
      if (removeButton) {
        this.showDeepLinkElement(removeButton);
        return;
      }
      console.warn(`Element with deep link id ${settingId} not focusable.`);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  }

  override currentRouteChanged(route: Route, _oldRoute: Route): void {
    // Does not apply to this page.
    if (route !== routes.ACCOUNTS) {
      return;
    }

    this.attemptDeepLink();
  }

  private openAddUserDialog_(e: Event): void {
    e.preventDefault();
    this.$.addUserDialog.open();
  }

  private onAddUserDialogClose_(): void {
    this.focusAddUserButton_();
  }

  private isEditingDisabled_(isOwner: boolean, isUserListManaged: boolean):
      boolean {
    return !isOwner || isUserListManaged;
  }

  private isEditingUsersEnabled_(
      isOwner: boolean, isUserListManaged: boolean, allowGuest: boolean,
      isChild: boolean): boolean {
    return isOwner && !isUserListManaged && !allowGuest && !isChild;
  }

  private shouldHideModifiedByOwnerLabel_(): boolean {
    return this.isUserListManaged_ || this.isOwner_;
  }

  private focusAddUserButton_(): void {
    focusWithoutInk(
        castExists(this.shadowRoot!.querySelector('#add-user-button a')));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsManageUsersSubpageElement.is]: SettingsManageUsersSubpageElement;
  }
}

customElements.define(
    SettingsManageUsersSubpageElement.is, SettingsManageUsersSubpageElement);
