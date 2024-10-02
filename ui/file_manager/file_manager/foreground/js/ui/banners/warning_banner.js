// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {util} from '../../../../common/js/util.js';
import {Banner} from '../../../../externs/banner.js';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * WarningBanner is a type of banner that is highest priority and is used to
 * showcase potential underlying issues for the filesystem (e.g. low disk space)
 * or that are contextually relevant (e.g. Google Drive is offline).
 *
 * To implement a WarningBanner, extend from this banner and override the
 * allowedVolumes method where you want the warning message shown. The
 * connectedCallback method can be used to set the warning text and an optional
 * link to provide more information. All other configuration elements are
 * optional and can be found documented on the Banner extern.
 *
 * For example the following banner will show when a user navigates to the
 * Downloads volume type:
 *
 *    class ConcreteWarningBanner extends WarningBanner {
 *      allowedVolumes() {
 *        return [{type: VolumeManagerCommon.VolumeType.DOWNLOADS}];
 *      }
 *    }
 *
 * Create a HTML template with the same file name as the banner and override
 * the text using slots with the content that you want:
 *
 *    <warning-banner>
 *      <span slot="text">Warning Banner text</span>
 *      <cr-button slot="extra-button" href="{{url_to_navigate}}">
 *        Extra button text
 *      </cr-button>
 *    </warning-banner>
 */
export class WarningBanner extends Banner {
  constructor() {
    super();

    const fragment = this.getTemplate();
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Returns the HTML template for the Warning Banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * Called when the web component is connected to the DOM. This will be called
   * for both the inner warning-banner component and the concrete
   * implementations that extend from it.
   */
  connectedCallback() {
    // If a WarningBanner subclass overrides the default dismiss button, the
    // button will not exist in the shadowRoot. Add the event listener to the
    // overridden dismiss button first and fall back to the default button if
    // no overridden button.
    const overridenDismissButton =
        this.querySelector('[slot="dismiss-button"]');
    const defaultDismissButton =
        this.shadowRoot.querySelector('#dismiss-button');
    if (overridenDismissButton) {
      overridenDismissButton.addEventListener(
          'click', this.onDismissClickHandler_.bind(this));
    } else if (defaultDismissButton) {
      defaultDismissButton.addEventListener(
          'click', this.onDismissClickHandler_.bind(this));
    }

    // Attach an onclick handler to the extra-button slot. This enables a new
    // element to leverage the href tag on the element to have a URL opened.
    // TODO(crbug.com/1228128): Add UMA trigger to capture number of extra
    // button clicks.
    const extraButton = this.querySelector('[slot="extra-button"]');
    if (extraButton) {
      extraButton.addEventListener('click', (e) => {
        util.visitURL(extraButton.getAttribute('href'));
        e.preventDefault();
      });
    }
  }

  /**
   * When a WarningBanner is dismissed, do not show it again for another 36
   * hours.
   * @returns {number}
   */
  hideAfterDismissedDurationSeconds() {
    return 36 * 60 * 60;  // 36 hours, 129,600 seconds.
  }

  /**
   * All banners that inherit this class should override with their own
   * volume types to allow. Setting this explicitly as an empty array ensures
   * banners that don't override this are not shown by default.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    return [];
  }

  /**
   * Handler for the dismiss button on click, switches to the custom banner
   * dismissal event to ensure the controller can catch the event.
   * @param {!Event} event The click event.
   * @private
   */
  onDismissClickHandler_(event) {
    const parent = this.getRootNode() && this.getRootNode().host;
    let bannerInstance = this;
    // In the case the warning-banner web component is not the root node (e.g.
    // it is contained within another web component) prefer the outer component.
    if (parent && parent instanceof WarningBanner) {
      bannerInstance = parent;
    }
    this.dispatchEvent(new CustomEvent(
        Banner.Event.BANNER_DISMISSED,
        {bubbles: true, composed: true, detail: {banner: bannerInstance}}));
  }
}

customElements.define('warning-banner', WarningBanner);
