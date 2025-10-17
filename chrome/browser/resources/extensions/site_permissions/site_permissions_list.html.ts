// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsSitePermissionsListElement} from './site_permissions_list.js';

export function getHtml(this: ExtensionsSitePermissionsListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="site-list-header-container">
  <span>${this.header}</span>
  <cr-button id="addSite" @click="${this.onAddSiteClick_}">
    $i18n{add}
  </cr-button>
</div>
<div id="no-sites" ?hidden="${this.hasSites_()}">$i18n{noSitesAdded}</div>
<div id="sites-list" ?hidden="${!this.hasSites_()}">
  ${this.sites.map(item => html`
    <div class="site-row">
      <div class="site-favicon"
          .style="background-image:${this.getFaviconUrl_(item)}">
      </div>
      <span class="site">${item}</span>
      <cr-icon-button class="icon-more-vert no-overlap" data-site="${item}"
          @click="${this.onDotsClick_}">
      </cr-icon-button>
    </div>`)}
</div>

<cr-action-menu id="siteActionMenu">
  <button class="dropdown-item" id="edit-site-url"
      @click="${this.onEditSiteUrlClick_}">
    $i18n{sitePermissionsEditUrl}
  </button>
  <button class="dropdown-item" id="edit-site-permissions"
      @click="${this.onEditSitePermissionsClick_}">
    $i18n{sitePermissionsEditPermissions}
  </button>
  <button class="dropdown-item" id="remove-site"
      @click="${this.onRemoveSiteClick_}">
    $i18n{remove}
  </button>
</cr-action-menu>

${this.showEditSiteUrlDialog_ ? html`
  <site-permissions-edit-url-dialog .delegate="${this.delegate}"
      .siteToEdit="${this.siteToEdit_}" .siteSet="${this.siteSet}"
      @close="${this.onEditSiteUrlDialogClose_}">
  </site-permissions-edit-url-dialog>` : ''}

${this.showEditSitePermissionsDialog_ ? html`
  <site-permissions-edit-permissions-dialog .delegate="${this.delegate}"
      .extensions="${this.extensions}" .site="${this.siteToEdit_}"
      .originalSiteSet="${this.siteSet}"
      @close="${this.onEditSitePermissionsDialogClose_}">
  </site-permissions-edit-permissions-dialog>` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
