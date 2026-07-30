import {download} from './downloader';
import {Media} from './media';

const CHUNK_SIZE = 1024 * 1024;  // 1MB
const DOWNLOAD_BUFFER_MAX_SIZE = 10;

type DownloadBufferListener = (results: Map<Media, MediaDownloaderStatus>) =>
    void;

/**
 * Download buffer fetches the passed in media list chunk by chunk. It
 * automatically pauses the download when the buffer reaches the limit and
 * resumes when a chunk is taken out from the buffer.
 */
export class DownloadBuffer {
  private isDownloading = false;
  private currentMediaIndex = 0;
  private listeners: DownloadBufferListener[] = [];
  private downloaderMap = new Map<Media, MediaDownloader>();

  constructor(
      private readonly mediaList: Media[],
      private readonly limit = DOWNLOAD_BUFFER_MAX_SIZE) {
    for (const media of mediaList) {
      this.downloaderMap.set(media, new MediaDownloader(media));
    }
    this.download();
  }

  /** Registers callback to listen to download buffer changes. */
  register(callback: DownloadBufferListener) {
    this.listeners.push(callback);
    return () => {
      const index = this.listeners.indexOf(callback);
      if (index !== -1) {
        this.listeners.splice(index, 1);
      }
    }
  }

  /** Calls listeners with the current media downloaders' status. */
  private reportChanges() {
    const result = new Map<Media, MediaDownloaderStatus>();
    this.downloaderMap.forEach((downloader) => {
      result.set(downloader.media, downloader.status());
    });
    for (const listener of this.listeners) {
      listener(result);
    }
  }

  /** @return The total buffer sizes of all media. */
  size() {
    let res = 0;
    this.downloaderMap.forEach((downloader) => {
      res += downloader.size();
    });
    return res;
  }

  private async download() {
    if (this.isDownloading) {
      return;
    }
    const media = this.mediaList[this.currentMediaIndex];
    if (!media) {
      // All media are processed.
      return;
    }
    const downloader = this.downloaderMap.get(media);
    if (!downloader) {
      throw new Error(
          `${media.url} does not have a downloader. This should not happen.`);
    }
    if (downloader.downloadFinished()) {
      // The current media has finished downloading. Move on to the next one.
      this.currentMediaIndex++;
      this.download();
      return;
    }
    this.isDownloading = true;
    await downloader.downloadAndEnqueueNextChunk();
    this.isDownloading = false;
    this.reportChanges();
    if (this.size() < this.limit) {
      this.download();
    }
  }

  /** Shifts a chunk of the requested media. */
  shift(media: Media) {
    return new Promise<ArrayBuffer|undefined>((resolve, reject) => {
      const downloader = this.downloaderMap.get(media);
      if (!downloader) {
        throw new Error(
            `${media.url} does not have a downloader. This should not happen.`);
      }
      const chunk = downloader.shift();
      if (chunk) {
        this.reportChanges();
        this.download();
        resolve(chunk);
        return;
      }
      if (downloader.downloadFinished()) {
        resolve();
        return;
      }
      // The request media has not been downloaded. Wait until it is ready.
      downloader.registerOnce(() => {
        const chunk = downloader.shift();
        if (chunk) {
          this.reportChanges();
          this.download();
          resolve(chunk);
        } else {
          reject(new Error('Something is wrong'));
        }
      });
    });
  }
}

export interface MediaDownloaderStatus {
  downloadedBytes: number;
  queuedChunks: number;
  finished: boolean;
}

/** Controls how to download a media. */
class MediaDownloader {
  private finished = false;
  private chunks: ArrayBuffer[] = [];
  private startingByte = 0;
  private listeners: Array<() => void> = [];

  constructor(readonly media: Media) {}

  status(): MediaDownloaderStatus {
    return {
      downloadedBytes: this.startingByte,
      queuedChunks: this.size(),
      finished: this.downloadFinished(),
    };
  }

  size() {
    return this.chunks.length;
  }

  /**
   * Listens when a chunk is enqueued. The callback will only be triggered
   * once.
   */
  registerOnce(callback: () => void) {
    this.listeners.push(callback);
  }

  downloadFinished(): boolean {
    return this.finished;
  }

  shift(): ArrayBuffer|undefined {
    return this.chunks.shift();
  }

  /** Downloads next chunk and enqueues the chunk to the buffer. */
  async downloadAndEnqueueNextChunk() {
    if (this.finished) {
      throw new Error(`Attempt to download ${
          this.media.url} after it has finished downloading.`);
    }
    const data = await download(
        this.media.url, this.startingByte, this.startingByte + CHUNK_SIZE - 1);
    if (data.byteLength === 0) {
      console.log(`${this.media.url} is fully downloaded.`);
      this.finished = true;
      return;
    }
    console.log(`Downloaded ${this.media.url} at ${this.startingByte}`);
    this.startingByte += data.byteLength;
    this.chunks.push(data);
    while (this.listeners.length > 0) {
      this.listeners.shift()!();
    }
  }
}
