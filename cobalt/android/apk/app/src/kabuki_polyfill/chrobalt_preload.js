import { initializeH5vccPlatformService } from './h5vcc_platform_service.js';
import { initializeHTMLMediaElement } from './html_media_element_extension.js';

export function chrobaltPreload() {
    initializeH5vccPlatformService();
    initializeHTMLMediaElement();
}
