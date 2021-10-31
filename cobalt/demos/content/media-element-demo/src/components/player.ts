import {DownloadBuffer} from '../utils/download_buffer';
import {PlayMode, SwitchMode} from '../utils/enums';
import {LimitedSourceBuffer} from '../utils/limited_source_buffer';
import {Media} from '../utils/media';

import {Component, renderComponent} from './component';
import {DownloadBufferInfo} from './download_buffer_info';
import {SourceBufferInfo} from './source_buffer_info';
import {VideoInfo} from './video_info';

interface Props {
  video?: string|string[];
  audio?: string|string[];
  switchMode?: SwitchMode;
}

/** A component that controls video playback. */
export class Player extends Component<Props> {
  /** The videos to play. */
  private readonly videos: Media[];

  /** The audios to play. */
  private readonly audios: Media[];

  /** The <video> element. */
  private videoEl!: HTMLVideoElement;

  /** The element displaying video download buffer info. */
  private videoDownloadBufferInfo!: Element;

  /** The element displaying audio download buffer info. */
  private audioDownloadBufferInfo!: Element;

  /** The element displaying video source buffer info. */
  private videoSourceBufferInfo!: Element;

  /** The element displaying audio source buffer info. */
  private audioSourceBufferInfo!: Element;

  /** The element displaying video info. */
  private videoInfo!: Element;

  /**
   * Switch mode defines whether to create a new playback session or <video>
   * element after a media finishes playing.
   */
  private switchMode?: SwitchMode;

  /** Play mode defines whether to play progressively or adaptively. */
  private playMode?: PlayMode;

  constructor(props: Props) {
    super(props);
    this.videos = convertToMediaArray(props.video);
    this.audios = convertToMediaArray(props.audio);
  }

  /** @override */
  render() {
    return `
      <div class="video-container"></div>
      <div class="video-info"></div>
      <div class="video-source-buffer-info"></div>
      <div class="audio-source-buffer-info"></div>
      <div class="video-download-buffer-info"></div>
      <div class="audio-download-buffer-info"></div>
    `;
  }

  /** @override */
  async afterRender() {
    this.videoInfo = this.el.querySelector('.video-info') as HTMLVideoElement;
    this.videoEl = document.createElement('video');
    this.videoEl.style.width = '1280px';
    this.videoEl.style.height = '720px';
    this.videoEl.addEventListener('timeupdate', () => {
      this.renderVideoInfo();
    });
    this.videoEl.addEventListener('durationchange', (evt) => {
      this.renderVideoInfo();
    });
    this.el.querySelector('.video-container')!.appendChild(this.videoEl);

    this.videoSourceBufferInfo =
        this.el.querySelector('.video-source-buffer-info')!;
    this.audioSourceBufferInfo =
        this.el.querySelector('.audio-source-buffer-info')!;
    this.videoDownloadBufferInfo =
        this.el.querySelector('.video-download-buffer-info')!;
    this.audioDownloadBufferInfo =
        this.el.querySelector('.audio-download-buffer-info')!;
    this.play();
  }

  private renderVideoInfo() {
    renderComponent(
        VideoInfo, {
          duration: this.videoEl.duration,
          currentTime: this.videoEl.currentTime,
        },
        this.videoInfo);
  }

  private async play() {
    // Do not reorder methods.
    await this.validateParams();
    await this.initPlayMode();
    await this.initSwitchMode(this.props.switchMode);
    if (this.playMode === PlayMode.PROGRESSIVE) {
      this.playProgressiveVideo();
    } else {
      this.playAdaptiveVideo();
    }
  }

  private async initSwitchMode(override?: SwitchMode) {
    // TODO: Add support for RELOAD and RECREATE_ELEMENT.
    this.switchMode = SwitchMode.NORMAL;
  }

  private async initPlayMode() {
    // Because `validateProgressive_` ensures progressive videos cannot be
    // played together with adaptive audios/videos. We can decide whether to use
    // progressive mode by checking the type of the first videos.
    if (this.videos.length > 0 && await this.videos[0].isProgressive()) {
      this.playMode = PlayMode.PROGRESSIVE;
    } else {
      this.playMode = PlayMode.ADAPTIVE;
    }
  }

  /**
   * Plays all videos as progressive videos, assuming only progressive mp4
   * videos are provided.
   */
  private playProgressiveVideo(videoIndex = 0) {
    const currentMedia = this.videos[videoIndex];
    if (!currentMedia) {
      return;
    }
    this.videoEl.src = currentMedia.url;
    const handleVideoEnd = () => {
      this.videoEl.removeEventListener('ended', handleVideoEnd);
      this.playProgressiveVideo(videoIndex++);
    };
    this.videoEl.addEventListener('ended', handleVideoEnd);
    this.videoEl.play();
  }

  /**
   * Plays all videos as adaptive videos.
   * TODO: dynmaically calculate the source buffer MIME.
   */
  private playAdaptiveVideo() {
    const ms = new MediaSource();
    this.videoEl.src = URL.createObjectURL(ms);
    ms.addEventListener('sourceopen', async () => {
      if (this.videos.length > 0) {
        const videoSourceBuffer =
            ms.addSourceBuffer('video/mp4; codecs="avc1.640028"');
        videoSourceBuffer.addEventListener('updateend', () => {
          renderComponent(
              SourceBufferInfo,
              {name: 'Video', sourceBuffer: videoSourceBuffer},
              this.videoSourceBufferInfo);
        });
        const downloadBuffer = new DownloadBuffer(this.videos);
        downloadBuffer.register((reportMap) => {
          renderComponent(
              DownloadBufferInfo, {mediaList: this.videos, reportMap},
              this.videoDownloadBufferInfo);
        });
        new LimitedSourceBuffer(
            this.videoEl, videoSourceBuffer, this.videos, downloadBuffer);
      }
      if (this.audios.length > 0) {
        const audioSourceBuffer =
            ms.addSourceBuffer('audio/mp4; codecs="mp4a.40.2"');
        audioSourceBuffer.addEventListener('updateend', () => {
          renderComponent(
              SourceBufferInfo,
              {name: 'Audio', sourceBuffer: audioSourceBuffer},
              this.audioSourceBufferInfo);
        });
        const downloadBuffer = new DownloadBuffer(this.audios);
        downloadBuffer.register(
            (reportMap) => {renderComponent(
                DownloadBufferInfo, {mediaList: this.audios, reportMap},
                this.audioDownloadBufferInfo)});
        new LimitedSourceBuffer(
            this.videoEl, audioSourceBuffer, this.audios, downloadBuffer);
      }
    });
    this.videoEl.play();
  }

  private async validateParams() {
    this.validateMediaExists();
    await this.validateProgressive_();
  }

  /** Validates at least one video or audio is provided. */
  private validateMediaExists() {
    if (this.videos.length === 0 && this.audios.length === 0) {
      throw new Error(
          `No audio or video is specified. Please pass values to 'audio=' or ` +
          `'video=' params.`);
    }
  }

  /**
   * Validates progressive videos are not played together with adaptive audios.
   */
  async validateProgressive_() {
    let progressiveVideosCount = 0;
    for (const video of this.videos) {
      if (await video.isProgressive()) {
        progressiveVideosCount++;
      }
    }
    if (progressiveVideosCount > 0 &&
        (progressiveVideosCount !== this.videos.length ||
         this.audios.length > 0)) {
      throw new Error(
          'Progressive video[s] cannot be played together with adaptive ' +
          'video[s] and audio[s]');
    }
  }
}

/** Converts filenames to a list of media. */
function convertToMediaArray(filenames?: string|string[]): Media[] {
  if (!filenames) {
    return [];
  }
  if (!Array.isArray(filenames)) {
    return [new Media(filenames)];
  }
  return filenames.map(filename => new Media(filename));
}
