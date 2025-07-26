// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
// <if expr="is_ios">
// TODO(crbug.com/41173939): Remove this once injected by web. -->
import 'chrome://resources/js/ios/web_ui.js';

// </if>

import {assert} from 'chrome://resources/js/assert.js';
import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {WebuiUrlInfo} from './chrome_urls.mojom-webui.js';

export const INTERNAL_DEBUG_PAGES_HASH: string = 'internal-debug-pages';

export class ChromeUrlsAppElement extends CrLitElement {
  static get is() {
    return 'chrome-urls-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      debugPagesButtonDisabled_: {type: Boolean},
      internalUrlInfos_: {type: Array},
      webuiUrlInfos_: {type: Array},
      commandUrls_: {type: Array},
      internalUisEnabled_: {type: Boolean},
    };
  }

  protected debugPagesButtonDisabled_: boolean = false;
  protected webuiUrlInfos_: WebuiUrlInfo[] = [];
  protected internalUrlInfos_: WebuiUrlInfo[] = [];
  protected commandUrls_: Url[] = [];
  protected internalUisEnabled_: boolean = false;
  protected tracker_: EventTracker = new EventTracker();


  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('internalUrlInfos_') &&
        this.internalUrlInfos_.length > 0) {
      this.onHashChanged_(CrRouter.getInstance().getHash());
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    this.onHashChanged_(CrRouter.getInstance().getHash());
    this.tracker_.add(
        CrRouter.getInstance(), 'cr-router-hash-changed',
        (e: Event) => this.onHashChanged_((e as CustomEvent<string>).detail));

    BrowserProxyImpl.getInstance().handler.getUrls().then(({urlsData}) => {
      // Since we use GURL on the C++ side, we need to remove the trailing
      // '/' here for nicer display.
      function getPrettyUrl(url: Url): Url {
        return {url: url.url.replace(/\/$/, '')};
      }
      urlsData.webuiUrls.forEach(info => {
        info.url = getPrettyUrl(info.url);
      });
      this.webuiUrlInfos_ = urlsData.webuiUrls.filter(info => !info.internal);
      this.internalUrlInfos_ = urlsData.webuiUrls.filter(info => info.internal);
      this.commandUrls_ = urlsData.commandUrls.map(url => getPrettyUrl(url));
      this.internalUisEnabled_ = urlsData.internalDebuggingUisEnabled;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.tracker_.removeAll();
  }

  private onHashChanged_(hash: string) {
    if (hash !== INTERNAL_DEBUG_PAGES_HASH ||
        this.internalUrlInfos_.length === 0) {
      return;
    }

    const header =
        this.shadowRoot.querySelector<HTMLElement>('#internal-debugging-pages');
    assert(header);
    header.scrollIntoView(true);
  }

  protected getDebugPagesEnabledText_(): string {
    return this.internalUisEnabled_ ? 'enabled' : 'disabled';
  }

  protected getDebugPagesToggleButtonLabel_(): string {
    return this.internalUisEnabled_ ? 'Disable internal debugging pages' :
                                      'Enable internal debugging pages';
  }

  protected async onToggleDebugPagesClick_() {
    this.debugPagesButtonDisabled_ = true;
    const enabled = !this.internalUisEnabled_;
    await BrowserProxyImpl.getInstance().handler.setDebugPagesEnabled(enabled);
    this.internalUisEnabled_ = enabled;
    this.debugPagesButtonDisabled_ = false;
  }

  protected isInternalUiEnabled_(info: WebuiUrlInfo): boolean {
    return info.enabled && this.internalUisEnabled_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chrome-urls-app': ChromeUrlsAppElement;
  }
}

customElements.define(ChromeUrlsAppElement.is, ChromeUrlsAppElement);
