import {logError} from '../components/error_logger';

import {download} from './downloader';

/** A class that holds media metadata. */
export class Media {
  readonly url: string;
  private metadata?: MediaMetadata;

  constructor(filename: string) {
    /** @type {string} Asset url. */
    this.url = `assets/${filename}`;
  }

  async getMetadata(): Promise<MediaMetadata> {
    if (!this.metadata) {
      this.metadata = await this.fetchMetadata();
    }
    return this.metadata;
  }

  /**
   * @return Whether the video is a progressive video.
   */
  async isProgressive(): Promise<boolean> {
    const metadata = await this.getMetadata();
    return metadata.type === 'mp42';
  }

  /** Downloads the head and builds metadata. */
  private async fetchMetadata(): Promise<MediaMetadata> {
    if (this.url.substr(-3) !== 'mp4') {
      // TODO: Add more media type support.
      return {};
    }
    // Metadata should be included in the first 10K.
    const buffer = await download(this.url, 0, 1024 * 10);
    if (buffer.byteLength === 0) {
      return {};
    }
    const headBuffer = buffer.slice(0, getSize(buffer));
    const type = getFtypType(headBuffer);
    return {
      type,
    };
  }
}

interface MediaMetadata {
  /** The ftyp type. */
  type?: string;
}

/**
 * Gets the buffer size from the first 4 bytes.
 * @return Buffer size.
 */
function getSize(buffer: ArrayBuffer): number {
  const dv = new DataView(buffer.slice(0, 4));
  return dv.getUint32(0);
}

/**
 * Turns the buffer into a string.
 * @return The generated string.
 */
function stringifyBuffer(
    buffer: ArrayBuffer, begin: number, end: number): string {
  return String.fromCharCode.apply(
      null, Array.from(new Uint8Array(buffer.slice(begin, end))));
}

/**
 * Gets the ftyp type from the buffer.
 * @return The ftyp type. Undefined if there is an error.
 */
function getFtypType(buffer: ArrayBuffer): string|undefined {
  const code = stringifyBuffer(buffer, 4, 8);
  if (code !== 'ftyp') {
    logError(`unknown type code ${code}`);
    return;
  }
  return stringifyBuffer(buffer, 8, 12);
}
