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

package dev.cobalt.coat;

import java.util.HashMap;

/** A singleton holding the app's crash context. */
public enum CrashContext {
  INSTANCE;

  // TODO(cobalt, b/383301493): at time of writing all clients of this class are on the same thread.
  // But as johnx@ suggested we should either enforce this assumption or use a mutex to guard
  // concurrent access from different threads.
  private final HashMap<String, String> crashContext = new HashMap<>();
  private CrashContextUpdateHandler crashContextUpdateHandler;

  public void setCrashContext(String key, String value) {
    crashContext.put(key, value);
    if (this.crashContextUpdateHandler != null) {
      this.crashContextUpdateHandler.onCrashContextUpdate();
    }
  }

  HashMap<String, String> getCrashContext() {
    return this.crashContext;
  }

  void registerCrashContextUpdateHandler(CrashContextUpdateHandler handler) {
    this.crashContextUpdateHandler = handler;
  }
}
