# Android GameActivity code description

The source code in this directory is copied from the AndroidX GameActivity
release package, matching the version specified in
 `//starboard/android/apk/app/build.gradle`.

To learn more about GameActivity, refer to [the official GameActivity
documenation](https://d.android.com/games/agdk/game-activity).

## Updating instructions

To update GameActivity to the latest version, do the following:

1. In
   `//starboard/android/apk/app/build.gradle`, update the dependency version
   for `androidx.games::games-activity`. The current version is `1.2.1`, and
   you can find the latest version from [the AndroidX games release website]
   (https://developer.android.com/jetpack/androidx/releases/games).
1. Build Cobalt. This triggers gradle to downloaded the release package to
   its local cache (normally under the `$HOME/.gradle` folder).
1. Find the downloaded game-activity package, usually under
   `$HOME/.gradle/caches/...`. The directory structure should match the
   structure under `//third_party/android_game_activity/include/...`. You can
   use `find` with a specific file to locate the exact path for the package,
   as shown in the following example:
   ```
     find   ~/.gradle/caches   | grep   GameActivity.cpp
   ```
1. Copy all C++ files for the matching games-activity version to this
   directory (the directory that is hosting this README.md file). For example,
   with version 1.2.1, the path might be `$HOME/.gradle/caches/transforms-3/355ab20937e7dabe38cca2293f9f651b/transformed/jetified-games-activity-1.2.1/prefab/modules/game-activity/include/game-activity/GameActivity.cpp`, just pull the content from
   `.../jetified-games-activity-1.2.1/prefab/modules/game-activity`:
   ```
   pushd ${cobalt_src_dir}/third_party/android_game_activity
   rm -fr include module.json
   cp -fr .../jetified-games-activity-1.2.1/prefab/modules/game-activity  third_party/android_game_activity/
   popd
   ```
