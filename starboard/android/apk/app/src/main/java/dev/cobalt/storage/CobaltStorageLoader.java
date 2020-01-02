// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.storage;

import static dev.cobalt.util.Log.TAG;

import android.text.TextUtils;
import androidx.annotation.Nullable;
import com.google.protobuf.InvalidProtocolBufferException;
import dev.cobalt.storage.StorageProto.Storage;
import dev.cobalt.util.Log;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Arrays;

/** A class to load Cobalt storage from the file system. */
public class CobaltStorageLoader {
  private final File filesDir;

  private static final String STORAGE_NAME_PREFIX = ".starboard";
  private static final String STORAGE_NAME_SUFFIX = ".storage";
  private static final String STORAGE_HEADER = "SAV1";

  /**
   * Initializes the loader with the files directory root.
   *
   * @param filesDir a File object representing the files directory.
   */
  public CobaltStorageLoader(File filesDir) {
    if (filesDir == null) {
      throw new IllegalArgumentException("A valid filesDir object is required");
    }
    this.filesDir = filesDir;
  }

  /**
   * Reads synchronously the Cobalt storage from the file system.
   *
   * @return a snapshot of the Cobalt storage as a proto message.
   */
  public Storage loadStorageSnapshot() {
    String fileName = getStorageFileName();
    if (fileName == null) {
      return Storage.getDefaultInstance();
    }
    byte[] storageBlob = getStorageBlob(fileName);
    if (storageBlob == null) {
      Log.e(TAG, "Failed to get storage blob");
      return Storage.getDefaultInstance();
    }
    if (!validateStorageBlob(storageBlob)) {
      Log.e(TAG, "Invalid storage blob");
      return Storage.getDefaultInstance();
    }
    storageBlob = Arrays.copyOfRange(storageBlob, STORAGE_HEADER.length(), storageBlob.length);
    try {
      return Storage.parseFrom(storageBlob);
    } catch (InvalidProtocolBufferException e) {
      Log.e(TAG, "Failed parsing the blob", e);
    }
    return Storage.getDefaultInstance();
  }

  private boolean validateStorageBlob(byte[] storageBlob) {
    if (storageBlob == null || storageBlob.length < STORAGE_HEADER.length()) {
      return false;
    }
    String header = new String(storageBlob, 0, STORAGE_HEADER.length());
    return header.equals(STORAGE_HEADER);
  }

  @Nullable
  private byte[] getStorageBlob(String fileName) {
    if (TextUtils.isEmpty(fileName)) {
      Log.e(TAG, "Invalid empty file name");
      return null;
    }
    try (FileInputStream in = new FileInputStream(new File(filesDir, fileName));
        ByteArrayOutputStream out = new ByteArrayOutputStream()) {
      byte[] buffer = new byte[8192];
      int len;
      while ((len = in.read(buffer)) != -1) {
        out.write(buffer, 0, len);
      }
      return out.toByteArray();
    } catch (IOException e) {
      Log.e(TAG, "Failed to read storage blob", e);
    }
    return null;
  }

  @Nullable
  private String getStorageFileName() {
    String[] fileNames = filesDir.list();
    if (fileNames == null) {
      Log.w(TAG, "Empty file list");
      return null;
    }
    for (String fileName : fileNames) {
      if (fileName.startsWith(STORAGE_NAME_PREFIX) && fileName.endsWith(STORAGE_NAME_SUFFIX)) {
        File storageFile = new File(filesDir, fileName);
        if (storageFile.length() > 0) {
          return fileName;
        }
      }
    }
    Log.w(TAG, "Failed to find storage file name");
    return null;
  }
}
