// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-image.js';
import './language-dropdown.js';
import './speaker-label-consent-dialog-content.js';

import {
  createRef,
  css,
  html,
  keyed,
  PropertyDeclarations,
  PropertyValues,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n, NoArgStringName} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {LanguageCode} from '../core/soda/language_info.js';
import {
  settings,
  SpeakerLabelEnableState,
} from '../core/state/settings.js';
import {
  disableTranscription,
  enableTranscription,
  setTranscriptionLanguage,
} from '../core/state/transcription.js';
import {
  assertExhaustive,
  assertExists,
  assertInstanceof,
} from '../core/utils/assert.js';

import {CraButton} from './cra/cra-button.js';
import {LanguageDropdown} from './language-dropdown.js';
import {
  DESCRIPTION_NAMES as SPEAKER_LABEL_DIALOG_DESCRIPTION_NAMES,
} from './speaker-label-consent-dialog-content.js';

/**
 * A dialog for showing the onboarding flow.
 */
export class OnboardingDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    #dialog {
      background: var(--cros-sys-base_elevated);
      border: none;
      border-radius: 20px;
      box-shadow: var(--cros-sys-app_elevation3);
      color: var(--cros-sys-on_surface);
      display: flex;
      flex-flow: column;
      height: 512px;
      padding: 0;

      /* Want at least 80px left/right margin. */
      width: min(512px, 100vw - 160px);

      &::backdrop {
        background: var(--cros-sys-scrim);
        pointer-events: none;
      }

      /* From CrOS dialog style. Min width for Recorder App is 480px. */
      @media (width < 520px) {
        width: 360px;
      }
    }

    #illust {
      align-items: center;
      background: var(--cros-sys-highlight_shape);
      display: flex;
      height: 236px;
      justify-content: center;
      overflow: hidden;
      width: 100%;
    }

    #content {
      display: flex;
      flex: 1;
      flex-flow: column;
      min-height: 0;
      padding: 32px 32px 28px;
    }

    #header {
      font: var(--cros-display-7-font);
      margin: 0 0 16px;
    }

    language-dropdown {
      margin-top: 16px;
    }

    #description {
      color: var(--cros-sys-on_surface_variant);
      flex: 1;
      font: var(--cros-body-1-font);
      margin-bottom: 32px;
      overflow-y: auto;
    }

    #buttons {
      display: flex;
      flex-flow: row;
      gap: 8px;
      justify-content: right;

      & > .left {
        margin-right: auto;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    open: {type: Boolean},
    step: {state: true},
  };

  /**
   * Whether the dialog is opened.
   */
  open = false;

  /**
   * The currently shown step index starting from 0.
   *
   * Step 0: Welcoming page.
   * Step 1: Transcription consent dialog for users to turn on transcription and
   * download en-US model.
   * Step 2: Transcription consent dialog for users to turn on transcription.
   * Step 3: Transcription language selection dialog for users to choose and
   * download language model.
   * Step 4: Speaker label consent dialog for users to turn on speaker label.
   *
   * When there are multiple language options, skip step 1. Otherwise, skip step
   * 2 and step 3.
   */
  step: 0|1|2|3|4 = 0;

  private readonly platformHandler = usePlatformHandler();

  private readonly autoFocusItem = createRef<CraButton>();

  private readonly dropdown = createRef<LanguageDropdown>();

  get dialog(): HTMLDivElement {
    return assertInstanceof(
      this.shadowRoot?.getElementById('dialog'),
      HTMLDivElement,
    );
  }

  override updated(changedProperties: PropertyValues<this>): void {
    if (changedProperties.has('open') || changedProperties.has('step')) {
      if (this.open) {
        this.dialog.showPopover();
      } else {
        this.dialog.hidePopover();
      }
    }

    if (changedProperties.has('step')) {
      const autoFocusItem = this.autoFocusItem.value;
      if (autoFocusItem !== undefined) {
        autoFocusItem.updateComplete.then(() => {
          autoFocusItem.focus();
        });
      }
    }
  }

  private sendOnboardEvent(): void {
    this.platformHandler.eventsSender.sendOnboardEvent({
      speakerLabelEnableState: settings.value.speakerLabelEnabled,
      transcriptionAvailable: this.platformHandler.isSodaAvailable(),
      transcriptionEnableState: settings.value.transcriptionEnabled,
    });
  }

  private close() {
    this.sendOnboardEvent();
    this.dispatchEvent(new Event('close'));
  }

  private renderDialog(
    step: number,
    imageName: string,
    header: string,
    description: RenderResult,
    buttons: RenderResult,
  ): RenderResult {
    // TODO(pihsun): Extract this to a separate component if any other place
    // need unclosable modal dialog.
    // We can't use <dialog> / <md-dialog> / <cra-dialog> here since the dialog
    // is always cancelable by pressing ESC, and the onboarding flow should not
    // be cancelable.
    // See https://issues.chromium.org/issues/346597066.
    //
    // Force render a different element when step change, so ChromeVox would
    // convey the header of the new dialog.
    return keyed(
      step,
      html`<div
        id="dialog"
        popover="manual"
        ?inert=${!this.open}
        aria-labelledby="header"
        role="dialog"
      >
        <div id="illust">
          <cra-image .name=${imageName}></cra-image>
        </div>
        <div id="content">
          <h2 id="header">${header}</h2>
          <div id="description">${description}</div>
          <div id="buttons">${buttons}</div>
        </div>
      </div>`,
    );
  }

  override render(): RenderResult {
    // Note that all the onboarding_ images are currently placeholders and
    // don't use dynamic color tokens yet.
    // TODO: b/344785475 - Change to final illustration when ready.
    switch (this.step) {
      case 0: {
        const nextStep = () => {
          if (!this.platformHandler.isSodaAvailable()) {
            // SODA isn't available on this platform. Don't ask for enabling
            // transcription or speaker label.
            this.close();
            return;
          }
          this.step =
            this.platformHandler.getLangPackList().length <= 1 ? 1 : 2;
        };
        return this.renderDialog(
          this.step,
          'onboarding_welcome',
          i18n.onboardingDialogWelcomeHeader,
          i18n.onboardingDialogWelcomeDescription,
          html`<cra-button
            .label=${i18n.onboardingDialogWelcomeNextButton}
            @click=${nextStep}
            ${ref(this.autoFocusItem)}
          ></cra-button>`,
        );
      }
      case 1: {
        const turnOnTranscription = () => {
          enableTranscription();
          setTranscriptionLanguage(LanguageCode.EN_US);
          if (!this.platformHandler.canUseSpeakerLabel.value) {
            // Speaker label isn't supported on this platform.
            this.close();
            return;
          }
          this.step = 4;
        };
        const turnOffTranscription = () => {
          disableTranscription(/* firstTime= */ true);
          this.close();
        };
        return this.renderDialog(
          this.step,
          'onboarding_transcription',
          i18n.onboardingDialogTranscriptionHeader,
          i18n.onboardingDialogTranscriptionDescription,
          html`
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionDeferButton}
              class="left"
              @click=${this.close}
              ${ref(this.autoFocusItem)}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionCancelButton}
              @click=${turnOffTranscription}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionDownloadButton}
              @click=${turnOnTranscription}
            ></cra-button>
          `,
        );
      }
      case 2: {
        const turnOnTranscription = () => {
          enableTranscription();
          this.step = 3;
        };
        const turnOffTranscription = () => {
          disableTranscription(/* firstTime= */ true);
          this.close();
        };
        return this.renderDialog(
          this.step,
          'onboarding_transcription',
          i18n.onboardingDialogTranscriptionTurnOnHeader,
          i18n.onboardingDialogTranscriptionTurnOnDescription,
          html`
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionDeferButton}
              class="left"
              @click=${this.close}
              ${ref(this.autoFocusItem)}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionCancelButton}
              @click=${turnOffTranscription}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionTurnOnButton}
              @click=${turnOnTranscription}
            ></cra-button>
          `,
        );
      }
      case 3: {
        const dialogBody = html`
          ${i18n.onboardingDialogLanguageSelectionDescription}
          <language-dropdown
            .languageList=${this.platformHandler.getLangPackList()}
            ${ref(this.dropdown)}
          >
          </language-dropdown>
        `;

        const downloadLanguage = () => {
          const languageCode = assertExists(this.dropdown.value).value;
          if (languageCode === null) {
            return;
          }
          setTranscriptionLanguage(languageCode);
          if (!this.platformHandler.canUseSpeakerLabel.value) {
            // Speaker label isn't supported on this platform.
            this.close();
            return;
          }
          this.step = 4;
        };

        const cancelSelection = () => {
          this.close();
        };

        return this.renderDialog(
          this.step,
          'onboarding_transcription',
          i18n.onboardingDialogLanguageSelectionHeader,
          dialogBody,
          html`
            <cra-button
              .label=${i18n.onboardingDialogLanguageSelectionCancelButton}
              @click=${cancelSelection}
            ></cra-button>
            <cra-button
              label=${i18n.onboardingDialogLanguageSelectionDownloadButton}
              @click=${downloadLanguage}
            ></cra-button>
          `,
        );
      }
      case 4: {
        const ALLOW_BUTTON_NAME: NoArgStringName =
          'onboardingDialogSpeakerLabelAllowButton';
        const DISALLOW_BUTTON_NAME: NoArgStringName =
          'onboardingDialogSpeakerLabelDisallowButton';
        const disableSpeakerLabel = () => {
          settings.mutate((s) => {
            s.speakerLabelEnabled = SpeakerLabelEnableState.DISABLED_FIRST;
          });
          this.platformHandler.recordSpeakerLabelConsent(
            false,
            SPEAKER_LABEL_DIALOG_DESCRIPTION_NAMES,
            DISALLOW_BUTTON_NAME,
          );
          this.close();
        };

        const enableSpeakerLabel = () => {
          settings.mutate((s) => {
            s.speakerLabelEnabled = SpeakerLabelEnableState.ENABLED;
          });
          this.platformHandler.recordSpeakerLabelConsent(
            true,
            SPEAKER_LABEL_DIALOG_DESCRIPTION_NAMES,
            ALLOW_BUTTON_NAME,
          );
          this.close();
        };

        return this.renderDialog(
          this.step,
          'onboarding_speaker_label',
          i18n.onboardingDialogSpeakerLabelHeader,
          html`<speaker-label-consent-dialog-content>
          </speaker-label-consent-dialog-content>`,
          html`
            <cra-button
              .label=${i18n.onboardingDialogSpeakerLabelDeferButton}
              class="left"
              @click=${this.close}
              ${ref(this.autoFocusItem)}
            ></cra-button>
            <cra-button
              .label=${i18n[DISALLOW_BUTTON_NAME]}
              @click=${disableSpeakerLabel}
            ></cra-button>
            <cra-button
              .label=${i18n[ALLOW_BUTTON_NAME]}
              @click=${enableSpeakerLabel}
            ></cra-button>
          `,
        );
      }
      default:
        assertExhaustive(this.step);
    }
  }
}

window.customElements.define('onboarding-dialog', OnboardingDialog);

declare global {
  interface HTMLElementTagNameMap {
    'onboarding-dialog': OnboardingDialog;
  }
}
