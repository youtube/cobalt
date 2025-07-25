// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-live-caption' is a component for showing Live Caption
 * settings. It appears on the accessibility subpage
 * (chrome://settings/accessibility) on Mac and some versions of Windows and on
 * the captions subpage (chrome://settings/captions) on Linux and other versions
 * of Windows.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import '../strings.m.js';

import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CaptionsBrowserProxy, CaptionsBrowserProxyImpl, LiveCaptionLanguage, LiveCaptionLanguageList} from '/shared/settings/a11y_page/captions_browser_proxy.js';
import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';

import {getTemplate} from './live_caption_section.html.js';

// clang-format off
// <if expr="not is_chromeos">
import '/shared/settings/controls/settings_dropdown_menu.js';
import './live_translate_section.js';
import '../languages_page/add_languages_dialog.js';

import {LanguageHelper, LanguagesModel} from '../languages_page/languages_types.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {DropdownMenuOptionList} from '/shared/settings/controls/settings_dropdown_menu.js';
import {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// </if>
// clang-format on


// <if expr="is_chromeos">
const SettingsLiveCaptionElementBase =
    WebUiListenerMixin(PrefsMixin(PolymerElement));
// </if>
// <if expr="not is_chromeos">
const SettingsLiveCaptionElementBase = WebUiListenerMixin(
    ListPropertyUpdateMixin(PrefsMixin(I18nMixin(PolymerElement))));
// </if>

export class SettingsLiveCaptionElement extends SettingsLiveCaptionElementBase {
  static get is() {
    return 'settings-live-caption';
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

      /**
       * The subtitle to display under the Live Caption heading. Generally, this
       * is a generic subtitle describing the feature. While the SODA model is
       * being downloading, this displays the download progress.
       */
      enableLiveCaptionSubtitle_: {
        type: String,
        value: loadTimeData.getString('captionsEnableLiveCaptionSubtitle'),
      },

      enableLiveCaptionMultiLanguage_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableLiveCaptionMultiLanguage');
        },
      },

      // <if expr="not is_chromeos">
      /**
       * Read-only reference to the languages model provided by the
       * 'settings-languages' instance.
       */
      languages: {
        type: Object,
        notify: true,
      },

      languageHelper: Object,

      enableLiveTranslate_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableLiveTranslate');
        },
      },

      installedLanguagePacks_: {
        type: Array,
        value: () => [],
        observer: 'updateLanguageOptions_',
      },

      availableLanguagePacks_: {
        type: Array,
        value: () => [],
      },

      languageOptions_: {
        type: Array,
        value: () => [],
      },

      showAddLanguagesDialog_: Boolean,
      // </if>
    };
  }

  // <if expr="not is_chromeos">
  languages: LanguagesModel;
  languageHelper: LanguageHelper;
  private enableLiveTranslate_: boolean;
  private installedLanguagePacks_: LiveCaptionLanguageList;
  private availableLanguagePacks_: LiveCaptionLanguageList;
  private languageOptions_: DropdownMenuOptionList;
  private showAddLanguagesDialog_: boolean;
  // </if>
  private browserProxy_: CaptionsBrowserProxy =
      CaptionsBrowserProxyImpl.getInstance();
  private enableLiveCaptionSubtitle_: string;
  private enableLiveCaptionMultiLanguage_: boolean;

  override ready() {
    super.ready();
    // <if expr="not is_chromeos">
    this.browserProxy_.getInstalledLanguagePacks().then(
        (installedLanguagePacks: LiveCaptionLanguageList) => {
          this.installedLanguagePacks_ = installedLanguagePacks;
        });

    this.browserProxy_.getAvailableLanguagePacks().then(
        (availableLanguagePacks: LiveCaptionLanguageList) => {
          this.availableLanguagePacks_ = availableLanguagePacks;
        });
    // </if>

    // <if expr="is_chromeos">
    this.addWebUiListener(
        'soda-download-progress-changed',
        (sodaDownloadProgress: string) =>
            this.onSodaDownloadProgressChanged_(sodaDownloadProgress));
    // </if>
    // <if expr="not is_chromeos">
    this.addWebUiListener(
        'soda-download-progress-changed',
        (sodaDownloadProgress: string, languageCode: string) =>
            this.onSodaDownloadProgressChangedForLanguage_(
                sodaDownloadProgress, languageCode));
    // </if>

    this.browserProxy_.liveCaptionSectionReady();
  }

  /**
   * @return the Live Caption toggle element.
   */
  getLiveCaptionToggle(): SettingsToggleButtonElement {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#liveCaptionToggleButton')!;
  }

  private onLiveCaptionEnabledChanged_(event: Event) {
    const liveCaptionEnabled =
        (event.target as SettingsToggleButtonElement).checked;
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveCaption.EnableFromSettings', liveCaptionEnabled);

    // <if expr="not is_chromeos">
    if (this.installedLanguagePacks_.length === 0) {
      this.installLanguagePacks_(
          [this.getPref('accessibility.captions.live_caption_language').value]);
    }
    // </if>
  }

  private onLiveCaptionMaskOffensiveWordsChanged_(event: Event) {
    const liveCaptionMaskOffensiveWords =
        (event.target as SettingsToggleButtonElement).checked;
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveCaption.MaskOffensiveWords',
        liveCaptionMaskOffensiveWords);
  }

  // <if expr="not is_chromeos">
  private onAddLanguagesClick_(e: Event) {
    e.preventDefault();
    this.showAddLanguagesDialog_ = true;
  }

  private onAddLanguagesDialogClose_() {
    this.showAddLanguagesDialog_ = false;
    const toFocus = this.shadowRoot!.querySelector<HTMLElement>('#addLanguage');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  private onRemoveLanguageClick_(e: DomRepeatEvent<LiveCaptionLanguage>) {
    this.installedLanguagePacks_ = this.installedLanguagePacks_.filter(
        languagePack => languagePack.code !== e.model.item.code);
    this.browserProxy_.removeLanguagePack(e.model.item.code);

    if (this.installedLanguagePacks_.length === 0) {
      this.setPrefValue('accessibility.captions.live_caption_enabled', false);
      return;
    }

    if (!this.installedLanguagePacks_.some(
            languagePack => languagePack.code ===
                this.getPref('accessibility.captions.live_caption_language')
                    .value)) {
      this.setPrefValue(
          'accessibility.captions.live_caption_language',
          this.installedLanguagePacks_[0].code);
    }
  }

  private onLanguagesAdded_(e: CustomEvent<string[]>) {
    this.installLanguagePacks_(e.detail);
  }

  private installLanguagePacks_(languageCodes: string[]) {
    const newLanguagePacks: LiveCaptionLanguageList = [];
    languageCodes.forEach(languageCode => {
      const languagePackToAdd = this.availableLanguagePacks_.find(
          languagePack => languagePack.code === languageCode);
      if (languagePackToAdd) {
        newLanguagePacks.push(languagePackToAdd);
      }
    });

    this.updateList(
        `installedLanguagePacks_`, item => item.code,
        this.installedLanguagePacks_.concat(newLanguagePacks));
    this.browserProxy_.installLanguagePacks(languageCodes);

    // Explicitly call the function to update the language options because
    // updating the list does not trigger the observer function.
    this.updateLanguageOptions_();
  }

  private updateLanguageOptions_() {
    this.languageOptions_ = this.installedLanguagePacks_.map(languagePack => {
      return {value: languagePack.code, name: languagePack.displayName};
    });
  }

  private filterAvailableLanguagePacks_(
      availableLanguagePacks: LiveCaptionLanguageList,
      installedLanguagePacks: LiveCaptionLanguageList):
      chrome.languageSettingsPrivate.Language[] {
    const filteredLanguagePacks =
        availableLanguagePacks.filter(availableLanguagePack => {
          return !installedLanguagePacks.some(
              installedLanguagePack =>
                  installedLanguagePack.code === availableLanguagePack.code);
        });

    return filteredLanguagePacks.map(
        languagePack => ({
          code: languagePack.code,
          displayName: languagePack.displayName,

          // The native display name for language packs is not shown.
          nativeDisplayName: languagePack.nativeDisplayName,
        }));
  }
  // </if>

  // <if expr="is_chromeos">
  /**
   * Displays SODA download progress in the UI.
   * @param sodaDownloadProgress The message sent from the webui to be displayed
   *     as download progress for Live Caption.
   */
  private onSodaDownloadProgressChanged_(sodaDownloadProgress: string) {
    this.enableLiveCaptionSubtitle_ = sodaDownloadProgress;
  }
  // </if>

  // <if expr="not is_chromeos">
  /**
   * Displays SODA download progress in the UI. When the language UI is visible,
   * which occurs when the kLiveCaptionMultiLanguage feature is enabled and when
   * the kLiveCaptionEnabled pref is true, download progress should appear next
   * to the selected language. Otherwise, the download progress appears as a
   * subtitle below the Live Caption toggle.
   * @param sodaDownloadProgress The message sent from the webui to be displayed
   *     as download progress for Live Caption.
   * @param languageCode The language code indicating which language pack the
   *     message applies to.
   */
  private onSodaDownloadProgressChangedForLanguage_(
      sodaDownloadProgress: string, languageCode: string) {
    if (!this.enableLiveCaptionMultiLanguage_) {
      this.enableLiveCaptionSubtitle_ = sodaDownloadProgress;
      return;
    }

    for (let i = 0; i < this.installedLanguagePacks_.length; i++) {
      const language = this.installedLanguagePacks_[i];
      if (language.code === languageCode) {
        language.downloadProgress = sodaDownloadProgress;
        this.notifyPath('installedLanguagePacks_.' + i + '.downloadProgress');
        break;
      }
    }
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-live-caption': SettingsLiveCaptionElement;
  }
}

customElements.define(
    SettingsLiveCaptionElement.is, SettingsLiveCaptionElement);
