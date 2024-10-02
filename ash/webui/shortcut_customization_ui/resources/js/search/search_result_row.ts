// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../text_accelerator.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {FocusRowMixin} from 'chrome://resources/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {keyToIconNameMap} from '../input_key.js';
import {mojoString16ToString} from '../mojo_utils.js';
import {Router} from '../router.js';
import {LayoutStyle, MojoAcceleratorInfo, MojoSearchResult, StandardAcceleratorInfo, TextAcceleratorInfo, TextAcceleratorPart} from '../shortcut_types.js';
import {getModifiersForAcceleratorInfo, getURLForSearchResult, isStandardAcceleratorInfo, isTextAcceleratorInfo} from '../shortcut_utils.js';
import {TextAcceleratorElement} from '../text_accelerator.js';

import {getBoldedDescription} from './search_result_bolding.js';
import {getTemplate} from './search_result_row.html.js';

/**
 * @fileoverview
 * 'search-result-row' is the container for one search result.
 */

const SearchResultRowElementBase = FocusRowMixin(I18nMixin(PolymerElement));

export class SearchResultRowElement extends SearchResultRowElementBase {
  static get is(): string {
    return 'search-result-row';
  }

  static get properties(): PolymerElementProperties {
    return {
      searchResult: {
        type: Object,
      },

      /** The query used to fetch this result. */
      searchQuery: {
        type: String,
      },

      /** Whether the search result row is selected. */
      selected: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'makeA11yAnnouncementIfSelectedAndUnfocused',
      },

      /** Aria label for the row. */
      ariaLabel: {
        type: String,
        computed: 'computeAriaLabel(searchResult)',
        reflectToAttribute: true,
      },

      /** Number of rows in the list this row is part of. */
      listLength: Number,
    };
  }

  override ariaLabel: string;
  listLength: number;
  searchResult: MojoSearchResult;
  searchQuery: string;
  selected: boolean;

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  private isStandardLayout(): boolean {
    return this.searchResult.acceleratorLayoutInfo.style ===
        LayoutStyle.kDefault;
  }

  private isTextLayout(): boolean {
    return !this.isStandardLayout();
  }

  private getTextAcceleratorParts(): TextAcceleratorPart[] {
    assert(isTextAcceleratorInfo(this.searchResult.acceleratorInfos[0]));
    return TextAcceleratorElement.getTextAcceleratorParts(
        this.searchResult.acceleratorInfos as TextAcceleratorInfo[]);
  }

  private getStandardAcceleratorInfos(): StandardAcceleratorInfo[] {
    assert(this.isStandardLayout());
    // Convert MojoAcceleratorInfo from the search result into the converted
    // AcceleratorInfo type used throughout the app.
    // In practice, this means that we need to convert the String16 keyDisplay
    // property on each accelerator into strings.
    return this.searchResult.acceleratorInfos.map(
        (acceleratorInfo: MojoAcceleratorInfo) => {
          assert(isStandardAcceleratorInfo(acceleratorInfo));
          return {
            ...acceleratorInfo,
            layoutProperties: {
              ...acceleratorInfo.layoutProperties,
              standardAccelerator: {
                ...acceleratorInfo.layoutProperties.standardAccelerator,
                keyDisplay:
                    mojoString16ToString(acceleratorInfo.layoutProperties
                                             .standardAccelerator.keyDisplay),
              },
            },
            // Cast to StandardAcceleratorInfo here since that type uses strings
            // instead of String16s, and we couldn't perform the assignment
            // above if it were still a MojoAcceleratorInfo.
          } as StandardAcceleratorInfo;
        });
  }

  private getStandardAcceleratorModifiers(
      acceleratorInfo: StandardAcceleratorInfo): string[] {
    return getModifiersForAcceleratorInfo(acceleratorInfo);
  }

  private getStandardAcceleratorKey(acceleratorInfo: StandardAcceleratorInfo):
      string {
    return acceleratorInfo.layoutProperties.standardAccelerator.keyDisplay;
  }

  // If true, show "or" after the AcceleratorInfo at the given index.
  private shouldShowTextDivider(indexOfAcceleratorInfo: number): boolean {
    return indexOfAcceleratorInfo !==
        this.searchResult.acceleratorInfos.length - 1;
  }

  /**
   * Only relevant when the focus-row-control is focus()ed. This keypress
   * handler specifies that pressing 'Enter' should cause a route change.
   */
  private onKeyPress(e: KeyboardEvent): void {
    if (e.key === 'Enter') {
      e.stopPropagation();
      this.onSearchResultSelected();
    }
  }

  /**
   * Navigate to a search result route based on the search result.
   */
  onSearchResultSelected(): void {
    Router.getInstance().navigateTo(getURLForSearchResult(this.searchResult));
    this.dispatchEvent(new CustomEvent(
        'navigated-to-result-route', {bubbles: true, composed: true}));
  }

  private getSearchResultDescriptionInnerHtml(): string {
    return getBoldedDescription(
        mojoString16ToString(
            this.searchResult.acceleratorLayoutInfo.description),
        this.searchQuery);
  }

  /**
   * @return Aria label string for ChromeVox to verbalize.
   */
  private computeAriaLabel(): string {
    const description = mojoString16ToString(
        this.searchResult.acceleratorLayoutInfo.description);
    let searchResultText;
    if (this.isStandardLayout()) {
      searchResultText =
          `${description}, ${this.getAriaLabelForStandardLayoutSearchResult()}`;
    } else {
      searchResultText =
          `${description}, ${this.getAriaLabelForTextLayoutSearchResult()}`;
    }

    return this.i18n(
        'searchResultSelectedAriaLabel', this.focusRowIndex + 1,
        this.listLength, searchResultText);
  }

  /**
   * @returns the Aria label for the accelerators of this search result.
   */
  private getAriaLabelForStandardLayoutSearchResult(): string {
    return this.getStandardAcceleratorInfos()
        .map(
            acceleratorInfo =>
                this.getAriaLabelForStandardAcceleratorInfo(acceleratorInfo))
        .join(` ${this.i18n('searchAcceleratorTextDivider')} `);
  }

  /**
   * @returns the Aria label for the given StandardAcceleratorInfo.
   */
  private getAriaLabelForStandardAcceleratorInfo(
      acceleratorInfo: StandardAcceleratorInfo): string {
    const keyOrIcon =
        acceleratorInfo.layoutProperties.standardAccelerator.keyDisplay;
    return getModifiersForAcceleratorInfo(acceleratorInfo)
        .join(' ')
        .concat(` ${this.getKeyDisplay(keyOrIcon)}`);
  }

  /**
   *
   * @param keyOrIcon the text for an individual accelerator key.
   * @returns the associated icon name for the given `keyOrIcon` text if it
   *     exists, otherwise returns `keyOrIcon` itself.
   */
  private getKeyDisplay(keyOrIcon: string): string {
    const iconName = keyToIconNameMap[keyOrIcon];
    return iconName ? iconName : keyOrIcon;
  }

  /**
   * @returns the Aria label for the accelerators of this search result.
   */
  private getAriaLabelForTextLayoutSearchResult(): string {
    return this.getTextAcceleratorParts()
        .map(part => this.getKeyDisplay(mojoString16ToString(part.text)))
        .join('');
  }

  private makeA11yAnnouncementIfSelectedAndUnfocused(): void {
    if (!this.selected || this.lastFocused) {
      // Do not alert the user if the result is not selected, or
      // the list is focused, defer to aria tags instead.
      return;
    }

    // The selected item is normally not focused when selected, the
    // selected search result should be verbalized as it changes.
    getAnnouncerInstance().announce(this.ariaLabel);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-result-row': SearchResultRowElement;
  }
}

customElements.define(SearchResultRowElement.is, SearchResultRowElement);
