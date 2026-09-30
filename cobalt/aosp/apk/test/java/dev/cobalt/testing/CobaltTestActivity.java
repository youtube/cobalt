// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.testing;

import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;
import dev.cobalt.app.MainActivity;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import org.chromium.build.gtest_apk.NativeTestIntent;
import org.chromium.test.reporter.TestStatusReporter;

/**
 * Activity that runs a gtest suite through the Evergreen loader.
 *
 * <p>This is shared by every test apk. The module library and content directory it runs with are
 * passed as meta-data.
 */
public class CobaltTestActivity extends MainActivity {
  private static final String TAG = "CobaltTestInstrumentation";

  // Manifest meta-data holding this apk's loader arguments, written by modular_apk.gni.
  private static final String META_DATA_LIBRARY = "dev.cobalt.testing.EvergreenLibrary";
  private static final String META_DATA_CONTENT = "dev.cobalt.testing.EvergreenContent";

  private TestStatusReporter mReporter;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    mReporter = new TestStatusReporter(this);
    mReporter.testRunStarted(Process.myPid());
    super.onCreate(savedInstanceState);
  }

  @Override
  protected void onDestroy() {
    mReporter.testRunFinished(Process.myPid());
    super.onDestroy();
  }

  @Override
  protected String[] getArgs() {
    ArrayList<String> args = new ArrayList<>();

    Bundle metaData = getMetaData();
    String library = metaData.getString(META_DATA_LIBRARY);
    String content = metaData.getString(META_DATA_CONTENT);
    assert library != null && content != null : "This apk declares no Evergreen module.";

    args.add("--evergreen_library=" + library);
    args.add("--evergreen_content=" + content);

    // Suites running on base::TestLauncher spawn a child process per test, which Evergreen cannot
    // do, so they have to run their tests in-process. nplb has its own main and ignores the flag.
    args.add("--single-process-tests");

    args.addAll(gtestArgsFromIntent());
    Log.i(TAG, "Test loader argv: " + args);
    return args.toArray(new String[0]);
  }

  /** Returns the application meta-data, or an empty bundle if it cannot be read. */
  private Bundle getMetaData() {
    try {
      ApplicationInfo info =
          getPackageManager().getApplicationInfo(getPackageName(), PackageManager.GET_META_DATA);
      if (info.metaData != null) {
        return info.metaData;
      }
    } catch (PackageManager.NameNotFoundException e) {
      Log.e(TAG, "Failed to read the application meta-data", e);
    }
    return new Bundle();
  }

  private ArrayList<String> gtestArgsFromIntent() {
    ArrayList<String> flags = new ArrayList<>();
    Intent intent = getIntent();

    // Flags may arrive as a command-line file, an inline string, or a gtest-filter extra.
    String cmdFile = intent.getStringExtra(NativeTestIntent.EXTRA_COMMAND_LINE_FILE);
    if (cmdFile != null && !cmdFile.isEmpty()) {
      flags.addAll(readCommandLineFile(cmdFile));
    }

    String cmdFlags = intent.getStringExtra(NativeTestIntent.EXTRA_COMMAND_LINE_FLAGS);
    if (cmdFlags != null && !cmdFlags.trim().isEmpty()) {
      for (String flag : cmdFlags.trim().split("\\s+")) {
        flags.add(flag);
      }
    }

    String gtestFilter = intent.getStringExtra(NativeTestIntent.EXTRA_GTEST_FILTER);
    if (gtestFilter != null && !gtestFilter.isEmpty()) {
      flags.add("--gtest_filter=" + gtestFilter);
    }

    // Redirect the loader's stdout/stderr to the runner's stdout file.
    String stdoutFile = intent.getStringExtra(NativeTestIntent.EXTRA_STDOUT_FILE);
    if (stdoutFile != null && !stdoutFile.isEmpty()) {
      flags.add("--android_stdout_file=" + stdoutFile);
    }
    return flags;
  }

  private static ArrayList<String> readCommandLineFile(String path) {
    ArrayList<String> tokens = new ArrayList<>();
    try (BufferedReader reader = new BufferedReader(new FileReader(new File(path)))) {
      StringBuilder builder = new StringBuilder();
      String line;
      while ((line = reader.readLine()) != null) {
        builder.append(line).append(' ');
      }
      String content = builder.toString().trim();
      if (!content.isEmpty()) {
        String[] parts = content.split("\\s+");
        // The first token is conventionally the program name; drop it if it isn't a flag.
        int start = (parts.length > 0 && !parts[0].startsWith("-")) ? 1 : 0;
        for (int i = start; i < parts.length; i++) {
          tokens.add(parts[i]);
        }
      }
    } catch (IOException e) {
      Log.w(TAG, "Failed to read command line file: " + path, e);
    }
    return tokens;
  }
}
