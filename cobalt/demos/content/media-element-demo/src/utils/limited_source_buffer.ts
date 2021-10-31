import {DownloadBuffer} from './download_buffer';
import {Media} from './media';

const UNPLAYED_BUFFER_MAX_SIZE = 10;  // Only buffer 10 seconds maximum.

/**
 * A source buffer wrapper that limits the array buffers that can be added to
 * the source buffer. If the bufferred length exceeds the limit, it will
 * automatically pauses and waits until the remaining playback time is below the
 * limit.
 */
export class LimitedSourceBuffer {
  private scheduleTimeoutId = -1;
  private currentMediaIndex = 0;

  constructor(
      private readonly videoEl: HTMLVideoElement,
      private readonly sourceBuffer: SourceBuffer,
      private readonly mediaList: Media[],
      private readonly downloadBuffer: DownloadBuffer) {
    this.appendNext();
  }

  /** Appends the next array buffer to the source buffer. */
  async appendNext() {
    const unplayedBufferLength = this.unplayedBufferLength();
    if (unplayedBufferLength > UNPLAYED_BUFFER_MAX_SIZE) {
      // Buffered too many content. Wait until we need to add buffer again.
      const timeout = unplayedBufferLength - UNPLAYED_BUFFER_MAX_SIZE;
      clearTimeout(this.scheduleTimeoutId);
      console.log(`Schedule appendNext in ${timeout} sec.`);
      this.scheduleTimeoutId =
          setTimeout(() => this.appendNext(), timeout * 1000);
      return;
    }
    const media = this.mediaList[this.currentMediaIndex];
    if (!media) {
      // All media are appended.
      console.log('All media are appended');
      return;
    }
    const chunk = await this.downloadBuffer.shift(media);
    if (!chunk) {
      // No more buffer left to append in the current media. Move on to the next
      // one.
      console.log(`${media.url} finished appending. Move on to the next one.`);
      this.currentMediaIndex++;
      const bufferedLength = this.sourceBuffer.buffered.end(0);
      this.sourceBuffer.timestampOffset = bufferedLength;
      this.appendNext();
      return;
    }
    console.log(`Appending new chunk from ${media.url}`);
    await this.appendChunkToBuffer(chunk);
    this.appendNext();
  }

  /**
   * Appends a chunk to the source buffer. Waits until the update is completed.
   */
  private async appendChunkToBuffer(chunk: ArrayBuffer) {
    return new Promise((resolve, reject) => {
      this.sourceBuffer.appendBuffer(chunk);
      const unsubscribe = () => {
        this.sourceBuffer.removeEventListener('updateend', onupdateend);
        this.sourceBuffer.removeEventListener('error', onerror);
      };
      const onerror = () => {
        unsubscribe();
        reject(new Error('Append to buffer failed.'));
      };
      const onupdateend = () => {
        unsubscribe();
        resolve();
      };
      this.sourceBuffer.addEventListener('updateend', onupdateend);
      this.sourceBuffer.addEventListener('error', onerror);
    });
  }

  private unplayedBufferLength() {
    if (this.sourceBuffer.buffered.length === 0) {
      return 0;
    }
    return this.sourceBuffer.buffered.end(0) - this.videoEl.currentTime;
  }
}
