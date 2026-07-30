# Android GameActivity

The library in this directory is copied from the AndroidX GameActivity
release package.

To learn more about GameActivity, refer to [the official GameActivity
documentation](https://d.android.com/games/agdk/game-activity).

The library in this directory is used by the Android in
//starboard/android/apk/app/build.gradle and also the native files are
automatically extracted and used in //starboard/android/shared/BUILD.gn.

## Updating instructions

To update GameActivity to the latest version, do the following:

1. Download the .aar file from Google Maven at
   https://maven.google.com/web/index.html#androidx.games:games-activity
   into this directory.

1. Update `game_activity_aar_file` in //starboard/android/shared/BUILD.gn
   to reflect the new .aar filename.

1. Delete the old .aar file -- there should only be a single .aar in this
   directory.
