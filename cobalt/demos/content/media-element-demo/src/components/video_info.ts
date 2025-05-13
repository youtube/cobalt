interface Props {
  duration: number;
  currentTime: number;
  audioConnectors: string;
}

/** A component that displays video info. */
export function VideoInfo({duration, currentTime, audioConnectors}: Props) {
  if (audioConnectors) {
    return `<div>${currentTime} / ${duration} / audioConnectors: ${audioConnectors}</div>`;
  }
  return `<div>${currentTime} / ${duration}</div>`;
}
