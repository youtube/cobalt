interface Props {
  sourceBuffer: SourceBuffer;
  name: string;
}

/** A component that displays the source buffer info. */
export function SourceBufferInfo({sourceBuffer, name}: Props) {
  return `<div>${name} buffered: ${sourceBuffer.buffered.end(0)} sec</div>`;
}
