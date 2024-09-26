// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VulkanInfo_Deserialize} from './vulkan_info.mojom-webui.js';

export class VulkanInfo {
  constructor(base64Data) {
    const array = Uint8Array.from(atob(base64Data), c => c.charCodeAt(0));
    const dataView = new DataView(array.buffer);
    this.vulkanInfo_ = VulkanInfo_Deserialize(dataView);
    this.beautify(this.vulkanInfo_);
  }

  beautify(obj) {
    for (const key of Object.keys(obj)) {
      const value = obj[key];

      if (key === 'specVersion') {
        continue;
      }

      if (key.endsWith('Version')) {
        obj[key] = this.beautifyVersion(value);
        continue;
      }

      if (key === 'extensions' || key === 'instanceExtensions') {
        obj[key] = this.beautifyExtensions(value);
        continue;
      }

      if (key.endsWith('UUID')) {
        obj[key] = this.beautifyUUID(value);
        continue;
      }

      if (typeof value === 'bigint') {
        // JSON.stringify() doesn't support bigint.
        obj[key] = Number(value);
        continue;
      }

      if (typeof value === 'object') {
        this.beautify(value);
        continue;
      }
    }
  }

  beautifyVersion(version) {
    const major = version >> 22;
    const minor = (version >> 12) & 0x3ff;
    const patch = version & 0xfff;
    return `${major}.${minor}.${patch}`;
  }

  beautifyUUID(uuid) {
    // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    let result = '';
    for (let i = 0; i < 16; ++i) {
      const value = uuid[i];
      if (i === 4 || i === 6 || i === 8 || i === 10) {
        result += '-';
      }
      if (value < 0x10) {
        result += '0';
      }
      result += value.toString(16);
    }
    return result;
  }

  beautifyExtensions(extensions) {
    const result = {};
    for (const extension of extensions) {
      const name = extension['extensionName'];
      const version = extension['specVersion'];
      result[name] = version;
    }
    return result;
  }

  toString() {
    return JSON.stringify(this.vulkanInfo_, null, 2);
  }
}
