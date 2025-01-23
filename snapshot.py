import json
import os
import zipfile

def zip_files_from_json(json_data, output_zip_name="archive.zip"):
    """Zips files and directories specified in a JSON structure.

    Args:
        json_data: A dictionary containing the JSON data.
        output_zip_name: The name of the output zip file.
    """

    try:
        data = json.loads(json_data)  # If json_data is a string
    except json.JSONDecodeError:
        data = json_data  # If json_data is already a dictionary

    with zipfile.ZipFile(output_zip_name, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for archive_data in data.get("archive_datas", []):
            files = archive_data.get("files", [])
            dirs = archive_data.get("dirs", [])

            for file_path in files:
                if os.path.exists(file_path):
                    print(f"Adding file: {file_path}")
                    zipf.write(file_path, arcname=os.path.basename(file_path)) # Store only the file name in the archive
                else:
                    print(f"Warning: File not found: {file_path}")

            for dir_path in dirs:
                if os.path.exists(dir_path):
                    print(f"Adding directory: {dir_path}")
                    for root, _, files_in_dir in os.walk(dir_path):
                        for file in files_in_dir:
                            file_path = os.path.join(root, file)
                            zipf.write(file_path, arcname=os.path.relpath(file_path, start=dir_path)) # Preserve directory structure within the archive
                else:
                    print(f"Warning: Directory not found: {dir_path}")


# Example usage (assuming the JSON data is in a string variable called 'json_string'):
json_string = """
{
    "archive_datas": [
        {
            "files": [
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/org/chromium/base/natives/GEN_JNI.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/org/chromium/build/NativeLibraries.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/org/chromium/build/BuildConfig.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/gen/base_module/R.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/androidx/appcompat/R.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/androidx/appcompat/resources/R.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/androidx/fragment/R.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/androidx/core/R.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/org/chromium/ui/R.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/org/chromium/content/R.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/org/chromium/content_shell/R.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/org/chromium/content_shell_apk/R.java",
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars/org/chromium/media/R.java",
                "../../cobalt/android/apk/app/proguard-rules.pro",
                "obj/third_party/android_deps/chromium_play_services_availability_java.javac.jar",
                "obj/components/metrics/metrics_java.javac.jar",
                "../../content/shell/android/java/res/layout/shell_view.xml",
                "../../content/shell/android/shell_apk/res/layout/content_shell_activity.xml",
                "../../content/shell/android/java/res/drawable/progress.xml",
                "snapshot_blob.bin",
                "icudtl.dat",
                "content_shell.pak",
                "libcobalt_content_shell_content_view.so",
                "libstarboard_jni_state.so",
                "gen/cobalt/android/cobalt_manifest/AndroidManifest.xml"
            ],
            "dirs": [
                "gen/cobalt/android/cobalt_apk/generated_java/input_srcjars",
                "lib.java",
                "../../cobalt/android/apk/app/src/main/res"
            ]
        }
    ]
}
"""

zip_files_from_json(json_string) # Creates archive.zip

# Or, if the JSON is already parsed as a Python dictionary:
import json
json_data = json.loads(json_string)
zip_files_from_json(json_data, "my_archive.zip") # Creates my_archive.zip
A

