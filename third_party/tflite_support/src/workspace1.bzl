"""
Partial workspace defintion for the TFLite Support Library. See WORKSPACE for usage.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@android_test_support//:repo.bzl", "android_test_repositories")
load("@rules_jvm_external//:defs.bzl", "maven_install")

def tflite_support_workspace1():
    """Partial workspace definition for the TFLite Support Library. See WORKSPACE for usage."""
    android_test_repositories()

    # Maven dependencies.
    maven_install(
        artifacts = [
            "androidx.annotation:annotation:aar:1.1.0",
            "androidx.annotation:annotation-experimental:1.1.0",
            "androidx.multidex:multidex:jar:2.0.1",
            "androidx.test:core:jar:1.3.0",
            "androidx.test.ext:junit:jar:1.1.2",
            "androidx.test:runner:jar:1.3.0",
            "com.google.android.odml:image:aar:1.0.0-beta1",
            "com.google.truth:truth:jar:1.1",
            "commons-io:commons-io:jar:2.8.0",
            # Mockito >= 3.4.6 cannot pass bazel desugar.
            "org.mockito:mockito-android:jar:3.0.0",
            "org.mockito:mockito-core:jar:3.0.0",
            "org.mockito:mockito-inline:jar:3.0.0",
            "org.robolectric:robolectric:jar:4.7.3",
            "junit:junit:jar:4.13",
        ],
        repositories = [
            "https://maven.google.com",
            "https://dl.google.com/dl/android/maven2",
            "https://repo1.maven.org/maven2",
        ],
        fetch_sources = True,
        version_conflict_policy = "pinned",
    )

    http_archive(
        name = "tf_toolchains",
        sha256 = "d550e6260b7bd8a7f2c2e3a20ba3c4b6c519985f5a164a1bf3601dd96852a05b",
        strip_prefix = "toolchains-1.4.6",
        urls = [
            "http://mirror.tensorflow.org/github.com/tensorflow/toolchains/archive/v1.4.6.tar.gz",
            "https://github.com/tensorflow/toolchains/archive/v1.4.6.tar.gz",
        ],
    )
