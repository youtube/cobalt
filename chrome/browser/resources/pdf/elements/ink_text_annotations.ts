// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAnnotation} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';
import {pageToScreenCoordinates} from '../ink_text_annotation_utils.js';
import type {Viewport} from '../viewport.js';

import {getCss} from './ink_text_annotations.css.js';
import {getHtml} from './ink_text_annotations.html.js';

export interface InkTextAnnotationsElement {
  $: {
    container: HTMLElement,
  };
}

export class InkTextAnnotationsElement extends CrLitElement {
  static get is() {
    return 'ink-text-annotations';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      annotations_: {type: Array},
      viewport: {type: Object},
    };
  }

  protected accessor annotations_: TextAnnotation[] = [];
  accessor viewport: Viewport|null = null;
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    const manager = Ink2Manager.getInstance();
    this.eventTracker_.add(
        manager, 'annotations-updated', () => this.updateAnnotations_());
    this.updateAnnotations_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  protected getPlaceholderRotations_(annotation: TextAnnotation): number {
    if (!this.viewport) {
      return 0;
    }
    return (this.viewport.getClockwiseRotations() +
            annotation.textOrientation) %
        4;
  }

  private updateAnnotations_() {
    const manager = Ink2Manager.getInstance();
    const allAnnotations: TextAnnotation[] = [];
    const sortedPages =
        Array.from(manager.annotations.keys()).sort((a, b) => a - b);

    for (const page of sortedPages) {
      const pageAnnotationsMap = manager.annotations.get(page);
      if (!pageAnnotationsMap) {
        continue;
      }
      const pageAnnotations = Array.from(pageAnnotationsMap.values());
      // Sort visually: top-to-bottom, then left-to-right
      pageAnnotations.sort((a, b) => {
        if (a.textBoxRect.locationY !== b.textBoxRect.locationY) {
          return a.textBoxRect.locationY - b.textBoxRect.locationY;
        }
        return a.textBoxRect.locationX - b.textBoxRect.locationX;
      });
      allAnnotations.push(...pageAnnotations);
    }
    this.annotations_ = allAnnotations;
    this.updateComplete.then(() => this.updatePlaceholders_());
  }

  private updatePlaceholders_() {
    if (!this.viewport) {
      return;
    }

    const placeholders =
        this.$.container.querySelectorAll<HTMLElement>('.placeholder');
    for (const placeholder of placeholders) {
      const index = Number(placeholder.dataset['index']!);
      const annotation = this.annotations_[index];
      if (!annotation) {
        continue;
      }

      const screenRect = pageToScreenCoordinates(
          annotation.pageIndex, annotation.textBoxRect, this.viewport);

      placeholder.style.setProperty('--left', `${screenRect.locationX}px`);
      placeholder.style.setProperty('--top', `${screenRect.locationY}px`);
      placeholder.style.setProperty('--width', `${screenRect.width}px`);
      placeholder.style.setProperty('--height', `${screenRect.height}px`);
    }
  }

  protected onPlaceholderFocus_(e: FocusEvent) {
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    const annotation = this.annotations_[index];
    assert(annotation);
    assert(this.viewport);

    const screenRect = pageToScreenCoordinates(
        annotation.pageIndex, annotation.textBoxRect, this.viewport);
    Ink2Manager.getInstance().textBoxFocused(screenRect);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-text-annotations': InkTextAnnotationsElement;
  }
}

customElements.define(InkTextAnnotationsElement.is, InkTextAnnotationsElement);
