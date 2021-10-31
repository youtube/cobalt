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

import android.os.Build;
import android.os.FileObserver;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import com.google.protobuf.InvalidProtocolBufferException;
import dev.cobalt.storage.StorageProto.Storage;
import dev.cobalt.util.Log;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Arrays;

/**
 * A class to load Cobalt storage from the file system.
 */
public class CobaltStorageLoader {

  private final File filesDir;
  private final StorageFileObserver fileObserver;

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
    this.fileObserver = StorageFileObserver.create(filesDir);
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
    Storage storage = loadStorageFile(new File(filesDir, fileName));
    return (storage != null) ? storage : Storage.getDefaultInstance();
  }

  @Nullable
  private static Storage loadStorageFile(File file) {
    byte[] storageBlob = getStorageBlob(file);
    if (storageBlob == null) {
      Log.e(TAG, "Failed to get storage blob");
      return null;
    }
    if (!validateStorageBlob(storageBlob)) {
      Log.e(TAG, "Invalid storage blob");
      return null;
    }
    storageBlob = Arrays.copyOfRange(storageBlob, STORAGE_HEADER.length(), storageBlob.length);
    try {
      return Storage.parseFrom(storageBlob);
    } catch (InvalidProtocolBufferException e) {
      Log.e(TAG, "Failed parsing the blob", e);
    }
    return null;
  }

  private static boolean validateStorageBlob(byte[] storageBlob) {
    if (storageBlob == null || storageBlob.length < STORAGE_HEADER.length()) {
      return false;
    }
    String header = new String(storageBlob, 0, STORAGE_HEADER.length());
    return header.equals(STORAGE_HEADER);
  }

  @Nullable
  private static byte[] getStorageBlob(File file) {
    try (FileInputStream in = new FileInputStream(file);
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
      if (isStorageFile(fileName)) {
        File storageFile = new File(filesDir, fileName);
        if (storageFile.length() > 0) {
          return fileName;
        }
      }
    }
    Log.w(TAG, "Failed to find storage file name");
    return null;
  }

  private static boolean isStorageFile(String fileName) {
    return fileName != null
        && fileName.startsWith(STORAGE_NAME_PREFIX)
        && fileName.endsWith(STORAGE_NAME_SUFFIX);
  }

  /**
   * Set the observer to be notified when storage changes.
   *
   * @param storageObserver will be called when storage changes.
   */
  public void setObserver(@Nullable CobaltStorageObserver storageObserver) {
    fileObserver.setStorageObserver(storageObserver);
  }

  /**
   * Observer that is called when Cobalt storage changes.
   */
  public interface CobaltStorageObserver {

    /**
     * Called when Cobalt storage changes.
     *
     * <p>This method is invoked on a special thread. It runs independently of any threads, so take
     * care to use appropriate synchronization! Consider using Handler#post(Runnable) to shift event
     * handling work to the main thread to avoid concurrency problems.
     *
     * @param snapshot the Cobalt storage as a proto message.
     */
    void onCobaltStorageChanged(Storage snapshot);
  }

  private static class StorageFileObserver extends FileObserver {

    private final File filesDir;
    private CobaltStorageObserver storageObserver = null;

    /**
     * Factory method to differentiate which constructor to use.
     */
    static StorageFileObserver create(File filesDir) {
      if (Build.VERSION.SDK_INT >= 29) {
        return new StorageFileObserver(filesDir);
      } else {
        return new StorageFileObserver(filesDir.getAbsolutePath());
      }
    }

    @SuppressWarnings("deprecation")
    StorageFileObserver(String filesDirPath) {
      super(filesDirPath, MOVED_TO);
      this.filesDir = new File(filesDirPath);
    }

    @RequiresApi(29)
    StorageFileObserver(File filesDir) {
      super(filesDir, MOVED_TO);
      this.filesDir = filesDir;
    }

    void setStorageObserver(@Nullable CobaltStorageObserver storageObserver) {
      if (this.storageObserver != null) {
        stopWatching();
      }
      this.storageObserver = storageObserver;
      if (this.storageObserver != null) {
        startWatching();
      }
    }

    @Override
    public void onEvent(int event, @Nullable String fileName) {
      // Expect a MOVED_TO event since SbStorageWrite() creates a temp file then moves it in place.
      if (event != MOVED_TO || storageObserver == null || !isStorageFile(fileName)) {
        return;
      }
      Storage storage = loadStorageFile(new File(filesDir, fileName));
      if (storage != null) {
        storageObserver.onCobaltStorageChanged(storage);
      }
    }
  }
}
