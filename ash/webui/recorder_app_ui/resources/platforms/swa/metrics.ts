// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  // Events
  CrOSEvents_RecorderApp_AppStartPerf,
  CrOSEvents_RecorderApp_ChangePlaybackSpeed,
  CrOSEvents_RecorderApp_ChangePlaybackVolume,
  CrOSEvents_RecorderApp_Export,
  CrOSEvents_RecorderApp_ExportPerf,
  CrOSEvents_RecorderApp_FeedbackSummary,
  CrOSEvents_RecorderApp_FeedbackTitleSuggestion,
  CrOSEvents_RecorderApp_Onboard,
  CrOSEvents_RecorderApp_Record,
  CrOSEvents_RecorderApp_RecordingSavingPerf,
  CrOSEvents_RecorderApp_StartSession,
  CrOSEvents_RecorderApp_SuggestTitle,
  CrOSEvents_RecorderApp_Summarize,
  CrOSEvents_RecorderApp_SummaryModelDownloadPerf,
  CrOSEvents_RecorderApp_SummaryPerf,
  CrOSEvents_RecorderApp_TitleSuggestionPerf,
  CrOSEvents_RecorderApp_TranscriptionModelDownloadPerf,
  // Enums
  CrOSEvents_RecorderAppAudioFormat,
  CrOSEvents_RecorderAppMicrophoneType,
  CrOSEvents_RecorderAppModelFeedback,
  CrOSEvents_RecorderAppModelResultStatus,
  CrOSEvents_RecorderAppSpeakerLabelEnableState,
  CrOSEvents_RecorderAppSummaryEnableState,
  CrOSEvents_RecorderAppTranscriptFormat,
  CrOSEvents_RecorderAppTranscriptionEnableState,
  CrOSEvents_RecorderAppTranscriptionLocale,
} from 'chrome://resources/ash/common/metrics/structured_events.js';
import {
  record,
} from 'chrome://resources/ash/common/metrics/structured_metrics_service.js';

import {
  ChangePlaybackSpeedParams,
  ChangePlaybackVolumeParams,
  EventsSender as EventsSenderBase,
  ExportEventParams,
  FeedbackEventParams,
  isTranscriptionModelDownloadPerf,
  OnboardEventParams,
  PerfEvent,
  RecordEventParams,
  StartSessionEventParams,
  SuggestTitleEventParams,
  SummarizeEventParams,
} from '../../core/events_sender.js';
import {ModelResponseError} from '../../core/on_device_model/types.js';
import {LanguageCode} from '../../core/soda/language_info.js';
import {
  ExportAudioFormat,
  ExportSettings,
  ExportTranscriptionFormat,
  SpeakerLabelEnableState,
  SummaryEnableState,
  TranscriptionEnableState,
} from '../../core/state/settings.js';
import {assertExhaustive} from '../../core/utils/assert.js';

function getSpeakerLabelEnableState(
  transcriptionAvailable: boolean,
  speakerLabelState: SpeakerLabelEnableState,
): CrOSEvents_RecorderAppSpeakerLabelEnableState {
  if (!transcriptionAvailable) {
    return CrOSEvents_RecorderAppSpeakerLabelEnableState.NOT_AVAILABLE;
  }

  switch (speakerLabelState) {
    case SpeakerLabelEnableState.DISABLED:
      return CrOSEvents_RecorderAppSpeakerLabelEnableState.DISABLED;
    case SpeakerLabelEnableState.DISABLED_FIRST:
      return CrOSEvents_RecorderAppSpeakerLabelEnableState.DISABLED_FIRST;
    case SpeakerLabelEnableState.ENABLED:
      return CrOSEvents_RecorderAppSpeakerLabelEnableState.ENABLED;
    case SpeakerLabelEnableState.UNKNOWN:
      return CrOSEvents_RecorderAppSpeakerLabelEnableState.UNKNOWN;
    default:
      assertExhaustive(speakerLabelState);
  }
}

function getSummaryEnableState(
  featuresAvailable: boolean,
  state: SummaryEnableState,
): CrOSEvents_RecorderAppSummaryEnableState {
  if (!featuresAvailable) {
    return CrOSEvents_RecorderAppSummaryEnableState.NOT_AVAILABLE;
  }

  switch (state) {
    case SummaryEnableState.DISABLED:
      return CrOSEvents_RecorderAppSummaryEnableState.DISABLED;
    case SummaryEnableState.ENABLED:
      return CrOSEvents_RecorderAppSummaryEnableState.ENABLED;
    case SummaryEnableState.UNKNOWN:
      return CrOSEvents_RecorderAppSummaryEnableState.UNKNOWN;
    default:
      assertExhaustive(state);
  }
}

function getTranscriptionEnableState(
  transcriptionEnabled: boolean,
  state: TranscriptionEnableState,
): CrOSEvents_RecorderAppTranscriptionEnableState {
  if (!transcriptionEnabled) {
    return CrOSEvents_RecorderAppTranscriptionEnableState.NOT_AVAILABLE;
  }
  switch (state) {
    case TranscriptionEnableState.DISABLED:
      return CrOSEvents_RecorderAppTranscriptionEnableState.DISABLED;
    case TranscriptionEnableState.DISABLED_FIRST:
      return CrOSEvents_RecorderAppTranscriptionEnableState.DISABLED_FIRST;
    case TranscriptionEnableState.ENABLED:
      return CrOSEvents_RecorderAppTranscriptionEnableState.ENABLED;
    case TranscriptionEnableState.UNKNOWN:
      return CrOSEvents_RecorderAppTranscriptionEnableState.UNKNOWN;
    default:
      assertExhaustive(state);
  }
}

function convertToMicrophoneType(
  isInternalMicrophone: boolean,
): CrOSEvents_RecorderAppMicrophoneType {
  return isInternalMicrophone ? CrOSEvents_RecorderAppMicrophoneType.INTERNAL :
                                CrOSEvents_RecorderAppMicrophoneType.EXTERNAL;
}

function convertTranscriptionLocaleType(
  language: LanguageCode|null,
): CrOSEvents_RecorderAppTranscriptionLocale {
  switch (language) {
    case null:
      return CrOSEvents_RecorderAppTranscriptionLocale.NONE;
    case LanguageCode.EN_US:
      return CrOSEvents_RecorderAppTranscriptionLocale.EN_US;
    case LanguageCode.JA_JP:
      return CrOSEvents_RecorderAppTranscriptionLocale.JA_JP;
    case LanguageCode.DE_DE:
      return CrOSEvents_RecorderAppTranscriptionLocale.DE_DE;
    case LanguageCode.ES_ES:
      return CrOSEvents_RecorderAppTranscriptionLocale.ES_ES;
    case LanguageCode.FR_FR:
      return CrOSEvents_RecorderAppTranscriptionLocale.FR_FR;
    case LanguageCode.IT_IT:
      return CrOSEvents_RecorderAppTranscriptionLocale.IT_IT;
    case LanguageCode.EN_CA:
      return CrOSEvents_RecorderAppTranscriptionLocale.EN_CA;
    case LanguageCode.EN_AU:
      return CrOSEvents_RecorderAppTranscriptionLocale.EN_AU;
    case LanguageCode.EN_GB:
      return CrOSEvents_RecorderAppTranscriptionLocale.EN_GB;
    case LanguageCode.EN_IE:
      return CrOSEvents_RecorderAppTranscriptionLocale.EN_IE;
    case LanguageCode.EN_SG:
      return CrOSEvents_RecorderAppTranscriptionLocale.EN_SG;
    case LanguageCode.FR_BE:
      return CrOSEvents_RecorderAppTranscriptionLocale.FR_BE;
    case LanguageCode.FR_CH:
      return CrOSEvents_RecorderAppTranscriptionLocale.FR_CH;
    case LanguageCode.EN_IN:
      return CrOSEvents_RecorderAppTranscriptionLocale.EN_IN;
    case LanguageCode.IT_CH:
      return CrOSEvents_RecorderAppTranscriptionLocale.IT_CH;
    case LanguageCode.DE_AT:
      return CrOSEvents_RecorderAppTranscriptionLocale.DE_AT;
    case LanguageCode.DE_BE:
      return CrOSEvents_RecorderAppTranscriptionLocale.DE_BE;
    case LanguageCode.DE_CH:
      return CrOSEvents_RecorderAppTranscriptionLocale.DE_CH;
    case LanguageCode.ES_US:
      return CrOSEvents_RecorderAppTranscriptionLocale.ES_US;
    case LanguageCode.HI_IN:
      return CrOSEvents_RecorderAppTranscriptionLocale.HI_IN;
    case LanguageCode.PT_BR:
      return CrOSEvents_RecorderAppTranscriptionLocale.PT_BR;
    case LanguageCode.ID_ID:
      return CrOSEvents_RecorderAppTranscriptionLocale.ID_ID;
    case LanguageCode.KO_KR:
      return CrOSEvents_RecorderAppTranscriptionLocale.KO_KR;
    case LanguageCode.PL_PL:
      return CrOSEvents_RecorderAppTranscriptionLocale.PL_PL;
    case LanguageCode.TH_TH:
      return CrOSEvents_RecorderAppTranscriptionLocale.TH_TH;
    case LanguageCode.TR_TR:
      return CrOSEvents_RecorderAppTranscriptionLocale.TR_TR;
    case LanguageCode.ZH_CN:
      return CrOSEvents_RecorderAppTranscriptionLocale.ZH_CN;
    case LanguageCode.ZH_TW:
      return CrOSEvents_RecorderAppTranscriptionLocale.ZH_TW;
    case LanguageCode.DA_DK:
      return CrOSEvents_RecorderAppTranscriptionLocale.DA_DK;
    case LanguageCode.FR_CA:
      return CrOSEvents_RecorderAppTranscriptionLocale.FR_CA;
    case LanguageCode.NB_NO:
      return CrOSEvents_RecorderAppTranscriptionLocale.NB_NO;
    case LanguageCode.NL_NL:
      return CrOSEvents_RecorderAppTranscriptionLocale.NL_NL;
    case LanguageCode.SV_SE:
      return CrOSEvents_RecorderAppTranscriptionLocale.SV_SE;
    case LanguageCode.RU_RU:
      return CrOSEvents_RecorderAppTranscriptionLocale.RU_RU;
    case LanguageCode.VI_VN:
      return CrOSEvents_RecorderAppTranscriptionLocale.VI_VN;
    default:
      assertExhaustive(language);
  }
}

function convertToModelResultStatus(
  responseError: ModelResponseError|null,
): CrOSEvents_RecorderAppModelResultStatus {
  if (responseError === null) {
    return CrOSEvents_RecorderAppModelResultStatus.SUCCESS;
  }

  const {
    UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG,
    UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT,
  } = CrOSEvents_RecorderAppModelResultStatus;

  switch (responseError) {
    case ModelResponseError.GENERAL:
      return CrOSEvents_RecorderAppModelResultStatus.GENERAL_ERROR;
    case ModelResponseError.UNSAFE:
      return CrOSEvents_RecorderAppModelResultStatus.UNSAFE;
    case ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT:
      return UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT;
    case ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG:
      return UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG;
    case ModelResponseError.UNSUPPORTED_LANGUAGE:
      return CrOSEvents_RecorderAppModelResultStatus.UNSUPPORTED_LANGUAGE;
    default:
      assertExhaustive(responseError);
  }
}

function convertToAudioFormat(
  settings: ExportSettings,
): CrOSEvents_RecorderAppAudioFormat {
  if (!settings.audio) {
    return CrOSEvents_RecorderAppAudioFormat.NOT_EXPORTED;
  }

  switch (settings.audioFormat) {
    case ExportAudioFormat.WEBM_ORIGINAL:
      return CrOSEvents_RecorderAppAudioFormat.WEBM_ORIGINAL;
    default:
      assertExhaustive(settings.audioFormat);
  }
}

function convertToTranscriptFormat(
  settings: ExportSettings,
  transcriptionAvailable: boolean,
): CrOSEvents_RecorderAppTranscriptFormat {
  if (!transcriptionAvailable) {
    return CrOSEvents_RecorderAppTranscriptFormat.NOT_AVAILABLE;
  } else if (!settings.transcription) {
    return CrOSEvents_RecorderAppTranscriptFormat.NOT_EXPORTED;
  }

  switch (settings.transcriptionFormat) {
    case ExportTranscriptionFormat.TXT:
      return CrOSEvents_RecorderAppTranscriptFormat.TXT;
    default:
      assertExhaustive(settings.transcriptionFormat);
  }
}

function convertToModelFeedback(
  isPositive: boolean,
): CrOSEvents_RecorderAppModelFeedback {
  return isPositive ? CrOSEvents_RecorderAppModelFeedback.POSITIVE :
                      CrOSEvents_RecorderAppModelFeedback.NEGATIVE;
}

export class EventsSender extends EventsSenderBase {
  override sendStartSessionEvent({
    speakerLabelEnableState,
    summaryAvailable,
    summaryEnableState,
    titleSuggestionAvailable,
    transcriptionAvailable,
    transcriptionEnableState,
  }: StartSessionEventParams): void {
    const speakerLabel = getSpeakerLabelEnableState(
      transcriptionAvailable,
      speakerLabelEnableState,
    );

    const summary = getSummaryEnableState(
      summaryAvailable && titleSuggestionAvailable,
      summaryEnableState,
    );

    const transcription = getTranscriptionEnableState(
      transcriptionAvailable,
      transcriptionEnableState,
    );

    const event = new CrOSEvents_RecorderApp_StartSession()
                    .setSpeakerLabelEnableState(speakerLabel)
                    .setSummaryEnableState(summary)
                    .setTranscriptionEnableState(transcription)
                    .build();

    record(event);
  }

  override sendRecordEvent(params: RecordEventParams): void {
    const mic = convertToMicrophoneType(params.isInternalMicrophone);
    const locale = convertTranscriptionLocaleType(params.transcriptionLocale);
    const speakerLabel = getSpeakerLabelEnableState(
      params.transcriptionAvailable,
      params.speakerLabelEnableState,
    );
    const transcription = getTranscriptionEnableState(
      params.transcriptionAvailable,
      params.transcriptionEnableState,
    );

    const event = new CrOSEvents_RecorderApp_Record()
                    .setAudioDuration(BigInt(params.audioDuration))
                    .setEverMuted(BigInt(params.everMuted))
                    .setEverPaused(BigInt(params.everPaused))
                    .setIncludeSystemAudio(BigInt(params.includeSystemAudio))
                    .setMicrophoneType(mic)
                    .setRecordDuration(BigInt(params.recordDuration))
                    .setRecordingSaved(BigInt(params.recordingSaved))
                    .setSpeakerCount(BigInt(params.speakerCount))
                    .setSpeakerLabelEnableState(speakerLabel)
                    .setTranscriptionLabelEnableState(transcription)
                    .setTranscriptionLocale(locale)
                    .setWordCount(BigInt(params.wordCount))
                    .build();

    record(event);
  }

  override sendSuggestTitleEvent(params: SuggestTitleEventParams): void {
    const status = convertToModelResultStatus(params.responseError);
    const event =
      new CrOSEvents_RecorderApp_SuggestTitle()
        .setAcceptedSuggestionIndex(BigInt(params.acceptedSuggestionIndex))
        .setSuggestionAccepted(BigInt(params.suggestionAccepted))
        .setResultStatus(status)
        .setWordCount(BigInt(params.wordCount))
        .build();

    record(event);
  }

  override sendSummarizeEvent(params: SummarizeEventParams): void {
    const event =
      new CrOSEvents_RecorderApp_Summarize()
        .setResultStatus(convertToModelResultStatus(params.responseError))
        .setWordCount(BigInt(params.wordCount))
        .build();

    record(event);
  }

  override sendFeedbackTitleSuggestionEvent({
    isPositive,
  }: FeedbackEventParams): void {
    const event = new CrOSEvents_RecorderApp_FeedbackTitleSuggestion()
                    .setFeedback(convertToModelFeedback(isPositive))
                    .build();

    record(event);
  }

  override sendFeedbackSummaryEvent({isPositive}: FeedbackEventParams): void {
    const event = new CrOSEvents_RecorderApp_FeedbackSummary()
                    .setFeedback(convertToModelFeedback(isPositive))
                    .build();

    record(event);
  }

  override sendOnboardEvent(params: OnboardEventParams): void {
    const speakerLabel = getSpeakerLabelEnableState(
      params.transcriptionAvailable,
      params.speakerLabelEnableState,
    );
    const transcription = getTranscriptionEnableState(
      params.transcriptionAvailable,
      params.transcriptionEnableState,
    );

    const event = new CrOSEvents_RecorderApp_Onboard()
                    .setSpeakerLabelEnableState(speakerLabel)
                    .setTranscriptionEnableState(transcription)
                    .build();

    record(event);
  }

  override sendExportEvent(params: ExportEventParams): void {
    const {exportSettings, transcriptionAvailable} = params;
    const audioFormat = convertToAudioFormat(exportSettings);
    const transcriptFormat = convertToTranscriptFormat(
      exportSettings,
      transcriptionAvailable,
    );

    const event = new CrOSEvents_RecorderApp_Export()
                    .setAudioFormat(audioFormat)
                    .setTranscriptFormat(transcriptFormat)
                    .build();

    record(event);
  }

  override sendChangePlaybackSpeedEvent(
    params: ChangePlaybackSpeedParams,
  ): void {
    const event = new CrOSEvents_RecorderApp_ChangePlaybackSpeed()
                    .setPlaybackSpeed(params.playbackSpeed)
                    .build();

    record(event);
  }

  override sendChangePlaybackVolumeEvent(
    params: ChangePlaybackVolumeParams,
  ): void {
    const event = new CrOSEvents_RecorderApp_ChangePlaybackVolume()
                    .setMuted(BigInt(params.muted))
                    .setVolume(BigInt(params.volume))
                    .build();

    record(event);
  }

  override sendPerfEvent(event: PerfEvent, duration: number): void {
    if (isTranscriptionModelDownloadPerf(event)) {
      return this.sendTranscriptionModelDownloadPerf(
        duration,
        event.transcriptionLocale,
      );
    }
    const {kind} = event;
    switch (kind) {
      case 'appStart':
        return this.sendAppStartPerf(duration);
      case 'export':
        return this.sendExportPerf(duration, event.recordingSize);
      case 'record':
        return this.sendRecordingSavingPerf(
          duration,
          event.audioDuration,
          event.wordCount,
        );
      case 'summary':
        return this.sendSummaryPerf(duration, event.wordCount);
      case 'summaryModelDownload':
        return this.sendSummaryModelDownloadPerf(duration);
      case 'titleSuggestion':
        return this.sendTitleSuggestionPerf(duration, event.wordCount);
      default:
        assertExhaustive(kind);
    }
  }

  private sendAppStartPerf(duration: number): void {
    const dur = BigInt(duration);
    const event =
      new CrOSEvents_RecorderApp_AppStartPerf().setDuration(dur).build();

    record(event);
  }

  private sendTranscriptionModelDownloadPerf(
    duration: number,
    language: LanguageCode,
  ): void {
    const event =
      new CrOSEvents_RecorderApp_TranscriptionModelDownloadPerf()
        .setDuration(BigInt(duration))
        .setTranscriptionLocale(convertTranscriptionLocaleType(language))
        .build();

    record(event);
  }

  private sendSummaryModelDownloadPerf(duration: number): void {
    const event = new CrOSEvents_RecorderApp_SummaryModelDownloadPerf()
                    .setDuration(BigInt(duration))
                    .build();

    record(event);
  }

  private sendExportPerf(duration: number, recordingSize: number): void {
    const event = new CrOSEvents_RecorderApp_ExportPerf()
                    .setDuration(BigInt(duration))
                    .setRecordingSize(BigInt(recordingSize))
                    .build();

    record(event);
  }

  private sendRecordingSavingPerf(
    duration: number,
    audioDuration: number,
    wordCount: number,
  ): void {
    const event = new CrOSEvents_RecorderApp_RecordingSavingPerf()
                    .setAudioDuration(BigInt(audioDuration))
                    .setDuration(BigInt(duration))
                    .setWordCount(BigInt(wordCount))
                    .build();

    record(event);
  }

  private sendTitleSuggestionPerf(duration: number, wordCount: number): void {
    const event = new CrOSEvents_RecorderApp_TitleSuggestionPerf()
                    .setDuration(BigInt(duration))
                    .setWordCount(BigInt(wordCount))
                    .build();

    record(event);
  }

  private sendSummaryPerf(duration: number, wordCount: number): void {
    const event = new CrOSEvents_RecorderApp_SummaryPerf()
                    .setDuration(BigInt(duration))
                    .setWordCount(BigInt(wordCount))
                    .build();

    record(event);
  }
}
