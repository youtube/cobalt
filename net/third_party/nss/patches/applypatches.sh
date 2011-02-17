# Run this script in the mozilla/security/nss/lib directory in a NSS source
# tree.
#
# Point patches_dir to the src/net/third_party/nss/patches directory in a
# chromium source tree.
patches_dir=/Users/wtc/chrome1/src/net/third_party/nss/patches

patch -p5 < $patches_dir/nextproto.patch

patch -p4 < $patches_dir/falsestart.patch
patch -p4 < $patches_dir/falsestart2.patch

patch -p5 < $patches_dir/versionskew.patch

patch -p4 < $patches_dir/renegoscsv.patch

patch -p4 < $patches_dir/cachecerts.patch

patch -p4 < $patches_dir/weakserverkey.patch

patch -p5 < $patches_dir/snapstart.patch
patch -p3 < $patches_dir/snapstart2.patch

patch -p3 < $patches_dir/peercertchain.patch

patch -p4 < $patches_dir/ocspstapling.patch

patch -p4 < $patches_dir/clientauth.patch
