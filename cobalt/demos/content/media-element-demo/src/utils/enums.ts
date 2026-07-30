/** A player config defines how to switch between videos. */
export enum SwitchMode {
  // Play adaptive videos under the same playback session using one media
  // source. Play under different playback sessions if adaptive playback is not
  // possible.
  NORMAL = 'normal',
  // Play adaptive videos under different playback sessions.
  RELOAD = 'reload',
  // Use a new video element when switching to the next media file.
  RECREATE_ELEMENT = 'recreate-element',
}

export enum PlayMode {
  PROGRESSIVE = 'progressive',
  ADAPTIVE = 'adaptive',
}
