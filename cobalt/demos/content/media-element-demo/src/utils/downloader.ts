import {logError} from '../components/error_logger';

/**
 * Downloads an asset with the provided URL.
 * @param url Asset path.
 * @param begin Start byte.
 * @param end End byte.
 */
export async function download(
    url: string, begin: number, end: number): Promise<ArrayBuffer> {
  const response = await fetch(url, {
    headers: {
      'Range': `bytes=${begin}-${end}`,
    },
  });
  if (response.status === 404) {
    logError(`${url} does not exist.`);
  }
  return response.arrayBuffer();
}
