// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
  assertString,
} from '../assert.js';
import * as error from '../error.js';
import * as expert from '../expert.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as state from '../state.js';
import {
  ErrorLevel,
  ErrorType,
  Facing,
  Mode,
  Resolution,
} from '../type.js';
import * as util from '../util.js';
import {CancelableEvent, WaitableEvent} from '../waitable_event.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';
import {
  CaptureCandidate,
  FakeCameraCaptureCandidate,
} from './capture_candidate.js';
import {CaptureCandidatePreferrer} from './capture_candidate_preferrer.js';
import {Modes, Video} from './mode/index.js';
import {Preview} from './preview.js';
import {StreamConstraints} from './stream_constraints.js';
import {StreamManager} from './stream_manager.js';
import {
  CameraConfig,
  CameraConfigCandidate,
  CameraInfo,
  CameraViewUI,
  ModeConstraints,
} from './type.js';


interface ConfigureCandidate {
  deviceId: string;
  mode: Mode;
  captureCandidate: CaptureCandidate;
  constraints: StreamConstraints;
  videoSnapshotResolution: Resolution|null;
}

export interface EventListener {
  onTryingNewConfig(config: CameraConfigCandidate): void;
  onUpdateConfig(config: CameraConfig): Promise<void>;
  onUpdateCapability(cameraInfo: CameraInfo): void;
}

/**
 * Controller for closing or opening camera with specific |CameraConfig|.
 */
class Reconfigurer {
  /**
   * Preferred configuration.
   */
  config: CameraConfig|null = null;

  private readonly initialFacing: Facing|null;

  private readonly initialMode: Mode;

  private shouldSuspend = false;

  readonly capturePreferrer = new CaptureCandidatePreferrer();

  constructor(
      private readonly preview: Preview,
      private readonly modes: Modes,
      private readonly listener: EventListener,
      private readonly modeConstraints: ModeConstraints,
      facing: Facing|null,
  ) {
    this.initialMode = modeConstraints.mode;
    this.initialFacing = facing;
  }

  setShouldSuspend(value: boolean) {
    this.shouldSuspend = value;
  }

  /**
   * Gets the video device ids sorted by preference.
   */
  private getDeviceIdCandidates(cameraInfo: CameraInfo): string[] {
    let devices: Array<Camera3DeviceInfo|MediaDeviceInfo>;
    /**
     * Object mapping from device id to facing. Set to null for fake cameras.
     */
    let facings: Record<string, Facing>|null = null;

    const camera3Info = cameraInfo.camera3DevicesInfo;
    if (camera3Info !== null) {
      devices = camera3Info;
      facings = {};
      for (const {deviceId, facing} of camera3Info) {
        facings[deviceId] = facing;
      }
    } else {
      devices = cameraInfo.devicesInfo;
    }

    const preferredFacing =
        this.config?.facing ?? this.initialFacing ?? util.getDefaultFacing();
    // Put the selected video device id first.
    const sorted = devices.map((device) => device.deviceId).sort((a, b) => {
      if (a === b) {
        return 0;
      }
      if (this.config !== null ? a === this.config.deviceId :
                                 (facings && facings[a] === preferredFacing)) {
        return -1;
      }
      return 1;
    });
    return sorted;
  }

  private async getModeCandidates(deviceId: string): Promise<Mode[]> {
    if (this.modeConstraints.kind === 'exact') {
      assert(await this.modes.isSupported(this.modeConstraints.mode, deviceId));
      return [this.modeConstraints.mode];
    }
    return this.modes.getModeCandidates(
        deviceId, this.config?.mode ?? this.initialMode);
  }

  private async *
      getConfigurationCandidates(cameraInfo: CameraInfo):
          AsyncIterable<ConfigureCandidate> {
    const deviceOperator = DeviceOperator.getInstance();
    const hasAudio = await this.isAudioInputAvailable();

    for (const deviceId of this.getDeviceIdCandidates(cameraInfo)) {
      for (const mode of await this.getModeCandidates(deviceId)) {
        let candidates: CaptureCandidate[];
        let photoResolutions;
        if (deviceOperator !== null) {
          assert(cameraInfo.camera3DevicesInfo !== null);
          candidates = this.capturePreferrer.getSortedCandidates(
              cameraInfo.camera3DevicesInfo, deviceId, mode, hasAudio);
          photoResolutions = await deviceOperator.getPhotoResolutions(deviceId);
        } else {
          candidates = [new FakeCameraCaptureCandidate(
              deviceId, mode === Mode.VIDEO, hasAudio)];
          photoResolutions = candidates.map((c) => c.resolution);
        }
        const maxResolution = photoResolutions.reduce(
            (maxR, r) =>
                r !== null && (maxR === null || r.area > maxR.area) ? r : maxR);
        for (const c of candidates) {
          const videoSnapshotResolution =
              expert.isEnabled(
                  expert.ExpertOption.ENABLE_FULL_SIZED_VIDEO_SNAPSHOT) ?
              maxResolution :
              c.resolution;
          for (const constraints of c.getStreamConstraintsCandidates()) {
            yield {
              deviceId,
              mode,
              captureCandidate: c,
              constraints,
              videoSnapshotResolution,
            };
          }
        }
      }
    }
  }

  private async isAudioInputAvailable(): Promise<boolean> {
    const devices = await navigator.mediaDevices.enumerateDevices();
    return devices.some((device) => device.kind === 'audioinput');
  }

  /**
   * Checks if PTZ can be enabled.
   */
  private async checkEnablePTZ(c: ConfigureCandidate): Promise<void> {
    const enablePTZ = await (async () => {
      if (!this.preview.isSupportPTZ()) {
        return false;
      }
      const modeSupport = state.get(state.State.USE_FAKE_CAMERA) ||
          (c.captureCandidate.resolution !== null &&
           this.modes.isSupportPTZ(
               c.mode,
               c.captureCandidate.resolution,
               this.preview.getResolution(),
               ));
      if (!modeSupport) {
        await this.preview.resetPTZ();
        return false;
      }
      return true;
    })();
    state.set(state.State.ENABLE_PTZ, enablePTZ);
  }

  async start(cameraInfo: CameraInfo): Promise<boolean> {
    await this.stopStreams();
    return this.startConfigure(cameraInfo);
  }

  /**
   * @return If the reconfiguration finished successfully.
   */
  async startConfigure(cameraInfo: CameraInfo): Promise<boolean> {
    if (this.shouldSuspend) {
      return false;
    }

    const deviceOperator = DeviceOperator.getInstance();
    state.set(state.State.USE_FAKE_CAMERA, deviceOperator === null);

    for await (const c of this.getConfigurationCandidates(cameraInfo)) {
      if (this.shouldSuspend) {
        return false;
      }

      let facing = c.deviceId !== null ?
          cameraInfo.getCamera3DeviceInfo(c.deviceId)?.facing ?? null :
          null;
      this.listener.onTryingNewConfig({
        deviceId: c.deviceId,
        facing,
        mode: c.mode,
        captureCandidate: c.captureCandidate,
      });
      this.modes.setCaptureParams(
          c.mode, c.constraints, c.captureCandidate.resolution,
          c.videoSnapshotResolution);
      try {
        await this.modes.prepareDevice();
        const factory = this.modes.getModeFactory(c.mode);
        await this.preview.open(c.constraints);
        // For non-ChromeOS VCD, the facing and device id can only be known
        // after preview is actually opened.
        facing = this.preview.getFacing();
        const deviceId = assertString(this.preview.getDeviceId());

        await this.checkEnablePTZ(c);
        factory.setPreviewVideo(this.preview.getVideo());
        factory.setFacing(facing);
        await this.modes.updateMode(factory);
        this.config = {
          deviceId,
          facing,
          mode: c.mode,
          captureCandidate: c.captureCandidate,
        };
        if (this.config.mode === Mode.VIDEO) {
          const fps = this.config.captureCandidate.getConstFps();
          state.set(state.State.FPS_30, fps === 30);
          state.set(state.State.FPS_60, fps === 60);
        }
        this.capturePreferrer.onUpdateConfig(this.config);
        await this.listener.onUpdateConfig(this.config);

        return true;
      } catch (e) {
        await this.stopStreams();

        let errorToReport: Error;
        // Since OverconstrainedError is not an Error instance.
        if (e instanceof OverconstrainedError) {
          if (facing === Facing.EXTERNAL && e.constraint === 'deviceId') {
            // External camera configuration failed with OverconstrainedError
            // of deviceId means that the device is no longer available and is
            // likely caused by external camera disconnected. Ignore this
            // error.
            continue;
          }
          errorToReport =
              new Error(`${e.message} (constraint = ${e.constraint})`);
          errorToReport.name = 'OverconstrainedError';
        } else {
          assert(e instanceof Error);
          if (e.name === 'NotReadableError') {
            // TODO(b/187879603): Remove this hacked once we understand more
            // about such error.
            // We cannot get the camera facing from stream since it might
            // not be successfully opened. Therefore, we asked the camera
            // facing via Mojo API.
            let facing: Facing|null = null;
            if (deviceOperator !== null) {
              facing = await deviceOperator.getCameraFacing(c.deviceId);
            }
            errorToReport = new Error(`${e.message} (facing = ${facing})`);
            errorToReport.name = 'NotReadableError';
          } else {
            errorToReport = e;
          }
        }
        error.reportError(
            ErrorType.START_CAMERA_FAILURE, ErrorLevel.ERROR, errorToReport);
      }
    }
    return false;
  }

  /**
   * Stop extra stream and preview stream.
   */
  private async stopStreams() {
    await this.modes.clear();
    await this.preview.close();
  }
}

class Capturer {
  constructor(private readonly modes: Modes) {}

  async start(): Promise<[Promise<void>]> {
    assert(this.modes.current !== null);
    return this.modes.current.startCapture();
  }

  stop() {
    assert(this.modes.current !== null);
    this.modes.current.stopCapture();
  }

  takeVideoSnapshot() {
    if (this.modes.current instanceof Video) {
      this.modes.current.takeSnapshot();
    }
  }

  toggleVideoRecordingPause() {
    if (this.modes.current instanceof Video) {
      this.modes.current.togglePaused();
    }
  }
}

enum OperationType {
  CAPTURE = 'capture',
  RECONFIGURE = 'reconfigure',
}

export class OperationScheduler {
  cameraInfo: CameraInfo|null = null;

  private pendingUpdateInfo: CameraInfo|null = null;

  private readonly firstInfoUpdate = new WaitableEvent();

  readonly reconfigurer: Reconfigurer;

  readonly capturer: Capturer;

  readonly modes = new Modes();

  private ongoingOperationType: OperationType|null = null;

  private pendingReconfigureWaiters: Array<CancelableEvent<boolean>> = [];

  constructor(
      private readonly listener: EventListener,
      preview: Preview,
      defaultFacing: Facing|null,
      modeConstraints: ModeConstraints,
  ) {
    this.reconfigurer = new Reconfigurer(
        preview,
        this.modes,
        listener,
        modeConstraints,
        defaultFacing,
    );
    this.capturer = new Capturer(this.modes);
    StreamManager.getInstance().addRealDeviceChangeListener((devices) => {
      const info = new CameraInfo(devices);
      if (this.ongoingOperationType !== null) {
        this.pendingUpdateInfo = info;
        return;
      }
      this.doUpdate(info);
    });
  }

  async initialize(cameraViewUI: CameraViewUI): Promise<void> {
    this.modes.initialize(cameraViewUI);
    await StreamManager.getInstance().deviceUpdate();
    await this.firstInfoUpdate.wait();
  }

  private doUpdate(cameraInfo: CameraInfo) {
    const isFirstUpdate = this.cameraInfo === null;
    this.cameraInfo = cameraInfo;
    if (cameraInfo.camera3DevicesInfo !== null) {
      this.reconfigurer.capturePreferrer.updateCapability(
          cameraInfo.camera3DevicesInfo);
    }
    this.listener.onUpdateCapability(cameraInfo);
    if (isFirstUpdate) {
      this.firstInfoUpdate.signal();
    }
  }

  async reconfigure(): Promise<boolean> {
    if (this.ongoingOperationType !== null) {
      const event = new CancelableEvent<boolean>();
      this.pendingReconfigureWaiters.push(event);
      this.stopCapture();
      return event.wait();
    }
    return this.startReconfigure();
  }

  takeVideoSnapshot(): void {
    if (this.ongoingOperationType === OperationType.CAPTURE) {
      this.capturer.takeVideoSnapshot();
    }
  }

  toggleVideoRecordingPause(): void {
    if (this.ongoingOperationType === OperationType.CAPTURE) {
      this.capturer.toggleVideoRecordingPause();
    }
  }

  private clearPendingReconfigureWaiters() {
    for (const waiter of this.pendingReconfigureWaiters) {
      waiter.signal(false);
    }
    this.pendingReconfigureWaiters = [];
  }

  private finishOperation(): void {
    this.ongoingOperationType = null;

    // Check pending operations.
    if (this.pendingUpdateInfo !== null) {
      this.doUpdate(this.pendingUpdateInfo);
      this.pendingUpdateInfo = null;
    }
    if (this.pendingReconfigureWaiters.length !== 0) {
      const starting = this.startReconfigure();
      for (const waiter of this.pendingReconfigureWaiters) {
        waiter.signalAs(starting);
      }
      this.pendingReconfigureWaiters = [];
    }
  }

  async startCapture(): Promise<[Promise<void>]|null> {
    if (this.ongoingOperationType !== null) {
      return null;
    }
    this.ongoingOperationType = OperationType.CAPTURE;

    try {
      return await this.capturer.start();
    } finally {
      this.finishOperation();
    }
  }

  stopCapture(): void {
    if (this.ongoingOperationType === OperationType.CAPTURE) {
      this.capturer.stop();
    }
  }

  private async startReconfigure(): Promise<boolean> {
    assert(this.ongoingOperationType === null);
    this.ongoingOperationType = OperationType.RECONFIGURE;

    const cameraInfo = assertInstanceof(this.cameraInfo, CameraInfo);
    try {
      const succeed = await this.reconfigurer.start(cameraInfo);
      if (!succeed) {
        this.clearPendingReconfigureWaiters();
      }
      return succeed;
    } catch (e) {
      this.clearPendingReconfigureWaiters();
      throw e;
    } finally {
      this.finishOperation();
    }
  }
}
