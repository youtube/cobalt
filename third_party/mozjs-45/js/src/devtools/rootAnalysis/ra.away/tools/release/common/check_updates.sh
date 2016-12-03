check_updates () {
  # called with 6 args - platform, source package, target package, update package, old updater boolean, updates-settings.ini values
  update_platform=$1
  source_package=$2
  target_package=$3
  locale=$4
  use_old_updater=$5
  mar_channel_IDs=$6

  # cleanup
  rm -rf source/*
  rm -rf target/*

  unpack_build $update_platform source "$source_package" $locale '' $mar_channel_IDs
  if [ "$?" != "0" ]; then
    echo "FAILED: cannot unpack_build $update_platform source $source_package"
    return 1
  fi
  unpack_build $update_platform target "$target_package" $locale 
  if [ "$?" != "0" ]; then
    echo "FAILED: cannot unpack_build $update_platform target $target_package"
    return 1
  fi
  
  case $update_platform in
      Darwin_ppc-gcc | Darwin_Universal-gcc3 | Darwin_x86_64-gcc3 | Darwin_x86-gcc3-u-ppc-i386 | Darwin_x86-gcc3-u-i386-x86_64 | Darwin_x86_64-gcc3-u-i386-x86_64) 
          platform_dirname="*.app"
          updater="Contents/MacOS/updater.app/Contents/MacOS/updater"
          binary_file_pattern='^Binary files'
          ;;
      WINNT*) 
          platform_dirname="bin"
          updater="updater.exe"
          binary_file_pattern='^Files.*and.*differ$'
          ;;
      Linux_x86-gcc | Linux_x86-gcc3 | Linux_x86_64-gcc3) 
          platform_dirname=`echo $product | tr '[A-Z]' '[a-z]'`
          updater="updater"
          binary_file_pattern='^Binary files'
          # Bug 1209376. Linux updater linked against other libraries in the installation directory
          export LD_LIBRARY_PATH=$PWD/source/$platform_dirname
          ;;
  esac

  if [ -f update/update.status ]; then rm update/update.status; fi
  if [ -f update/update.log ]; then rm update/update.log; fi

  if [ -d source/$platform_dirname ]; then
    cd source/$platform_dirname;
    cp $updater ../../update
    if [ "$use_old_updater" = "1" ]; then
        ../../update/updater ../../update . 0
    else
        ../../update/updater ../../update . . 0
    fi
    cd ../..
  else
    echo "FAIL: no dir in source/$platform_dirname"
    return 1
  fi

  cat update/update.log
  update_status=`cat update/update.status`

  if [ "$update_status" != "succeeded" ]
  then
    echo "FAIL: update status was not succeeded: $update_status"
    return 1
  fi

  diff -r source/$platform_dirname target/$platform_dirname  > results.diff
  diffErr=$?
  cat results.diff
  grep ^Only results.diff | sed 's/^Only in \(.*\): \(.*\)/\1\/\2/' | \
  while read to_test; do
    if [ -d "$to_test" ]; then 
      echo Contents of $to_test dir only in source or target
      find "$to_test" -ls | grep -v "${to_test}$"
    fi
  done
  grep "$binary_file_pattern" results.diff > /dev/null
  grepErr=$?
  if [ $grepErr == 0 ]
  then
    echo "FAIL: binary files found in diff"
    return 1
  elif [ $grepErr == 1 ]
  then
    if [ -s results.diff ]
    then
      echo "WARN: non-binary files found in diff"
      return 2
    fi
  else
    echo "FAIL: unknown error from grep: $grepErr"
    return 3
  fi
  if [ $diffErr != 0 ]
  then
    echo "FAIL: unknown error from diff: $diffErr"
    return 3
  fi
}
