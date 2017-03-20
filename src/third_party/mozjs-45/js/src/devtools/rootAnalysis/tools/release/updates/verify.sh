#!/bin/bash
#set -x

. ../common/cached_download.sh
. ../common/unpack.sh 
. ../common/download_mars.sh
. ../common/download_builds.sh
. ../common/check_updates.sh

clear_cache
create_cache

ftp_server_to="http://stage.mozilla.org/pub/mozilla.org"
ftp_server_from="http://stage.mozilla.org/pub/mozilla.org"
aus_server="https://aus4.mozilla.org"
to=""

pushd `dirname $0` &>/dev/null
MY_DIR=$(pwd)
popd &>/dev/null
retry="$MY_DIR/../../buildfarm/utils/retry.py -s 1 -r 3"

runmode=0
config_file="updates.cfg"
UPDATE_ONLY=1
TEST_ONLY=2
MARS_ONLY=3
COMPLETE=4

usage()
{
  echo "Usage: verify.sh [OPTION] [CONFIG_FILE]"
  echo "    -u, --update-only      only download update.xml"
  echo "    -t, --test-only        only test that MARs exist"
  echo "    -m, --mars-only        only test MARs"
  echo "    -c, --complete         complete upgrade test"
}

if [ -z "$*" ]
then
  usage
  exit 0
fi

pass_arg_count=0
while [ "$#" -gt "$pass_arg_count" ]
do
  case "$1" in
    -u | --update-only)
      runmode=$UPDATE_ONLY
      shift
      ;;
    -t | --test-only)
      runmode=$TEST_ONLY
      shift
      ;;
    -m | --mars-only)
      runmode=$MARS_ONLY
      shift
      ;;
    -c | --complete)
      runmode=$COMPLETE
      shift
      ;;
    *)
      # Move the unrecognized arg to the end of the list
      arg="$1"
      shift
      set -- "$@" "$arg"
      pass_arg_count=`expr $pass_arg_count + 1`
  esac
done

if [ -n "$arg" ]
then
  config_file=$arg
  echo "Using config file $config_file"
else
  echo "Using default config file $config_file"
fi

if [ "$runmode" == "0" ]
then
  usage
  exit 0
fi

while read entry
do
  # initialize all config variables
  release="" 
  product="" 
  platform="" 
  build_id="" 
  locales=""
  channel=""
  from=""
  patch_types="complete"
  use_old_updater=0
  mar_channel_IDs=""
  eval $entry

  # the arguments for updater changed in Gecko 34/SeaMonkey 2.31
  major_version=`echo $release | cut -f1 -d.`
  if [[ "$product" == "seamonkey" ]]; then
    minor_version=`echo $release | cut -f2 -d.`
    if [[ $major_version -le 2 && $minor_version -lt 31 ]]; then
      use_old_updater=1
    fi
  elif [[ $major_version -lt 34 ]]; then
      use_old_updater=1
  fi

  for locale in $locales
  do
    rm -f update/partial.size update/complete.size
    for patch_type in $patch_types
    do
      if [ "$runmode" == "$MARS_ONLY" ] || [ "$runmode" == "$COMPLETE" ] ||
         [ "$runmode" == "$TEST_ONLY" ]
      then
        if [ "$runmode" == "$TEST_ONLY" ]
        then
          download_mars "${aus_server}/update/3/$product/$release/$build_id/$platform/$locale/$channel/default/default/default/update.xml?force=1" $patch_type 1
          err=$?
        else
          download_mars "${aus_server}/update/3/$product/$release/$build_id/$platform/$locale/$channel/default/default/default/update.xml?force=1" $patch_type
          err=$?
        fi
        if [ "$err" != "0" ]; then
          echo "FAIL: download_mars returned non-zero exit code: $err"
          continue
        fi
      else
        update_path="$product/$release/$build_id/$platform/$locale/$channel/default/default/default"
        mkdir -p updates/$update_path/complete
        mkdir -p updates/$update_path/partial
        $retry wget --no-check-certificate -q -O $patch_type updates/$update_path/$patch_type/update.xml "${aus_server}/update/3/$update_path/update.xml?force=1"

      fi
      if [ "$runmode" == "$COMPLETE" ]
      then
        if [ -z "$from" ] || [ -z "$to" ]
        then
          continue
        fi
        from_path=`echo $from | sed "s/%locale%/${locale}/"`
        to_path=`echo $to | sed "s/%locale%/${locale}/"`
        download_builds "${ftp_server_from}/${from_path}" "${ftp_server_to}/${to_path}"
        err=$?
        if [ "$err" != "0" ]; then
          echo "FAIL: download_builds returned non-zero exit code: $err"
          continue
        fi
        source_file=`basename "$from_path"`
        target_file=`basename "$to_path"`
        check_updates "$platform" "downloads/$source_file" "downloads/$target_file" $locale $use_old_updater $mar_channel_IDs
        err=$?
        if [ "$err" == "0" ]; then
          continue
        elif [ "$err" == "1" ]; then
          echo "FAIL: check_updates returned failure for $platform downloads/$source_file vs. downloads/$target_file: $err"
        elif [ "$err" == "2" ]; then
          echo "WARN: check_updates returned warning for $platform downloads/$source_file vs. downloads/$target_file: $err"
        else
          echo "FAIL: check_updates returned unknown error for $platform downloads/$source_file vs. downloads/$target_file: $err"
        fi
      fi
    done
    if [ -f update/partial.size ] && [ -f update/complete.size ]; then
        partial_size=`cat update/partial.size`
        complete_size=`cat update/complete.size`
        if [ $partial_size -gt $complete_size ]; then
            echo "FAIL: partial updates are larger than complete updates"
        elif [ $partial_size -eq $complete_size ]; then
            echo "WARN: partial updates are the same size as complete updates, this should only happen for major updates"
        else
            echo "SUCCESS: partial updates are smaller than complete updates, all is well in the universe"
        fi
    fi
  done
done < $config_file

clear_cache
