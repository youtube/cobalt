interface Props {
  duration: number;
  currentTime: number;
}

/** A component that displays video info. */
export function VideoInfo({duration, currentTime}: Props) {
  return `<div>${currentTime} / ${duration}</div>`;
}
