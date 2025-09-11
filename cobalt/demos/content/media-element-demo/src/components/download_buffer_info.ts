import {MediaDownloaderStatus} from '../utils/download_buffer';
import {Media} from '../utils/media';

interface Props {
  mediaList: Media[];
  reportMap: Map<Media, MediaDownloaderStatus>;
}

/** A component that displays the download buffer info. */
export function DownloadBufferInfo({mediaList, reportMap}: Props) {
  const elements = mediaList.map((video) => {
    const report = reportMap.get(video);
    if (!report) {
      throw new Error(`Download buffer info for ${video.url} not found.`);
    }
    return `
      <div>
        ${video.url}: downloaded ${report.downloadedBytes} bytes ${
        report.finished ? '[Done]' : ''}, queued ${report.queuedChunks} chunks.
      </div>`;
  });
  return elements.join('');
}
