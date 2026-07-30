// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_text_box.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAnnotation, TextBoxRect} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';
import type {TextBoxInit} from '../ink2_manager.js';
import {pageToScreenCoordinates} from '../ink_text_annotation_utils.js';
import type {Viewport, ViewportRect} from '../viewport.js';

import {getCss} from './ink_text_annotations.css.js';
import {getHtml} from './ink_text_annotations.html.js';
import type {InkTextBoxElement} from './ink_text_box.js';
import {TextBoxState} from './ink_text_box.js';

export interface InkTextAnnotationsElement {
  $: {
    container: HTMLElement,
    textBox: InkTextBoxElement,
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
      activeAnnotation_: {type: Object},
      activePageDimensions_: {type: Object},
    };
  }

  protected accessor annotations_: TextAnnotation[] = [];
  accessor viewport: Viewport|null = null;
  protected accessor activeAnnotation_: TextAnnotation|null = null;
  protected accessor activePageDimensions_: ViewportRect|null = null;
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    const manager = Ink2Manager.getInstance();
    this.eventTracker_.add(
        manager, 'annotations-updated', () => this.updateAnnotations_());
    this.eventTracker_.add(
        manager, 'initialize-text-box',
        (e: Event) =>
            this.onInitializeTextBox_((e as CustomEvent<TextBoxInit>).detail));
    this.updateAnnotations_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  viewportChanged() {
    this.updatePlaceholders_();
    this.$.textBox.viewportChanged();
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
    this.scrollToShowTextBox_(screenRect);
  }

  protected async onPlaceholderClick_(e: MouseEvent) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    await this.activateAnnotationByIndex_(index);
  }

  protected async onPlaceholderKeydown_(e: KeyboardEvent) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }
    e.preventDefault();
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    await this.activateAnnotationByIndex_(index);
  }

  private async activateAnnotationByIndex_(index: number) {
    // Grab the annotation first, since committing may update the annotations
    // list and make `index` refer to a different annotation than intended.
    const annotation = this.annotations_[index];
    assert(annotation);

    if (this.activeAnnotation_) {
      // The requested annotation is already active. This also ensures that if
      // committing deletes an annotation, it isn't the one being activated.
      if (this.activeAnnotation_.id === annotation.id) {
        return;
      }
      await this.$.textBox.commitTextAnnotation();
    }

    assert(this.viewport);

    // Convert box to screen coordinates.
    const screenRect = pageToScreenCoordinates(
        annotation.pageIndex, annotation.textBoxRect, this.viewport);

    // Create a copy of the annotation with screen coordinates for the textbox.
    const annotationToActivate = structuredClone(annotation);
    annotationToActivate.textBoxRect = screenRect;

    // Notify the backend.
    Ink2Manager.getInstance().reactivateTextAnnotation(annotation);
    this.activeAnnotation_ = annotationToActivate;
    this.activePageDimensions_ =
        this.viewport.getPageScreenRect(annotation.pageIndex);
  }

  commitActiveAnnotation(): Promise<void> {
    return this.$.textBox.commitTextAnnotation();
  }

  blurActiveAnnotation() {
    this.$.textBox.blur();
  }

  protected onTextBoxStateChanged_(e: CustomEvent<TextBoxState>) {
    if (e.detail === TextBoxState.INACTIVE) {
      this.activeAnnotation_ = null;
      this.activePageDimensions_ = null;
    }
    this.fire('state-changed', e.detail);
  }

  private async onInitializeTextBox_(data: TextBoxInit) {
    if (this.activeAnnotation_) {
      await this.$.textBox.commitTextAnnotation();
    }
    this.activeAnnotation_ = data.annotation;
    this.activePageDimensions_ = data.pageDimensions;
  }

  protected onTextboxFocused_(e: CustomEvent<TextBoxRect>) {
    this.scrollToShowTextBox_(e.detail);
  }

  private scrollToShowTextBox_(textBoxRect: TextBoxRect) {
    assert(this.viewport);
    const viewportPosition = this.viewport.position;
    const viewportSize = this.viewport.size;

    let scrollX: number|undefined;
    let scrollY: number|undefined;
    if (textBoxRect.locationX < 0 ||
        textBoxRect.locationX + textBoxRect.width > viewportSize.width) {
      // Adjusting by 10% of viewport, rather than putting the text box on the
      // exact edge of the viewport.
      scrollX = viewportPosition.x + textBoxRect.locationX -
          Math.floor(viewportSize.width / 10);
    }

    if (textBoxRect.locationY < 0 ||
        textBoxRect.locationY + textBoxRect.height > viewportSize.height) {
      // Adjusting by 10% of viewport, rather than putting the text box on the
      // exact edge of the viewport.
      scrollY = viewportPosition.y + textBoxRect.locationY -
          Math.floor(viewportSize.height / 10);
    }

    if (scrollX !== undefined || scrollY !== undefined) {
      // TODO(crbug.com/40218278): Re-enable smooth scrolling for all codepaths.
      this.viewport.scrollTo({
        x: scrollX,
        y: scrollY,
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-text-annotations': InkTextAnnotationsElement;
  }
}

customElements.define(InkTextAnnotationsElement.is, InkTextAnnotationsElement);
