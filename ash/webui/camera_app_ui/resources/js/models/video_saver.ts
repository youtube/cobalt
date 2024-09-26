// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from '../assert.js';
import {Intent} from '../intent.js';
import * as Comlink from '../lib/comlink.js';
import {
  MimeType,
  Resolution,
} from '../type.js';
import {getVideoProcessorHelper} from '../untrusted_scripts.js';
import {WaitableEvent} from '../waitable_event.js';

import {AsyncWriter} from './async_writer.js';
import {
  VideoProcessor,
  VideoProcessorConstructor,
} from './ffmpeg/video_processor.js';
import {
  createGifArgs,
  createMp4Args,
  createTimeLapseArgs,
} from './ffmpeg/video_processor_args.js';
import {createPrivateTempVideoFile} from './file_system.js';
import {FileAccessEntry} from './file_system_access_entry.js';

// This is used like a class constructor.
// eslint-disable-next-line @typescript-eslint/naming-convention
const FFMpegVideoProcessor = (async () => {
  const workerChannel = new MessageChannel();
  const videoProcessorHelper = await getVideoProcessorHelper();
  await videoProcessorHelper.connectToWorker(
      Comlink.transfer(workerChannel.port2, [workerChannel.port2]));
  return Comlink.wrap<VideoProcessorConstructor>(workerChannel.port1);
})();


/**
 * Creates a VideoProcessor instance for recording video.
 */
async function createVideoProcessor(output: AsyncWriter, videoRotation: number):
    Promise<Comlink.Remote<VideoProcessor>> {
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createMp4Args(videoRotation, output.seekable()));
}

/**
 * Creates a VideoProcessor instance for recording gif.
 */
async function createGifVideoProcessor(
    output: AsyncWriter,
    resolution: Resolution): Promise<Comlink.Remote<VideoProcessor>> {
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createGifArgs(resolution));
}

/*
 * Creates a VideoProcessor instance for recording time-lapse.
 */
async function createTimeLapseProcessor(
    output: AsyncWriter, resolution: Resolution,
    fps: number): Promise<Comlink.Remote<VideoProcessor>> {
  return new (await FFMpegVideoProcessor)(
      Comlink.proxy(output), createTimeLapseArgs(resolution, fps));
}

/**
 * Creates an AsyncWriter that writes to the given intent.
 */
function createWriterForIntent(intent: Intent): AsyncWriter {
  async function write(blob: Blob) {
    await intent.appendData(new Uint8Array(await blob.arrayBuffer()));
  }
  // TODO(crbug.com/1140852): Supports seek.
  return new AsyncWriter({write, seek: null, close: null});
}

/**
 * Used to save captured video.
 */
export class VideoSaver {
  constructor(
      private readonly file: FileAccessEntry,
      private readonly processor: Comlink.Remote<VideoProcessor>) {}

  /**
   * Writes video data to result video.
   */
  async write(blob: Blob): Promise<void> {
    await this.processor.write(blob);
  }

  /**
   * Cancels and drops all the written video data.
   */
  async cancel(): Promise<void> {
    await this.processor.cancel();
    return this.file.remove();
  }

  /**
   * Finishes the write of video data parts and returns result video file.
   *
   * @return Result video file.
   */
  async endWrite(): Promise<FileAccessEntry> {
    await this.processor.close();
    return this.file;
  }

  /**
   * Creates video saver which saves video into a temporary file.
   * TODO(b/184583382): Saves to the target file directly once the File System
   * Access API supports cleaning temporary file when leaving the page without
   * closing the file stream.
   */
  static async create(videoRotation: number): Promise<VideoSaver> {
    const file = await createPrivateTempVideoFile();
    const writer = await file.getWriter();
    const processor = await createVideoProcessor(writer, videoRotation);
    return new VideoSaver(file, processor);
  }

  /**
   * Creates video saver for the given intent.
   */
  static async createForIntent(intent: Intent, videoRotation: number):
      Promise<VideoSaver> {
    const file = await createPrivateTempVideoFile();
    const fileWriter = await file.getWriter();
    const intentWriter = createWriterForIntent(intent);
    const writer = AsyncWriter.combine(fileWriter, intentWriter);
    const processor = await createVideoProcessor(writer, videoRotation);
    return new VideoSaver(file, processor);
  }
}

/**
 * Used to save captured gif.
 */
export class GifSaver {
  constructor(
      private readonly blobs: Blob[],
      private readonly processor: Comlink.Remote<VideoProcessor>) {}

  async write(frame: Uint8ClampedArray): Promise<void> {
    await this.processor.write(new Blob([frame]));
  }

  /**
   * Finishes the write of gif data parts and returns result gif blob.
   */
  async endWrite(): Promise<Blob> {
    await this.processor.close();
    return new Blob(this.blobs, {type: MimeType.GIF});
  }

  /**
   * Creates video saver for the given file.
   */
  static async create(resolution: Resolution): Promise<GifSaver> {
    const blobs: Blob[] = [];
    const writer = new AsyncWriter({
      write(blob) {
        blobs.push(blob);
      },
      seek: null,
      close: null,
    });
    const processor = await createGifVideoProcessor(writer, resolution);
    return new GifSaver(blobs, processor);
  }
}

class TimeLapseFixedSpeedSaver {
  private maxWrittenFrame: number|null = null;

  constructor(
      readonly speed: number, readonly file: FileAccessEntry,
      private readonly processor: Comlink.Remote<VideoProcessor>) {}

  write(blob: Blob, frameNo: number): void {
    this.processor.write(blob);
    this.maxWrittenFrame = frameNo;
  }

  getNextFrame(): number {
    return this.maxWrittenFrame === null ? 0 :
                                           this.maxWrittenFrame + this.speed;
  }

  includeFrameNo(frameNo: number): boolean {
    return frameNo % this.speed === 0;
  }

  async cancel(): Promise<void> {
    await this.processor.cancel();
    return this.file.remove();
  }

  async endWrite(): Promise<void> {
    await this.processor.close();
  }

  static async create(speed: number, resolution: Resolution, fps: number):
      Promise<TimeLapseFixedSpeedSaver> {
    const file = await createPrivateTempVideoFile(`tmp-video-${speed}x.mp4`);
    const writer = await file.getWriter();
    const processor = await createTimeLapseProcessor(writer, resolution, fps);
    return new TimeLapseFixedSpeedSaver(speed, file, processor);
  }
}

/**
 * Maximum duration for the time-lapse video in seconds.
 */
const TIME_LAPSE_MAX_DURATION = 30;

/**
 * Default number of fps in case it's not defined from the original video.
 */
const TIME_LAPSE_DEFAULT_FRAME_RATE = 30;

/**
 * Time interval to repeatedly call |manageSavers|.
 */
const SAVER_MANAGER_TIMEOUT_MS = 100;

/**
 * Used to save time-lapse video.
 */
export class TimeLapseSaver {
  /**
   * Video encoder used to encode frame.
   */
  private readonly encoder: VideoEncoder;

  /**
   * Map a frame's timestamp with frameNo, only store frame that's being
   * encoded.
   */
  private readonly frameNoMap = new Map<number, number>();

  /**
   * Store all encoded frames with its frame numbers.
   * TODO(b/236800499): Investigate if it is OK to store number of blobs in
   * memory.
   */
  private readonly frames = new Map<number, Blob>();

  /**
   * Max frameNo that has been encoded and stored so far.
   */
  private maxFrameNo = -1;

  /**
   * Whether the saving is canceled.
   */
  private canceled = false;

  /**
   * Whether the saving is ended because users stop recording.
   */
  private ended = false;

  /**
   * A waitable event which resolves when the saver finishes saving/canceling.
   */
  private readonly onFinished = new WaitableEvent();

  // These variables are assigned in |init| function, which is called after the
  // constructor in |TimeLapseSaver.create| function. Since it is the only way
  // to construct the saver, it is assured that these will never be undefined.
  /**
   * Saver for the video of current speed. The resulting file from this saver
   * will be the final result, unless the speed is updated and |nextSpeedSaver|
   * replaces it.
   */
  private currSpeedSaver!: TimeLapseFixedSpeedSaver;

  /**
   * Saver for the video of the next speed.
   */
  private nextSpeedSaver!: TimeLapseFixedSpeedSaver;

  /**
   * Maximum frameNo for the current speed. If |maxFrameWritten| exceeds this
   * number, the speed needs to be updated.
   */
  private speedCheckpoint!: number;

  /**
   * Initial time lapse speed.
   */
  private initialSpeed!: number;

  private constructor(
      encoderConfig: VideoEncoderConfig,
      private readonly resolution: Resolution, private readonly fps: number) {
    this.encoder = new VideoEncoder({
      error: (error) => {
        throw error;
      },
      output: (chunk) => this.onFrameEncoded(chunk),
    });
    this.encoder.configure(encoderConfig);
  }

  /**
   * Initializes the saver with the given initial speed.
   */
  private async init(speed: number): Promise<void> {
    this.initialSpeed = speed;
    this.currSpeedSaver = await this.createSaver(speed);
    this.nextSpeedSaver = await this.createSaver(this.getNextSpeed(speed));
    this.speedCheckpoint = speed * TIME_LAPSE_MAX_DURATION * this.fps;
    setTimeout(() => this.manageSavers(), SAVER_MANAGER_TIMEOUT_MS);
  }

  /**
   * Callback to be called when the frame is encoded. Converts an encoded chunk
   * to Blob and stores with its frame number.
   */
  onFrameEncoded(chunk: EncodedVideoChunk): void {
    const frameNo = this.frameNoMap.get(chunk.timestamp);
    assert(frameNo !== undefined);
    const chunkData = new Uint8Array(chunk.byteLength);
    chunk.copyTo(chunkData);
    this.frames.set(frameNo, new Blob([chunkData]));
    this.maxFrameNo = frameNo;
    this.frameNoMap.delete(chunk.timestamp);
  }

  /**
   * Sends the frame to the encoder.
   */
  write(frame: VideoFrame, frameNo: number): void {
    if (!frame.timestamp || this.ended || this.canceled) {
      return;
    }
    this.frameNoMap.set(frame.timestamp, frameNo);
    // Frames that are only in the initial speed video don't have to be encoded
    // as key frames because they'll be dropped soon.
    const keyFrame = frameNo % (this.initialSpeed * 2) === 0;
    this.encoder.encode(frame, {keyFrame});
  }

  /**
   * Stops the encoder by flushing and closing it.
   */
  async closeEncoder(): Promise<void> {
    await this.encoder.flush();
    this.encoder.close();
  }

  /**
   * Finishes the write of video and returns result video file.
   */
  async endWrite(): Promise<FileAccessEntry> {
    this.ended = true;
    await this.closeEncoder();
    await this.onFinished.wait();
    return this.currSpeedSaver.file;
  }

  /**
   * Cancels the write of video.
   */
  async cancel(): Promise<void> {
    this.canceled = true;
    await this.closeEncoder();
    await this.onFinished.wait();
  }

  /**
   * Gets current or final time-lapse video speed.
   */
  get speed(): number {
    return this.currSpeedSaver.speed;
  }

  /**
   * Writes the next frame, if exists, to the saver. Returns a boolean
   * indicating if there are more frames to be written.
   */
  private writeNextFrame(saver: TimeLapseFixedSpeedSaver): boolean {
    const frameNo = saver.getNextFrame();
    if (frameNo > this.maxFrameNo) {
      return true;
    }
    const blob = this.frames.get(frameNo);
    saver.write(assertInstanceof(blob, Blob), frameNo);
    return frameNo >= this.maxFrameNo;
  }

  /**
   * Updates savers and drops unused frames according to the new speed.
   */
  private async updateSpeed(): Promise<void> {
    // Updates savers and next speed checkpoint.
    await this.currSpeedSaver.cancel();
    this.currSpeedSaver = this.nextSpeedSaver;

    const speed = this.currSpeedSaver.speed;
    this.nextSpeedSaver = await this.createSaver(this.getNextSpeed(speed));
    this.speedCheckpoint = speed * TIME_LAPSE_MAX_DURATION * this.fps;

    // Drops unused frames.
    for (const frameNo of Array.from(this.frames.keys())) {
      if (!this.currSpeedSaver.includeFrameNo(frameNo) &&
          !this.nextSpeedSaver.includeFrameNo(frameNo)) {
        this.frames.delete(frameNo);
      }
    }
  }

  /**
   * Manages initializing (of the new savers), writing, ending, and canceling of
   * |TimeLapseFixedSpeedSaver|. Most operations except encoding are supposed to
   * do here to avoid race conditions.
   */
  private async manageSavers(): Promise<void> {
    if (this.ended) {
      await this.nextSpeedSaver.cancel();
      let done = false;
      while (!done) {
        done = this.writeNextFrame(this.currSpeedSaver);
      }
      await this.currSpeedSaver.endWrite();
    } else if (this.canceled) {
      await Promise.all([
        this.currSpeedSaver.cancel(),
        this.nextSpeedSaver.cancel(),
      ]);
    } else {
      this.writeNextFrame(this.currSpeedSaver);
      this.writeNextFrame(this.nextSpeedSaver);
      if (this.maxFrameNo >= this.speedCheckpoint) {
        await this.updateSpeed();
      }
    }

    // Repeatedly call this function until the saver is ended/canceled.
    if (this.ended || this.canceled) {
      this.onFinished.signal();
    } else {
      setTimeout(() => this.manageSavers(), SAVER_MANAGER_TIMEOUT_MS);
    }
  }

  /**
   * Returns the time-lapse speed after the given speed.
   */
  getNextSpeed(speed: number): number {
    return speed * 2;
  }

  /**
   * Creates a saver for the specified speed.
   */
  async createSaver(speed: number): Promise<TimeLapseFixedSpeedSaver> {
    return TimeLapseFixedSpeedSaver.create(speed, this.resolution, this.fps);
  }

  /**
   * Creates video saver with encoder using provided |encoderConfig|.
   */
  static async create(
      encoderConfig: VideoEncoderConfig, resolution: Resolution, fps: number,
      speed: number): Promise<TimeLapseSaver> {
    const encoderSupport = await VideoEncoder.isConfigSupported(encoderConfig);
    if (!encoderSupport.supported) {
      throw new Error('Video encoder is not supported.');
    }

    fps = fps > 0 ? fps : TIME_LAPSE_DEFAULT_FRAME_RATE;
    const saver = new TimeLapseSaver(encoderConfig, resolution, fps);
    await saver.init(speed);
    return saver;
  }
}
