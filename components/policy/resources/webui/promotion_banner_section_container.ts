// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './promotion_banner_section_container.css.js';
import {getHtml} from './promotion_banner_section_container.html.js';

export class PromotionBannerSectionContainerElement extends CrLitElement {
  static get is() {
    return 'promotion-banner-section-container';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onRedirectClick() {
    this.fire('redirect');
  }

  protected onDismissClick() {
    this.fire('dismiss');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'promotion-banner-section-container':
        PromotionBannerSectionContainerElement;
  }
}
customElements.define(
    PromotionBannerSectionContainerElement.is,
    PromotionBannerSectionContainerElement);
