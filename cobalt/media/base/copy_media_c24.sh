# This script should be executed in //cobalt, then follow the comments at the
# bottom of the script to modify "third_party/chromium/media/BUILD.gn".
#
# There will be a few compiler errors after applying the following steps, that
# have to be manually addressed.  See "NOTE:" section" below.

set -x

mkdir -p third_party/chromium
cp -R ~/cobalt_c24/third_party/chromium/media third_party/chromium/
cp media/base/media_log_record.h third_party/chromium/media/base/media_log_record.h

find third_party/chromium/media -type f -exec sed -i -e 's/namespace media/namespace media_m96/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/media::/media_m96::/g' {} \;

find third_party/chromium/media -type f -exec sed -i -e 's/media\/base\/bind_to_current_loop.h/base\/task\/bind_post_task.h/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/BindToCurrentLoop/base::BindPostTaskToCurrentDefault/g' {} \;

find third_party/chromium/media -type f -exec sed -i -e 's/#include \"media\//#include \"third_party\/chromium\/media\//g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/#ifndef MEDIA_/#ifndef THIRD_PARTY_CHROMIUM_MEDIA_/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/#define MEDIA_/#define THIRD_PARTY_CHROMIUM_MEDIA_/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/#endif  \/\/ MEDIA_/#endif  \/\/ THIRD_PARTY_CHROMIUM_MEDIA_/g' {} \;

# Switch them back
find third_party/chromium/media -type f -exec sed -i -e 's/THIRD_PARTY_CHROMIUM_MEDIA_EXPORT/MEDIA_EXPORT/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/THIRD_PARTY_CHROMIUM_MEDIA_SHMEM_EXPORT/MEDIA_SHMEM_EXPORT/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/THIRD_PARTY_CHROMIUM_MEDIA_LOG_EVENT_TYPELESS/MEDIA_LOG_EVENT_TYPELESS/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/THIRD_PARTY_CHROMIUM_MEDIA_LOG_EVENT_NAMED_DATA/MEDIA_LOG_EVENT_NAMED_DATA/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/THIRD_PARTY_CHROMIUM_MEDIA_LOG_PROPERTY_SUPPORTS_TYPE/MEDIA_LOG_PROPERTY_SUPPORTS_TYPE/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/THIRD_PARTY_CHROMIUM_MEDIA_LOG/MEDIA_LOG/g' {} \;


find third_party/chromium/media -type f -exec sed -i -e 's/chromium_media_test/chromium_media_test_c24/g' {} \;

find third_party/chromium/media -type f -exec sed -i -e 's/base\:\:size/std\:\:size/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/base\/callback_forward.h/base\/functional\/callback_forward.h/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/base\/callback_helpers.h/base\/functional\/callback_helpers.h/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/base::Value::Type::DICTIONARY/base::Value::Type::DICT/g' {} \;

find third_party/chromium/media -type f -exec sed -i -e 's/ToSbTime/InMicroseconds/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/SbTime/int64_t/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/base::strncasecmp/strncasecmp/g' {} \;

find third_party/chromium/media -type f -exec sed -i -e 's/SMPTEST432_1/P3/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/IEC61966_2_1/SRGB/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/ARIB_STD_B67/HLG/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/SMPTEST2084/PQ/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_r.x()/primaries.fRX/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_r.y()/primaries.fRY/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_g.x()/primaries.fGX/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_g.y()/primaries.fGY/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_b.x()/primaries.fBX/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_b.y()/primaries.fBY/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/white_point.x()/primaries.fWX/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/white_point.y()/primaries.fWY/g' {} \;

find third_party/chromium/media -type f -exec sed -i -e 's/primary_r.set_x(val)/primaries.fRX = val/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_r.set_y(val)/primaries.fRY = val/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_g.set_x(val)/primaries.fGX = val/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_g.set_y(val)/primaries.fGY = val/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_b.set_x(val)/primaries.fBX = val/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/primary_b.set_y(val)/primaries.fBY = val/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/white_point.set_x(val)/primaries.fWX = val/g' {} \;
find third_party/chromium/media -type f -exec sed -i -e 's/white_point.set_y(val)/primaries.fWY = val/g' {} \;

sed -i -e 's/#include \"base\/cxx17_backports\.h\"/#include \<array\>\n\n#include \"base\/cxx17_backports\.h\"/g' third_party/chromium/media/formats/mpeg/adts_constants.cc
sed -i -e 's/reinterpret_cast<const char\*>(buf_ + pos_)/buf_ + pos_/g' third_party/chromium/media/formats/mp4/box_reader.cc


set +x

# Add the following to "deps" in "third_party/chromium/media/BUILD.gn".
#    "//ui/gfx:gfx",
#
# Search for "sources += [" and remove the whole section for all cobalt specialized source code.

# Add "//third_party/chromium/media" to "deps" section of cobalt/media/BUILD.gn, somewhere below "//media".

# NOTE:
#   Now manually address the few build errors.

# Diffs for WebMediaPlayer interface: https://github.com/youtube/cobalt/pull/1910 (go/cobalt-cl/270181)
# Other media related source code
# cobalt/dom/source_buffer.cc
# cobalt/dom/source_buffer_algorithm.cc
# cobalt/dom/source_buffer_metrics.h
# cobalt/dom/source_buffer_metrics.cc
# cobalt/dom/source_buffer_metrics_test.cc
# cobalt/dom/html_media_element.cc
# cobalt/dom/media_settings.h
# cobalt/dom/media_settings.cc
# cobalt/dom/media_settings_test.cc
# cobalt/dom/media_source.h
# cobalt/dom/media_source.cc
