// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import {
  EmptyThumbnailError,
  LoadError,
  MimeType,
  PlayError,
  PlayMalformedError,
} from './type.js';
import {canvasToJpegBlob, newDrawingCanvas} from './util.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * Converts the element to a jpeg blob by drawing it on a canvas.
 *
 * @param element Source element.
 * @param width Canvas width.
 * @param height Canvas height.
 * @return Converted jpeg blob.
 * @throws {EmptyThumbnailError} Thrown when the data to generate thumbnail is
 *     empty.
 */
async function elementToJpegBlob(
    element: CanvasImageSource, width: number, height: number): Promise<Blob> {
  const {canvas, ctx} = newDrawingCanvas({width, height});
  ctx.drawImage(element, 0, 0, width, height);

  /* A one-dimensional pixels array in RGBA order. */
  const data = ctx.getImageData(0, 0, width, height).data;
  if (data.every((byte) => byte === 0)) {
    throw new EmptyThumbnailError();
  }

  return canvasToJpegBlob(canvas);
}

/**
 * Loads the blob into a <video> element.
 *
 * @throws Thrown when it fails to load video.
 */
async function loadVideoBlob(blob: Blob): Promise<HTMLVideoElement> {
  const el = document.createElement('video');
  try {
    const hasLoaded = new WaitableEvent<boolean>();
    el.addEventListener('error', () => {
      hasLoaded.signal(false);
    });
    el.addEventListener('loadeddata', () => {
      hasLoaded.signal(true);
    });
    const gotFrame = new WaitableEvent();
    el.requestVideoFrameCallback(() => {
      gotFrame.signal();
    });
    // Since callbacks registered in `requestVideoFrameCallback` doesn't fire
    // when the page is in the background, use `timeupdate` event to indicate
    // that the video has been played successfully. Note that waiting until
    // `canplay` is not enough (see b/172214187).
    el.addEventListener('timeupdate', () => {
      gotFrame.signal();
    }, {once: true});

    el.preload = 'auto';
    el.muted = true;
    el.src = URL.createObjectURL(blob);
    if (!(await hasLoaded.wait())) {
      throw new LoadError(el.error?.message);
    }

    try {
      await el.play();
    } catch (e) {
      throw new PlayError(assertInstanceof(e, Error).message);
    }

    try {
      // `gotFrame` may not resolve when playing malformed video. Set 1 second
      // timeout here to prevent UI from being blocked forever.
      await gotFrame.timedWait(1000);
    } catch (e) {
      throw new PlayMalformedError(assertInstanceof(e, Error).message);
    } finally {
      el.pause();
    }
  } finally {
    URL.revokeObjectURL(el.src);
  }
  return el;
}

/**
 * Loads the blob into an <img> element.
 */
async function loadImageBlob(blob: Blob): Promise<HTMLImageElement> {
  const el = new Image();
  try {
    await new Promise<void>((resolve, reject) => {
      el.addEventListener('error', () => {
        reject(new Error('Failed to load image'));
      });
      el.addEventListener('load', () => {
        resolve();
      });
      el.src = URL.createObjectURL(blob);
    });
  } finally {
    URL.revokeObjectURL(el.src);
  }
  return el;
}

/**
 * Creates a thumbnail of video by scaling the first frame to the target size.
 *
 * @param blob Blob of video to be scaled.
 * @param width Target width.
 * @param height Target height. Preserve the aspect ratio if not set.
 * @return Promise of the thumbnail as a jpeg blob.
 */
async function scaleVideo(
    blob: Blob, width: number, height?: number): Promise<Blob> {
  const el = await loadVideoBlob(blob);
  if (height === undefined) {
    height = Math.round(width * el.videoHeight / el.videoWidth);
  }
  return elementToJpegBlob(el, width, height);
}

/**
 * Creates a thumbnail of image by scaling it to the target size.
 *
 * @param blob Blob of image to be scaled.
 * @param width Target width.
 * @param height Target height. Preserve the aspect ratio if not set.
 * @return Promise of the thumbnail as a jpeg blob.
 */
export async function scaleImage(
    blob: Blob, width: number, height?: number): Promise<Blob> {
  const el = await loadImageBlob(blob);
  if (height === undefined) {
    height = Math.round(width * el.naturalHeight / el.naturalWidth);
  }
  return elementToJpegBlob(el, width, height);
}

/**
 * Failed to find image in pdf error.
 */
class NoImageInPdfError extends Error {
  constructor(message = 'Failed to find image in pdf') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Gets image embedded in a PDF.
 *
 * @param blob Blob of PDF.
 * @return Promise resolved to image blob inside PDF.
 */
async function getImageFromPdf(blob: Blob): Promise<Blob> {
  const buf = await blob.arrayBuffer();
  const view = new Uint8Array(buf);
  let i = 0;
  /**
   * Finds |patterns| in view starting from |i| and moves |i| to end of found
   * pattern index.
   *
   * @return Returns begin of found pattern index or -1 for no further pattern
   *     is found.
   */
  function findPattern(...patterns: number[]): number {
    for (; i + patterns.length < view.length; i++) {
      if (patterns.every((b, index) => b === view[i + index])) {
        const ret = i;
        i += patterns.length;
        return ret;
      }
    }
    return -1;
  }
  // Parse object contains /Subtype /Image name and field from pdf format:
  // <</Name1 /Field1... \n/Name2... >>...<<...>>
  // The jpeg stream will follow the target object with length in field of
  // /Length.
  while (i < view.length) {
    const start = findPattern(0x3c, 0x3c);  // <<
    if (start === -1) {
      throw new NoImageInPdfError();
    }
    const end = findPattern(0x3e, 0x3e);  // >>
    if (end === -1) {
      throw new NoImageInPdfError();
    }
    const s = String.fromCharCode(...view.slice(start + 2, end));
    const objs = s.split('\n');
    let isImage = false;
    let length = 0;
    for (const obj of objs) {
      const [name, field] = obj.split(' ');
      switch (name) {
        case '/Subtype':
          isImage = field === '/Image';
          break;
        case '/Length':
          length = Number(field);
          break;
        default:
          // nothing to do.
      }
    }
    if (isImage) {
      i += ' stream\n'.length;
      return new Blob([buf.slice(i, i + length)], {type: 'image/jpeg'});
    }
  }
  throw new NoImageInPdfError();
}

/**
 * Throws when the input blob type is not supported by thumbnailer.
 */
class InvalidBlobTypeError extends Error {
  constructor(type: string) {
    super(`Invalid thumbnailer blob input type: ${type}`);
    this.name = this.constructor.name;
  }
}

/**
 * For non-video type cover, keeps the original size as possible to support drag
 * drop share. Scales video type which don't support drag drop share.
 */
const VIDEO_COVER_WIDTH = 240;

/**
 * Extracts image blob from an arbitrary type of blob.
 *
 * @return Resolved to the image blob.
 */
export async function extractImageFromBlob(blob: Blob): Promise<Blob> {
  switch (blob.type) {
    case MimeType.GIF:
    case MimeType.JPEG:
      return blob;
    case MimeType.MP4:
      return scaleVideo(blob, VIDEO_COVER_WIDTH);
    case MimeType.PDF:
      return getImageFromPdf(blob);
    default:
      throw new InvalidBlobTypeError(blob.type);
  }
}
