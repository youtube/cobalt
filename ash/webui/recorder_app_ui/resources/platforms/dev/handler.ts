// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * TODO(pihsun): For simplicity sake currently everything is packed into
 * release. Reconsider this and whether we need the mock/alternative
 * implementation other than mojo to exist in release image.
 */
import 'chrome://resources/cros_components/dropdown/dropdown_option.js';
import 'chrome://resources/cros_components/switch/switch.js';
import '../../components/cra/cra-dropdown.js';
import './error-view.js';

import {
  Switch as CrosSwitch,
} from 'chrome://resources/cros_components/switch/switch.js';
import {html, map, styleMap} from 'chrome://resources/mwc/lit/index.js';

import {CraDropdown} from '../../components/cra/cra-dropdown.js';
import {SAMPLE_RATE} from '../../core/audio_constants.js';
import {NoArgStringName} from '../../core/i18n.js';
import {InternalMicInfo} from '../../core/microphone_manager.js';
import {
  LoadModelResult,
  Model,
  ModelExecutionError,
  ModelLoader,
  ModelLoadError,
  ModelResponse,
  ModelState,
} from '../../core/on_device_model/types.js';
import {PerfLogger} from '../../core/perf.js';
import {
  PlatformHandler as PlatformHandlerBase,
} from '../../core/platform_handler.js';
import {computed, Signal, signal} from '../../core/reactive/signal.js';
import {LangPackInfo, LanguageCode} from '../../core/soda/language_info.js';
import {
  HypothesisPart,
  SodaEvent,
  SodaSession,
  TimeDelta,
} from '../../core/soda/types.js';
import {
  assert,
  assertEnumVariant,
  assertExhaustive,
  assertExists,
  assertInstanceof,
  checkEnumVariant,
} from '../../core/utils/assert.js';
import {
  Observer,
  ObserverList,
  Unsubscribe,
} from '../../core/utils/observer_list.js';
import {sleep} from '../../core/utils/utils.js';

import {ErrorView} from './error-view.js';
import {EventsSender} from './metrics.js';
import {
  ColorTheme,
  devSettings,
  init as settingsInit,
} from './settings.js';
import {strings} from './strings.js';

class TitleSuggestionModelDev implements Model<string[]> {
  async execute(content: string): Promise<ModelResponse<string[]>> {
    await sleep(3000);
    const words = content.split(' ');
    const result = [
      `Title for "${words[2]}"`,
      `Longer long title for "${words[1]}"`,
      `This is a very long long title that is too long for "${words[0]}"`,
    ];
    return {kind: 'success', result};
  }

  close(): void {}
}

class SummaryModelDev implements Model<string> {
  async execute(content: string): Promise<ModelResponse<string>> {
    await sleep(3000);
    const result = `Summary for ${content.substring(0, 40)}...`;
    return {kind: 'success', result};
  }

  close(): void {}
}

class ModelLoaderDev<T> extends ModelLoader<T> {
  constructor(
    private readonly model: Model<T>,
    private readonly platformHandler: PlatformHandler,
  ) {
    super();
  }

  override state = signal<ModelState>({kind: 'notInstalled'});

  private modelLoadErrorToModelState(loadError: ModelLoadError): ModelState {
    switch (loadError) {
      case ModelLoadError.LOAD_FAILURE:
        return {kind: 'error'};
      case ModelLoadError.NEEDS_REBOOT:
        return {kind: 'needsReboot'};
      default:
        return assertExhaustive(loadError);
    }
  }

  override async load(): Promise<LoadModelResult<T>> {
    // The simulation in `load` is not reentrant, `load` should not be called
    // again before the previous call completes.
    // TODO(hsuanling): Make `load` reentrant by putting model loading
    // simulation into an AsyncJobQueue so that multiple calls will wait for the
    // one AsyncJob to resolve.
    assert(
      this.state.value.kind !== 'installing',
      'Requested model installation when model is installing.',
    );
    console.log('model installation requested');
    if (this.state.value.kind !== 'installed') {
      this.state.value = {kind: 'installing', progress: 0};
      // Simulate the loading of model.
      let progress = 0;
      while (true) {
        await sleep(200);
        // 4% per 200 ms -> simulate 5 seconds for the whole installation.
        progress += 4;
        if (progress >= 100) {
          break;
        }
        this.state.value = {kind: 'installing', progress};
      }
      this.state.value = {kind: 'installed'};
      devSettings.mutate((s) => {
        s.genAiInstalled = true;
      });
    }
    const loadError = this.platformHandler.forceGenAiModelLoadError.value;
    // Changes model state if load error is specified.
    if (loadError !== null) {
      this.state.value = this.modelLoadErrorToModelState(loadError);
      devSettings.mutate((s) => {
        s.genAiInstalled = false;
      });
    }

    if (this.state.value.kind !== 'installed') {
      return {kind: 'error', error: loadError ?? ModelLoadError.LOAD_FAILURE};
    }

    // Returns model only if it is installed.
    return {kind: 'success', model: this.model};
  }

  override async loadAndExecute(
    content: string,
    language: LanguageCode,
  ): Promise<ModelResponse<T>> {
    if (!this.platformHandler.getLangPackInfo(language).isGenAiSupported) {
      return {kind: 'error', error: ModelExecutionError.UNSUPPORTED_LANGUAGE};
    }
    const result = await this.load();
    if (result.kind === 'error') {
      return result;
    }
    const executionError =
      this.platformHandler.forceGenAiModelExecutionError.value;
    if (executionError !== null) {
      return {kind: 'error', error: executionError};
    }
    try {
      return await result.model.execute(content, language);
    } finally {
      result.model.close();
    }
  }
}

// Random placeholder text from ChromeOS blog.
const TRANSCRIPTION_LINES = `In July we announced that we were working on
Google Chrome OS, an open source operating system for people who spend most of
their time on the web.

Today we are open-sourcing the project as Chromium OS. We are doing this early,
a year before Google Chrome OS will be ready for users, because we are eager to
engage with partners, the open source community and developers. As with the
Google Chrome browser, development will be done in the open from this point on.

This means the code is free, accessible to anyone and open for contributions.
The Chromium OS project includes our current code base, user interface
experiments and some initial designs for ongoing development.

This is the initial sketch and we will color it in over the course of the next
year. We want to take this opportunity to explain why we're excited about the
project and how it is a fundamentally different model of computing.

First, it's all about the web. All apps are web apps. The entire experience
takes place within the browser and there are no conventional desktop
applications. This means users do not have to deal with installing, managing
and updating programs.

Second, because all apps live within the browser, there are significant
benefits to security. Unlike traditional operating systems, Chrome OS doesn't
trust the applications you run. Each app is contained within a security sandbox
making it harder for malware and viruses to infect your computer.

Furthermore, Chrome OS barely trusts itself. Every time you restart your
computer the operating system verifies the integrity of its code. If your
system has been compromised, it is designed to fix itself with a reboot.
While no computer can be made completely secure, we're going to make life
much harder (and less profitable) for the bad guys. If you dig security, read
the Chrome OS Security Overview or watch the video.

Most of all, we are obsessed with speed. We are taking out every unnecessary
process, optimizing many operations and running everything possible in
parallel. This means you can go from turning on the computer to surfing the web
in a few seconds. Our obsession with speed goes all the way down to the metal.

We are specifying reference hardware components to create the fastest
experience for Google Chrome OS.

There is still a lot of work to do, and we're excited to work with the open
source community. We have benefited hugely from projects like GNU, the Linux
Kernel, Moblin, Ubuntu, WebKit and many more. We will be contributing our code
upstream and engaging closely with these and other open source efforts.
Google Chrome OS will be ready for consumers this time next year. Sign up here
for updates or if you like building your operating system from source, get
involved at chromium.org.

Lastly, here is a short video that explains why we're so excited about Google
Chrome OS.`.split('\n\n').map((line) => line.split(/\s+/));

// Emit one word per 200 ms.
const WORD_INTERVAL_MS = 200;

const MAX_NUM_SPEAKER = 3;

function timeDelta(milliseconds: number): TimeDelta {
  return {microseconds: BigInt(milliseconds * 1000)};
}

class SodaSessionDev implements SodaSession {
  private readonly observers = new ObserverList<SodaEvent>();

  private numSamples = 0;

  private currentLineIdx = 0;

  private currentWordIdx = 0;

  private fakeTimeMs = 0;

  private speakerLabelCorrectionPart: HypothesisPart|null = null;

  // TODO(pihsun): Simulate partial result being changed/corrected.
  private emitSodaNextWord(finishLine = false): void {
    this.fakeTimeMs += WORD_INTERVAL_MS;
    // Don't emit any word for the first two seconds, to simulate waiting state.
    if (this.fakeTimeMs <= 2000) {
      return;
    }
    const currentLine = assertExists(TRANSCRIPTION_LINES[this.currentLineIdx]);
    const lineStartTimeMs =
      this.fakeTimeMs - (this.currentWordIdx + 1) * WORD_INTERVAL_MS;
    const timingEvent = {
      audioStartTime: timeDelta(lineStartTimeMs),
      eventEndTime: timeDelta(this.fakeTimeMs),
    };
    // Speaker label starts from "1".
    const lineSpeakerLabel = (this.currentLineIdx % MAX_NUM_SPEAKER) + 1;
    const hypothesisPart =
      currentLine.slice(0, this.currentWordIdx + 1).map((w, i) => {
        let speakerLabel = lineSpeakerLabel;
        if (i === 0 && this.currentLineIdx > 0 && !finishLine) {
          // Change speaker label of first word of each line to "wrong" speaker
          // ID, to simulate speaker label correction event.
          speakerLabel--;
          if (speakerLabel === 0) {
            speakerLabel = MAX_NUM_SPEAKER;
          }
        }
        return {
          text: [w],
          alignment: timeDelta(i * WORD_INTERVAL_MS),
          leadingSpace: true,
          speakerLabel: speakerLabel.toString(),
        } satisfies HypothesisPart;
      });

    // Emit the previous line correction event on the half point of the
    // next line.
    if ((this.currentWordIdx >= currentLine.length / 2 || finishLine) &&
        this.speakerLabelCorrectionPart !== null) {
      this.observers.notify({
        labelCorrectionEvent: {
          hypothesisParts: [this.speakerLabelCorrectionPart],
        },
      });
      this.speakerLabelCorrectionPart = null;
    }

    if (this.currentWordIdx === currentLine.length - 1 || finishLine) {
      this.speakerLabelCorrectionPart = {
        text: [assertExists(currentLine[0])],
        alignment: timeDelta(lineStartTimeMs),
        leadingSpace: true,
        speakerLabel: lineSpeakerLabel.toString(),
      };
      this.observers.notify({
        finalResult: {
          finalHypotheses: currentLine,
          hypothesisPart,
          timingEvent,
        },
      });

      this.currentWordIdx = 0;
      this.currentLineIdx++;
      if (this.currentLineIdx === TRANSCRIPTION_LINES.length) {
        this.currentLineIdx = 0;
      }
    } else {
      this.observers.notify({
        partialResult: {
          partialText: [
            currentLine.slice(0, this.currentWordIdx + 1).join(' '),
          ],
          hypothesisPart,
          timingEvent,
        },
      });
      this.currentWordIdx++;
    }
  }

  start(): Promise<void> {
    console.info('Soda session started');
    return Promise.resolve();
  }

  addAudio(samples: Float32Array): void {
    console.debug(`Soda add audio of length ${samples.length}`);

    this.numSamples += samples.length;
    const wordIntervalSamples = (SAMPLE_RATE * WORD_INTERVAL_MS) / 1000;
    while (this.numSamples >= wordIntervalSamples) {
      this.emitSodaNextWord();
      this.numSamples -= wordIntervalSamples;
    }
  }

  stop(): Promise<void> {
    console.info('Soda session stopped');
    this.emitSodaNextWord(true);
    return Promise.resolve();
  }

  subscribeEvent(observer: Observer<SodaEvent>): Unsubscribe {
    return this.observers.subscribe(observer);
  }
}

/**
 * Returns a formatted localized string where $1 to $9 are replaced by the
 * second to the tenth argument. Any standalone $ signs must be escaped as
 * $$.
 *
 * This is a copy of the function in
 * ui/webui/resources/js/load_time_data.ts to avoid pulling those as
 * dependency for dev.
 */
function substituteI18nString(label: string, ...args: Array<number|string>):
  string {
  return label.replace(/\$(.|$|\n)/g, (m) => {
    assert(
      /\$[$1-9]/.exec(m) !== null,
      'Unescaped $ found in localized string.',
    );
    if (m === '$$') {
      return '$';
    }

    const substitute = args[Number(m[1]) - 1];
    if (substitute === undefined || substitute === null) {
      // Not all callers actually provide values for all substitutes. Return
      // an empty value for this case.
      return '';
    }
    return substitute.toString();
  });
}

export class PlatformHandler extends PlatformHandlerBase {
  override readonly quietMode = signal(false);

  private readonly sodaStates = new Map<LanguageCode, Signal<ModelState>>();

  private readonly langPacks = new Map<LanguageCode, LangPackInfo>();

  override readonly canUseSpeakerLabel = computed(
    () => devSettings.value.canUseSpeakerLabel,
  );

  readonly errorView = new ErrorView();

  static override getStringF(id: string, ...args: Array<number|string>):
    string {
    const label = strings[id];
    if (label === undefined) {
      console.error(`Unknown string ${id}`);
      return '';
    }
    return substituteI18nString(label, ...args);
  }

  static override getDeviceType(): string {
    return 'Chromebook';
  }

  override readonly canCaptureSystemAudioWithLoopback = computed(
    () => devSettings.value.canCaptureSystemAudioWithLoopback,
  );

  private readonly forceLanguageSelection = computed(
    () => devSettings.value.forceLanguageSelection,
  );

  readonly forceGenAiModelExecutionError = computed(
    () => devSettings.value.forceGenAiModelExecutionError,
  );

  readonly forceGenAiModelLoadError = computed(
    () => devSettings.value.forceGenAiModelLoadError,
  );

  override init(): Promise<void> {
    document.body.appendChild(this.errorView);
    settingsInit();
    const sodaState = signal<ModelState>({kind: 'notInstalled'});
    this.sodaStates.set(LanguageCode.EN_US, sodaState);
    if (devSettings.value.sodaInstalled) {
      // TODO(pihsun): Remember the whole state in devSettings instead?
      sodaState.value = {kind: 'installed'};
    }
    if (devSettings.value.genAiInstalled) {
      this.summaryModelLoader.state = signal<ModelState>({kind: 'installed'});
      this.titleSuggestionModelLoader.state =
        signal<ModelState>({kind: 'installed'});
    }
    this.langPacks.set(LanguageCode.EN_US, {
      languageCode: LanguageCode.EN_US,
      displayName: 'English',
      isGenAiSupported: true,
      isSpeakerLabelSupported: true,
    });

    this.initPerfEventWatchers();
    return Promise.resolve();
  }

  override getDefaultLanguage(): LanguageCode {
    return LanguageCode.EN_US;
  }

  override getLangPackList(): readonly LangPackInfo[] {
    return Array.from(this.langPacks.values());
  }

  override getLangPackInfo(language: LanguageCode): LangPackInfo {
    return assertExists(this.langPacks.get(language));
  }

  override isMultipleLanguageAvailable(): boolean {
    if (this.forceLanguageSelection.value) {
      return true;
    }

    let count = 0;
    for (const state of this.sodaStates.values()) {
      if (state.value.kind !== 'unavailable') {
        count += 1;
      }
    }
    return count > 1;
  }

  override summaryModelLoader = new ModelLoaderDev(new SummaryModelDev(), this);

  override titleSuggestionModelLoader = new ModelLoaderDev(
    new TitleSuggestionModelDev(),
    this,
  );

  override eventsSender = new EventsSender();

  override perfLogger = new PerfLogger(this.eventsSender);

  override installSoda(language: LanguageCode): Promise<void> {
    console.log(`SODA lang pack ${language} installation requested`);
    const sodaState = this.getSodaState(language);
    if (sodaState.value.kind !== 'installed' &&
        sodaState.value.kind !== 'installing') {
      sodaState.value = {kind: 'installing', progress: 0};
      // Simulate the loading of SODA model.
      // Not awaiting the async block should be fine since this is only for
      // dev, and no two async block of this will run at the same time.
      void (async () => {
        let progress = 0;
        while (true) {
          await sleep(100);
          // 5% per 100 ms -> simulate 2 seconds for the whole installation.
          progress += 5;
          if (progress >= 100) {
            devSettings.mutate((s) => {
              s.sodaInstalled = true;
            });
            sodaState.value = {kind: 'installed'};
            return;
          }
          sodaState.value = {kind: 'installing', progress};
        }
      })();
    }
    return Promise.resolve();
  }

  override isSodaAvailable(): boolean {
    return true;
  }

  override getSodaState(language: LanguageCode): Signal<ModelState> {
    return assertExists(this.sodaStates.get(language));
  }

  override newSodaSession(_language: LanguageCode): Promise<SodaSession> {
    return Promise.resolve(new SodaSessionDev());
  }

  override getMicrophoneInfo(
    _deviceId: string,
  ): Promise<InternalMicInfo> {
    return Promise.resolve({isDefault: false, isInternal: false});
  }

  private renderGenAiErrorOptions<T extends ModelExecutionError|ModelLoadError>(
    errorValues: T[],
    selectedError: T|null,
  ): RenderResult {
    return html`
      <cros-dropdown-option
        headline="SUCCESS"
        ?selected=${selectedError === null}
      ></cros-dropdown-option>
      ${map(errorValues, (errorValue) => {
        return html`
          <cros-dropdown-option
          headline=${errorValue}
          ?selected=${errorValue === selectedError}
          ></cros-dropdown-option>
        `;
      })}
    `;
  }

  override renderDevUi(): RenderResult {
    function handleColorModeChange(ev: Event) {
      devSettings.mutate((s) => {
        s.forceTheme = assertEnumVariant(
          ColorTheme,
          assertInstanceof(ev.target, CraDropdown).value,
        );
      });
    }
    function handleCanUseSpeakerLabelChange(ev: Event) {
      const target = assertInstanceof(ev.target, CrosSwitch);
      devSettings.mutate((s) => {
        s.canUseSpeakerLabel = target.selected;
      });
    }
    function handleCanCaptureSystemAudioWithLoopbackChange(ev: Event) {
      const target = assertInstanceof(ev.target, CrosSwitch);
      devSettings.mutate((s) => {
        s.canCaptureSystemAudioWithLoopback = target.selected;
      });
    }
    function handleForceLanguageSelectionChange(ev: Event) {
      const target = assertInstanceof(ev.target, CrosSwitch);
      devSettings.mutate((s) => {
        s.forceLanguageSelection = target.selected;
      });
    }
    function handleForceGenAiModelLoadErrorChange(ev: Event) {
      devSettings.mutate((s) => {
        s.forceGenAiModelLoadError = checkEnumVariant(
          ModelLoadError,
          assertInstanceof(ev.target, CraDropdown).value,
        );
      });
    }
    function handleForceGenAiModelExecutionErrorChange(ev: Event) {
      devSettings.mutate((s) => {
        s.forceGenAiModelExecutionError = checkEnumVariant(
          ModelExecutionError,
          assertInstanceof(ev.target, CraDropdown).value,
        );
      });
    }
    // TODO(pihsun): Move the dev toggle to a separate component, so we don't
    // need to inline the styles.
    const labelStyle = {
      display: 'flex',
      flexFlow: 'row',
      alignItems: 'center',
    };
    const loadErrorValues = Object.values(ModelLoadError);
    const selectedLoadError = this.forceGenAiModelLoadError.value;
    const loadErrorOptions = this.renderGenAiErrorOptions<ModelLoadError>(
      loadErrorValues,
      selectedLoadError,
    );
    const executionErrorValues = Object.values(ModelExecutionError);
    const selectedExecutionError = this.forceGenAiModelExecutionError.value;
    const executionErrorOptions =
      this.renderGenAiErrorOptions<ModelExecutionError>(
        executionErrorValues,
        selectedExecutionError,
      );
    return html`
      <div class="section">
        <label style=${styleMap(labelStyle)}>
          <cra-dropdown
            label="dark/light mode"
            @change=${handleColorModeChange}
            .value=${devSettings.value.forceTheme ?? ColorTheme.SYSTEM}
          >
            <cros-dropdown-option
              headline="System default"
              value=${ColorTheme.SYSTEM}
            >
            </cros-dropdown-option>
            <cros-dropdown-option headline="Light" value=${ColorTheme.LIGHT}>
            </cros-dropdown-option>
            <cros-dropdown-option headline="Dark" value=${ColorTheme.DARK}>
            </cros-dropdown-option>
          </cra-dropdown>
        </label>
      </div>
      <div class="section">
        <label style=${styleMap(labelStyle)}>
          <!--
            TODO(pihsun): cros-switch doesn't automatically makes clicking the
            surrounding label toggles the switch, unlike md-switch.
          -->
          <cros-switch
            @change=${handleCanUseSpeakerLabelChange}
            .selected=${this.canUseSpeakerLabel.value}
          >
          </cros-switch>
          Toggle can use speaker label
        </label>
      </div>
      <div class="section">
        <label style=${styleMap(labelStyle)}>
          <!--
            TODO(hsuanling): cros-switch doesn't automatically makes clicking
            the surrounding label toggles the switch, unlike md-switch.
          -->
          <cros-switch
            @change=${handleCanCaptureSystemAudioWithLoopbackChange}
            .selected=${this.canCaptureSystemAudioWithLoopback.value}
          >
          </cros-switch>
          Toggle can capture system audio via getDisplayMedia
        </label>
      </div>
      <div class="section">
        <label style=${styleMap(labelStyle)}>
          <!--
            TODO(hsuanling): cros-switch doesn't automatically makes clicking
            the surrounding label toggles the switch, unlike md-switch.
          -->
          <cros-switch
            @change=${handleForceLanguageSelectionChange}
            .selected=${this.forceLanguageSelection.value}
          >
          </cros-switch>
          Toggle can force language selection display
        </label>
      </div>
      <div class="section">
        <label style=${styleMap(labelStyle)}>
          <cra-dropdown
            label="GenAi model load error"
            @change=${handleForceGenAiModelLoadErrorChange}
          >
            ${loadErrorOptions}
          </cra-dropdown>
        </label>
      </div>
      <div class="section">
        <label style=${styleMap(labelStyle)}>
          <cra-dropdown
            label="GenAi model execution error"
            @change=${handleForceGenAiModelExecutionErrorChange}
          >
            ${executionErrorOptions}
          </cra-dropdown>
        </label>
      </div>
    `;
  }

  override handleUncaughtError(error: unknown): void {
    this.errorView.error = error;
  }

  override showAiFeedbackDialog(description: string): void {
    console.log('Feedback report dialog requested: ', description);
    window.prompt('fake AI feedback dialog', description);
  }

  override async getSystemAudioMediaStream(): Promise<MediaStream> {
    // The video stream is required for getDisplayMedia() when
    // DISPLAY_MEDIA_SYSTEM_AUDIO permission is not granted, so we need to
    // remove the video tracks manually.
    const stream = await navigator.mediaDevices.getDisplayMedia({
      audio: {
        autoGainControl: {ideal: false},
        echoCancellation: {ideal: false},
        noiseSuppression: {ideal: false},
      },
      systemAudio: 'include',
    });
    const videoTracks = stream.getVideoTracks();
    for (const videoTrack of videoTracks) {
      stream.removeTrack(videoTrack);
    }
    return stream;
  }

  override getLocale(): string|undefined {
    // Always use en-US in dev mode, since mock for the main i18n also use en-US
    // translations.
    return 'en-US';
  }

  override recordSpeakerLabelConsent(
    consentGiven: boolean,
    consentDescriptionNames: NoArgStringName[],
    consentConfirmationName: NoArgStringName,
  ): void {
    console.info(
      'Recorded speaker label consent: ',
      consentGiven,
      consentDescriptionNames,
      consentConfirmationName,
    );
  }
}
