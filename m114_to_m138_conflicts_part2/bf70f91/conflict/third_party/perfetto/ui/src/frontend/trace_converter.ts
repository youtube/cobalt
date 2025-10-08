// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

<<<<<<< HEAD
import {assetSrc} from '../base/assets';
import {download} from '../base/clipboard';
import {defer} from '../base/deferred';
import {ErrorDetails} from '../base/logging';
import {utf8Decode} from '../base/string_utils';
import {time} from '../base/time';
import {AppImpl} from '../core/app_impl';
import {maybeShowErrorDialog} from './error_dialog';

type Args =
  | UpdateStatusArgs
  | JobCompletedArgs
  | DownloadFileArgs
  | OpenTraceInLegacyArgs
  | ErrorArgs;
=======
import {Actions} from '../common/actions';
import {
  ConversionJobName,
  ConversionJobStatus,
} from '../common/conversion_jobs';

import {download} from './clipboard';
import {maybeShowErrorDialog} from './error_dialog';
import {globals} from './globals';
import {openBufferWithLegacyTraceViewer} from './legacy_trace_viewer';

type Args = UpdateStatusArgs|UpdateJobStatusArgs|DownloadFileArgs|
    OpenTraceInLegacyArgs|ErrorArgs;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

interface UpdateStatusArgs {
  kind: 'updateStatus';
  status: string;
}

<<<<<<< HEAD
interface JobCompletedArgs {
  kind: 'jobCompleted';
=======
interface UpdateJobStatusArgs {
  kind: 'updateJobStatus';
  name: ConversionJobName;
  status: ConversionJobStatus;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

interface DownloadFileArgs {
  kind: 'downloadFile';
  buffer: Uint8Array;
  name: string;
}

interface OpenTraceInLegacyArgs {
  kind: 'openTraceInLegacy';
  buffer: Uint8Array;
}

interface ErrorArgs {
  kind: 'error';
<<<<<<< HEAD
  error: ErrorDetails;
}

type OpenTraceInLegacyCallback = (
  name: string,
  data: ArrayBuffer | string,
  size: number,
) => void;

async function makeWorkerAndPost(
  msg: unknown,
  openTraceInLegacy?: OpenTraceInLegacyCallback,
) {
  const promise = defer<void>();

  function handleOnMessage(msg: MessageEvent): void {
    const args: Args = msg.data;
    if (args.kind === 'updateStatus') {
      AppImpl.instance.omnibox.showStatusMessage(args.status);
    } else if (args.kind === 'jobCompleted') {
      promise.resolve();
    } else if (args.kind === 'downloadFile') {
      download(new File([new Blob([args.buffer])], args.name));
    } else if (args.kind === 'openTraceInLegacy') {
      const str = utf8Decode(args.buffer);
      openTraceInLegacy?.('trace.json', str, 0);
    } else if (args.kind === 'error') {
      maybeShowErrorDialog(args.error);
    } else {
      throw new Error(`Unhandled message ${JSON.stringify(args)}`);
    }
  }

  const worker = new Worker(assetSrc('traceconv_bundle.js'));
  worker.onmessage = handleOnMessage;
  worker.postMessage(msg);
  return promise;
}

export function convertTraceToJsonAndDownload(trace: Blob): Promise<void> {
  return makeWorkerAndPost({
=======
  error: string;
}


function handleOnMessage(msg: MessageEvent): void {
  const args: Args = msg.data;
  if (args.kind === 'updateStatus') {
    globals.dispatch(Actions.updateStatus({
      msg: args.status,
      timestamp: Date.now() / 1000,
    }));
  } else if (args.kind === 'updateJobStatus') {
    globals.setConversionJobStatus(args.name, args.status);
  } else if (args.kind === 'downloadFile') {
    download(new File([new Blob([args.buffer])], args.name));
  } else if (args.kind === 'openTraceInLegacy') {
    const str = (new TextDecoder('utf-8')).decode(args.buffer);
    openBufferWithLegacyTraceViewer('trace.json', str, 0);
  } else if (args.kind === 'error') {
    maybeShowErrorDialog(args.error);
  } else {
    throw new Error(`Unhandled message ${JSON.stringify(args)}`);
  }
}

function makeWorkerAndPost(msg: unknown) {
  const worker = new Worker(globals.root + 'traceconv_bundle.js');
  worker.onmessage = handleOnMessage;
  worker.postMessage(msg);
}

export function convertTraceToJsonAndDownload(trace: Blob) {
  makeWorkerAndPost({
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    kind: 'ConvertTraceAndDownload',
    trace,
    format: 'json',
  });
}

<<<<<<< HEAD
export function convertTraceToSystraceAndDownload(trace: Blob): Promise<void> {
  return makeWorkerAndPost({
=======
export function convertTraceToSystraceAndDownload(trace: Blob) {
  makeWorkerAndPost({
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    kind: 'ConvertTraceAndDownload',
    trace,
    format: 'systrace',
  });
}

<<<<<<< HEAD
export function convertToJson(
  trace: Blob,
  openTraceInLegacy: OpenTraceInLegacyCallback,
  truncate?: 'start' | 'end',
): Promise<void> {
  return makeWorkerAndPost(
    {
      kind: 'ConvertTraceAndOpenInLegacy',
      trace,
      truncate,
    },
    openTraceInLegacy,
  );
}

export function convertTraceToPprofAndDownload(
  trace: Blob,
  pid: number,
  ts: time,
): Promise<void> {
  return makeWorkerAndPost({
=======
export function convertToJson(trace: Blob, truncate?: 'start'|'end') {
  makeWorkerAndPost({
    kind: 'ConvertTraceAndOpenInLegacy',
    trace,
    truncate,
  });
}

export function convertTraceToPprofAndDownload(
    trace: Blob, pid: number, ts: number) {
  makeWorkerAndPost({
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    kind: 'ConvertTraceToPprof',
    trace,
    pid,
    ts,
  });
}
