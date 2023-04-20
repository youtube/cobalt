interface Props {
  sourceBuffer: SourceBuffer;
  name: string;
  maxWriteHeadDistance: number;
}

/** A component that displays the source buffer info. */
export function SourceBufferInfo({sourceBuffer, name, maxWriteHeadDistance}: Props) {
  return `<div>${name} buffered: ${sourceBuffer.buffered.end(0)} sec` +
      `, writeHead: ${sourceBuffer.writeHead} sec` +
      `, maxWriteHeadDistance: ${maxWriteHeadDistance} sec</div>`;
}
