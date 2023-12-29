### Metadata

This directory contains tools for validating METADATA files present in
upstream/third party directories.

The metadata_file.proto comes from Android Open Source project ( 427dacb )

https://android.googlesource.com/platform/build/+log/main/tools/protos

The files in Cobalt repo need to conform to this proto schema. However,
the validation rules are different.

Specifically, we use Identifier to track subdirectory code imports from
Chromium upstream as follows:

itentifer {
    type: "cobalt_chrome_dir"
    value: "/base" # origin directory from chrome repo
    version: "114.1.1" # origin tag or ref
}
