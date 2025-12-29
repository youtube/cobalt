// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import androidx.annotation.GuardedBy;
import com.google.android.gms.ads.identifier.AdvertisingIdClient;
import com.google.android.gms.common.GooglePlayServicesNotAvailableException;
import com.google.android.gms.common.GooglePlayServicesRepairableException;
import dev.cobalt.util.Log;
import java.io.IOException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** A class for managing the IfA for CoAT. https://developer.android.com/training/articles/ad-id */
public class AdvertisingId {
  private final Context mContext;
  private final ExecutorService mSingleThreadExecutor;

  @GuardedBy("mAdvertisingIdInfoLock")
  private volatile AdvertisingIdClient.Info mAdvertisingIdInfo;

  // Controls access to mAdvertisingIdInfo
  private final Object mAdvertisingIdInfoLock = new Object();

  public AdvertisingId(Context context) {
    mContext = context;
    mSingleThreadExecutor = Executors.newSingleThreadExecutor();
    mAdvertisingIdInfo = null;
    refresh();
  }

  public void refresh() {
    mSingleThreadExecutor.execute(
        () -> {
          try {
            // The following statement may be slow.
            AdvertisingIdClient.Info info = AdvertisingIdClient.getAdvertisingIdInfo(mContext);
            synchronized (mAdvertisingIdInfoLock) {
              mAdvertisingIdInfo = info;
            }
            Log.i(TAG, "Successfully retrieved Advertising ID (IfA).");
          } catch (IOException
              | GooglePlayServicesNotAvailableException
              | GooglePlayServicesRepairableException e) {
            Log.e(TAG, "Failed to retrieve Advertising ID (IfA).");
          }
        });
  }

  public String getId() {
    String result = "";
    synchronized (mAdvertisingIdInfoLock) {
      if (mAdvertisingIdInfo != null) {
        result = mAdvertisingIdInfo.getId();
      }
    }
    refresh();
    Log.d(TAG, "Returning IfA getId: " + result);
    return result;
  }

  public boolean isLimitAdTrackingEnabled() {
    boolean result = false;
    synchronized (mAdvertisingIdInfoLock) {
      if (mAdvertisingIdInfo != null) {
        result = mAdvertisingIdInfo.isLimitAdTrackingEnabled();
      }
    }
    refresh();
    Log.d(TAG, "Returning IfA LimitedAdTrackingEnabled: " + Boolean.toString(result));
    return result;
  }
}
