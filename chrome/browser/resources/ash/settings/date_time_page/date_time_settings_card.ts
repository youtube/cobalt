// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'date-time-settings-card' is the card element containing date and time
 * settings.
 */

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import './timezone_selector.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isChild, isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './date_time_settings_card.html.js';
import {TimeZoneBrowserProxy, TimeZoneBrowserProxyImpl} from './timezone_browser_proxy.js';

const DateTimeSettingsCardElementBase = DeepLinkingMixin(RouteOriginMixin(
    PrefsMixin(I18nMixin(WebUiListenerMixin(PolymerElement)))));

export class DateTimeSettingsCardElement extends
    DateTimeSettingsCardElementBase {
  static get is() {
    return 'date-time-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activeTimeZoneDisplayName: {
        type: String,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.k24HourClock,
          Setting.kChangeTimeZone,
        ]),
      },

      /**
       * Whether date and time are settable. Normally the date and time are
       * forced by network time, so default to false to initially hide the
       * button.
       */
      canSetDateTime_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the icon informing that this action is managed by a parent is
       * displayed.
       */
      shouldShowManagedByParentIcon_: {
        type: Boolean,
        value: () => {
          return isChild();
        },
      },

      timeZoneSettingSublabel_: {
        type: String,
        computed: `computeTimeZoneSettingSublabel_(
            activeTimeZoneDisplayName,
            prefs.generated.resolve_timezone_by_geolocation_on_off.value,
            prefs.generated.resolve_timezone_by_geolocation_method_short.value)`,
      },
    };
  }

  activeTimeZoneDisplayName: string;
  private browserProxy_: TimeZoneBrowserProxy;
  private canSetDateTime_: boolean;
  private shouldShowManagedByParentIcon_: boolean;
  private timeZoneSettingSublabel_: string;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = isRevampWayfindingEnabled() ? routes.SYSTEM_PREFERENCES :
                                               routes.DATETIME;

    this.browserProxy_ = TimeZoneBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'can-set-date-time-changed', this.onCanSetDateTimeChanged_.bind(this));
    this.browserProxy_.dateTimePageReady();
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(
        routes.DATETIME_TIMEZONE_SUBPAGE, '#timeZoneSettingsTrigger');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  private onCanSetDateTimeChanged_(canSetDateTime: boolean): void {
    this.canSetDateTime_ = canSetDateTime;
  }

  private onSetDateTimeClick_(): void {
    this.browserProxy_.showSetDateTimeUi();
  }

  private openTimeZoneSubpage_(): void {
    Router.getInstance().navigateTo(routes.DATETIME_TIMEZONE_SUBPAGE);
  }

  private computeTimeZoneSettingSublabel_(): string {
    // Note: `this.getPref()` will assert the queried pref exists, but the prefs
    // property may not be initialized yet when this element runs the first
    // computation of this method. Ensure prefs is initialized first.
    if (!this.prefs) {
      return '';
    }

    if (!this.getPref('generated.resolve_timezone_by_geolocation_on_off')
             .value) {
      return this.activeTimeZoneDisplayName;
    }
    const method =
        this.getPref<number>(
                'generated.resolve_timezone_by_geolocation_method_short')
            .value;
    const id = [
      'setTimeZoneAutomaticallyDisabled',
      'setTimeZoneAutomaticallyIpOnlyDefault',
      'setTimeZoneAutomaticallyWithWiFiAccessPointsData',
      'setTimeZoneAutomaticallyWithAllLocationInfo',
    ][method];
    return id ? this.i18n(id) : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DateTimeSettingsCardElement.is]: DateTimeSettingsCardElement;
  }
}

customElements.define(
    DateTimeSettingsCardElement.is, DateTimeSettingsCardElement);
