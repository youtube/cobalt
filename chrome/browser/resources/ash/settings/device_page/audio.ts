// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'audio-settings' allow users to configure their audio settings in system
 * settings.
 */

import '../icons.html.js';
import '../settings_shared.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrSliderElement} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import type {AudioDevice, AudioSystemProperties} from '../mojom-webui/cros_audio_config.mojom-webui.js';
import {AudioDeviceType, AudioEffectState, AudioEffectType, AudioSystemPropertiesObserverReceiver, MuteState} from '../mojom-webui/cros_audio_config.mojom-webui.js';
import type {VoiceIsolationUIAppearance} from '../mojom-webui/cros_audio_config.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {AudioAndCaptionsPageBrowserProxy} from '../os_a11y_page/audio_and_captions_page_browser_proxy.js';
import {AudioAndCaptionsPageBrowserProxyImpl} from '../os_a11y_page/audio_and_captions_page_browser_proxy.js';
import type {Route} from '../router.js';
import {routes} from '../router.js';

import {getTemplate} from './audio.html.js';
import type {CrosAudioConfigInterface} from './cros_audio_config.js';
import {getCrosAudioConfig} from './cros_audio_config.js';
import type {BatteryStatus, DevicePageBrowserProxy} from './device_page_browser_proxy.js';
import {DevicePageBrowserProxyImpl} from './device_page_browser_proxy.js';
import {FakeCrosAudioConfig} from './fake_cros_audio_config.js';

/** Utility for keeping percent in inclusive range of [0,100].  */
function clampPercent(percent: number): number {
  return Math.max(0, Math.min(percent, 100));
}

const SettingsAudioElementBase = WebUiListenerMixin(DeepLinkingMixin(
    PrefsMixin(RouteObserverMixin(I18nMixin(PolymerElement)))));
const VOLUME_ICON_OFF_LEVEL = 0;
// TODO(b/271871947): Match volume icon logic to QS revamp sliders.
// Matches level calculated in unified_volume_view.cc.
const VOLUME_ICON_LOUD_LEVEL = 34;
const SETTINGS_20PX_ICON_PREFIX = 'settings20:';

export class SettingsAudioElement extends SettingsAudioElementBase {
  static get is() {
    return 'settings-audio';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      crosAudioConfig_: {
        type: Object,
      },

      audioSystemProperties_: {
        type: Object,
      },

      isOutputMuted_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      isInputMuted_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      showVoiceIsolationSubsection_: {
        type: Boolean,
      },
      /**
       * Enum values for the
       * 'ash.input_voice_isolation_preferred_effect' preference. These
       * values map to cras::AudioEffectType, and are written to prefs.
       */
      voiceIsolationEffectModePrefValues_: {
        readOnly: true,
        type: Object,
        value: {
          STYLE_TRANSFER: AudioEffectType.kStyleTransfer,
          BEAMFORMING: AudioEffectType.kBeamforming,
        },
      },

      outputVolume_: {
        type: Number,
      },

      powerSoundsHidden_: {
        type: Boolean,
        computed: 'computePowerSoundsHidden_(batteryStatus_)',
      },

      startupSoundEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kChargingSounds,
          Setting.kLowBatterySound,
        ]),
      },

      showAllowAGC: {
        type: Boolean,
        value: loadTimeData.getBoolean('enableForceRespectUiGainsToggle'),
        readonly: true,
      },

      isAllowAGCEnabled: {
        type: Boolean,
        value: true,
      },

      isHfpMicSrEnabled: {
        type: Boolean,
      },

      showHfpMicSr: {
        type: Boolean,
      },

      showSpatialAudio: {
        type: Boolean,
      },

      isSpatialAudioEnabled_: {
        type: Boolean,
        value: true,
      },
    };
  }

  protected isAllowAGCEnabled: boolean;
  protected showAllowAGC: boolean;
  protected isHfpMicSrEnabled: boolean;
  protected showHfpMicSr: boolean;
  protected showSpatialAudio: boolean;

  private audioAndCaptionsBrowserProxy_: AudioAndCaptionsPageBrowserProxy;
  private devicePageBrowserProxy_: DevicePageBrowserProxy;
  private audioSystemProperties_: AudioSystemProperties;
  private audioSystemPropertiesObserverReceiver_:
      AudioSystemPropertiesObserverReceiver;
  private crosAudioConfig_: CrosAudioConfigInterface;
  private isOutputMuted_: boolean;
  private isInputMuted_: boolean;
  private showVoiceIsolationSubsection_: boolean;
  private isSpatialAudioEnabled_: boolean;
  private isSpatialAudioSupported_: boolean;
  private outputVolume_: number;
  private startupSoundEnabled_: boolean;
  private batteryStatus_: BatteryStatus|undefined;
  private powerSoundsHidden_: boolean;
  private isHfpMicSrSupported_: boolean;
  private voiceIsolationEffectModePrefValues_: {[key: string]: number};

  constructor() {
    super();
    this.crosAudioConfig_ = getCrosAudioConfig();

    this.audioSystemPropertiesObserverReceiver_ =
        new AudioSystemPropertiesObserverReceiver(this);

    this.audioAndCaptionsBrowserProxy_ =
        AudioAndCaptionsPageBrowserProxyImpl.getInstance();

    this.devicePageBrowserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.observeAudioSystemProperties_();

    this.addWebUiListener(
        'startup-sound-setting-retrieved', (startupSoundEnabled: boolean) => {
          this.startupSoundEnabled_ = startupSoundEnabled;
        });
    this.addWebUiListener(
        'battery-status-changed', this.set.bind(this, 'batteryStatus_'));

    // Manually call updatePowerStatus to ensure batteryStatus_ is initialized
    // and up to date.
    this.devicePageBrowserProxy_.updatePowerStatus();
  }

  /**
   * AudioSystemPropertiesObserverInterface override
   */
  onPropertiesUpdated(properties: AudioSystemProperties): void {
    this.audioSystemProperties_ = properties;

    this.isOutputMuted_ =
        this.audioSystemProperties_.outputMuteState !== MuteState.kNotMuted;
    this.isInputMuted_ =
        this.audioSystemProperties_.inputMuteState !== MuteState.kNotMuted;
    const activeInputDevice = this.audioSystemProperties_.inputDevices.find(
        (device: AudioDevice) => device.isActive);

    const toggleType =
        this.audioSystemProperties_.voiceIsolationUiAppearance.toggleType;
    this.showVoiceIsolationSubsection_ = toggleType !== AudioEffectType.kNone;

    this.isAllowAGCEnabled =
        (activeInputDevice?.forceRespectUiGainsState ===
         AudioEffectState.kNotEnabled);
    this.outputVolume_ = this.audioSystemProperties_.outputVolumePercent;
    this.isHfpMicSrEnabled =
        (activeInputDevice?.hfpMicSrState === AudioEffectState.kEnabled);
    this.isHfpMicSrSupported_ = activeInputDevice !== undefined &&
        activeInputDevice?.hfpMicSrState !== AudioEffectState.kNotSupported;
    this.showHfpMicSr =
        (this.isHfpMicSrSupported_ &&
         loadTimeData.getBoolean('enableAudioHfpMicSRToggle'));

    const activeOutputDevice = this.audioSystemProperties_.outputDevices.find(
        (device: AudioDevice) => device.isActive);
    this.isSpatialAudioEnabled_ = activeOutputDevice !== undefined &&
        activeOutputDevice?.spatialAudioState === AudioEffectState.kEnabled;
    this.isSpatialAudioSupported_ = activeOutputDevice !== undefined &&
        activeOutputDevice?.spatialAudioState !==
            AudioEffectState.kNotSupported;
    this.showSpatialAudio =
        (this.isSpatialAudioSupported_ &&
         loadTimeData.getBoolean('enableSpatialAudioToggle'));
  }

  getIsOutputMutedForTest(): boolean {
    return this.isOutputMuted_;
  }

  getIsInputMutedForTest(): boolean {
    return this.isInputMuted_;
  }

  private observeAudioSystemProperties_(): void {
    // Use fake observer implementation to access additional properties not
    // available on mojo interface.
    if (this.crosAudioConfig_ instanceof FakeCrosAudioConfig) {
      this.crosAudioConfig_.observeAudioSystemProperties(this);
      return;
    }

    this.crosAudioConfig_.observeAudioSystemProperties(
        this.audioSystemPropertiesObserverReceiver_.$
            .bindNewPipeAndPassRemote());
  }

  /** Determines if audio output is muted by policy. */
  protected isOutputMutedByPolicy_(): boolean {
    return this.audioSystemProperties_.outputMuteState ===
        MuteState.kMutedByPolicy;
  }

  protected onInputMuteClicked(): void {
    this.crosAudioConfig_.setInputMuted(!this.isInputMuted_);
  }

  /** Handles updating active input device. */
  protected onInputDeviceChanged(): void {
    const inputDeviceSelect = this.shadowRoot!.querySelector<HTMLSelectElement>(
        '#audioInputDeviceDropdown');
    assert(!!inputDeviceSelect);
    this.crosAudioConfig_.setActiveDevice(BigInt(inputDeviceSelect.value));
  }

  /**
   * Handles the event where the input volume slider is being changed.
   */
  protected onInputVolumeSliderChanged(): void {
    const sliderValue = this.shadowRoot!
                            .querySelector<CrSliderElement>(
                                '#audioInputGainVolumeSlider')!.value;
    this.crosAudioConfig_.setInputGainPercent(clampPercent(sliderValue));
  }

  /**
   * Handles the event where the output volume slider is being changed.
   */
  private onOutputVolumeSliderChanged_(): void {
    const sliderValue =
        this.shadowRoot!.querySelector<CrSliderElement>(
                            '#outputVolumeSlider')!.value;
    this.crosAudioConfig_.setOutputVolumePercent(clampPercent(sliderValue));
  }

  /** Handles updating active output device. */
  protected onOutputDeviceChanged(): void {
    const outputDeviceSelect =
        this.shadowRoot!.querySelector<HTMLSelectElement>(
            '#audioOutputDeviceDropdown');
    assert(!!outputDeviceSelect);
    this.crosAudioConfig_.setActiveDevice(BigInt(outputDeviceSelect.value));
  }

  /** Handles updating outputMuteState. */
  protected onOutputMuteButtonClicked(): void {
    this.crosAudioConfig_.setOutputMuted(!this.isOutputMuted_);
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    // TODO(crbug.com/1092970): Add DeepLinkingMixin and attempt deep link.
    if (route !== routes.AUDIO) {
      return;
    }

    this.audioAndCaptionsBrowserProxy_.getStartupSoundEnabled();
  }

  /** Handles updating the mic icon depending on the input mute state. */
  protected getInputIcon_(): string {
    return this.isInputMuted_ ? 'settings:mic-off' : 'cr:mic';
  }

  /**
   * Handles updating the output icon depending on the output mute state and
   * volume.
   */
  protected getOutputIcon_(): string {
    if (this.isOutputMuted_) {
      return SETTINGS_20PX_ICON_PREFIX + 'volume-up-off';
    }

    if (this.outputVolume_ === VOLUME_ICON_OFF_LEVEL) {
      return SETTINGS_20PX_ICON_PREFIX + 'volume-zero';
    }

    if (this.outputVolume_ < VOLUME_ICON_LOUD_LEVEL) {
      return SETTINGS_20PX_ICON_PREFIX + 'volume-down';
    }

    return SETTINGS_20PX_ICON_PREFIX + 'volume-up';
  }

  /**
   * Handles the case when there are no output devices. The output section
   * should be hidden in this case.
   */
  protected getOutputHidden_(): boolean {
    return this.audioSystemProperties_.outputDevices.length === 0;
  }

  /**
   * Handles the case when there are no input devices. The input section should
   * be hidden in this case.
   */
  protected getInputHidden_(): boolean {
    return this.audioSystemProperties_.inputDevices.length === 0;
  }

  /**
   * Returns true if input is muted by physical switch; otherwise, return false.
   */
  protected shouldDisableInputGainControls(): boolean {
    return this.audioSystemProperties_.inputMuteState ===
        MuteState.kMutedExternally;
  }

  /** Translates the device name if applicable. */
  private getDeviceName_(audioDevice: AudioDevice): string {
    switch (audioDevice.deviceType) {
      case AudioDeviceType.kHeadphone:
        return this.i18n('audioDeviceHeadphoneLabel');
      case AudioDeviceType.kMic:
        return this.i18n('audioDeviceMicJackLabel');
      case AudioDeviceType.kUsb:
        return this.i18n('audioDeviceUsbLabel', audioDevice.displayName);
      case AudioDeviceType.kBluetooth:
      case AudioDeviceType.kBluetoothNbMic:
        return this.i18n('audioDeviceBluetoothLabel', audioDevice.displayName);
      case AudioDeviceType.kHdmi:
        return this.i18n('audioDeviceHdmiLabel', audioDevice.displayName);
      case AudioDeviceType.kInternalSpeaker:
        return this.i18n('audioDeviceInternalSpeakersLabel');
      case AudioDeviceType.kInternalMic:
        return this.i18n('audioDeviceInternalMicLabel');
      case AudioDeviceType.kFrontMic:
        return this.i18n('audioDeviceFrontMicLabel');
      case AudioDeviceType.kRearMic:
        return this.i18n('audioDeviceRearMicLabel');
      default:
        return audioDevice.displayName;
    }
  }

  /**
   * Returns the appropriate tooltip for output and input device mute buttons
   * based on `muteState`.
   */
  private getMuteTooltip_(muteState: MuteState): string {
    switch (muteState) {
      case MuteState.kNotMuted:
        return this.i18n('audioToggleToMuteTooltip');
      case MuteState.kMutedByUser:
        return this.i18n('audioToggleToUnmuteTooltip');
      case MuteState.kMutedByPolicy:
        return this.i18n('audioMutedByPolicyTooltip');
      case MuteState.kMutedExternally:
        return this.i18n('audioMutedExternallyTooltip');
      default:
        return '';
    }
  }

  /** Returns the appropriate aria-label for input mute button. */
  protected getInputMuteButtonAriaLabel(): string {
    if (this.audioSystemProperties_.inputMuteState ===
        MuteState.kMutedExternally) {
      return this.i18n('audioInputMuteButtonAriaLabelMutedByHardwareSwitch');
    }

    return this.isInputMuted_ ?
        this.i18n('audioInputMuteButtonAriaLabelMuted') :
        this.i18n('audioInputMuteButtonAriaLabelNotMuted');
  }

  /** Returns the appropriate aria-label for output mute button. */
  protected getOutputMuteButtonAriaLabel(): string {
    return this.isOutputMuted_ ?
        this.i18n('audioOutputMuteButtonAriaLabelMuted') :
        this.i18n('audioOutputMuteButtonAriaLabelNotMuted');
  }

  private getVoiceIsolationToggleTitle_(voiceIsolationUIAppearance:
                                            VoiceIsolationUIAppearance|
                                        undefined): string {
    if (voiceIsolationUIAppearance === undefined) {
      return '';
    }
    switch (voiceIsolationUIAppearance.toggleType) {
      case AudioEffectType.kNoiseCancellation:
        return this.i18n('audioInputNoiseCancellationTitle');
      case AudioEffectType.kStyleTransfer:
        return this.i18n('audioInputStyleTransferTitle');
      case AudioEffectType.kBeamforming:
        return this.i18n('audioInputBeamformingTitle');
      default:
        return '';
    }
  }

  private getVoiceIsolationToggleDescription_(voiceIsolationUIAppearance:
                                                  VoiceIsolationUIAppearance|
                                              undefined): string {
    if (voiceIsolationUIAppearance === undefined) {
      return '';
    }
    switch (voiceIsolationUIAppearance.toggleType) {
      case AudioEffectType.kStyleTransfer:
        return this.i18n('audioInputStyleTransferDescription');
      case AudioEffectType.kBeamforming:
        return this.i18n('audioInputBeamformingDescription');
      default:
        return '';
    }
  }

  private shouldShowVoiceIsolationEffectModeOptions_(
      effectModeOptions: number, voiceIsolationEnabled: boolean): boolean {
    return effectModeOptions !== 0 && voiceIsolationEnabled;
  }

  private shouldShowVoiceIsolationFallbackMessage_(
      crasShowEffectFallbackMessage: boolean,
      voiceIsolationEnabled: boolean): boolean {
    return crasShowEffectFallbackMessage && voiceIsolationEnabled;
  }

  private toggleHfpMicSrEnabled_(e: CustomEvent<boolean>): void {
    this.crosAudioConfig_.setHfpMicSrEnabled(e.detail);
  }

  private toggleStartupSoundEnabled_(e: CustomEvent<boolean>): void {
    this.audioAndCaptionsBrowserProxy_.setStartupSoundEnabled(e.detail);
  }

  private toggleAllowAgcEnabled_(e: CustomEvent<boolean>): void {
    this.crosAudioConfig_.setForceRespectUiGainsEnabled(!e.detail);
  }

  private toggleSpatialAudioEnabled_(e: CustomEvent<boolean>): void {
    this.crosAudioConfig_.setSpatialAudioEnabled(e.detail);
  }

  private computePowerSoundsHidden_(): boolean {
    return !this.batteryStatus_?.present;
  }

  private onDeviceStartupSoundRowClicked_(): void {
    this.startupSoundEnabled_ = !this.startupSoundEnabled_;
    this.audioAndCaptionsBrowserProxy_.setStartupSoundEnabled(
        this.startupSoundEnabled_);
  }

  private onVoiceIsolationRowClicked_(): void {
    this.crosAudioConfig_.refreshVoiceIsolationState();
  }

  private onVoiceIsolationEffectModeChanged_(): void {
    this.crosAudioConfig_.refreshVoiceIsolationPreferredEffect();
  }

  private onSpatialAudioRowClicked_(): void {
    const spatialAudioToggle = strictQuery(
        '#audioOutputSpatialAudioToggle', this.shadowRoot, CrToggleElement);
    this.crosAudioConfig_.setSpatialAudioEnabled(!spatialAudioToggle.checked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-audio': SettingsAudioElement;
  }
}

customElements.define(SettingsAudioElement.is, SettingsAudioElement);
