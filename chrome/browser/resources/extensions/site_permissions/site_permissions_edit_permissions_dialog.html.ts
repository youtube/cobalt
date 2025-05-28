// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SitePermissionsEditPermissionsDialogElement} from './site_permissions_edit_permissions_dialog.js';

export function getHtml(this: SitePermissionsEditPermissionsDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach>
  <div slot="title" id="dialog-title">
    <div>$i18n{sitePermissionsEditPermissionsDialogTitle}</div>
    <div id="title-subtext">
      <span id="site">${this.getSiteWithoutSubdomainSpecifier_()}</span>
      <span id="includesSubdomains" ?hidden="${!this.matchesSubdomains_()}">
        $i18n{sitePermissionsIncludesSubdomains}
      </span>
    </div>
  </div>
  <div slot="header">
    <!-- The cr-radio-group is in the header instead of the body slot so it is
     fixed in place while the list of extensions in the body slot can scroll
     if the dialog's contents exceed the max height. -->
    ${!this.matchesSubdomains_() ? html`
      <cr-radio-group .selected="${this.siteSet_}"
          @selected-changed="${this.onSiteSetChanged_}">
        <cr-radio-button ?hidden="${!this.showPermittedOption_}"
            name="${chrome.developerPrivate.SiteSet.USER_PERMITTED}"
            label="${this.getPermittedSiteLabel_()}">
        </cr-radio-button>
        <cr-radio-button
            name="${chrome.developerPrivate.SiteSet.USER_RESTRICTED}"
            label="${this.getRestrictedSiteLabel_()}">
        </cr-radio-button>
        <cr-radio-button
            name="${chrome.developerPrivate.SiteSet.EXTENSION_SPECIFIED}"
            label="$i18n{editSitePermissionsCustomizePerExtension}">
        </cr-radio-button>
      </cr-radio-group>` : ''}
  </div>
  <div slot="body">
    ${this.showExtensionSiteAccessData_() ? html`
      <div class="${this.getDialogBodyContainerClass_()}">
        ${this.extensionSiteAccessData_.map((item, index) => html`
          <div class="extension-row">
            <img class="extension-icon" src="${item.iconUrl}" alt="">
            <span class="extension-name">${item.name}</span>
            <select class="extension-host-access md-select"
                ?disabled="${item.addedByPolicy}"
                @change="${this.onHostAccessChange_}" data-index="${index}">
              <option value="${chrome.developerPrivate.HostAccess.ON_CLICK}"
                  .selected="${this.isSelected_(item.id, item.siteAccess,
                      chrome.developerPrivate.HostAccess.ON_CLICK)}">
                $i18n{sitePermissionsAskOnEveryVisit}
              </option>
              <option
                  value="${chrome.developerPrivate.HostAccess
                      .ON_SPECIFIC_SITES}"
                  .selected="${this.isSelected_(item.id, item.siteAccess,
                      chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES)}">
                $i18n{sitePermissionsAlwaysOnThisSite}
              </option>
              <option value="${chrome.developerPrivate.HostAccess.ON_ALL_SITES}"
                  .selected="${this.isSelected_(item.id, item.siteAccess,
                      chrome.developerPrivate.HostAccess.ON_ALL_SITES)}"
                  ?disabled="${!item.canRequestAllSites}">
                $i18n{sitePermissionsAlwaysOnAllSites}
              </option>
            </select>
          </div>`)}
      </div>` : ''}
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" id="submit"
        @click="${this.onSubmitClick_}">
      $i18n{save}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
