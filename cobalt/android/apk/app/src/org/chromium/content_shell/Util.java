// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

package org.chromium.content_shell;

import android.view.View;
import android.view.ViewGroup;
import android.util.Log;

public class Util {
  private static String TAG = "cobalt";

  public static void printRootViewHierarchy(View rootView) {
    Log.i(TAG, "========== Dumping View Hierarchy ==========");
    printViewHierarchy(rootView, 0);
    Log.i(TAG, "==========================================");
  }

  private static void printViewHierarchy(View view, int depth) {
    // Build the indent string for nice formatting
    StringBuilder indent = new StringBuilder();
    for (int i = 0; i < depth; i++) {
        indent.append("  ");
    }

    // Get the unique identifier for the view object and format it as a hex string
    String address = Integer.toHexString(System.identityHashCode(view));

    // Log the view's class name and ID
    Log.i(TAG, indent + "- " + view.getClass().getSimpleName() + "@" + address);

    // If the view is a ViewGroup, recursively call this method for its children
    if (view instanceof ViewGroup) {
        ViewGroup viewGroup = (ViewGroup) view;
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View child = viewGroup.getChildAt(i);
            printViewHierarchy(child, depth + 1);
        }
    }
  }
}
