// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {isGlicVersion} from './profile_picker_flags.js';
import type {ProfilePickerMainViewElement} from './profile_picker_main_view.js';

export function getHtml(this: ProfilePickerMainViewElement) {
  return html`<!--_html_template_start_-->
<div class="flex-container">
  <div class="title-container">
    <div id="images-container">
      <img id="product-logo" @click="${this.onProductLogoClick_}"
          src="product_logo.svg" role="presentation">
      <img id="glic-logo" ?hidden="${!isGlicVersion()}" role="presentation">
    </div>
    <h1 class="title" .innerHTML="${this.getTitle_()}"></h1>
    <div class="subtitle" .innerHTML="${this.getSubtitle_()}"></div>
  </div>
  <div id="profilesWrapper" ?hidden="${(this.shouldHideProfilesWrapper_())}">
    <div id="profilesContainer" class="custom-scrollbar">
      ${this.profilesList_.map((item, index) => html`
        <profile-card class="profile-item" .profileState="${item}"
            data-index="${index}">
        </profile-card>
      `)}
      <cr-button id="addProfile" class="profile-item"
          @click="${this.onAddProfileClick_}"
          ?hidden="${!this.profileCreationAllowed_}"
          aria-labelledby="addProfileButtonLabel">
        <div id="addProfileButtonLabel"
            class="profile-card-info prominent-text">
          $i18n{addSpaceButton}
        </div>
        <cr-icon icon="profiles:add"></cr-icon>
      </cr-button>
    </div>
  </div>
  <div id="footer-text" class="subtitle"
      ?hidden="${this.shouldHideFooterText_()}">
    $i18nRaw{glicAddProfileHelper}
  </div>
</div>
<div class="footer">
  <cr-button id="browseAsGuestButton"
      @click="${this.onLaunchGuestProfileClick_}"
      ?hidden="${!this.guestModeEnabled_}">
    <cr-icon icon="profiles:account-circle" slot="prefix-icon"></cr-icon>
    $i18n{browseAsGuestButton}
  </cr-button>
  <cr-checkbox id="askOnStartup" ?checked="${this.askOnStartup_}"
      @checked-changed="${this.onAskOnStartupChangedByUser_}"
      ?hidden="${this.hideAskOnStartup_}">
    $i18n{askOnStartupCheckboxText}
  </cr-checkbox>
</div>

<cr-dialog id="forceSigninErrorDialog">
  <div slot="title" id="dialog-title" class="key-text">
    ${this.forceSigninErrorDialogTitle_}</div>
  <div slot="body" id="dialog-body" class="warning-message">
    ${this.forceSigninErrorDialogBody_}
  </div>
  <div slot="button-container" class="button-container">
    <cr-button id="cancel-button"
        @click="${this.onForceSigninErrorDialogOkButtonClicked_}">
      $i18n{ok}
    </cr-button>
    <cr-button id="button-sign-in" class="action-button"
        @click="${this.onReauthClicked_}"
        ?hidden="${!this.shouldShownSigninButton_}">
      $i18n{needsSigninPrompt}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
